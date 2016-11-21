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

#include "checkfile.h"
#include "zlib/zlib.h"
#include "utils.h"



/*
Fonction principale : recueille les header de chaque fichier dans l'archive (compressée ou non) ainsi que les données suivantes chaque header si il y en a.
Appelle ensuite les diverses fonctions utiles au traitement souhaité.
*/

int traitement(char *directory, int extract, int decomp, int listingd, int thrd, int nbthrd, int log) {

	/*
	Déclarations et initialisation des variables
	*/
	
	struct header_posix_ustar head;
	struct utimbuf tm; 	   //Structure pour le utime final des dossiers

	char *data; 		   //Buffer pour les données suivant le header.
	char *pipename;		//Nom du tube nommé utilisé lors de la décompression.
	char dirlist[500][100];  //Liste des dossiers. 500 max mais on peut monter ce nombre. Longueur de 100 max pour le nom.
	
	bool isEOF; 		//Flag d'End Of File.

	int file;
	int status;
	int extreturn;	     //Valeur de retour de extraction()
	int mtimes[500];    //Liste des mtime, associé au premier tableau (dans l'ordre).
	int nbdir; 		     //Nombre de dossiers
	int utim;
	int typeflag;
	int mtime;
	int k;
	int size;
	int size_reelle;
	int waitstatus;       //Valeur du status du waitpid

	FILE *logfile; //logfile de l'extraction/decomp (codes de retour des open, symlink, mkdir, etc)

	nbdir=0;
	size_reelle=0;
	isEOF=false;


	/*
	Génération du logfile si besoin -e. Option pour développeurs. (utile pour extraction et décompression)
	*/

	if (log==1) {
		logfile = genlogfile("logfile.txt", "a", directory);
	}

	/*
	Tester l'argument (existence et nom bien formé).
	*/

	if (checkfile(directory, decomp, extract, listingd, log, logfile)==false) {
		//Les messages d'erreurs sont gérés dans checkfile.c. 
		//Si l'archive est seulement compressée, tente une décompression directe: ptar n'est pas prévu pour cela, ce module est donc en bêta (développement inutil)
		return 1;
	}
	
	/*
	Ouverture de l'archive
	*/

	//On ouvre l'archive tar avec open() et le flag O_RDONLY (read-only). Si option -z, on décompresse, récupère les données dans un tube et on ouvre ce tube nommé.
	if (decomp==1) {
		//Décompression et ouverture dans un tube nommé tubedecompression.fifo
		pipename=decompress(directory, logfile, log, false, NULL);
		//Ouverture de la sortie du tube nommé.
		file = open(pipename, O_RDONLY);
		//Attente du fils pour continuer (provient du decompress). Sinon le programme se mettrais à lire avant la fin de l'écriture ! C'est plus sûr d'attendre.
		waitpid(-1, &waitstatus, 0);
	}
	else {
		file = open(directory, O_RDONLY);
	}

	//Cas d'erreur -1 du open() ou du decompress : problème dans le fichier.
	if (file<0) {
		printf("Erreur d'ouverture du fichier, open() retourne : %d. Le fichier n'existe pas ou alors est corrompu.\n", file);
		close(file);
		if (log==1) fclose(logfile); //Aussi le logfile.
		return 1;
	}	


	/*
	Traitement de chaque header les uns après les autres
	*/

	do {

		/*
		Récupération du header, détection de fin de fichier et récupération des données éventuelles.
		*/

		//On lit (read) un premier bloc (header) de 512 octets qu'on met dans une variable du type de la structure header_posix_ustar définie dans header.h (norme POSIX.1)
		status=read(file, &head, sizeof(head));  // Utiliser sizeof(head) est plus évolutif qu'une constante égale à 512.

		//Cas d'erreur -1 du read
		if (status<0) {
			printf("Erreur dans la lecture du header, read() retourne : %d \n", status);
			if (log==1) fclose(logfile);
			close(file);
			return 1;
		}


		/* La fin d'une archive tar se termine par 2 enregistrements d'octets valant 0x0. (voir tar(5))
		Donc la string head.name du premier des 2 enregistrement est forcément vide. 
		On s'en sert donc pour détecter la fin du fichier (End Of File) et ne pas afficher les 2 enregistrements de 0x0. */

		//Détection de l'End Of File : met le flag isEOF à true et stoppe la boucle. Le flag est destiné à aider les développeurs (il ne stoppe pas effectivement la boucle)
		if (strlen(head.name)==0 || status==0) {
			isEOF=true; 
			break;
		}
		

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
				size_reelle=512*((int)(size/512)+1);  //Utiliser (int)variable car c'est des int (floor est utilisé pour des double renvoyant un double)
			}
				
			//On n'alloue de la mémoire que si on a besoin des données (c'est-à-dire si on souhaite extraire)
			if (extract==1 || decomp==1) {
				//On utilise le buffer data pour le read() des données pour l'extraction
				data=malloc(size_reelle);
			
				//On récupère les données suivant le buffer, elles vont servir pour l'extraction
				status=read(file, data, size_reelle);

				//Libération de la mémoire si non extraction : on ne peut pas lseek dans un tube nommé.
				if (extract==0) {
					free(data);
				}
			}
			//Sinon un simple lseek() suffira (déplace la tête de lecture d'un certain offset). SAUF pour le tube nommé (voir plus haut).
			else {
				//Le flag (whence) SEEK_CUR assure que la tête de lecture est déplacée relativement à la position courante.
				status=lseek(file, size_reelle, SEEK_CUR);
			}

			//Cas d'erreur -1 du read
			if (status<0) {
				printf("Erreur dans la lecture du header, read() retourne : %d\n", status);
				if (log==1) fclose(logfile);
				close(file);
				return 1;
			}
		}


		/*
		Traitement du listing du header (détaillé -l ou non)
		*/

		if (listingd==1) { //Listing détaillé :
			listing(head);
		}
		else { //Listing basique :
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
				mtime=strtol(head.mtime, NULL, 8);
				strcpy(dirlist[nbdir], head.name);
				mtimes[nbdir]=mtime;
				nbdir++; //Incrémentaion du nombre de dossiers
			}

			//Extraction de l'élément.
			extreturn=extraction(head, NULL, data, logfile, log);

			if (log==1) fprintf(logfile, "Retour d'extraction de %s : %d\n", head.name, extreturn);
		}

		/*
		Traitement de la durabilité et de la parallélisation sur nthreads -p
		*/

		if (thrd==1) {
			printf("Parallélisation à faire !\n");
		}

	} while (isEOF==false);  //On aurait pu mettre while(1) puisque la boucle doit normalement se faire breaker plus haut.

	
	/*
	Fin des traitements
	*/

	//Fermeture de l'archive.
	close(file);

	//Suppression du tube nommé post-décompression
	if (decomp==1) {
		status=remove(pipename);
		if (log==1) fprintf(logfile, "[Fichier %s] Code retour du remove : %d\n", directory, status);
	}

	//Affectation du bon mtime pour les dossiers après traitement (nécessaire d'être en toute fin) de l'extraction.
	if (extract==1 && nbdir>0) { //Il faut au moins 1 dossier pour permettre l'action.
		for (k=0; k<nbdir; k++) {
			tm.actime=mtimes[k];
			tm.modtime=mtimes[k];
			utim=utime(dirlist[k], &tm);
			//Ajout des derniers retour d'utime() dans le logfile.
			if (log==1) fprintf(logfile, "[Dossier %s] Code retour du utime : %d\n", dirlist[k], utim);
		}
	}

	//Fermeture du logfile
	if (log==1) {
		fputs("Fin de decompression/extraction.\n\n", logfile);
		fclose(logfile);
	}

	return 0;
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

