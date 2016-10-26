//////////////////////////////////////////////////////////   ptar - Extracteur d'archives durable et parallèle  ///////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////   v 1.0.0.0        25/10/2016   //////////////////////////////////////////////////////////////////////////////////////

#include<stdio.h>
#include<stdlib.h>
#include<fcntl.h>
#include<string.h>
#include<unistd.h>
#include"header.h"

int main() {
	//Déclarations des variables
	struct header_posix_ustar head;
	char *directory="./archives_test/test.tar"; //Emplacement de l'archive à traiter.
	int status;
	int file;
	int size;
	int cpt=0; //Compteur de fichier/dossier

	//On ouvre l'archive tar avec open() et le flag O_RDONLY (read-only).
	//Le code de retour de open() (ici l'entier file) appellé handle représente l'id unique du fichier ouvert. On l'utilise ensuite pour read() ou close() l'archive. 
	//Si cet entier est négatif (vaut -1), il s'agit d'une erreur dans l'open().
	file=open(directory, O_RDONLY);

	//Cas d'erreur -1 du open()
	if (file<0) {
		printf("Erreur d'ouverture du fichier, open() retourne : %d \n", file);
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
				printf("Fin de traitement de l'archive (End of File)\n");
				close(file);
				return 0; 
			}

			//On récupère la taille (head.size) des données suivant le header. La variable head.size est une string, contenant la taille donnée en octal, qu'on convertit en décimal.
			size=strtol(head.size, NULL, 8);

			//Si des données non vides suivent le header (en pratique : si il ne s'agit pas d'un header de répertoire ou de fichier vide), on passe ces données sans les afficher:
			if (size > 0) {
				//On crée un buffer pour le read() des données inutiles (inutiles pour le listing !)
				int *ptvoid;
				ptvoid = malloc(size);
				
				//On "lit" avec read les données inutiles qu'on met dans le buffer. On récupère quand même le code de retour de ce read, si jamais il renvoie -1 ou 0 (End Of File).
				status=read(file, ptvoid, size);

				//On libère ensuite la mémoire allouée pour ce buffer
				free((void *)ptvoid);

				//Cas d'erreur -1 du read
				if (status<0) {
					printf("Erreur dans la lecture du header, read() retourne : %d \n", status);
					close(file);
					return -1;
				}
			}
			// On incrémente le compteur de fichier/dossier
			cpt++;
			// On affiche le numéro du dossier/fichier (cpt), son nom (head.name) et la taille des données suivant ce header en octets (size)
			printf("Nom du répertoire/fichier %d : %s et taille (en octets) : %d\n", cpt, head.name, size);
		
		//Puis, tant que le status ne vaut pas 0 (End Of File) ou -1 (erreur), ou que le head.name n'est pas vide (voir (1)), on réitère l'opération (à l'aide d'une boucle do {...} while();).
		} while (status != 0);

		//Cette partie du code n'est en principe jamais atteinte grâce à (1), puisque l'End Of File ne sera jamais atteint <=> status = 0 jamais atteint (read renvoie 0 en fin de fichier).
		printf("ptar a terminé correctement\n");
		close(file);
		return 0;
	}
}

