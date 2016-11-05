///////// Pour ptar 1.3 minimum /////////
/*

Fonctions utils

Fonctions principales du programme ptar :
Correspondantes aux options suivantes :
-x : Extraction.
-l : Listing détaillé.
-z : Décompression gzip avec la bibliothèque zlib.
-p NBTHREADS : (forcément couplée avec -x au moins) Durabilité et parallélisation de l'opération avec un nombre de threads choisi.

*/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <time.h>
#include <utime.h>

#include "header.h"


/*
Traite l'extraction des éléments (regular files, directory, symbolic links)
*/

int extraction(struct header_posix_ustar head, char *data, FILE *logfile) {

	struct utimbuf tm;
	
	int mode;
	int size;
	int uid;
	int gid;
	int mtime;

	//Code de retour des open/write/close/symlink/mkdir/setuid/setgid/utime - A lire dans logfile.txt		
	int etat;
	int file;
	int writ;
	int etatuid;
	int etatgid;
	int sync;
	int utim;

	//Récupération du mtime en secondes depuis l'Epoch. (voir tar(5))
	mtime=strtol(head.mtime, NULL, 8);

	//On set les champs de la struct utime avec la valeur mtime (date de modif).
	tm.actime=mtime; //La date d'accès est supposée être la même que la date de modif.
	tm.modtime=mtime;

	//Récupération du uid et gid convertit depuis l'octal en décimal.
	uid=strtol(head.uid, NULL, 8);
	gid=strtol(head.gid, NULL, 8);

	//On set l'uid et le gid du processus (cette instance d'extraction()) à ceux de l'élément à extraire. Voir setuid(3) et setgid(3).
	etatuid=setuid(uid);
	etatgid=setgid(gid);

	//Récupération de la taille (comme dans le main).
	size=strtol(head.size, NULL, 8);

	//Récupération des permissions du fichier en octal, converties en décimal.
	mode= strtol(head.mode, NULL, 8);

	//Séléction du type d'élément et actions.
	switch (head.typeflag[0]) {
		//Fichiers réguliers
		case '0' :
			fprintf(logfile, "[Fichier %s] Code retour du setuid : %d et du setgid : %d\n", head.name, etatuid, etatgid);
			file=open(head.name, O_CREAT | O_WRONLY, mode); //O_CREAT pour créer le fichier et O_WRONLY pour pouvoir écrire dedans.
			fprintf(logfile, "[Fichier %s] Code retour du open : %d\n", head.name, file);
			//Voir la partie (2)du main: récupération de données. Il faut utiliser size et pas size_reelle cette fois-ci
			if (size > 0) {  //Ecriture si seulement le fichier n'est pas vide !
      				writ=write(file, data, size);
				sync=fsync(file);
				fprintf(logfile, "[Fichier %s] Code retour du write : %d\n", head.name, writ);
				fprintf(logfile, "[Fichier %s] Code retour du fsync : %d\n", head.name, sync);
				free(data); //On libère la mémoire allouée pour le données pointées.
			}
			etat=close(file);
			utim=utime(head.name, &tm); //Une fois le fichier créer complétement, on configure son modtime (et actime).
			fprintf(logfile, "[Fichier %s] Code retour du close : %d\n", head.name, etat);
			fprintf(logfile, "[Fichier %s] Code retour du utime : %d\n", head.name, utim);
			break;
		//Liens symboliques
		case '2' :
			fprintf(logfile, "[Lien symbolique %s] Code retour du setuid : %d et du setgid : %d\n", head.name, etatuid, etatgid);
			etat=symlink(head.linkname, head.name);
			utim=utime(head.name, &tm);
			fprintf(logfile, "[Lien symbolique %s] Code retour du symlink : %d\n", head.name, etat);
			fprintf(logfile, "[Lien symbolique %s] Code retour du utime : %d\n", head.name, utim);
			break;
		//Répertoires
		case '5' :
			fprintf(logfile, "[Dossier %s] Code retour du setuid : %d et du setgid : %d\n", head.name, etatuid, etatgid);
			etat=mkdir(head.name, mode);
			utim=utime(head.name, &tm);
			fprintf(logfile, "[Dossier %s] Code retour du mkdir : %d\n", head.name, etat);
			fprintf(logfile, "[Dossier %s] Code retour du utime : %d\n", head.name, utim);
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
	printf( (typeflag==5) ? "d" : "");
	if (typeflag!=5) printf( (typeflag==2) ? "l" : "-");
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
	printf(" %d/%d %d %s %s %s %s\n", uid, gid, size, bmtime, head.name, (typeflag==2) ? "->" : "", head.linkname);

	return 0;
}