int listing(struct header_posix_ustar head) {
	
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

	/* 
	Ceci produit deux espaces en fin de ligne si l'élément n'est pas un symlink, et fait donc échouer la détection aux test blancs. (reste correct cela dit)
	printf(" %d/%d %d %s %s %s %s\n", uid, gid, size, bmtime, head.name, (typeflag==2) ? "->" : "", (typeflag==2) ? head.linkname : "");
	*/
	
	if (typeflag==2) printf(" %d/%d %d %s %s -> %s\n", uid, gid, size, bmtime, head.name, head.linkname);
	else printf(" %d/%d %d %s %s\n", uid, gid, size, bmtime, head.name);

	return 0;
}



/*
Traite l'extraction des éléments (regular files, directory, symbolic links)
*/

int extraction(struct header_posix_ustar head, const char *name, char *data, FILE *logfile, int log) {

	struct timeval tv[2];
	
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

	//Récupération du mtime en secondes depuis l'Epoch. (voir tar(5))
	mtime=strtol(head.mtime, NULL, 8);	

	//On set les champs de la structure timeval[2] pour utimes() et lutimes(). Il FAUT un timeval[2] à l'inverse du warning de compilation !
	tv[0].tv_sec=mtime;
	tv[0].tv_usec=0;
	tv[1].tv_sec=mtime;
	tv[1].tv_usec=0;

	//Récupération du uid et gid convertit depuis l'octal en décimal.
	uid=strtol(head.uid, NULL, 8);
	gid=strtol(head.gid, NULL, 8);

	//On set l'uid et le gid du processus (cette instance d'extraction()) à ceux de l'élément à extraire. Voir setuid(3) et setgid(3).
	etatuid=setuid(uid);
	etatgid=setgid(gid);

	//Récupération des permissions du fichier en octal, converties en décimal.
	mode=strtol(head.mode, NULL, 8);

	//Récupération de la taille (comme dans le main).
	size=strtol(head.size, NULL, 8);

	//Séléction du type d'élément et actions.
	switch (head.typeflag[0]) {
		//Fichiers réguliers
		case '0' :
			if (log==1) fprintf(logfile, "[Fichier %s] Code retour du setuid : %d et du setgid : %d\n", head.name, etatuid, etatgid);
			if (name==NULL) {
				file=open(head.name, O_CREAT | O_WRONLY | O_SYNC | O_DSYNC, mode); //O_CREAT pour créer le fichier et O_WRONLY pour pouvoir écrire dedans. O_SYNC et O_DSYNC pour fsync().
			}
			else {
				file=open(name, O_CREAT | O_WRONLY | O_SYNC | O_DSYNC, mode); //Permet la personnalisation du nom de fichier (voir decompression)
			}
			if (log==1) fprintf(logfile, "[Fichier %s] Code retour du open : %d\n", head.name, file);
			//Voir la partie (2)du main: récupération de données. Il faut utiliser size et pas size_reelle cette fois-ci
			if (size > 0) {  //Ecriture si seulement le fichier n'est pas vide !
      				writ=write(file, data, size);
				if (log==1) fprintf(logfile, "[Fichier %s] Code retour du write : %d\n", head.name, writ);
				free(data);   //On libère la mémoire allouée pour les données pointées.
			}
			sync=fsync(file);
			if (log==1) fprintf(logfile, "[Fichier %s] Code retour du fsync : %d\n", head.name, sync);
			etat=close(file);
			if (log==1) fprintf(logfile, "[Fichier %s] Code retour du close : %d\n", head.name, etat);
			//On utilise utimes au lieu de utime pour factoriser la structure (donc le code) commune avec lutimes (symlinks).
			utim=utimes(head.name, &tv); //Une fois le fichier créé complétement (et fermé!), on configure son modtime (et actime).
			if (log==1) fprintf(logfile, "[Fichier %s] Code retour du utime : %d\n", head.name, utim);
			break;
		//Liens symboliques
		case '2' :
			if (log==1) fprintf(logfile, "[Lien symbolique %s] Code retour du setuid : %d et du setgid : %d\n", head.name, etatuid, etatgid);
			etat=symlink(head.linkname, head.name);
			if (log==1) fprintf(logfile, "[Lien symbolique %s] Code retour du symlink : %d\n", head.name, etat);
			utim=lutimes(head.name, &tv); //On utilise lutimes car utime ne fonctionne pas sur les symlink : voir lutimes(3).
			if (log==1) fprintf(logfile, "[Lien symbolique %s] Code retour du utime : %d\n", head.name, utim);
			break;
		//Répertoires
		case '5' :
			if (log==1) fprintf(logfile, "[Dossier %s] Code retour du setuid : %d et du setgid : %d\n", head.name, etatuid, etatgid);
			etat=mkdir(head.name, mode);
			//Le utime est fait à la fin (en effet on continue de parcourir les dossiers et d'y créer d'autre élément on modifie donc le mtime juste après !)
			if (log==1) fprintf(logfile, "[Dossier %s] Code retour du mkdir : %d\n", head.name, etat);
			break;
		default:
			printf("Elément inconnu par ptar : Typeflag = [%c]\n", head.typeflag[0]);
	}
	
	//Code de retour de extraction (évolutif, rajouter éventuellement des cas de return -1)
	if (etat<0  || file<0 || writ<0 || etatuid<0 || etatgid<0 || sync<0 || utim<0) {
		return -1;
	}
	else return 0;
}



