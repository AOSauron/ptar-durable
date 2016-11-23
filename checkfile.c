///////// Pour ptar 1.3 minimum/////////
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

#include "utils.h"

bool checkfile(char *file, FILE *logfile) {

	bool isonlygz;
	int cpt_token;
   	const char *delim;
   	char *token;
	char *token_courant;
	char *token_suivant;
	char filenamegz[100];
	char directory_test[200];

	cpt_token=0; //Compteur de token, ici compteur de mots séparés par des "." dans le nom
	delim=".";
	token_courant="";
	token_suivant="";
	isonlygz=false;

	//Test de l'existence d'un argument.
	if (file==NULL) {	
		printf("ptar : erreur pas d'archive tar en argument. Utilisation: ./ptar [-xlzp NBTHREADS] emplacement_archive.tar[.gz]\n");
		return false;
	}

	//On récupère l'argument (dans une variable temporaire car strtok agit dessus) et on test si c'est bien une archive .tar ou .tar.gz	
	strcpy(directory_test, file);   // strcpy(char dest, char src);
   
  	//Récupère le premier token (voir strtok(3))
  	token=strtok(directory_test, delim);
	
	//Initialisation du filenamegz pour le cas du fichier non archivé mais compressé.
	strcpy(filenamegz, "");

   	//Récupère les autres token et ne conserve que les 2 derniers à chaque fois. On récupère le nom sans l'extension .gz également.
   	do {
		cpt_token++;
		token_courant=token_suivant;
		token_suivant=token;
      		token=strtok(NULL, delim);
		strcat(filenamegz, token_courant);
		if (strcmp(token_courant,"")!=0 && strcmp(token_suivant,"gz")!=0) strcat(filenamegz, delim);
   	} while (token != NULL);

	//Détection de l'extension
	//Cas du .tar.gz ou .gz
	if (strcmp(token_suivant, "gz") == 0) {   //Voir strcmp(3)
		if ((strcmp(token_courant, "tar") == 0) && cpt_token > 2) {
			if (decomp==0) { //Vérification du flag de décompression
				printf("Séléctionnez l'option -z au minimum pour les fichiers au format .tar.gz\n");
				return false;
			}
			else {
				if (logflag==1) fprintf(logfile, "Nom du tube nommé utilisé pour la décompression : %s\n", pipenamed);
				return true;
			}
		}
		else if (extract==0 && listingd==0 && decomp==1) { //Cas spécial .gz pur.
			isonlygz=true;
			printf("Le nom du fichier %s ne semble pas être une archive .tar ou .tar.gz. Tentative de décompression...\n", file);
			//Appel au module optionnel de dézippage.
			decompress(file, logfile, isonlygz, filenamegz);
			return false;
		}
		else {
			printf("Le nom du fichier %s n'est pas au bon format. ([.tar].gz) ou n'utilisez que -z pour un fichier .gz pur.\n", file);
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
			printf(" Le nom du fichier %s n'est pas au bon format. (.tar[.gz])\n", file);
			return false;
		}
	}
	//Autres cas éventuels:
	else {
		printf(" Le nom du fichier %s n'est pas au bon format. (.tar[.gz])\n", file);
		return false; 
	}
}
