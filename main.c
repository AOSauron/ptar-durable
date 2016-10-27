//////////////////////////////////////////////////////////   ptar - Extracteur d'archives durable et parallèle  ///////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////   v 1.3.0.0        27/10/2016   //////////////////////////////////////////////////////////////////////////////////////

#include<stdio.h>
#include<stdlib.h>
#include<fcntl.h>
#include<string.h>
#include<unistd.h>
#include<sys/stat.h>
#include<math.h>

#include"header.h"

int main(int argc, char *argv[]) {
	//Déclarations des variables

	struct header_posix_ustar head;
	
	char *directory; //Emplacement de l'archive à traiter.
	char *data; //Buffer pour les données suivant le header.

	int extract;
	int listingd;
	int decomp;
	int opt;
	int nthreads;
	int thrd;
	int status;
	int file;
	int size;
	int size_reelle;
	int cpt; //Compteur de fichier/dossier

	cpt=0;
	size_reelle=0;
	//Flags des options de ligne de commande.
	nthreads=0;
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
				nthreads=atoi(optarg);
				//Si le p est mal placé (il optarg sera la lettre suivante), atoi renverra 0, c'était à dire que optarg n'est pas un nombre.
				//(On considère qu'on ne tolère pas 0 threads ...)
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

	//Affichage des flags (debugging)
	printf("**************************************************************************************\n");
	printf("Falgs  :  extract=%d; listing=%d; decomp=%d; thrd=%d; NTHREADS=%d; optind=%d\n", extract, listingd,decomp ,thrd, nthreads, optind);
	
	//Pour l'option -p, attente d'un argument pour p (et ce sera forcément un int à cause du test dans le case 'p')
	if (optind >= argc) {
		printf("Arguments attendus après options\n");
		return -1;
	}
	
	/*
	Tester si il y a un argument
	*/
	//Test de l'existence d'un argument.
	if (argv[optind]==NULL) {	
		printf("ptar : erreur pas d'archive tar en argument. Utilisation: %s [-x] emplacement_archive.tar[.gz]\n", argv[0]);
		return -1;
	}
	
	//Si il y a argument, on le récupère dans une variable pour plus tard.
	directory=argv[optind];	

	/*
	Tester si le nom du fichier se termine par .tar (ou .tar.gz). On verifiera l'existence/validité du fichier lors de l'open.
	*/
	
   	const char *delim = ".";
   	char *token;
	int cpt_token=0;
	char *token_courant="";
	char *token_suivant="";
	char directory_test[500];

	//On récupère l'argument (dans une variable temporaire car strtok agit dessus) et on test si c'est bien une archive .tar ou .tar.gz	
	strcpy(directory_test, argv[optind]);// strcpy(char dest, char src);
   
  	//Récupère le premier token (voir strtok(3))
  	token=strtok(directory_test, delim);
   
   	//Récupère les autres token et ne conserve que les 2 derniers à chaque fois.
   	while( token != NULL ) {
		cpt_token++;
		token_courant=token_suivant;
		token_suivant=token;
      		token=strtok(NULL, delim);
   	}
	
	//Détection de l'extension
	//Cas du .tar.gz
	if (strcmp(token_suivant, "gz") == 0) {
		if ((strcmp(token_courant, "tar") != 0) || cpt_token < 3) {
			printf(" Le nom du fichier %s n'est pas au bon format. (.tar[.gz])\n", directory);
			return -1; 
		}	
	}
	//Cas du .tar
	else if (strcmp(token_suivant, "tar") == 0) {
		if (cpt_token < 2) {
			printf(" Le nom du fichier %s n'est pas au bon format. (.tar[.gz])\n", directory);
			return -1;
		} 
	}
	//Autres cas éventuels:
	else {
		printf(" Le nom du fichier %s n'est pas au bon format. (.tar[.gz])\n", directory);
		return -1; 
	}

	//A ce niveau là le nom du fichier devrait être bien formé.
	printf("**************************************************************************************\n");
	printf("Début de traitement de l'archive %s\n", directory);
	printf("**************************************************************************************\n");
	printf("Les 'Codes retour' s'ils valent -1 montrent une erreur lors de l'opération concernée \n");//DEBUGGING
	printf("**************************************************************************************\n");
	printf("Listing basique ...\n");

	/*
	Listing basique (étape 1) et vérification de la validité/existence du fichier
	*/

	//On ouvre l'archive tar avec open() et le flag O_RDONLY (read-only).
	//Le code de retour de open() (ici l'entier file) appellé handle représente l'id unique du fichier ouvert. On l'utilise ensuite pour read() ou close() l'archive. 
	//Si cet entier est négatif (vaut -1), il s'agit d'une erreur dans l'open().
	file=open(directory, O_RDONLY);

	//Cas d'erreur -1 du open()
	if (file<0) {
		printf("Erreur d'ouverture du fichier, open() retourne : %d. Le fichier n'existe pas ou alors est corrompu.\n", file);
		//On ferme le fichier avec close() avant chaque return pour une libération propre de la mémoire.
		close(file);  //On ne gère pas pour l'instant le cas d'erreur -1 du close().
		return -1;
	}

	//Si l'open() s'est passé correctement on passe à la suite :
	else {
		do {
			//On lit (read) un premier bloc (header) de 512 octets qu'on met dans une variable du type de la structure header_posix_ustar définie dans header.h (norme POSIX.1)
			//On récupère le code de retour de read() dans la variable status pour le cas d'erreur -1.
			status=read(file, &head, sizeof(head));  // Utiliser sizeof(head) est plus évolutif qu'une constante égale à 512.

			//Cas d'erreur -1 du read
			if (status<0) {
				printf("Erreur dans la lecture du header, read() retourne : %d \n", status);
				close(file);
				return -1;
			}

			// (1) La fin d'une archive tar se termine par 2 enregistrements d'octets valant 0x0. (voir tar(5))
			// Donc la string head.name du premier des 2 enregistrement est forcément vide. 
			// On s'en sert donc pour détecter la fin du fichier (End Of File) et ne pas afficher les 2 enregistrements de 0x0.
			if (strlen(head.name)==0) {
				printf("**********************************************************************\n");
				printf("Fin de traitement de l'archive %s (End Of File)\n", argv[optind]); //directory produit un Seg Fault
				printf("**********************************************************************\n");
				close(file);
				return 0; 
			}

			//On récupère la taille (head.size) des données suivant le header. La variable head.size est une string, contenant la taille donnée en octal, convertit en décimal.
			size=strtol(head.size, NULL, 8);

			// (2) Si des données non vides suivent le header (en pratique : si il s'agit d'un header fichier non vide), on passe ces données sans les afficher:
			// On récupère les données dans un buffer pour l'extraction.                Vérifier le type "0" :  strcmp(head.typeflag,"0")==0
			if (size > 0) {
				//Les données sont rangées dans des blocs de 512 octets (on a trouvé ça en utilisant hexdump -C test.tar ...).
				//On utilise donc une variable temporaire pour stocker la taille totale allouée pour les données.
				size_reelle=512*((int)(size/512)+1);  //Utiliser (int)variable car c'est des int (et floor est utilisé pour des double)
				
				//On utilise le buffer data pour le read() des données inutiles pour le listing (mais pas pour l'extraction)
				data=malloc(size_reelle);
				
				//On "lit" avec read les données inutiles qu'on met dans le buffer. On récupère le code de retour de ce read, si jamais il renvoie -1 ou 0 (End Of File).
				//Ces datas vont servir pour l'extraction des fichiers !!!!!!!!!
				status=read(file, &data, size_reelle);

				//Cas d'erreur -1 du read
				if (status<0) {
					printf("Erreur dans la lecture du header, read() retourne : %d\n", status);
					close(file);
					return -1;
				}
			}
			// On incrémente le compteur de fichier/dossier
			cpt++;

			// On affiche le numéro du dossier/fichier (cpt), son nom (head.name) et la taille des données suivant ce header en octets (size)
			if (strcmp(head.typeflag,"0")==0) { // Si c'est un fichier (vide ou non) 
				printf("Nom de l'élément %d : %s  , taille (octets) : %d, et taille réelle (octets) : %d \n", cpt, head.name, size, size_reelle); 
			}
			else { //Tout autre élément (dossier, lien symbolique ...)
				printf("Nom de l'élément %d : %s\n", cpt, head.name); 
			}

			/* 
			Traitement de l'extraction -x
			*/
			
			if (extract==1) {
				
				int etat0;
				int file0;
				int write0;
				int etat2;
				int etat5;
				int mode;

				//head.mode = les permissions du fichier en octal, on les convertit en décimal.
				mode= strtol(head.mode, NULL, 8);

				//Séléction du type d'élément et actions. Les printf aident au débugging. 
				switch (head.typeflag[0]) {
					//Fichiers réguliers
					case '0' :
						//printf("Permissions : %d\n", mode); // DEBUGGING
						file0=open(head.name, O_CREAT | O_WRONLY, mode); //O_CREAT pour créer le fichier et O_WRONLY pour pouvoir écrire dedans.
						printf("Code retour du open : %d\n", file0);
						//Voir la partie (2) récupération de données. Utiliser sizeof(data) est plus économe en mémoire que size_reelle (qui est un multiple de 512 octets)
						if (size > 0) { 
        						write0=write(file0, &data, size); //Ecriture si seulement le fichier n'est pas vide !
							printf("Code retour du write (octets écrits) : %d\n", write0);
						}
						etat0=close(file0);
						printf("Code retour du close : %d\n", etat0);
						break;
					//Liens symboliques
					case '2' :
						etat2=symlink(head.linkname, head.name);
						printf("Code retour du symlink : %d\n", etat2); 
						break;
					//Répertoires
					case '5' :
						//printf("Permissions : %d\n", mode); // DEBUGGING
						etat5=mkdir(head.name, mode);
						printf("Code retour du mkdir : %d\n", etat5);
						break;
					default:
						printf("Typeflag = [ %s ]\n", head.typeflag);
				}
			}



		//Puis, tant que le status ne vaut pas 0 (End Of File) ou -1 (erreur), ou que le head.name n'est pas vide (voir (1)), on réitère l'opération (à l'aide d'une boucle do {...} while();).
		} while (status != 0);

		//Cette partie du code n'est en principe jamais atteinte, puisque l'End Of File ne sera jamais atteint (grâce à (1))<=> status = 0 jamais atteint (read renvoie 0 en fin de fichier).
		printf("ptar a terminé correctement\n");
		close(file);
		return 0;
	}
}