/*
Traite la décompression de l'archive .tar.gz avec zlib (éventuellement d'une archive .gz pure aussi)
*/

char *decompress(char *directory, FILE *logfile, int log, bool isonlygz, const char *filenamegz) {

	void *handle;
	char *error;
	char *erroropen;
	char *errorread;
	char *errorclose;
	char *data; // Nom du tube nommé.
	char *pipename;
	gzFile file;

	gzFile (*gzopen)();
	int (*gzread)();
	int (*gzclose)();
	int status;
	int stat;
	int etat;
	int pipstat;
	int entreetube; 	//Entrée du tube nommé. (retourné par la fonction)
	pid_t pid;         //pid pour le fork, permet à la fonction de terminer pour ouvrir le tube de l'autre côté.
	pid_t fpid;       //pid du processus fils, on va lui envoyer un signal pour lui dire que ce processus (le père) a terminé.

	if (log==1) fprintf(logfile, "Debut de la decompression de l'archive %s\n", directory);

	//Chargement de la bibliothèque zlib avec dlopen.
	handle=dlopen("./zlib/libz.so", RTLD_NOW);

	//Gestion du cas ou la bibliothèque ne charge pas.
	if (!handle) {
		error=dlerror();
		printf("Erreur dans le chargement de zlib : dlerror() = %s\n", error);
		if (log==1) fprintf(logfile, "[Archive %s] Code retour du open : %s\n", directory, error);
		exit(EXIT_FAILURE);
	}
	
	//On récupère les fonctions utiles à la décompression avec dlsym
	gzopen=dlsym(handle, "gzopen");
	erroropen=dlerror();
	if (log==1) fprintf(logfile, "[Archive %s] Code retour du dlsym(gzopen) : %s\n", directory, erroropen);
	gzread=dlsym(handle, "gzread");
	errorread=dlerror();
	if (log==1) fprintf(logfile, "[Archive %s] Code retour du dlsym(gzread) : %s\n", directory, errorread);
	gzclose=dlsym(handle, "gzclose");
	errorclose=dlerror();
	if (log==1) fprintf(logfile, "[Archive %s] Code retour du dlsym(gzclose) : %s\n", directory, errorclose);

	//Cas d'erreur des dlsym
	if (erroropen!=NULL || errorread!=NULL || errorclose!=NULL) {
		printf("Erreur detectee par dlerror() : %s %s %s\n", erroropen, errorread, errorclose);
		exit(EXIT_FAILURE);
	}

	//Open de l'archive avec la bibliothèque zlib
	file=(*gzopen)(directory,"r");
	
	//Cas où l'open échoue.
	if (file==NULL) {
		printf("Erreur dans l'ouverture du gzfile\n");
		if (log==1) fprintf(logfile, "[Archive %s] Echec de l'ouverture avec gzopen : code retour = NULL\n", directory);
		exit(EXIT_FAILURE);
	}

	/* OPTIONNEL et NON NECESSAIRE. Semi-fonctionnel, developpement inutile pour le projet. */
	//Cas de l'archive .gz (et pas .tar.gz) : écriture directe.
	if (isonlygz==true) {
		status=open(filenamegz, O_CREAT | O_WRONLY, S_IRWXU); //Il faudrait récupérer les permissions du fichier.
		if (log==1) fprintf(logfile, "[Archive %s] Code retour du open : %d\n", directory, status);
		data=malloc(1);
		while ((stat=(*gzread)(file, data, 1))!=0) {
			etat=write(status, data, 1);
		}
		if (log==1) fprintf(logfile, "[Archive %s] Code retour du gzread : %d\n", directory, stat);
		if (log==1) fprintf(logfile, "[Archive %s] Code retour du write : %d\n", directory, etat);
		etat=close(status);
		if (log==1) fprintf(logfile, "[Archive %s] Code retour du close : %d\n", directory, etat);
		free(data);
		printf("%s\n", filenamegz);
		//Etant donné que ce n'est pas censé être une utilisation normale de ptar, on termine le processus.
		exit(EXIT_FAILURE);
	}
	/* FIN DU MODULE OPTIONNEL */
	
	/* ECRITURE dans un tube nommé, en vu de traitement dans traitement() */

	//On donne un nom au tube 
	pipename="tubedecompression.fifo";

	//Suppression d'un éventuel tube nommé du même nom déjà existant (qui ferait échouer le programme)
	remove(pipename);	
	
	//Création effective du tube nommé
	pipstat = mkfifo(pipename, S_IRWXU | S_IRGRP | S_IWGRP);
	if (log==1) fprintf(logfile, "[Archive %s] Code retour du mkfifo : %d\n", directory, pipstat);

	//Cas d'erreur lors de la création du tube.
	if (pipstat==-1) {
		printf("Erreur dans la création du tube nommé (pour la décompression).\n");
		exit(EXIT_FAILURE);
	}

	//fork du processus, le père va servir à lancer l'ouverture de la sortie du tube dans le programme appelant.
	pid=fork();
	
	//Si on est dans le père, on termine ce processus afin que traitement() puisse continuer et ouvrir le tube de l'autre côté.
	if (pid!=0) {
		if (log==1) fprintf(logfile, "[Archive %s] Return du processus père ... pid : %d\n", directory, pid);
		return pipename;
	} 
	
	//Sinon si on est dans le fils on continue : on écrit les données dans le tube puis le fils est kill.
	else {
		//Ouverture de l'entrée du tube nommé.
		entreetube=open(pipename, O_WRONLY);
		if (log==1) fprintf(logfile, "[Archive %s] Code retour du open du tube : %d\n", directory, entreetube);

		//Cas d'erreur lors de l'ouverture de l'entrée du tube.
		if (entreetube==-1) {
			printf("Erreur dans l'ouverture (de l'entrée) du tube nommé (pour la décompression).\n");
			exit(EXIT_FAILURE);
		}
	
		//Comme dans une archive tar tout est "rangé" dans des blocs de 512 octets, on lit donc par 512 octets.
		data=malloc(512);
		while ((stat=(*gzread)(file, data, 512))!=0) {
			status=write(entreetube, data, 512);
		}
		free(data); 
		if (log==1) fprintf(logfile, "[Archive %s] Code retour du dernier gzread : %d\n", directory, stat);
		if (log==1) fprintf(logfile, "[Archive %s] Code retour du dernier write : %d\n", directory, status);


		//Close du .tar.gz
		etat=(*gzclose)(file);
		if (log==1) fprintf(logfile, "[Archive %s] Code retour du gzclose : %d\n", directory, etat);

		//Déchargement de la bibliothèque dynamique.
		dlclose(handle);

		//Kill du processus fils
		fpid = getpid();
		kill(fpid, SIGQUIT);

		//Exit si erreur (normalement jamais atteint à cause du kill précédent);
		printf("Une erreur est survenu lors du kill du processus fils...");
		exit(EXIT_FAILURE);
	}
}
