///////// Pour ptar 1.3 minimum /////////
/*

Fonctions utils

Fonctions principales du programme ptar :
Correspondantes aux options suivantes :
-x : Extraction.
-l : Listing détaillé.
-z : Décompression gzip avec la bibliothèque zlib.
-p NBTHREADS : (forcément couplée avec -x au moins) Durabilité et parallélisation de l'opération avec un nombre de threads choisi.
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
#include <stdbool.h>
#include <math.h>
#include <sys/wait.h>
#include <limits.h>

#include "checkfile.h"
#include "zlib/zlib.h"
#include "utils.h"
//#include "sorting.h"


/*
Fonction principale : recueille les header de chaque fichier dans l'archive (compressée ou non) ainsi que les données suivantes chaque header si il y en a.
Appelle ensuite les diverses fonctions utiles au traitement souhaité.
*/

int traitement(char *folder) {

	/*
	Déclarations et initialisation des variables
	*/

	headerTar head;										//Structure des header tar POSIX ustar.
	//gzHeadertype composition;				//Structure retournée par la fonction analyse(), il s'agit de headers classés et de compteurs: voir sorting.h.
	FILE *logfile; 										//Logfile de l'extraction/decompression/analyse (codes de retour des open, symlink, mkdir, etc).
	struct utimbuf tm; 	   						//Structure pour le utime final des dossiers.

	char *data; 		   								//Buffer pour les données suivant le header.
	char dirlist[PATH_LENGTH][100];		//Liste des dossiers. La taille du folder est limitée à 100 octets.
	const char *pipename;							//Nom du tube nommé utilisé lors de la décompression, retourné par decompress().

	bool isEOF; 											//Flag d'End Of File.
	bool isCorrupted;									//Flag de checksum : true <=> header corrompu.

	int *mtimes;								   		//Liste des mtime, associé au premier tableau (dans l'ordre). Taille variable (par des realloc).
	int *mtimestemp;									//Buffer temporaire pour realloc(mtimes).
	int file;													//Descripteur de fichier retourné par l'open de l'archive.
	int status;												//Valeur de retour des read() successifs dans la boucle principale : 0 <=> EOF, -1 <=> erreur.
	int waitstatus;										//Status pour le waitpid.
	int extreturn;	     							//Valeur de retour de extraction().
	//int postdecomp;									//Valeur de retour de traitepostdecomp() de sorting.h.
	int nbdir; 		     								//Nombre de dossiers (sert au utime final).
	int utim;													//Valeur de retour des utime() finaux pour les dossiers.
	int typeflag;											//Le type de fichier : 0 = fichier, 2 = lien symbolique, 5 = dossier.
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
	Génération du logfile si besoin -e. Option pour développeurs. (utile pour extraction et décompression)
	*/

	if (logflag==1) {
		logfile = genlogfile("logfile.txt", "a", folder);
	}

	/*
	Tester l'argument (existence et nom bien formé).
	*/

	if (checkfile(folder, logfile)==false) {
		//Les messages d'erreurs sont gérés dans checkfile.c.
		//Si l'archive est seulement compressée, tente une décompression directe: ptar n'est pas prévu pour cela, ce module est donc en bêta (développement inutil)
		if(logflag==1) fclose(logfile);
		return 1;
	}

	/*
	Gestion du cas d'une archive compressée .tar.gz
	*/

	//On ouvre l'archive tar avec open() et le flag O_RDONLY (read-only). Si option -z, on décompresse, récupère les données dans un tube et on ouvre ce tube nommé.
	if (decomp==1) {

		//Décompression et ouverture dans un tube nommé tubedecompression.fifo
		pipename=decompress(folder, logfile, false, NULL);

		/*  sorting.h  */
		/* Stratégie trop gourmande en mémoire
		//L'ouverture de la sortie du tube nommé se fait dans analyse(), tout comme le waitpid (sinon le programme se mettrais à lire avant la fin de l'écriture).
		composition=analyse(pipename, logfile);							//Analyse l'archive et retourne une structure adaptée.
		tribulle(composition);															//Effectue un tri à bulle sur la profondeur des dossiers.
		postdecomp=traitepostdecomp(composition, logfile);	//Effectue les traitements grâce aux fonctions extraction() et listing().

		//On n'oublie pas de libérer la mémoire allouée.
		free(composition.headDir);
		free(composition.headFile);
		free(composition.headSymlink);
		free(composition.datas);

		//On supprime le tube nommé.
		status=remove(pipename);
		if (logflag==1) fprintf(logfile, "[Fichier %s] Code retour du remove : %d\n", folder, status);
		if (logflag==1) fclose(logfile);

		//Retourne la valeur de retour de traitepostdecomp(), soit 0 (ok) ou 1 (au moins 1 erreur).
		return postdecomp;
		*/

		/*  Stratégie considérant un bon ordre en tar.gz (ce que n'est pas le cas de nos exemples)  */

		//Ouverture de la sortie du tube nommé.
		file=open(pipename, O_RDONLY);

		//On attend le processus fils qui écrit dans le tube nommé.
		waitpid(-1, &waitstatus, 0);

		/*      fin stratégie de base       */

	}

	/*
	Gestion d'une archive non compressée .tar
	*/

	else {
		file=open(folder, O_RDONLY);
	}

	//Cas d'erreur -1 du open() ou du decompress : problème dans le fichier.
	if (file<0) {
		printf("Erreur d'ouverture du fichier, open() retourne : %d. Le fichier n'existe pas ou alors est corrompu.\n", file);
		close(file);
		if (logflag==1) fclose(logfile); //Aussi le logfile.
		return 1;
	}

	/*
	Traitement de chaque header les uns après les autres
	*/

	do {

		/*
		Récupération du header, détection de fin de fichier et vérification du format archive POSIX ustar du fichier.
		*/

		//On lit (read) un premier bloc (header) de 512 octets qu'on met dans une variable du type de la structure header_posix_ustar définie dans header.h (norme POSIX.1)
		status=read(file, &head, sizeof(head));  // Utiliser sizeof(head) est plus évolutif qu'une constante égale à 512.

		//Cas d'erreur -1 du read
		if (status<0) {
			printf("Erreur dans la lecture du header, read() retourne : %d \n", status);
			if (logflag==1) fclose(logfile);
			close(file);
			return 1;
		}

		/* La fin d'une archive tar se termine par 2 enregistrements d'octets valant 0x0. (voir tar(5))
		Donc la string head.name du premier des 2 enregistrement est forcément vide.
		On s'en sert donc pour détecter la fin du fichier (End Of File) et ne pas afficher les 2 enregistrements de 0x0. */

		//Détection de l'End Of File : met le flag isEOF à true et stoppe la boucle. Le flag est destiné à aider les développeurs (il ne sert pas à stopper la boucle)
		if (strlen(head.name)==0 || status==0) {
			isEOF=true;
			break;
		}


		//Cas où le fichier passé en paramètre n'est pas une archive tar POSIX ustar : vérification avec le champ magic du premier header.
		//Si le premier header est validé, alors l'archive est conforme et ce test ne devrait pas être infirmé quelque soient les header suivant, par construction.
		if (strcmp("ustar", head.magic)!=0 && strcmp("ustar  ", head.magic)!=0) {
			printf("Le fichier %s ne semble pas être une archive POSIX ustar.\n", folder);
			if (logflag==1) fclose(logfile);
			close(file);
			return 1;
		}

		/*
		Vérification de la somme de contrôle (checksum)
		*/

		isCorrupted=checksum(&head);

		if (isCorrupted==true) {
			printf("La somme de contrôle (checksum) de %s est invalide.\n", head.name);
			break;
		}

		/* TESTS */

		//getparentpath(head.name);




		/*
		Récupération des data (dans le cas d'un fichier non vide).
		*/

		//On récupère la taille (head.size) des données suivant le header. La variable head.size est une string, contenant la taille donnée en octal, convertit en décimal.
		size=strtol(head.size, NULL, 8);

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
			if (extract==1 || decomp==1) {
				//On utilise le buffer data pour le read() des données pour l'extraction
				data=malloc(size_reelle);

				//On récupère les données suivant le buffer, elles vont servir pour l'extraction
				status=read(file, data, size_reelle);

				//Libération de la mémoire si non extraction : on ne peut pas lseek() dans un tube nommé.
				if (extract==0) {
					free(data);
				}
			}
			//Sinon un simple lseek() suffira (déplace la tête de lecture d'un certain offset). SAUF pour le tube nommé (voir plus haut).
			else {
				//Le flag (whence) SEEK_CUR assure que la tête de lecture est déplacée de size_réelle octets relativement à la position courante.
				status=lseek(file, size_reelle, SEEK_CUR);
			}

			//Cas d'erreur -1 du read
			if (status<0) {
				printf("Erreur dans la lecture du header, read() retourne : %d\n", status);
				if (logflag==1) fclose(logfile);
				close(file);
				return 1;
			}
		}

		/*
		Traitement du listing du header (détaillé -l ou non)
		*/

		//Listing détaillé :
		if (listingd==1) {
			listing(head);
		}
		//Listing basique :
		else {
			printf("%s\n", head.name);
		}

		/*
		Traitement de l'extraction -x de l'élément lié au header
		*/

		if (extract==1) {

			//Récupération du type d'élément.
			typeflag=strtol(head.typeflag, NULL, 10);

			//Récupération du nom dans un tableau si c'est un dossier (et de son mtime)
			if (typeflag==5) {

				//Réalloc de dirlist
				//dirlist=realloc(&dirlist,(nbdir+1)*sizeof(char));
				/*
				if (dirlisttemp==NULL) {
							free(dirlist);    //Désallocation
							printf("Problème de réallocation du tableau de dossier (utime final).\n");
							break;
				}
				else dirlist=dirlisttemp;*/

				//On récupère le nom
				//dirlist[nbdir]=(char *)malloc(sizeof(char)*100);
				//dirlist2=(char *)head.name;
				//dirlist[nbdir]=&dirlist2[nbdir*strlen(head.name)];
				strcpy(dirlist[nbdir], head.name);

				//On incrémente le nombre de char
				//nbchar=nbchar+sizeof(head.name);

				//printf("NOM : %s\n", dirlist[nbdir]);

				//Réalloc de mtimes
				mtime=strtol(head.mtime, NULL, 8);
				mtimestemp=realloc(mtimes, (nbdir+1)*sizeof(int));
				if (mtimestemp==NULL) {
							free(mtimes);    //Désallocation
							printf("Problème de réallocation du tableaux des mtime.\n");
							break;
				}
				else mtimes=mtimestemp;

				//Récupération à proprement parler.
				mtime=strtol(head.mtime, NULL, 8);
				mtimes[nbdir]=mtime;
				nbdir++; //Incrémentaion du nombre de dossiers (représente enfait l'indice dans les tableaux)
			}

			//Extraction de l'élément.
			extreturn=extraction(&head, NULL, data, logfile);

			if (logflag==1) fprintf(logfile, "Retour d'extraction de %s : %d\n", head.name, extreturn);
		}

		if (thrd==1) {
			nthreads=0; // JUSTE POUR FAIRE TAIRE CE WARNING UNUSED PARAMETER
		}

	} while (isEOF==false);  //On aurait pu mettre while(1) puisque la boucle doit normalement se faire breaker plus haut.

	/*
	Fin des traitements
	*/

	//Fermeture de l'archive/tube nommé suivant le cas.
	close(file);

	//Suppression du tube nommé si il a été créé biensur.
	if (decomp==1) {
		status=remove(pipename);
		if (logflag==1) fprintf(logfile, "[Fichier %s] Code retour du remove : %d\n", folder, status);
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
		fputs("Les sommes de contrôle (checksum) sont toutes valides.\n", logfile);
		fputs("Fin de decompression/extraction.\n\n", logfile);
		fclose(logfile);
	}

	//Si l'archive est corrompu, ptar doit renvoyer 1
	if (isCorrupted==true) return 1;

	//Sinon normalement tout s'est bien passé, et ptar renvoie 0.
	else return 0;
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

	int size;
	int mode;
	int uid;
	int gid;
	int typeflag;

	//Récupération de la taille (comme dans le main)
	size=strtol(head.size, NULL, 8);

	//head.mode = les permissions du fichier en octal, on les convertit en décimal.
	mode=strtol(head.mode, NULL, 8);

	//Récupération du uid et gid convertit depuis l'octal
	uid=strtol(head.uid, NULL, 8);
	gid=strtol(head.gid, NULL, 8);

	//Récupération du typeflag
	typeflag=strtol(head.typeflag, NULL, 10);

	//Récupération du mtime et conversion/formatage sous la forme désirée. (mtime est en secondes écoulées depuis l'Epoch !)
	mtime=strtol(head.mtime, NULL, 8);
	ts=*localtime(&mtime);  //Formate en "aaaa-mm-jj hh:mm:ss"
	strftime(bmtime, sizeof(bmtime), "%Y-%m-%d %H:%M:%S", &ts);

	//Print des permissions de l'élément
	if (typeflag==5) printf("d");
	else printf( (typeflag==2) ? "l" : "-");
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
	printf(" %d/%d %d %s %s%s%s\n", uid, gid, size, bmtime, head.name, (typeflag==2) ? " -> " : "", (typeflag==2) ? head.linkname : "");

	return 0;
}



