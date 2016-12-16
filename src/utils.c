///////// Pour ptar 1.3 minimum /////////
/*

Fonctions utils

Fonctions principales du programme ptar :
Correspondantes aux options suivantes :
-x : Extraction.
-l : Listing détaillé.
-z : Décompression gzip avec la bibliothèque zlib.
-p NBTHREADS : Durabilité et parallélisation de l'opération avec un nombre de threads choisi.
-e : Ecriture d'un logfile.txt. Seulement compatible avec l'extraction (-x) et la décompression (-z) pour le moment.

*/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <utime.h>
#include <dlfcn.h>
#include <wait.h>
#include <math.h>
#include <limits.h>
#include <pthread.h>

#include "checkfile.h"
#include "utils.h"

/*
Fonction d'appel des threads.
Fonction principale : recueille les header de chaque fichier dans l'archive (compressée ou non) ainsi que les données suivantes chaque header si il y en a.
Appelle ensuite les diverses fonctions utiles au traitement souhaité.
*/

void *traitement(char *folder) {

	/*
	Déclarations et initialisation des variables
	*/

	headerTar head;										//Structure des header tar POSIX ustar.
	struct utimbuf tm; 	   						//Structure pour le utime final des dossiers.

	char *data; 		   								//Buffer pour les données suivant le header.
	char dirlist[MAXDIR][PATHLENGTH];	//Liste des dossiers. La taille du chemin d'accès est limitée à 255 octets, contenance max : 2048 dossiers.
	char typeflag;										//Le type de fichier : '0' = fichier, '2' = lien symbolique, '5' = dossier.
	char ssize[12];										//Champ size avec \0 forcé.
	char sustar[6];										//Champ magic avec \0 forcé.
	char smtime[12];									//Champ mtime avec \0 forcé.
	char sname[100];									//Champ name avec \0 forcé

	int mtimes[MAXDIR];								//Liste des mtime, associé au premier tableau (dans l'ordre).
	int status;												//Valeur de retour des read() successifs dans la boucle principale : 0 <=> EOF, -1 <=> erreur.
	int extreturn;	     							//Valeur de retour de extraction().
	int nbdir; 		     								//Nombre de dossiers (sert au utime final).
	int utim;													//Valeur de retour des utime() finaux pour les dossiers.
	int mtime;												//Date de modification en secondes depuis l'Epoch (valeur décimale).
	int k;														//Indice pour la boucle for du utime final des dossiers.
	int size;													//Taille du champ size du header, c'est la taille des données suivant le header.
	int size_reelle;									//Taille des données utiles suivant le header, multiple de 512, puisque les données sont stockées dans des blocks de 512 octets.

	nbdir=0;
	size_reelle=0;

	/*
	Traitement de chaque header les uns après les autres
	*/

	do {

		//On lock le mutex pour protéger la ressource avant lecture. Lit donc un élément et traite dessus : lit aussi les potentielles données suivant le header.
		pthread_mutex_lock(&MutexRead);

		//Si l'End Of File ou un checksum est invalide, stop immédiatemment l'éxécution des autres threads.
		if (isEOF==true) {
			pthread_mutex_unlock(&MutexRead);
			if (thrd==1) {
				pthread_exit((int *) NULL);
			}
			else break;
		}

		//On lit (read) un premier bloc (header) de 512 octets qu'on met dans une variable du type de la structure header_posix_ustar définie dans header.h (norme POSIX.1)
		if (decomp==0) {
			//Cas du .tar
			status=read(file, &head, sizeof(head));  // Utiliser sizeof(head) est plus évolutif qu'une constante égale à 512.
		}
		else {
			//Cas du .tar.gz
			status=(*gzRead)(filez, &head, sizeof(head));
		}

		//Cas d'erreur -1 du read
		if (status<0) {
			printf("Erreur dans la lecture du header, read() retourne : %d \n", status);
			if (logflag==1) fclose(logfile);
			if (decomp==0) {
				close(file);
			}
			else {
				(*gzClose)(filez);
			}
			exit(EXIT_FAILURE);
		}

		//Parsing correct du champ name en forçant la terminaison par \0
		strncpy(sname, head.name, sizeof(head.name));
		strcat(sname,"\0");

		/* La fin d'une archive tar se termine par 2 enregistrements d'octets valant 0x0. (voir tar(5))
		Donc la string head.name du premier des 2 enregistrement est forcément vide.
		On s'en sert donc pour détecter la fin du fichier (End Of File) et ne pas afficher les 2 enregistrements de 0x0. */

		//Détection de l'End Of File : met le flag isEOF à true et stoppe la boucle. Le flag est destiné à stopper les autres threads.
		if (strlen(sname)==0 || status==0 || sname==NULL) {
			isEOF=true;
			pthread_mutex_unlock(&MutexRead);
			break;
		}

		//Cas où le fichier passé en paramètre n'est pas une archive tar POSIX ustar : vérification avec le champ magic du premier header.
		//Si le premier header est validé, alors l'archive est conforme et ce test ne devrait pas être infirmé quelque soient les header suivant, par construction.
		strncpy(sustar, head.magic, sizeof(head.magic));
		strcat(sustar,"\0");

		if (strcmp("ustar", sustar)!=0 && strcmp("ustar  ", sustar)!=0 && strcmp("ustar ", sustar)!=0) {
			printf("Le fichier %s ne semble pas être une archive POSIX ustar.\n", folder);
			if (logflag==1) fclose(logfile);
			if (decomp==0) {
				close(file);
			}
			else {
				(*gzClose)(filez);
			}
			exit(EXIT_FAILURE);
		}

		/*
		Récupération des data (dans le cas d'un fichier non vide).
		*/

		//On récupère la taille (head.size) des données suivant le header. La variable head.size est une string, contenant la taille donnée en octal, convertit en décimal.
		strncpy(ssize, head.size, sizeof(head.size));
		strcat(ssize,"\0");
		size=strtol(ssize, NULL, 8);

		// (2) Si des données non vides suivent le header (en pratique : si il s'agit d'un header fichier non vide), on passe ces données sans les afficher:
		if (size > 0) {
			//Les données sont stockées dans des blocs de 512 octets (on a trouvé ça en utilisant hexdump -C testall.tar ...) après le header.
			//On utilise donc une variable temporaire pour stocker la taille totale allouée pour les données dans l'archive tar.
			if (size%512==0) {
				size_reelle=size; // Il faut sortir les cas particuliers multiples de 512. (Sinon il y aura 1 bloc de 512 octets en trop)
			}
			else {
				size_reelle=512*((int)(size/512)+1);  // En castant avec (int) cela agit comme une partie entière. Ce n'est pas nécessaire mais plus sûr.
			}

			//On n'alloue de la mémoire que si on a besoin des données (c'est-à-dire si on souhaite extraire)
			if (extract==1) {
				//On utilise le buffer data pour le read() des données pour l'extraction
				data=malloc(size_reelle);

				//On récupère les données suivant le buffer, elles vont servir pour l'extraction
				if (decomp==0) {
					status=read(file, data, size_reelle);
				}
				else {
					status=(*gzRead)(filez, data, size_reelle);
				}
			}
			//Sinon un simple lseek() suffira (déplace la tête de lecture d'un certain offset).
			else {
				//Le flag (whence) SEEK_CUR assure que la tête de lecture est déplacée de size_réelle octets relativement à la position courante.
				if (decomp==0) {
					status=lseek(file, size_reelle, SEEK_CUR);
				}
				else {
					status=(*gzSeek)(filez, size_reelle, SEEK_CUR);
				}
			}

			//Cas d'erreur -1 du read
			if (status<0) {
				printf("Erreur dans la lecture du header, read() retourne : %d\n", status);
				if (logflag==1) fclose(logfile);
				if (decomp==0) {
					close(file);
				}
				else {
					(*gzClose)(filez);
				}
				exit(EXIT_FAILURE);
			}
		}

		/*
		Traitement du listing du header (détaillé -l ou non)
		*/

		//On ne print pas le nom du header Pax (de type g) si il y en a un.
		if (head.typeflag[0] != 'x' || head.typeflag[0] == 'g') {
			//Listing détaillé :
			if (listingd==1) {
				listing(head);
			}
			//Listing basique :
			else {
				printf("%s\n", sname);
				//Force l'affichage sur la sortie standard pour les threads.
				if (thrd==1) fflush(stdout);
			}
		}

		/*
		Calcul du checksum et vérification du type de header : USTAR standard ou Pax
		*/

		//On compare les checksum.
		isCorrupted=checksum(&head);

		//Si le checksum de du header est corrompu, ou si il s'agit d'un header Pax, on passe au suivant.
		if (isCorrupted==true || head.typeflag[0] == 'x'|| head.typeflag[0] == 'g') {

			//On set le super-flag de corruption à true pour le print final avant exit(), si il s'agit bien de corruption.
			if (isCorrupted==true) {
				corrupted=true;
				printf("La somme de contrôle (checksum) de %s est invalide... On passe à l'en-tête suivante.\n", sname);
			}

			//On libère également les données potentielles suivant le header si le buffer est rempli :
			if (extract==1 || size>0) {
				free(data);
			}

			//On délock le mutex.
			pthread_mutex_unlock(&MutexRead);

			//Si l'End Of File ou un checksum est invalide, stop immédiatemment l'éxécution des autres threads.
			if (isEOF==true) {
				if (thrd==1) {
					pthread_exit((int *) NULL);
				}
				else break;
			}
		}

		//Sinon on continu normalement !
		else {

			//On débloque le mutex en lecture.
			pthread_mutex_unlock(&MutexRead);

			/*
			Traitement de l'extraction -x de l'élément lié au header
			*/

			if (extract==1) {

				//Récupération du type d'élément.
				typeflag=head.typeflag[0];

				//Récupération du nom dans un tableau si c'est un dossier (et de son mtime)
				if (typeflag=='5') {

					//On récupère le nom
					strcpy(dirlist[nbdir], sname);

					//RCopie de mtime
					strncpy(smtime, head.mtime, sizeof(head.mtime));
					strcat(smtime,"\0");
					mtime=strtol(head.mtime, NULL, 8);

					//Récupération à proprement parler.
					mtimes[nbdir]=mtime;

					//Incrémentaion du nombre de dossiers (représente enfait l'indice dans les tableaux)
					nbdir++;
				}

				//Extraction de l'élément.
				extreturn=extraction(&head, NULL, data);

				if (logflag==1) fprintf(logfile, "Retour d'extraction de %s : %d\n", sname, extreturn);
			}
		}
	} while (isEOF==false);  //On aurait pu mettre while(1) puisque la boucle doit normalement se faire breaker plus haut.

	/*
	Fin des traitements
	*/

	//Affectation du bon mtime pour les dossiers après traitement (nécessaire d'être en toute fin) de l'extraction.
	if (extract==1 && nbdir>0) { //Il faut au moins 1 dossier pour permettre l'action.
		for (k=0; k<nbdir; k++) {
			tm.actime=mtimes[k];
			tm.modtime=mtimes[k];
			utim=utime(dirlist[k], &tm);
			if (logflag==1) fprintf(logfile, "[Dossier %s] Code retour du utime : %d\n", dirlist[k], utim);
		}
	}

	//Si l'archive est corrompu, ptar doit renvoyer 1
	if (corrupted==true) {
		printf("ptar : arrêt avec somme de contrôle invalide d'un des élément de l'archive.\n");
		exit(EXIT_FAILURE);
	}

	//Sinon normalement tout s'est bien passé, on return ou thrd_exit selon les options.
	else {
		if (thrd==0) return NULL;
		else pthread_exit((int *) NULL);
	}
}



