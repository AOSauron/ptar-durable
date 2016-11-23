///////// Pour ptar 1.5 minimum /////////
/*

Fonctions de tris

Utile seulement après la décompression.
Liste et comptent chaque élément de chaque types.
Sert à trier les éléments récupérés après la décompression, en effet ils sont désordonnés post-décompression.
Il arrive alors que certains dossiers arrivent avant leur dossiers parents (qui ne sont pas encore créés) et donc il y a erreur.

*/

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/wait.h>

#include "utils.h"
#include "sorting.h"


/*
Analyseur d'archive : compte le nombre d'élément de chaque type contenu dans l'archive.
Retourne une structure adaptée.
Le paramètre doit être le tube nommé créé après la décompression.
*/

gzHeadertype analyse(const char *folder, FILE *logfile) {

	gzHeadertype composition;
	struct header_posix_ustar head;
	struct header_posix_ustar *headersdir=NULL;
	struct header_posix_ustar *headersfile=NULL;
	struct header_posix_ustar *headerssymlink=NULL;
	struct header_posix_ustar *ptrintermediaire=NULL;
	char *datas=NULL;
	char *buffintermediaire=NULL;

	int nbdir=0;
	int nbfile=0;
	int nbsymlink=0;
	int nbblocdata=0;
	int fd;
	int status;
	int waitstatus;
	int etat;
	long size;

	//Ouverture de la sortie du tube nommé.
	fd=open(folder, O_RDONLY);
	
	if (fd<0) {
		printf("Problème d'ouverture de la sortie du tube nommé pour l'analyse.\n");
		if (logflag==1) fclose(logfile);
		close(fd);
		exit(EXIT_FAILURE);
	}

	//On attend le processus fils qui écrit dans le tube nommé.
	waitpid(-1, &waitstatus, 0);

	//Construction au fur et à mesure des tableaux avec des realloc
	do {
		status=read(fd, &head, sizeof(head));

		if (status<0) {
			printf("Erreur dans la lecture du header lors de l'analyse, read() retourne : %d \n", status);
			if (logflag==1) fclose(logfile);
			close(fd);
			exit(EXIT_FAILURE);
		}

		//Sortie effective de la boucle.
		if (strlen(head.name)==0 || status==0) {
			break;
		}

		switch (head.typeflag[0]) {
			case '0' :
				nbfile++;
				ptrintermediaire=realloc(headersfile, 512*nbfile);
				if (ptrintermediaire==NULL){
					free(headersfile);    //Désallocation
					printf("Problème de réallocation lors de l'analyse.\n");
					if (logflag==1) fclose(logfile);
					close(fd);
					exit(EXIT_FAILURE);
				}
				else headersfile=ptrintermediaire;

				headersfile[nbfile-1]=head;

				size=strtol(head.size, NULL, 8);
				if (size>0) {
					if (size%512==0) {
						nbblocdata=nbblocdata+size/512;
					}
					else {
						size=512*((int)(size/512)+1);
					}
					nbblocdata=nbblocdata+size/512;
					buffintermediaire=realloc(datas, 512*nbblocdata);
					if (buffintermediaire==NULL){
					    	free(datas);    //Désallocation
					    	printf("Problème de réallocation lors de l'analyse.\n");
						if (logflag==1) fclose(logfile);
						close(fd);
						exit(EXIT_FAILURE);
					}
					else datas=buffintermediaire;

					status=read(fd, &datas[nbfile-1], size);
				}
				break;
			case '2' :
				nbsymlink++;
				ptrintermediaire=realloc(headerssymlink, 512*nbsymlink);
				if (ptrintermediaire==NULL){
				    	free(headerssymlink);    //Désallocation
				    	printf("Problème de réallocation lors de l'analyse.\n");
					if (logflag==1) fclose(logfile);
					exit(EXIT_FAILURE);
					close(fd);
				}
				else headerssymlink=ptrintermediaire;

				headerssymlink[nbsymlink-1]=head;
				break;
			case '5' :
				nbdir++;
				ptrintermediaire=realloc(headersdir, 512*nbdir);
				if (ptrintermediaire==NULL){
				    	free(headersdir);    //Désallocation
				    	printf("Problème de réallocation lors de l'analyse.\n");
					if (logflag==1) fclose(logfile);
					close(fd);
					exit(EXIT_FAILURE);
				}
				else headersdir=ptrintermediaire;

				headersdir[nbdir-1]=head;
				break;
			default :
				printf("Typeflag inconnu lors de l'analyse.\n");
				if (logflag==1) fclose(logfile);
				remove(pipenamed);
				exit(EXIT_FAILURE);
				close(fd);
		}
		
	} while (status>0);

	//On met à jour la structure gzHeadertype
	composition.headDir=headersdir;
	composition.headFile=headersfile;
	composition.headSymlink=headerssymlink;
	composition.datas=datas;
	composition.nbDir=nbdir;
	composition.nbFile=nbfile;
	composition.nbSymlink=nbsymlink;
	
	etat=close(fd);
	if (logflag==1) {
		fprintf(logfile, "Code de retour du close de la sortie du tube %s : %d\n", pipenamed, etat);
		fclose(logfile);
	}

	return composition;
}