/*
Traite l'extraction des éléments (regular files, directory, symbolic links)
*/

int extraction(headerTar *head, char *namex, char *data, FILE *logfile) {

	struct timeval *tv;
	char *name;
	char *namep;

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

	//Nécessaire d'initialiser ces flags car pas initialisés dans tous les cas (et causent un 'faux' return -1 dans certains cas).
	file=0;
	writ=0;
	sync=0;
	utim=0;
	etatgid=0;
	etatuid=0;
	etat=0;

	//Récupération du name
	if (namex!=NULL) {
		name=namex;
	}
	else {
		name=head->name;
	}

	//Récupération du mtime en secondes depuis l'Epoch. (voir tar(5))
	mtime=strtol(head->mtime, NULL, 8);

	//On set les champs de la structure timeval[2] pour utimes() et lutimes().
	tv=malloc(2*sizeof(struct timeval));
	tv[0].tv_sec=mtime;
	tv[0].tv_usec=0;
	tv[1].tv_sec=mtime;
	tv[1].tv_usec=0;

	//Récupération du uid et gid convertit depuis l'octal en décimal.
	uid=strtol(head->uid, NULL, 8);
	gid=strtol(head->gid, NULL, 8);

	//On set l'uid et le gid du processus (cette instance d'extraction()) à ceux de l'élément à extraire. Voir setuid(3) et setgid(3).
	etatuid=setuid(uid);
	etatgid=setgid(gid);
	if (logflag==1) fprintf(logfile, "[Element %s] Code retour du setuid : %d et du setgid : %d\n", name, etatuid, etatgid);

	//Récupération des permissions du fichier en octal, converties en décimal.
	mode=strtol(head->mode, NULL, 8);

	//Récupération de la taille (comme dans le main).
	size=strtol(head->size, NULL, 8);

	//Séléction du type d'élément et actions.
	switch (head->typeflag[0]) {
		//Fichiers réguliers
		case '0' :
			//openb() avec O_CREAT pour créer le fichier et O_WRONLY pour pouvoir écrire dedans. O_EXCL pour détecerla préexistence.
			file=open(name, O_CREAT | O_EXCL | O_WRONLY, mode);
			if (logflag==1) fprintf(logfile, "[Fichier %s] Code retour du open : %d\n", name, file);
			//Ajustement des permissions (dans le cas de l'extraction post décompression) : le fichier existe déjà donc file<0 à cause de O_EXCL
			if (file<0) {
				etat=chmod(name, mode);
				if (logflag==1) fprintf(logfile, "[Fichier %s] Code retour du chmod : %d\n", name, etat);
			}

			//Voir la partie (2) de traitement(): récupération de données. Il faut utiliser size et pas size_reelle cette fois-ci
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
			etat=close(file);
			if (logflag==1) fprintf(logfile, "[Fichier %s] Code retour du close : %d\n", name, etat);
			//On utilise utimes au lieu de utime pour factoriser la structure (donc le code) commune avec lutimes (symlinks).
			utim=utimes(name, tv); //Une fois le fichier créé complétement (et fermé!), on configure son modtime (et actime).
			if (logflag==1) fprintf(logfile, "[Fichier %s] Code retour du utime : %d\n", name, utim);
			break;
		//Liens symboliques
		case '2' :
			//On vérifie si l'arborescence de dossiers existe et on la crée le cas échéant.
			/*       CHECK PATH    */
			//On vérifie si le fichier pointé existe et on le créer le cas échéant.
			if (existe(head->linkname)==false) {
					/*  CREER FICHIER POINTé ET SON ARBORESCENCE  */
			}
			//On créé ensuite le lien symbolique
			etat=symlink(head->linkname, name);
			if (logflag==1) fprintf(logfile, "[Lien symbolique %s] Code retour du symlink : %d\n", name, etat);
			//On utilise lutimes car utime ne fonctionne pas sur les symlink : voir lutimes(3).
			utim=lutimes(name, tv);
			if (logflag==1) fprintf(logfile, "[Lien symbolique %s] Code retour du utime : %d\n", name, utim);
			break;
		//Répertoires
		case '5' :
			//On vérifie l'existence du dossier.
			if (existe(name)==true) {
				//Si le dossier existe et qu'on a son header, on met à jour ses permissions.
				printf("ATTTEEEEENTTIOOOON\n");
				etat=chmod(name, mode);
				if (logflag==1) fprintf(logfile, "[Fichier %s] Code retour du chmod : %d\n", name, etat);
			}
			//Le dossier n'existe pas, on va chercher à le créer si son parent existe, récursivement.
			else {
				/*    CHECK PATH    */
				etat=mkdir(name, mode);
				if (logflag==1) fprintf(logfile, "[Dossier %s] Code retour du mkdir : %d\n", name, etat);
			}
			break;
		default:
			printf("Elément inconnu par ptar : Typeflag = [%c]\n", head->typeflag[0]);
			etat=-1;
			break;
	}

	//On libère la mémoire allouée pour les timeval
	free(tv);

	//Code de retour de extraction (évolutif, rajouter éventuellement des cas de return -1)
	if (etat<0  || file<0 || writ<0 || etatuid<0 || etatgid<0 || sync<0 || utim<0) {
		return -1;
	}
	else return 0;
}