/*
Fonction génératrice de logfile.
logname = Nom du logfile (normalement : logfile.txt)
option = Options du fopen (normalement "a" pour faire un ajout en fin de fichier et pas l'écraser à chaque fois)
*/

FILE *genlogfile(const char *logname, const char *option, char *filename) {

	struct tm instant;
	time_t secondes;
	FILE *logfile;

	time(&secondes);
	instant=*localtime(&secondes);
	logfile=fopen(logname, option);
	fprintf(logfile, "Logfile du traitement de l'archive %s le %d/%d a %d:%d:%d\n", filename, instant.tm_mday, instant.tm_mon+1, instant.tm_hour, instant.tm_min, instant.tm_sec);

	return logfile;
}



/*
Traite le listing détaillé des éléments (regular files, directory, symbolic links).
*/

int listing(headerTar head) {

	time_t mtime;
	struct tm ts;
	char bmtime[80];
	char typeflag;
	char ssize[12];
	char suid[8];
	char sgid[8];
	char smode[8];
	char smtime[12];
	char sname[100];

	int size;
	int mode;
	int uid;
	int gid;

	//Parsing correct de la terminaison par \0
	strncpy(ssize, head.size, sizeof(head.size));
	strncpy(suid, head.uid, sizeof(head.uid));
	strncpy(sgid, head.gid, sizeof(head.gid));
	strncpy(smode, head.mode, sizeof(head.mode));
	strncpy(smtime, head.mtime, sizeof(head.mtime));
	strncpy(sname, head.name, sizeof(head.name));
	strcat(ssize,"\0");
	strcat(suid,"\0");
	strcat(sgid,"\0");
	strcat(smode,"\0");
	strcat(smtime,"\0");
	strcat(sname,"\0");

	//Récupération de la taille (comme dans le main)
	size=strtol(ssize, NULL, 8);

	//head.mode = les permissions du fichier en octal, on les convertit en décimal.
	mode=strtol(smode, NULL, 8);

	//Récupération du uid et gid convertit depuis l'octal
	uid=strtol(suid, NULL, 8);
	gid=strtol(sgid, NULL, 8);

	//Récupération du typeflag (seulement le 1er char du champ)
	typeflag=head.typeflag[0];

	//Récupération du mtime et conversion/formatage sous la forme désirée. (mtime est en secondes écoulées depuis l'Epoch !)
	mtime=strtol(smtime, NULL, 8);
	ts=*localtime(&mtime);  //Formate en "aaaa-mm-jj hh:mm:ss"
	strftime(bmtime, sizeof(bmtime), "%Y-%m-%d %H:%M:%S", &ts);

	//Print des permissions de l'élément
	if (typeflag=='5') printf("d");
	else printf( (typeflag=='2') ? "l" : "-");
	printf( (mode & S_IRUSR) ? "r" : "-");
	printf( (mode & S_IWUSR) ? "w" : "-");
	printf( (mode & S_IXUSR) ? "x" : "-");
	printf( (mode & S_IRGRP) ? "r" : "-");
	printf( (mode & S_IWGRP) ? "w" : "-");
	printf( (mode & S_IXGRP) ? "x" : "-");
	printf( (mode & S_IROTH) ? "r" : "-");
	printf( (mode & S_IWOTH) ? "w" : "-");
	printf( (mode & S_IXOTH) ? "x" : "-");

	//Print des autres informations : uid, gid, taille, date de modification, nom, et nom du fichier linké (vide si ce n'est pas un lien symbo)
	printf(" %d/%d %d %s %s%s%s\n", uid, gid, size, bmtime, sname, (typeflag=='2') ? " -> " : "", (typeflag=='2') ? head.linkname : "");

	//Force l'affichage sur la sortie standard pour les threads.
	if (thrd==1) fflush(stdout);

	return 0;
}



