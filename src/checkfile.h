///////// PTAR 1.7 /////////
/*
Si l'archive passée en paramètre est compressée (.tar.gz) il faut au minimum mettre l'option -z pour la traiter.
Si l'archive passée en paramètre n'est pas compressée (.tar), l'option -z ne doit pas être spécifiée.

ptar n'est pas prévu pour le cas suivant mais un module est tout de même présent **[DESACTIVE DEPUIS 1.7]** :
Si le fichier passé en paramètre est simplement compressé (.gz), seule l'option -z doit être spécifiée. (et éventuellement -e)
	-> Dans ce cas, appelle directement la fonction decompress avec un flag spécial. Renvoie false pour terminer ptar.
     Décompresse uniquement les données brutes en attribuant tous les droits du fichiers à l'utilisateur courant, aucun autre attribut n'est mis à jour.

La validité du fichier (c'est-à-dire savoir si il s'agit rééllement d'une archive .tar ou .tar.gz) sera vérifiée
lors du premier open() dans la boucle principale de traitement(), et lors du check du champ ustar du header.
*/

#ifndef INCLUDE_CHECKFILE_H
#define INCLUDE_CHECKFILE_H

#include <stdbool.h>


/*
Permet de vérifier l'extension du fichier passé en paramètre et de la cohérence des options.
Sert à vérifier si l'argument passé lors de l'éxécution. (i.e. l'archive)
Si il existe, vérifie est bien nommé sous la forme : "*.tar" ou "*.tar.gz" (si l'extension existe et si elle est correcte)
ptar ne gère que des archives dont le NOM se termine seulement par ".tar" ou ".tar.gz".
En effet, tar lui peut gérer des archives 'mal nommées' (c'est-à-dire sans extension).
En vérifiant le champ magic du premier header, il est en réalité possible de gérer des archives mal nommées (ceci est tout
de même testé dans utils.h, pour les archives mal nommées ! cf testfalsearch.tar[.gz]).
Appelle directement decompress() lors d'un .gz pur et fait terminer le programme. (module désactivé depuis 1.7)
Retourne true si le nom est bien formé et si les options sont cohérentes avec ce dernier.
Retourne false sinon, et termine donc ptar.
*/

bool checkfile(char *file);


/*
Cette fonction vérifie l'existence du dossier passé en paramètre. Sert à l'extraction() et à checkpath().
Retourne true si il existe, false sinon.
*/

bool existeDir(char *folder);


/*
Cette fonction vérifie l'existence du fichier passé en paramètre. Sert à l'extraction() et à checkpath.
Retourne true si il existe, false sinon.
*/

bool existeFile(char *file);


/*
[Cette fonction n'est plus utilisée depuis 1.7.4 : ptar ne crée plus l'élément pointé si il n'existe pas]
Fonction pour les liens symboliques : reconstitue le chemin d'accès complet à partir du linkname du header
et du pathname du lien symobolique.
Retourne le chemin d'accès complet du fichier pointé par le lien symbolique. Il n'est pas nécessaire
de récupérer le retour puisque c'est *pathname qui est modifié.
*/

char *recoverpath(char *linkname, char *pathlink, char pathname[]);


/*
Fonction vérifiant l'existence de l'arborescence de l'élément passé en paramètre.
Vérifie chaque dossier parent en partant de la racine relative, et le crée si n'existe pas.
Retourne 0 si la création/exploration a fonctionné et que le path existe.
Retourne -1 sinon.
*/

int checkpath(char *path);


#endif