/*
Traite la décompression de l'archive .tar.gz avec zlib (éventuellement d'une archive .gz pure aussi)
*/

const char *decompress(char *folder, FILE *logfile, bool isonlygz, const char *filenamegz) {

	void *handle;
	char *error;
	char *erroropen;
	char *errorread;
	char *errorclose;
	char *erroreof;
	char *errorrewind;
	char *data;
	gzFile file;

	gzFile (*gzopen)();
	int (*gzread)();
	int (*gzclose)();
	int (*gzrewind)();
	int (*gzeof)();
	int status;
	int stat;
	int etat;
	int pipstat;
	int entreetube; 			//Entrée du tube nommé. (retourné par la fonction)
	pid_t pid;        		//pid pour le fork, permet à la fonction de terminer pour ouvrir le tube de l'autre côté.
	pid_t fpid;       		//pid du processus fils, on va lui envoyer un signal pour lui dire que ce processus (le père) a terminé.


	if (logflag==1) fprintf(logfile, "Debut de la decompression de l'archive %s\n", folder);

	//Chargement de la bibliothèque zlib avec dlopen.
	handle=dlopen("./zlib/libz.so", RTLD_NOW);

	//Solution bricolée pour le test blanc: On suppose que si ça ne marche pas, c'est qu'on essaie de charger depuis un sous dossier.
	if (!handle) {
		handle=dlopen("../zlib/libz.so", RTLD_NOW);
	}

	//Ultime tentative de charger zlib depuis un dossier où elle est couramment installée.
	if (!handle) {
		handle=dlopen("/usr/lib/libz.so", RTLD_NOW);
	}

	//Gestion du cas ou la bibliothèque ne charge pas.
	if (!handle) {
		error=dlerror();
		printf("Erreur dans le chargement de zlib : dlerror() = %s\n", error);
		if (logflag==1) {
			fprintf(logfile, "[Archive %s] Code retour du open : %s\n", folder, error);
			fclose(logfile);
		}
		exit(EXIT_FAILURE);
	}

	//On récupère les fonctions utiles à la décompression avec dlsym
	gzopen=dlsym(handle, "gzopen");
	erroropen=dlerror();
	if (logflag==1) fprintf(logfile, "[Archive %s] Code retour du dlsym(gzopen) : %s\n", folder, erroropen);
	gzrewind=dlsym(handle, "gzrewind");
	errorrewind=dlerror();
	if (logflag==1) fprintf(logfile, "[Archive %s] Code retour du dlsym(gzrewind) : %s\n", folder, errorrewind);
	gzeof=dlsym(handle, "gzeof");
	erroreof=dlerror();
	if (logflag==1) fprintf(logfile, "[Archive %s] Code retour du dlsym(gzeof) : %s\n", folder, erroreof);
	gzread=dlsym(handle, "gzread");
	errorread=dlerror();
	if (logflag==1) fprintf(logfile, "[Archive %s] Code retour du dlsym(gzread) : %s\n", folder, errorread);
	gzclose=dlsym(handle, "gzclose");
	errorclose=dlerror();
	if (logflag==1) fprintf(logfile, "[Archive %s] Code retour du dlsym(gzclose) : %s\n", folder, errorclose);

	//Cas d'erreur des dlsym
	if (erroropen!=NULL || errorread!=NULL || errorclose!=NULL || errorrewind!=NULL || erroreof!=NULL) {
		printf("Erreur detectee par dlerror() : %s %s %s\n", erroropen, errorread, errorclose);
		dlclose(handle);
		if (logflag==1) fclose(logfile);
		exit(EXIT_FAILURE);
	}

	/* ECRITURE dans un tube nommé, en vu de traitement dans traitement() */

	//Le nom du tube nommé est passé en variable globale.

	//Suppression d'un éventuel tube nommé du même nom déjà existant (qui ferait échouer le programme)
	remove(pipenamed);

	//Création effective du tube nommé
	pipstat = mkfifo(pipenamed, S_IRWXU | S_IRGRP | S_IWGRP);
	if (logflag==1) fprintf(logfile, "[Archive %s] Code retour du mkfifo : %d\n", folder, pipstat);

	//Cas d'erreur lors de la création du tube.
	if (pipstat==-1) {
		printf("Erreur dans la création du tube nommé (pour la décompression).\n");
		dlclose(handle);
		if (logflag==1) fclose(logfile);
		exit(EXIT_FAILURE);
	}

	//fork du processus, le père va servir à lancer l'ouverture de la sortie du tube dans le programme appelant.
	pid=fork();

	//Si on est dans le père, on termine ce processus afin que traitement() puisse continuer et ouvrir le tube de l'autre côté.
	if (pid!=0) {
		if (logflag==1) fprintf(logfile, "[Archive %s] Return du processus père ... pid : %d\n", folder, pid);

		//Déchargement de la bibliothèque dynamique.
		dlclose(handle);

		//Retour du nom du tube nommé permettant de l'ouvrir dans le père (post retour)
		return pipenamed;
	}

	//Sinon si on est dans le fils on continue : on écrit les données dans le tube puis le fils est kill.
	else {

		//Open de l'archive avec la bibliothèque zlib
		file=(*gzopen)(folder,"rb");

		//On rembobine la tête de lecture au cas où
		(*gzrewind)(file);

		//Cas où l'open échoue.
		if (file==NULL) {
			printf("Erreur dans l'ouverture du gzfile\n");
			if (logflag==1) {
				fprintf(logfile, "[Archive %s] Echec de l'ouverture avec gzopen : code retour = NULL\n", folder);
				fclose(logfile);
			}
			exit(EXIT_FAILURE);
		}

		/* OPTIONNEL et NON NECESSAIRE. Semi-fonctionnel, developpement inutile pour le projet. */
		//Cas de l'archive .gz (et pas .tar.gz) : écriture directe.
		if (isonlygz==true) {
			status=open(filenamegz, O_CREAT | O_WRONLY, S_IRWXU); //Il faudrait récupérer les permissions du fichier.
			if (logflag==1) fprintf(logfile, "[Archive %s] Code retour du open : %d\n", folder, status);
			data=malloc(1);
			while (!gzeof(file)) {
				stat=(*gzread)(file, data, 1);
				etat=write(status, data, 1);
			}
			if (logflag==1) fprintf(logfile, "[Archive %s] Code retour du gzread : %d\n", folder, stat);
			if (logflag==1) fprintf(logfile, "[Archive %s] Code retour du write : %d\n", folder, etat);
			etat=close(status);
			if (logflag==1) {
				fprintf(logfile, "[Archive %s] Code retour du close : %d\n", folder, etat);
				fclose(logfile);
			}
			free(data);
			printf("%s\n", filenamegz);
			//Etant donné que ce n'est pas censé être une utilisation normale de ptar, on termine le processus.
			exit(EXIT_FAILURE);
		}
		/* FIN DU MODULE OPTIONNEL */

		//Ouverture de l'entrée du tube nommé.
		entreetube=open(pipenamed, O_WRONLY);
		if (logflag==1) fprintf(logfile, "[Archive %s] Code retour du open du tube : %d\n", folder, entreetube);

		//Cas d'erreur lors de l'ouverture de l'entrée du tube.
		if (entreetube==-1) {
			printf("Erreur dans l'ouverture (de l'entrée) du tube nommé (pour la décompression).\n");
			if (logflag==1) fclose(logfile);
			exit(EXIT_FAILURE);
		}

		//Comme dans une archive tar tout est "rangé" dans des blocs de 512 octets, on lit donc par 512 octets.
		data=malloc(512);
		while (!gzeof(file)) {
			stat=(*gzread)(file, data, sizeof(data));
			status=write(entreetube, data, sizeof(data));
		}
		free(data);
		if (logflag==1) fprintf(logfile, "[Archive %s] Code retour du dernier gzread : %d\n", folder, stat);
		if (logflag==1) fprintf(logfile, "[Archive %s] Code retour du dernier write : %d\n", folder, status);


		//Close du .tar.gz
		etat=(*gzclose)(file);
		if (logflag==1) {
			fprintf(logfile, "[Archive %s] Code retour du gzclose : %d\n", folder, etat);
			if (logflag==1) fclose(logfile);
		}

		//Déchargement de la bibliothèque dynamique.
		dlclose(handle);

		//Kill du processus fils
		fpid = getpid();
		kill(fpid, SIGQUIT);

		//Exit si erreur (normalement jamais atteint à cause du kill précédent);
		printf("Une erreur est survenu lors du kill du processus fils...");
		if (logflag==1) fclose(logfile);
		exit(EXIT_FAILURE);
	}
}



/*
Sert à vérifier la non-corruption d'une archive tar après sont téléchargement.
Vérifie la somme de contrôle stockée dans le header passé en paramère.
Pour cela, recalcule le checksum du header et le compare au champs checksum sur header.
Avant la comparaison. on applique un masque 0x3FFFF au checksum calculé car seul les 18 bits de poids faible
nous importent ici pourla comparaison. Pour cela on utilise un ET bit-à-bit. Ce n'est pas nécessaire dans la
plupart des cas mais c'est plus sûr pour la comparaison da
Retourne false si le cheader n'est pas corrompu, true sinon.
*/

bool checksum(headerTar *head) {

	int i;
	unsigned int chksum;
	unsigned int chksumtotest;
	char *pHeader;

	chksum=0;
	chksumtotest=0;

	//On récupère le champs checksum du header, il est en octal ASCII.
	chksumtotest = strtol(head->checksum, NULL, 8);

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