/*
Traite l'extraction des éléments (regular files, directory, symbolic links)
*/

int extraction(headerTar *head, char *namex, char *data) {

	struct timeval *tv;
	char *name;
	char typeflag;
	char ssize[12];
	char suid[8];
	char sgid[8];
	char smode[8];
	char smtime[12];
	char sname[100];
	char slink[100];

	int mode;
	int size;
	int uid;
	int gid;
	int mtime;

	//Code de retour des open/write/close/symlink/mkdir/setuid/setgid/utimes/lutimes - A lire dans logfile.txt
	int etat;
	int filed;
	int writ;
	int etatuid;
	int etatgid;
	int sync;
	int utim;
	int chkpath;

	//Nécessaire d'initialiser ces flags car pas initialisés dans tous les cas (et causent un 'faux' return -1 dans certains cas).
	filed=0;
	writ=0;
	sync=0;
	utim=0;
	etatgid=0;
	etatuid=0;
	etat=0;
	chkpath=0;

	//Parsing correct de la terminaison par \0
	strncpy(ssize, head->size, sizeof(head->size));
	strncpy(suid, head->uid, sizeof(head->uid));
	strncpy(sgid, head->gid, sizeof(head->gid));
	strncpy(smode, head->mode, sizeof(head->mode));
	strncpy(smtime, head->mtime, sizeof(head->mtime));
	strncpy(sname, head->name, sizeof(head->name));
	strncpy(slink, head->linkname, sizeof(head->linkname));
	strcat(ssize,"\0");
	strcat(suid,"\0");
	strcat(sgid,"\0");
	strcat(smode,"\0");
	strcat(smtime,"\0");
	strcat(sname,"\0");
	strcat(slink,"\0");

	//Récupération du name
	if (namex!=NULL) {
		name=namex;
	}
	else {
		name=sname;
	}

	//Récupération du mtime en secondes depuis l'Epoch. (voir tar(5))
	mtime=strtol(smtime, NULL, 8);

	//On set les champs de la structure timeval[2] pour utimes() et lutimes().
	tv=malloc(2*sizeof(struct timeval));
	tv[0].tv_sec=mtime;
	tv[0].tv_usec=0;
	tv[1].tv_sec=mtime;
	tv[1].tv_usec=0;

	//Le typeflag NUL doit être traité comme un typeflag 0 (voir tar(5))
	if (head->typeflag[0]==0) {
		typeflag='0';
	}
	else typeflag=head->typeflag[0];

	//Récupération du uid et gid convertit depuis l'octal en décimal.
	uid=strtol(suid, NULL, 8);
	gid=strtol(sgid, NULL, 8);

	/* INEFFICACE SI PAS DE DROIT ADMINISTRATEUR */
	//On set l'uid et le gid du processus (cette instance d'extraction()) à ceux de l'élément à extraire. Voir setuid(3) et setgid(3).
	etatuid=setuid(uid);
	etatgid=setgid(gid);
	if (logflag==1 && thrd==0) fprintf(logfile, "[Element %s] Code retour du setuid : %d et du setgid : %d\n", name, etatuid, etatgid);

	//Récupération des permissions du fichier en octal, converties en décimal.
	mode=strtol(smode, NULL, 8);

	//Récupération de la taille (comme dans le main).
	size=strtol(ssize, NULL, 8);

	//Séléction du type d'élément et actions.
	switch (typeflag) {

		//Fichiers réguliers
		case '0' :
			//Si le fichier existe on met à jour ses permissions.
			if (existeFile(name)==true) {
				etat=chmod(name, mode);
				if (logflag==1 && thrd==0) fprintf(logfile, "[Fichier %s] Code retour du chmod : %d\n", name, etat);
			}

			//On vérifie si l'arborescence de dossiers existe et on la crée le cas échéant.
			chkpath=checkpath(name);
			if (logflag==1 && thrd==0) fprintf(logfile, "[Fichier %s] Code retour du checkpath : %d\n", name, chkpath);

			//open() avec O_CREAT pour créer le fichier et O_WRONLY pour pouvoir écrire dedans. O_EXCL pour détecerla préexistence.
			filed=open(name, O_CREAT | O_WRONLY, mode);
			if (logflag==1 && thrd==0) fprintf(logfile, "[Fichier %s] Code retour du open : %d\n", name, file);

			//Ecriture : Voir la partie (2) de traitement(): récupération de données. Il faut utiliser size et pas size_reelle cette fois-ci
			if (size > 0) {  //Ecriture si seulement le fichier n'est pas vide !
				writ=write(filed, data, size);
				if (logflag==1 && thrd==0) fprintf(logfile, "[Fichier %s] Code retour du write : %d\n", name, writ);

				//Durabilité de l'écriture sur disque si option -p
				if (thrd==1 && thrd==0) {
					sync=fsync(filed);
					if (logflag==1 && thrd==0) fprintf(logfile, "[Fichier %s] Code retour du fsync : %d\n", name, sync);
				}

				//Libération de la mémoire allouée pour les données suivant le header.
				free(data);
			}

			//Fermeture du fichier.
			etat=close(filed);
			if (logflag==1 && thrd==0) fprintf(logfile, "[Fichier %s] Code retour du close : %d\n", name, etat);

			//Une fois le fichier créé complétement (et fermé!), on configure son modtime (et actime).
			utim=utimes(name, tv);
			if (logflag==1 && thrd==0) fprintf(logfile, "[Fichier %s] Code retour du utime : %d\n", name, utim);

			break;

		//Liens symboliques
		case '2' :
			//On ne vérifie plus si l'élément pointé existe : symlink peut linker sur des éléments inexistant dans l'environnement courant.

			//On vérifie si l'arborescence de dossiers du lien existe et on la crée le cas échéant.
			chkpath=checkpath(name);
			if (logflag==1 && thrd==0) fprintf(logfile, "[Lien symobolique %s] Code retour du checkpath : %d\n", name, chkpath);

			//On créé ensuite le lien symbolique
			etat=symlink(slink, name);
			if (logflag==1 && thrd==0) fprintf(logfile, "[Lien symbolique %s] Code retour du symlink : %d\n", name, etat);

			//On utilise lutimes car utime ne fonctionne pas sur les symlink : voir lutimes(3).
			utim=lutimes(name, tv);
			if (logflag==1 && thrd==0) fprintf(logfile, "[Lien symbolique %s] Code retour du utime : %d\n", name, utim);

			break;

		//Répertoires
		case '5' :
			//On vérifie l'existence du dossier.
			if (existeDir(name)==true) {
				//Si le dossier existe et qu'on a son header, on met à jour ses permissions.
				etat=chmod(name, mode);
				if (logflag==1 && thrd==0) fprintf(logfile, "[Dossier %s] Code retour du chmod : %d\n", name, etat);
			}

			//Le dossier n'existe pas :
			else {
				//On crée son arborescence de dossiers parente.
				chkpath=checkpath(name);
				if (logflag==1 && thrd==0) fprintf(logfile, "[Dossier %s] Code retour du checkpath du dossier: %d\n", name, chkpath);

				//Puis on crée le dossier concerné.
				etat=mkdir(name, mode);
				if (logflag==1 && thrd==0) fprintf(logfile, "[Dossier %s] Code retour du mkdir : %d\n", name, etat);
			}
			break;

		//Autre typeflag : peut être des typeflag de GNU tar non gérés par ptar.
		default:
			printf("Elément inconnu par ptar : Typeflag = [%c]\n", typeflag);
			etat=-1;
			break;
	}

	//On libère la mémoire allouée pour les timeval
	free(tv);

	//Code de retour de extraction (évolutif, rajouter éventuellement des cas de return -1)
	if (etat<0  || filed<0 || writ<0 || etatuid<0 || etatgid<0 || sync<0 || utim<0) {
		return -1;
	}

	else return 0;
}