/*
Trieur : trie les dossiers importent avec un tri à bulle classique.
*/

void tribulle(gzHeadertype composition) {

	const char *delim;
	
	struct header_posix_ustar headtemp;

	int i;
	int j;

	delim="/";

	//On boucle sur les noms de dossier, en comptant leur niveau de profondeur en effectuant un tri à bulle.
	for (i=composition.nbDir-1; i>0; i--) {
		for (j=0; j<i; j++) {
			//Comparaison
			if (getnbtoken(composition.headDir[j+1].name, delim)<getnbtoken(composition.headDir[j].name, delim)) {
				headtemp=composition.headDir[j+1];
				composition.headDir[j+1]=composition.headDir[j];
				composition.headDir[j]=headtemp;
			}
		}
	}
}


/*
Compte le nombre de token séparés par le délimiteur passé en paramètre.
*/

int getnbtoken(char *name, const char *delim) {

	char *token;
	char bufname[100];

	int cpt_token;

	token="";
	cpt_token=0;

	strcpy(bufname, name);
	token=strtok(bufname, delim);

	do {
		cpt_token++;
      		token=strtok(NULL, delim);
   	} while (token != NULL);

	return cpt_token;
}


/*
Boucle sur les headers, dans le bon ordre, afin d'extraire/lister tous les éléments de façon satisfaisante.
*/

int traitepostdecomp(gzHeadertype composition, FILE *logfile) {

	struct timeval *tv;
	
	int i;
	int mtime;
	int utim;
	int extreturn;
	int returnvalue;
	
	returnvalue=0;

	//On commence par les dossiers
	for (i=0; i<composition.nbDir; i++) {
		if (extract==1) extreturn=extraction(composition.headDir[i], NULL, NULL, logfile);
		if (listingd==1) listing(composition.headDir[i]);
		else printf("%s\n", composition.headDir[i].name);
		if (logflag==1) fprintf(logfile, "[Dossier %s] Code retour d' extraction : %d\n", composition.headDir[i].name, extreturn);
		if (extreturn==-1) returnvalue=1;
	}
	
	//Puis les fichiers
	for (i=0; i<composition.nbFile; i++) {
		if (extract==1) extreturn=extraction(composition.headFile[i], NULL, &composition.datas[i], logfile);
		if (listingd==1) listing(composition.headFile[i]);
		else printf("%s\n", composition.headFile[i].name);
		if (logflag==1) fprintf(logfile, "[Fichier %s] Code retour d' extraction : %d\n", composition.headFile[i].name, extreturn);
		if (extreturn==-1) returnvalue=1;
	}

	//On finit par les liens symboliques
	for (i=0; i<composition.nbSymlink; i++) {
		if (extract==1) extreturn=extraction(composition.headSymlink[i], NULL, NULL, logfile);
		if (listingd==1) listing(composition.headSymlink[i]);
		else printf("%s\n", composition.headSymlink[i].name);
		if (logflag==1) fprintf(logfile, "[Lien symbolique %s] Code retour d' extraction : %d\n", composition.headSymlink[i].name, extreturn);
		if (extreturn==-1) returnvalue=1;
	}

	//En toute fin on fait le utime des dossiers.
	if (extract==1) {
		for (i=0; i<composition.nbDir; i++) {
			mtime=strtol(composition.headDir[i].mtime, NULL, 8);
			tv=malloc(2*sizeof(struct timeval));	
			tv[0].tv_sec=mtime;
			tv[0].tv_usec=0;
			tv[1].tv_sec=mtime;
			tv[1].tv_usec=0;
			utim=utimes(composition.headDir[i].name, tv);
			free(tv);
			if (logflag==1) fprintf(logfile, "[Fichier %s] Code retour du utimes : %d\n", composition.headDir[i].name, utim);
			if (utim==-1) returnvalue=1;
		}
	}

	return returnvalue;
}

