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
	char sname[100];									//Champ name avec \0 forcé.

	bool isEOF; 											//Flag d'End Of File : true <=> Fin de fichier atteint.
	bool isCorrupted;									//Flag de checksum : true <=> header corrompu.

	int *mtimes;								   		//Liste des mtime, associé au premier tableau (dans l'ordre). Taille variable (par des realloc).
	int *mtimestemp;									//Buffer temporaire pour realloc(mtimes).
	int status;												//Valeur de retour des read() successifs dans la boucle principale : 0 <=> EOF, -1 <=> erreur.
	int extreturn;	     							//Valeur de retour de extraction().
	int nbdir; 		     								//Nombre de dossiers (sert au utime final).
	int utim;													//Valeur de retour des utime() finaux pour les dossiers.
	int mtime;												//Date de modification en secondes depuis l'Epoch (valeur décimale).
	int k;														//Indice pour la boucle for du utime final des dossiers.
	int size;													//Taille du champ size du header, c'est la taille des données suivant le header.
	int size_reelle;									//Taille des données utiles suivant le header, multiple de 512, puisque les données sont stockées dans des blocks de 512 octets.

	//Initialisation des tableaux dynamiques à NULL pour les realloc (obligatoire).
	mtimes=NULL;
	mtimestemp=NULL;

	nbdir=0;
	size_reelle=0;
	isEOF=false;
	isCorrupted=false;

	/*
	Traitement de chaque header les uns après les autres
	*/

	do {
		//On lock le mutex pour protéger la ressource avant lecture. Lit donc un élément et traite dessus : lit aussi les potentielles données suivant le header.
		pthread_mutex_lock(&MutexRead);

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

		//Parsing correct du champ name.
		strncpy(sname, head.name, sizeof(head.name));
		strcat(sname,"\0");	//Forcing de la terminaison par \0

		/* La fin d'une archive tar se termine par 2 enregistrements d'octets valant 0x0. (voir tar(5))
		Donc la string head.name du premier des 2 enregistrement est forcément vide.
		On s'en sert donc pour détecter la fin du fichier (End Of File) et ne pas afficher les 2 enregistrements de 0x0. */

		//Détection de l'End Of File : met le flag isEOF à true et stoppe la boucle. Le flag est destiné à aider les développeurs (il ne sert pas à stopper la boucle)
		if (strlen(sname)==0 || status==0) {
			isEOF=true;
			break;
		}

		//Cas où le fichier passé en paramètre n'est pas une archive tar POSIX ustar : vérification avec le champ magic du premier header.
		//Si le premier header est validé, alors l'archive est conforme et ce test ne devrait pas être infirmé quelque soient les header suivant, par construction.
		strncpy(sustar, head.magic, sizeof(head.magic));
		strcat(sustar,"\0");						//Forcing de la terminaison par \0
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
		Vérification de la somme de contrôle (checksum)
		*/

		isCorrupted=checksum(&head);

		if (isCorrupted==true) {
			printf("La somme de contrôle (checksum) de %s est invalide.\n", sname);
			break;
		}

		/*
		Récupération des data (dans le cas d'un fichier non vide).
		*/

		//On récupère la taille (head.size) des données suivant le header. La variable head.size est une string, contenant la taille donnée en octal, convertit en décimal.
		strncpy(ssize, head.size, sizeof(head.size));
		strcat(ssize,"\0");	//Forcing de la terminaison par \0
		size=strtol(ssize, NULL, 8);

		// (2) Si des données non vides suivent le header (en pratique : si il s'agit d'un header fichier non vide), on passe ces données sans les afficher:
		// On récupère les données dans un buffer pour l'extraction.
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
			if (extract==1 || (decomp==1 && extract==1)) {
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
			//Sinon un simple lseek() suffira (déplace la tête de lecture d'un certain offset). SAUF pour le tube nommé (voir plus haut).
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

		//On débloque le mutex en lecture.
		pthread_mutex_unlock(&MutexRead);

		/*
		Traitement du listing du header (détaillé -l ou non)
		*/

		//Listing détaillé :
		if (listingd==1) {
			listing(head);
		}
		//Listing basique :
		else {
			printf("%s\n", sname);
		}

		/*
		Traitement de l'extraction -x de l'élément lié au header
		*/

		if (extract==1) {

			//On bloque le mutex en écriture.
			pthread_mutex_lock(&MutexWrite);

			//Récupération du type d'élément.
			typeflag=head.typeflag[0];

			//Récupération du nom dans un tableau si c'est un dossier (et de son mtime)
			if (typeflag=='5') {

				//On récupère le nom
				strcpy(dirlist[nbdir], sname);

				//Réalloc de mtimes
				strncpy(smtime, head.mtime, sizeof(head.mtime));
				strcat(smtime,"\0");	//Forcing de la terminaison par \0
				mtime=strtol(head.mtime, NULL, 8);

				mtimestemp=realloc(mtimes, (nbdir+1)*sizeof(int));
				if (mtimestemp==NULL) {
							free(mtimes);    //Désallocation
							printf("Problème de réallocation du tableaux des mtime.\n");
							break;
				}
				else mtimes=mtimestemp;

				//Récupération à proprement parler.
				mtimes[nbdir]=mtime;
				nbdir++; //Incrémentaion du nombre de dossiers (représente enfait l'indice dans les tableaux)
			}

			//Extraction de l'élément.
			extreturn=extraction(&head, NULL, data);

			if (logflag==1) fprintf(logfile, "Retour d'extraction de %s : %d\n", sname, extreturn);

			//On débloque le mutex en écriture.
			pthread_mutex_unlock(&MutexWrite);
		}

	} while (isEOF==false);  //On aurait pu mettre while(1) puisque la boucle doit normalement se faire breaker plus haut.

	/*
	Fin des traitements
	*/

	//Fermeture de l'archive.
	if (decomp==0) {
		close(file);
	}
	else {
		(*gzClose)(filez);
	}

	//Affectation du bon mtime pour les dossiers après traitement (nécessaire d'être en toute fin) de l'extraction.
	if (extract==1 && nbdir>0) { //Il faut au moins 1 dossier pour permettre l'action.
		for (k=0; k<nbdir; k++) {
			tm.actime=mtimes[k];
			tm.modtime=mtimes[k];
			utim=utime(dirlist[k], &tm);
			if (logflag==1) fprintf(logfile, "[Dossier %s] Code retour du utime : %d\n", dirlist[k], utim);
		}
		//Free des pointeurs utilisés pour l'extraction des dossiers.
		free(mtimes);
	}

	//Fermeture du logfile
	if (logflag==1) {
		if (isCorrupted==false) {
			fputs("Les sommes de contrôle (checksum) sont toutes valides.\n", logfile);
			fputs("Fin de decompression/extraction.\n\n", logfile);
		}
		else fputs("Une des sommes de contrôle n'est pas valide, arrêt de ptar...\n", logfile);
		fclose(logfile);
	}

	//Si l'archive est corrompu, ptar doit renvoyer 1
	if (isCorrupted==true) exit(EXIT_FAILURE);

	//Sinon normalement tout s'est bien passé, et ptar renvoie 0.
	else exit(EXIT_SUCCESS);
}