/*
Sert à vérifier la non-corruption d'une archive tar après son téléchargement.
Vérifie la somme de contrôle stockée dans le header passé en paramère.
Pour cela, recalcule le checksum du header et le compare au champs checksum sur header.
Avant la comparaison, on applique un masque 0x3FFFF au checksum calculé car seuls les 18 bits de poids faible
nous importent ici pour la comparaison. Pour cela on utilise un ET bit-à-bit. Ce n'est pas nécessaire dans la
plupart des cas mais c'est plus sûr pour la comparaison (les autres bits pouvant changer la valeur).
Un seconde masque 0xFF est appliqué sur chaque char du header, en effet certain caractère spéciaux comme
le char 'é' possède une valeur signée ffffffc3, on ne veut que les 8 derniers bits.
Retourne false si le header n'est pas corrompu, true sinon.
*/

bool checksum(headerTar *head) {

	int i;
	unsigned int chksum;
	unsigned int chksumtotest;
	char *pHeader;
	char schecksum[8];

	chksum=0;
	chksumtotest=0;

	//Parsing correct de la terminaison par \0
	strncpy(schecksum, head->checksum, sizeof(head->checksum));
	strcat(schecksum, "\0");

	//On récupère le champs checksum du header, il est en octal ASCII.
	chksumtotest = strtol(schecksum, NULL, 8);

	//On cast le header sous la forme d'un pointeur sur char.
	pHeader = (char *)head;

	//On boucle sur les caractères en s'arretant avant le champ checksum.
	for (i=0; i<148; i++) {
		chksum = chksum+((unsigned int)pHeader[i] & 0xFF);
	}

	//Ici on ne tient pas du champ checksum (pour des raisons évidentes...). On considère que c'est 8 blancs de valeur décimale 32 en ASCII
	for (i=148; i<156; i++) {
		chksum = chksum+32;
	}

	//Et on finit de boucler sur le reste des caractères.
	for (i=156; i<512; i++) {
		chksum = chksum+((unsigned int)pHeader[i] & 0xFF);
	}

	//On ne garde que les 18 bits de poids faible en appliquant un masque. Ce n'est pas absolument nécessaire mais c'est plus sûr pour la comparaison.
	chksum = chksum & 0x3FFFF;

	//Comparaison effective
	if (chksum!=chksumtotest) {
		return true;
	}
	else return false;
}



