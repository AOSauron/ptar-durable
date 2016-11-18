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
#include <dlfcn.h>
#include <stdbool.h>

#include "header.h"
#include "zlib/zlib.h"

/*
Traite l'extraction des éléments (regular files, directory, symbolic links)
*/

int extraction(struct header_posix_ustar head, char *name, char *data, FILE *logfile, int log) {

	struct timeval tv[2];
	
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
				file=open(head.name, O_CREAT | O_WRONLY, mode); //O_CREAT pour créer le fichier et O_WRONLY pour pouvoir écrire dedans.
			}
			else {
				file=open(name, O_CREAT | O_WRONLY, mode); //Permet la personnalisation du nom de fichier (voir decompression)
			}
			if (log==1) fprintf(logfile, "[Fichier %s] Code retour du open : %d\n", head.name, file);
			//Voir la partie (2)du main: récupération de données. Il faut utiliser size et pas size_reelle cette fois-ci
			if (size > 0) {  //Ecriture si seulement le fichier n'est pas vide !
      				writ=write(file, data, size);
				if (log==1) fprintf(logfile, "[Fichier %s] Code retour du write : %d\n", head.name, writ);
				//sync=fsync(file);
				if (log==1) fprintf(logfile, "[Fichier %s] Code retour du fsync : %d\n", head.name, sync);
				free(data); //On libère la mémoire allouée pour les données pointées.
			}
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
Traite la décompression de l'archive .tar.gz avec zlib
*/

char *decompress(char *directory, FILE *logfile, int log, int extract, bool isonlygz, char *filenamegz) {

	void *handle;
	char *error;
	char *erroropen;
	char *errorread;
	char *errorwrite;
	char *errorclose;
	char *data;
	char *filename;
	gzFile file;

	struct header_posix_ustar head;

	gzFile (*gzopen)();
	int (*gzread)();
	int (*gzwrite)(); //à utiliser plus tard pour la compression. (hors consignes)
	int (*gzclose)();
	int status;
	int stat;
	int etat;
	long size;

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
	gzwrite=dlsym(handle, "gzwrite");
	errorwrite=dlerror();
	if (log==1) fprintf(logfile, "[Archive %s] Code retour du dlsym(gzwrite) : %s\n", directory, errorwrite);
	gzclose=dlsym(handle, "gzclose");
	errorclose=dlerror();
	if (log==1) fprintf(logfile, "[Archive %s] Code retour du dlsym(gzclose) : %s\n", directory, errorclose);

	//Cas d'erreur des dlsym
	if (erroropen!=NULL || errorread!=NULL || errorwrite!=NULL || errorclose!=NULL) {
		printf("Erreur detectee par dlerror() : %s %s %s %s\n", erroropen, errorread, errorwrite, errorclose);
		exit(EXIT_FAILURE);
	}

	//Open de l'archive avec la bibliothèque zlib
	file=(*gzopen)(directory,"rb");
	
	//Cas où l'open échoue.
	if (file==NULL) {
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
		return 0;
	}
	/* FIN DU MODULE OPTIONNEL */


	//Read du head l'archive compressée avec la bibliothèque zlib. Une archive gz semble avoir le même header qu'une archive tar.
	status=(*gzread)(file, &head, sizeof(head));
	if (log==1) fprintf(logfile, "[Archive %s] Code retour du 1er gzread : %d\n", directory, status);

	//Récupération de la taille et allocation de la mémoire.
	size=strtol(head.size, NULL, 8);
	data=malloc(size);

	//Récupération des données suivant le head.
	status=(*gzread)(file, data, size);
	if (log==1) fprintf(logfile, "[Archive %s] Code retour du 2e gzread : %d\n", directory, status);

	//Copie et modification éventuelle (pour le cas temporaire) du filename
	filename=malloc(sizeof(head.name));
	strcpy(filename, head.name);

	//Extraction du fichier.tar, si l'option -x est spécifiée le fichier.tar sera supprimé plus tard. (nommé <filename>.tar~)
	if (extract==0) {
		status=extraction(head, NULL, data, logfile, log);
		if (log==1) fprintf(logfile, "[Archive %s] Code retour de la decompression (ecriture par extraction()) : %d\n", directory, status);
	}
	else {
		strcat(filename, "~"); // On ajoute un caractère spécial pour éviter toute suppression accidentelle de fichier préexistant.
		printf("filename temp: %s\n", filename);
		status=extraction(head, filename, data, logfile, log);
		if (log==1) fprintf(logfile, "[Archive %s] Code retour de la decompression (ecriture par extraction()) : %d\n", directory, status);
	}
	//Close du .gz
	etat=(*gzclose)(file);
	if (log==1) fprintf(logfile, "[Archive %s] Code retour du gzclose : %d\n", directory, etat);

	//Déchargement de la bibliothèque dynamique.
	dlclose(handle);
	
	//Retour du filename du fichier .tar
	return filename;
}

