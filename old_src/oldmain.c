//////////////////////////////////////////////////////////   ptar - Extracteur d'archives durable et parallèle  ///////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////   v 1.7.0.0        09/12/2016   ///////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <pthread.h>

#include "utils.h"

int main(int argc, char *argv[]) {

	/*
	Déclarations et initialisation des variables
	*/

	int j;							//Indice de la boucle for pour la création des threads;
	int opt;						//Valeur de retour de getopt().
	int waitstatus;			//Status pour le waitpid.
	pthread_t *tabthrd;	//Tableau des idf des threads;
	void *ret;					//Structure pour le join final des threads.

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

	//Pour l'option -p, attente d'un argument pour p (et ce sera forcément un int à cause du test dans le case 'p')
	if (optind >= argc) {
		printf("Arguments attendus après options\n");
		return 1;
	}

	/*
	Génération du logfile si besoin -e. Option pour développeurs. (utile pour extraction et décompression)
	*/

	if (logflag==1) {
		logfile = genlogfile("logfile.txt", "a", argv[optind]);
	}

	/*
	Ouverture du fichier et mise en variable globale du descripteur de fichier.
	*/

	//On ouvre l'archive tar avec open() et le flag O_RDONLY (read-only). Si option -z, on décompresse, récupère les données dans un tube et on ouvre ce tube nommé.
	if (decomp==1) {
			
		//Décompression et ouverture dans un tube nommé tubedecompression.fifo
		decompress(argv[optind], logfile, false, NULL);

		//Ouverture de la sortie du tube nommé.
		file=open(pipenamed, O_RDONLY);

		//On attend le processus fils qui écrit dans le tube nommé.
		waitpid(-1, &waitstatus, 0);
	}
	//Cas où pas compressée.
	else {
		file=open(argv[optind], O_RDONLY);
	}

	/*
	Création des threads
	*/

	//Si on ne souhaite pas utiliser plusieurs threads, on n'en initialise qu'un (le thread principal).
	if (thrd==0) {
		nthreads=1;
	}
	//Sinon on initialise le thread principal + nthreads threads.
	else {
		nthreads++;
	}

	//Tableau des pthread_t
	tabthrd=malloc(nthreads*sizeof(pthread_t));

	//Lancement de chaque threads.
	for (j=0; j<nthreads; j++) {
		pthread_create(&tabthrd[j], NULL, traitement(argv[optind]), &j);
	}

	//Join des threads
	for (j=0; j<nthreads; j++) {
		(void)pthread_join(tabthrd[j], &ret);
	}

	//Libération du pointeur sur les threads.
	free(tabthrd);

	return 0;
}