/*
Charge la librairie dynamique zlib et les fonctions utilisées pour la décompression/extraction.
*/

void loadzlib() {

	char *error;
	char *erroropen;
	char *errorread;
	char *errorclose;
	char *errorseek;
	char *errorrewind;

	if (logflag==1) fprintf(logfile, "Debut de la decompression de l'archive ...\n");

	//Chargement de la bibliothèque zlib avec dlopen. dlopen va chercher libz.so dans le système.
	handle=dlopen("libz.so", RTLD_NOW);

	//Deuxième tentative en chargant la lib fourni par le git.
	if (!handle) {
		handle=dlopen("zlib/libz.so", RTLD_NOW);
	}

	//Ultime tentative de charger zlib depuis un dossier où elle est couramment installée.
	if (!handle) {
		handle=dlopen("/usr/lib/libz.so", RTLD_NOW);
	}

	//Gestion du cas où la bibliothèque ne charge pas.
	if (!handle) {
		error=dlerror();
		printf("Erreur dans le chargement de zlib : dlerror() = %s\n", error);
		if (logflag==1) {
			fprintf(logfile, "[Chargement dynamique de zlib] Code retour du dlopen : %s\n", error);
			fclose(logfile);
		}
		exit(EXIT_FAILURE);
	}

	//On récupère les fonctions utiles à la décompression avec dlsym
	gzOpen=dlsym(handle, "gzopen");
	erroropen=dlerror();
	if (logflag==1) fprintf(logfile, "Code retour du dlsym(gzopen) : %s\n", erroropen);

	gzRewind=dlsym(handle, "gzrewind");
	errorrewind=dlerror();
	if (logflag==1) fprintf(logfile, "Code retour du dlsym(gzrewind) : %s\n", errorrewind);

	gzSeek=dlsym(handle, "gzseek");
	errorseek=dlerror();
	if (logflag==1) fprintf(logfile, "Code retour du dlsym(gzeof) : %s\n", errorseek);

	gzRead=dlsym(handle, "gzread");
	errorread=dlerror();
	if (logflag==1) fprintf(logfile, "Code retour du dlsym(gzread) : %s\n", errorread);

	gzClose=dlsym(handle, "gzclose");
	errorclose=dlerror();
	if (logflag==1) fprintf(logfile, "Code retour du dlsym(gzclose) : %s\n", errorclose);

	//Cas d'erreur des dlsym
	if (erroropen!=NULL || errorread!=NULL || errorclose!=NULL || errorrewind!=NULL || errorseek!=NULL) {
		printf("Erreur detectee par dlerror() : open:%s read:%s close:%s rewind:%s seek:%s\n", erroropen, errorread, errorclose, errorrewind, errorseek);
		dlclose(handle);
		if (logflag==1) fclose(logfile);
		exit(EXIT_FAILURE);
	}
}
