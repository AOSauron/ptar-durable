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

#include "header.h"


/*
Traite l'exctraction des éléments (regular files, directory, symbolic links)
*/

void extraction(struct header_posix_ustar head, char *data, FILE *logfile) {

	int mode;
	int size;

	//Code de retour des open/write/close/symlink/mkdir - A lire dans logfile.txt		
	int etat0;
	int file0;
	int write0;
	int etat2;
	int etat5;

	//Récupération de la taille (comme dans le main)
	size=strtol(head.size, NULL, 8);

	//head.mode = les permissions du fichier en octal, on les convertit en décimal.
	mode= strtol(head.mode, NULL, 8);

	//Séléction du type d'élément et actions. Les printf aident au débugging. 
	switch (head.typeflag[0]) {
		//Fichiers réguliers
		case '0' :
			//printf("Permissions : %d\n", mode); // DEBUGGING
			file0=open(head.name, O_CREAT | O_WRONLY, mode); //O_CREAT pour créer le fichier et O_WRONLY pour pouvoir écrire dedans.
			fprintf(logfile, "[Fichier %s] Code retour du open : %d\n", head.name, file0);
			//Voir la partie (2)du main: récupération de données. Il faut utiliser size et pas size_reelle cette fois-ci
			if (size > 0) {  //Ecriture si seulement le fichier n'est pas vide !
      				write0=write(file0, data, size);
				fprintf(logfile, "[Fichier %s] Code retour du write : %d\n", head.name, write0);
				free(data); //On libère la mémoire allouée pour le données pointées.
			}
			etat0=close(file0);
			fprintf(logfile, "[Fichier %s] Code retour du close : %d\n", head.name, etat0);
			break;
		//Liens symboliques
		case '2' :
			etat2=symlink(head.linkname, head.name);
			fprintf(logfile, "[Lien symbolique %s] Code retour du symlink : %d\n", head.name, etat2);
			break;
		//Répertoires
		case '5' :
			etat5=mkdir(head.name, mode);
			fprintf(logfile, "[Dossier %s] Code retour du mkdir : %d\n", head.name, etat5);
			break;
		default:
			printf("Elément inconnu par ptar : Typeflag = [%c]\n", head.typeflag[0]);
	}
}
