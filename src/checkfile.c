///////// PTAR 1.7 /////////
/*
Sert à vérifier si l'argument passé lors de l'éxécution. (i.e. l'archive)
Si il existe, vérifie est bien nommé sous la forme : "*.tar" ou "*.tar.gz" (si l'extension existe et si elle est correcte)
ptar ne gère que des archives dont le NOM se termine seulement par ".tar" ou ".tar.gz".
En effet, tar lui peut gérer des archives 'mal nommées' (c'est-à-dire sans extension)

La validité du fichier (c'est-à-dire savoir si il s'agit rééllement d'une archive .tar ou .tar.gz) sera vérifiée
lors du premier open() dans la boucle principale du main.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>

#include "utils.h"

bool checkfile(char *filename) {

	int cpt_token;
	const char *delim;
	char *token;
	char *token_courant;
	char *token_suivant;
	char directory_test[255];

	cpt_token=0; //Compteur de token, ici compteur de mots séparés par des "." dans le nom
	delim=".";
	token_courant="";
	token_suivant="";

	//Faire taire ces warning unused
	MutexRead=(pthread_mutex_t) MutexRead;
	MutexWrite=(pthread_mutex_t) MutexWrite;

	//Test de l'existence d'un argument.
	if (filename==NULL) {
		printf("ptar : erreur pas d'archive tar en argument. Utilisation: ./ptar [-xlzp NBTHREADS] emplacement_archive.tar[.gz]\n");
		return false;
	}

	//On récupère l'argument (dans une variable temporaire car strtok agit dessus) et on test si c'est bien une archive .tar ou .tar.gz
	strcpy(directory_test, filename);

	//Récupère le premier token (voir strtok(3))
	token=strtok(directory_test, delim);

 	//Récupère les autres token et ne conserve que les 2 derniers à chaque fois. On récupère le nom sans l'extension .gz également.
 	do {
		cpt_token++;
		token_courant=token_suivant;
		token_suivant=token;
	  token=strtok(NULL, delim);
 	} while (token != NULL);

	//Détection de l'extension
	//Cas du .tar.gz ou .gz
	if (strcmp(token_suivant, "gz") == 0) {
		if ((strcmp(token_courant, "tar") == 0) && cpt_token > 2) {
			if (decomp==0) { //Vérification du flag de décompression
				printf("Séléctionnez l'option -z au minimum pour les fichiers au format .tar.gz\n");
				return false;
			}
			else {
				return true;
			}
		}
		else if (extract==0 && listingd==0 && decomp==1) { //Cas spécial .gz pur.
			printf("Le nom du fichier %s semble être un .gz pur, ptar n'est pas prévu pour un tel cas !\n", filename);
			return false;
		}
		else {
			printf("Le nom du fichier %s n'est pas au bon format. ([.tar].gz) ou n'utilisez que -z pour un fichier .gz pur.\n", filename);
			return false;
		}
	}
	//Cas du .tar
	else if (strcmp(token_suivant, "tar") == 0) {
		if (cpt_token >= 2) {
			if (decomp==1) { //Vérification du flag de décompression
				printf("L'option -z s'utilise pour décompresser les fichiers au format [.tar].gz\n");
				return false;
			}
			else return true;
		}
		else {
			printf("Le nom du fichier %s n'est pas au bon format. (.tar[.gz])\n", filename);
			return false;
		}
	}
	//Autres cas éventuels:
	else {
		printf("Le nom du fichier %s n'est pas au bon format. (.tar[.gz])\n", filename);
		return false;
	}
}


/*
Cette fonction vérifie l'existence du fichier ou dossier ou lien symbolique passé en paramètre. Sert à l'extraction().
Retourne true si il existe, false sinon.
*/

bool existeDir(char *folder) {

	DIR *dirstream;

	dirstream=opendir(folder);

	if (dirstream==NULL) {
		return false;
	}
	else {
		closedir(dirstream);
		return true;
	}
}


