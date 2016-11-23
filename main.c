//////////////////////////////////////////////////////////   ptar - Extracteur d'archives durable et parallèle  ///////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////   v 1.5.1.0        23/11/2016   //////////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "utils.h"

int main(int argc, char *argv[]) {

	/*
	Déclarations et initialisation des variables
	*/

	int opt;
	int status;

	//Initialisation des flags des options de ligne de commande. Ce sont des variables globales dans utils.h
	thrd=0;
	extract=0;
	listingd=0;
	decomp=0;
	logflag=0;


	/*
	Traitement des options de ligne de commande. Set des flags (var globales) d'options.
	*/

	while ((opt=getopt(argc, argv, "xelzp:")) != -1) {
		switch (opt) {
			//Logfile (utile seulement pour l'extraction pour l'instant)
			case 'e' :
				logflag=1;
				break;
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
					return 1;
				}
				thrd=1;
				break;
			default:
				printf("Utilisation: %s [-x] [-l] [-z] [-p NBTHREADS] emplacement_archive\n", argv[0]);
				return 1;
		}
	}

	//printf("Falgs  :  extract=%d; listing=%d; decomp=%d; thrd=%d; NTHREADS=%d; optind=%d\n", extract, listingd, decomp ,thrd, nthreads, optind); - DEBUG
	
	//Pour l'option -p, attente d'un argument pour p (et ce sera forcément un int à cause du test dans le case 'p')
	if (optind >= argc) {
		printf("Arguments attendus après options\n"); 
		return 1;
	}

	/*
	Traitement effectif de l'archive
	*/
	
	status = traitement(argv[optind]);

	if (status==1) {
		printf("ptar a renvoyé 1. Il y a eu au moins 1 erreur (probablement lors de l'extraction). Le nom du tube nommé utilisé pour la décompression (si elle a été spécifiée) est %s\n", pipenamed);
	}

	return status;
}

