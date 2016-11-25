///////// Pour ptar 1.3 minimum /////////
/*
Sert à vérifier si l'argument passé lors de l'éxécution. (i.e. l'archive)
Si il existe, vérifie est bien nommé sous la forme : "*.tar" ou "*.tar.gz" (si l'extension existe et si elle est correcte)
ptar ne gère que des archives dont le NOM se termine seulement par ".tar" ou ".tar.gz".
En effet, tar lui peut gérer des archives 'mal nommées' (c'est-à-dire sans extension).
En vérifiant le champ magic du premier header, il est en réalité possible de gérer des archives mal nommées.
(sauf pour la décompression).

Cette fonction récupère le flag de décompression (option -z).
Si l'archive passée en paramètre est compressée (.tar.gz) il faut au minimum mettre l'option -z pour la traiter.
Si l'archive passée en paramètre n'est pas compressée (.tar), l'option -z ne doit pas être spécifiée.

ptar n'est pas prévu pour le cas suivant mais un module est tout de même présent :
Si le fichier passé en paramètre est simplement compressé (.gz), seule l'option -z doit être spécifiée. (et éventuellement -e)
	-> Dans ce cas, appelle directement la fonction decompress avec un flag spécial. Renvoie false pour terminer ptar.
     Décompresse uniquement les données brutes en attribuant tous les droits du fichiers à l'utilisateur courant, aucun autre attribut n'est mis à jour.

Retourne true si le nom est bien formé et si les options sont cohérentes avec ce dernier.
Retourne false sinon, et termine donc ptar.

La validité du fichier (c'est-à-dire savoir si il s'agit rééllement d'une archive .tar ou .tar.gz) sera vérifiée
lors du premier open() dans la boucle principale de traitement().
*/

#ifndef INCLUDE_CHECKFILE_H
#define INCLUDE_CHECKFILE_H

#include <stdbool.h>


/*
Permet de vérifier l'extension du fichier passé en paramètre et de la cohérence des options.
Appelle directement decompress() lors d'un .gz pur et fait terminer le programme. (module instable)
*/

bool checkfile(char *file, FILE *logfile);


/*
Cette fonction vérifie l'existence du fichier ou dossier passé en paramètre. Sert à l'extraction().
Retourne true si il existe, false sinon.
*/

bool existe(char *folder);


#endif
