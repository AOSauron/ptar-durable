//////////////////////////////////////////////////////////   ptar - Extracteur d'archives durable et parallèle  ///////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////   v 1.4.0.0        05/11/2016   //////////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <math.h>
#include <time.h>

#include "header.h"
#include "checkfile.h"
#include "utils.h"

int main(int argc, char *argv[]) {
	//Déclarations des variables

	struct header_posix_ustar head;

	time_t secondes;
	struct tm instant;

	FILE *logfile; //logfile de l'extraction (codes de retour des open, symlink, mkdir, etc)
	
	char *directory; //Emplacement de l'archive à traiter.
	char *data; //Buffer pour les données suivant le header.

	int extract;
	int extreturn; //Valeur de retour de extraction()
	int listingd;
	int decomp;
	int opt;
	int nthreads;
	int thrd;
	int status;
	int file;
	int size;
	int size_reelle;
	int cpt; //Compteur de fichier/dossier **inutilisé depuis 1.3**

	cpt=0;
	size_reelle=0;
	nthreads=0;   //Nombre des threads à utiliser (seulement si l'option -p est utilisée).
	//Initialisation des flags des options de ligne de commande.
	thrd=0;
	extract=0;
	listingd=0;
	decomp=0;

	/*
	Traitement des options de ligne de commande
	*/

	while ((opt=getopt(argc, argv, "xlzp:")) != -1) {
		switch (opt) {
			//Extraction : -x
			case 'x' :
				extract=1;
				break;
			//Listing détaillé : -l
			case 'l' :
				listingd=1;
				break;
			//Décompression : -z
			case 'z' :
				decomp=1;
				break;
			//Durabilité et parallélisation : -p NBTHREADS
			case 'p' :
				nthreads=atoi(optarg); // nthreads <-- NBTHREADS
				//Si le p est mal placé (il optarg sera la lettre suivante), atoi renverra 0, c'est-à-dire que optarg n'est pas un nombre.
				//(Et on considère qu'on ne tolère pas 0 threads ...)
				if (nthreads==0) {
					printf("Utilisation: %s [-x] [-l] [-z] [-p NBTHREADS] emplacement_archive\n", argv[0]);
					return -1;
				}
				thrd=1;
				break;
			default:
				printf("Utilisation: %s [-x] [-l] [-z] [-p NBTHREADS] emplacement_archive\n", argv[0]);
				return -1;
		}
	}

	//printf("Falgs  :  extract=%d; listing=%d; decomp=%d; thrd=%d; NTHREADS=%d; optind=%d\n", extract, listingd,decomp ,thrd, nthreads, optind); - DEBUG
	
	//Pour l'option -p, attente d'un argument pour p (et ce sera forcément un int à cause du test dans le case 'p')
	if (optind >= argc) {
		printf("Arguments attendus après options\n");
		return -1;
	}

	/*
	Tester l'argument (existence et nom bien formé).
	*/

	if (checkfile(argv[optind])==false) {
		//Les messages d'erreurs sont gérés dans checkfile.c	
		return -1;
	}

	//Si tout va bien alors on récupère l'argument dans une variable pour le traiter plus loin.
	directory=argv[optind];
	
	//Ouverture du logfile si il y a extraction
	if (extract==1) {
		time(&secondes);
		instant=*localtime(&secondes);
		logfile=fopen("logfile.txt", "a"); //L'option "a" pour faire un ajout en fin de fichier (et pas l'écraser à chaque fois)
		fprintf(logfile, "Logfile de l'extraction de l'archive %s le %d/%d à %d:%d:%d\n", directory, instant.tm_mday, instant.tm_mon+1, instant.tm_hour, instant.tm_min, instant.tm_sec);
	}
	
	/*
	Listing basique (étape 1) et vérification de la validité/existence du fichier.
	*/

	//On ouvre l'archive tar avec open() et le flag O_RDONLY (read-only).
	//Le code de retour de open() (ici l'entier file) appellé handle représente l'id unique du fichier ouvert. On l'utilise ensuite pour read() ou close() l'archive. 
	//Si cet entier est négatif (vaut -1), il s'agit d'une erreur dans l'open().
	file=open(directory, O_RDONLY);

	//Cas d'erreur -1 du open()
	if (file<0) {
		printf("Erreur d'ouverture du fichier, open() retourne : %d. Le fichier n'existe pas ou alors est corrompu.\n", file);
		//On ferme le fichier avec close() avant chaque return pour une libération propre de la mémoire.
		close(file);  //On ne gère pas pour l'instant le cas d'erreur -1 du close()
		if (extract==1) {
			fclose(logfile); //Aussi le logfile.
		}
		return -1;
	}

	

	//Si l'open() s'est passé correctement on passe à la suite :
	else {
		do {

			/*
			Traitement du listing basique (activé quelque soit l'action)
			*/
			
			//On lit (read) un premier bloc (header) de 512 octets qu'on met dans une variable du type de la structure header_posix_ustar définie dans header.h (norme POSIX.1)
			//On récupère le code de retour de read() dans la variable status pour le cas d'erreur -1.
			status=read(file, &head, sizeof(head));  // Utiliser sizeof(head) est plus évolutif qu'une constante égale à 512.

			//Cas d'erreur -1 du read
			if (status<0) {
				printf("Erreur dans la lecture du header, read() retourne : %d \n", status);
				if (extract==1) {
					fclose(logfile);
				}
				close(file);
				return -1;
			}

			// (1) La fin d'une archive tar se termine par 2 enregistrements d'octets valant 0x0. (voir tar(5))
			// Donc la string head.name du premier des 2 enregistrement est forcément vide. 
			// On s'en sert donc pour détecter la fin du fichier (End Of File) et ne pas afficher les 2 enregistrements de 0x0.
			if (strlen(head.name)==0) {	
				close(file);
				//Fermeture du logfile
				if (extract==1) {
					fputs("Fin d'extraction\n \n", logfile);
					fclose(logfile);
				}
				return 0; 
			}

			//On récupère la taille (head.size) des données suivant le header. La variable head.size est une string, contenant la taille donnée en octal, convertit en décimal.
			size=strtol(head.size, NULL, 8);

			// (2) Si des données non vides suivent le header (en pratique : si il s'agit d'un header fichier non vide), on passe ces données sans les afficher:
			// On récupère les données dans un buffer pour l'extraction.             Vérifier le type "0" :  strcmp(head.typeflag,"0")==0
			if (size > 0) {
				//printf("size modulo 512 :%d\n", size%512);  //DEBUGGING
				//Les données sont stockées dans des blocs de 512 octets (on a trouvé ça en utilisant hexdump -C testall.tar ...) après le header.
				//On utilise donc une variable temporaire pour stocker la taille totale allouée pour les données dans l'archive tar.
				if (size%512==0) {
					size_reelle=size; // Il faut sortir les cas particuliers multiples de 512. (Sinon il y aura 1 bloc de 512 octets en trop)
				}
				else {
					size_reelle=512*((int)(size/512)+1);  //Utiliser (int)variable car c'est des int (floor est utilisé pour des double renvoyant un double)
				}
				
				//On utilise le buffer data pour le read() des données inutiles pour le listing (mais pas pour l'extraction)
				data=malloc(size_reelle);
				
				//On "lit" avec read les données inutiles qu'on met dans le buffer. On récupère le code de retour de ce read, si jamais il renvoie -1 ou 0 (End Of File).
				//Ces datas vont servir pour l'extraction des fichiers !!!!
				status=read(file, data, size_reelle);

				//Cas d'erreur -1 du read
				if (status<0) {
					printf("Erreur dans la lecture du header, read() retourne : %d\n", status);
					if (extract==1) {
						fclose(logfile);
					}
					close(file);
					return -1;
				}
			}
			// On incrémente le compteur de fichier/dossier
			cpt++; // ---- devenu inutile en vu du format des tests

			// On affiche le numéro du dossier/fichier (cpt), son nom (head.name) et la taille des données suivant ce header en octets (size).
			//printf("Elément %d : %s   taille en octet : %d\n", cpt, head.name, size);
			// Pour les tests blancs on affiche simplement les head.name, seulement lors du listing basique.
			if (listingd==0) {
				printf("%s\n", head.name);
			} 

			/* 
			Traitement de l'extraction -x
			*/
			
			if (extract==1) {
				extreturn=extraction(head, data, logfile);
				fprintf(logfile, "Retour d'extraction de l'élément %s : %d\n", head.name, extreturn);
			}

			/*
			Traitement du listing détaillé -l
			*/

			if (listingd==1) {
				listing(head);
			}

			/*
			Traitement de la durabilité et de la parallélisation sur nthreads -p
			*/

			if (thrd==1) {
				printf("Parallélisation à faire !\n");
			}

			/*
			Traitement de la décompression -z
			*/

			if (decomp==1) {
				printf("Décompression à faire !\n");
			}

		//Puis, tant que le status ne vaut pas 0 (End Of File) ou -1 (erreur), ou que le head.name n'est pas vide (voir (1)), on réitère l'opération (à l'aide d'une boucle do {...} while();).
		} while (status != 0);

		//Cette partie du code n'est en principe jamais atteinte, puisque l'End Of File ne sera jamais atteint (grâce à (1))<=> status = 0 jamais atteint (read renvoie 0 en fin de fichier).
		printf("ptar n'aurait pas dû terminer de cette façon. (%s n'est probablement pas une véritable archive tar)\n", directory);
		close(file);
		if (extract==1) {
			fclose(logfile);
		}
		return 0;
	}
}