/*
Fonction génératrice de logfile
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

	//Récupération du typeflag
	/*
	ATTENTION IL FAUT SELECTIONNER SEULEMENT LE PREMIER CARACTERE SINON IL CHOPE PARFOIS LE CHAMP SUIVANT ET FOUS LA MERDE!
	*/
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

	return 0;
}



/*
Traite l'extraction des éléments (regular files, directory, symbolic links)
*/

int extraction(headerTar *head, char *namex, char *data) {

	struct timeval *tv;
	char *name;
	char filename[255];
	char typeflag;
	char ssize[12];
	char suid[8];
	char sgid[8];
	char smode[8];
	char smtime[12];
	char sname[100];
	char slink[100];
	bool notExisting;

	int mode;
	int size;
	int uid;
	int gid;
	int mtime;

	//Code de retour des open/write/close/symlink/mkdir/setuid/setgid/utimes/lutimes - A lire dans logfile.txt
	int etat;
	int file;
	int writ;
	int etatuid;
	int etatgid;
	int sync;
	int utim;
	int chkpath;
	int rm;

	//Nécessaire d'initialiser ces flags car pas initialisés dans tous les cas (et causent un 'faux' return -1 dans certains cas).
	file=0;
	writ=0;
	sync=0;
	utim=0;
	etatgid=0;
	etatuid=0;
	etat=0;
	chkpath=0;
	notExisting=false;

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

	//On set l'uid et le gid du processus (cette instance d'extraction()) à ceux de l'élément à extraire. Voir setuid(3) et setgid(3).
	etatuid=setuid(uid);
	etatgid=setgid(gid);
	if (logflag==1) fprintf(logfile, "[Element %s] Code retour du setuid : %d et du setgid : %d\n", name, etatuid, etatgid);

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
				if (logflag==1) fprintf(logfile, "[Fichier %s] Code retour du chmod : %d\n", name, etat);
			}
			//On vérifie si l'arborescence de dossiers existe et on la crée le cas échéant.
			chkpath=checkpath(name, logfile);
			if (logflag==1) fprintf(logfile, "[Fichier %s] Code retour du checkpath : %d\n", name, chkpath);
			//open() avec O_CREAT pour créer le fichier et O_WRONLY pour pouvoir écrire dedans. O_EXCL pour détecerla préexistence.
			file=open(name, O_CREAT | O_WRONLY, mode);
			if (logflag==1) fprintf(logfile, "[Fichier %s] Code retour du open : %d\n", name, file);

			//Ecriture : Voir la partie (2) de traitement(): récupération de données. Il faut utiliser size et pas size_reelle cette fois-ci
			if (size > 0) {  //Ecriture si seulement le fichier n'est pas vide !
				writ=write(file, data, size);
				if (logflag==1) fprintf(logfile, "[Fichier %s] Code retour du write : %d\n", name, writ);
				//Durabilité de l'écriture sur disque si option -p
				if (thrd==1) {
					sync=fsync(file);
					if (logflag==1) fprintf(logfile, "[Fichier %s] Code retour du fsync : %d\n", name, sync);
				}
				//Libération de la mémoire allouée pour les données suivant le header.
				free(data);
			}

			//Fermeture du fichier.
			etat=close(file);
			if (logflag==1) fprintf(logfile, "[Fichier %s] Code retour du close : %d\n", name, etat);
			//Une fois le fichier créé complétement (et fermé!), on configure son modtime (et actime).
			utim=utimes(name, tv);
			if (logflag==1) fprintf(logfile, "[Fichier %s] Code retour du utime : %d\n", name, utim);
			break;
		//Liens symboliques
		case '2' :
			//Dans le cas ou le fichier/dossier est "plus haut ou au même niveau" que le lien, le path est déjà complet.
			if (slink[0]!='/') {
				strcpy(filename,"");
				recoverpath(slink, name, filename);
			}
			else {
				//Cas où le linkname contient le chemin absolu
				strcpy(filename,slink);
			}
			//On vérifie si l'arborescence de dossiers existe et on la crée le cas échéant.
			chkpath=checkpath(name, logfile);
			if (logflag==1) fprintf(logfile, "[Lien symobolique %s] Code retour du checkpath : %d\n", name, chkpath);
			//On vérifie si le fichier/dossier pointé existe et on le créer le cas échéant.
			//Cas du dossier.
			if (filename[strlen(filename)-1]=='/' && existeDir(filename)==false) {
				notExisting=true;
				//Création de l'arborescence du dossier pointé.
				chkpath=checkpath(filename, logfile);
				if (logflag==1) fprintf(logfile, "[Lien symobolique %s] Code retour du checkpath du dossier pointé : %d\n", name, chkpath);
				//Création du dossier avec tous les droits (temporaires, seront mis à jour plus tard)
				mkdir(filename, S_IRWXO | S_IRWXO | S_IRWXO);
			}
			//Cas du reste (fichier/symlink)
			else if (existeFile(filename)==false) {
				notExisting=true;
				//Création de l'arborescence du fichier pointé.
				chkpath=checkpath(filename, logfile);
				if (logflag==1) fprintf(logfile, "[Lien symobolique %s] Code retour du checkpath du fichier pointé : %d\n", name, chkpath);
				//Création du fichier avec tous les droits (temporaires, seront mis à jour plus tard)
				file=open(filename, O_CREAT | O_WRONLY, S_IRWXO | S_IRWXO | S_IRWXO);
				close(file);
			}
			//On créé ensuite le lien symbolique
			etat=symlink(slink, name);
			if (logflag==1) fprintf(logfile, "[Lien symbolique %s] Code retour du symlink : %d\n", name, etat);
			//On utilise lutimes car utime ne fonctionne pas sur les symlink : voir lutimes(3).
			utim=lutimes(name, tv);
			if (logflag==1) fprintf(logfile, "[Lien symbolique %s] Code retour du utime : %d\n", name, utim);
			//Si l'élément pointé n'existait pas, on le supprime pour une création éventuelle plus propre tard.
			if (notExisting==true) {
				//Cas d'un dossier.
				if (filename[strlen(filename)-1]=='/') {
					rm=rmdir(filename);
				}
				//Le reste.
				else {
					rm=remove(filename);
				}
				if (logflag==1) fprintf(logfile, "[Lien symbolique %s] Code retour du remove/rmdir du tempfile : %d\n", name, rm);
			}
			break;
		//Répertoires
		case '5' :
			//On vérifie l'existence du dossier.
			if (existeDir(name)==true) {
				//Si le dossier existe et qu'on a son header, on met à jour ses permissions.
				etat=chmod(name, mode);
				if (logflag==1) fprintf(logfile, "[Dossier %s] Code retour du chmod : %d\n", name, etat);
			}
			//Le dossier n'existe pas :
			else {
				//On crée son arborescence de dossiers parente.
				chkpath=checkpath(name, logfile);
				if (logflag==1) fprintf(logfile, "[Dossier %s] Code retour du checkpath du dossier: %d\n", name, chkpath);
				//Puis on crée le dossier concerné.
				etat=mkdir(name, mode);
				if (logflag==1) fprintf(logfile, "[Dossier %s] Code retour du mkdir : %d\n", name, etat);
			}
			break;
		default:
			printf("Elément inconnu par ptar : Typeflag = [%c]\n", typeflag);
			etat=-1;
			break;
	}

	//On libère la mémoire allouée pour les timeval
	free(tv);

	//Code de retour de extraction (évolutif, rajouter éventuellement des cas de return -1)
	if (etat<0  || file<0 || writ<0 || etatuid<0 || etatgid<0 || sync<0 || utim<0 || rm<0) {
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
		chksum = chksum+(unsigned int)pHeader[i];
	}

	//Ici on ne tient pas du champ checksum (pour des raisons évidentes...). On considère que c'est 8 blancs de valeur décimale 32 en ASCII
	for (i=148; i<156; i++) {
		chksum = chksum+32;
	}

	//Et on finit de boucler sur le reste des caractères.
	for (i=156; i<512; i++) {
		chksum = chksum+(unsigned int)pHeader[i];
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