/*
Cette fonction vérifie l'existence du fichier passé en paramètre. Sert à l'extraction() et à checkpath.
Retourne true si il existe, false sinon.
*/

bool existeFile(char *file) {

	FILE *fd;

	//Utiliser fopen est mieux que open avec O_CREAT et O_EXCL, pour les modes.
	fd=fopen(file, "r");
	if (fd==NULL) {
		return false;
	}
	else {
		fclose(fd);
		return true;
	}
}


/*
Fonction pour les liens symboliques : reconstitue le chemin d'accès complet à partir du linkname du header
et du pathname du lien symobolique.
Retourne le chemin d'accès complet du fichier pointé par le lien symbolique.
Seulement si le lien est plus haut que le fichier pointé !!
*/

char *recoverpath(char *linkname, char *pathlink, char pathname[]) {

	char *token;
	char *token_suivant;
	char *token_courant;
	const char *delim;
	char linkbuf[255];
	char pathbuf[255];

	token="";
	token_courant="";
	token_suivant="";
	delim="/";
	strcpy(linkbuf, linkname);
	strcpy(pathbuf, pathlink);

	//Récupération de la première partie du path
	token=strtok(pathbuf, delim);

	do {
		token_courant=token_suivant;
		token_suivant=token;
	  token=strtok(NULL, delim);
		strcat(pathname, token_courant);
		if (strcmp(token_courant,"")!=0) strcat(pathname, delim);
 	} while (token != NULL);

	//Récupération de la deuxième partie du path et concaténation finale.
	token=linkbuf;

	//On élimine les 2 premiers caractère si c'est  "./"
	if (token[0]=='.' && token[1]=='/') {
		token++;
		token++;
	}

	strcat(pathname,token);

	return pathname;
}


/*
Fonction vérifiant l'existence de l'arborescence de l'élément passé en paramètre.
Vérifie chaque dossier parent en partant de la racine relative, et le crée si n'existe pas.
Ecrit les errno de chaque mkdir dans le logfile. Si aucun errno n'est généré, le errno prendra
le dernier appel de existeDir() qui renvoit habituellement No Such File or Directory
après l'éxécution.
Retourne 0 si la création/exploration a fonctionné et que le path existe.
Retourne -1 sinon.
*/

int checkpath(char *path) {

	char *token;
	char *currentfolder;
	char *followingfolder;
	const char *delim;
	char pathtotest[255];
	char pathbuf[255];
	int etat;
	int etatfinal;

	errno=0;
	etat=0;
	etatfinal=0;
	delim="/";
	token="";
	currentfolder="";
	followingfolder="";

	//On recopie le path dans une variable intermédiaire pour ne pas l'altérer : strtok agit dessus.
	strcpy(pathbuf, path);

	//Premier appel à strtok
	token=strtok(pathbuf, delim);
	currentfolder=token;
	strcpy(pathtotest, currentfolder);
	strcat(pathtotest, delim);

	do {
		token=strtok(NULL, delim);
		followingfolder=token;

		//Si le token suivant est NULL, c'est qu'on a atteind l'élément concerné (donc on return avant de tenter un mkdir dessus)
		if (token==NULL) {
			break;
		}

		//On test l'existence du dossier courant à ce moment de la boucle.
		if (existeDir(pathtotest)==false) {
			//On crée le dossier avec, pour l'instant, tous les droits (on ne sait jamais) sauf écriture pour other.
			etat=mkdir(pathtotest, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
			if (logflag==1) fprintf(logfile, "[Dossier %s] Code retour du errno du mkdir de checkpath (ou opendir si No such file or dir): %s\n", pathtotest, strerror(errno));
			if (etat<0) etatfinal=-1;
		}

		//On constitue le path du dossier suivant.
		strcat(pathtotest, followingfolder);
		strcat(pathtotest, delim);

	} while (token!=NULL);

	return etatfinal;
}
