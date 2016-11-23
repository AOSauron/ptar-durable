///////// Pour ptar 1.3 minimum /////////
/*

Fonctions utils

Fonctions principales du programme ptar :
Correspondantes aux options suivantes :
-x : Extraction.
-l : Listing détaillé.
-z : Décompression gzip avec la bibliothèque zlib.
-p NBTHREADS : (forcément couplée avec -x au moins) Durabilité et parallélisation de l'opération avec un nombre de threads choisi.
-e : Ecriture dans un logfile.txt. Utile pour -z et -x.

*/

#ifndef INCLUDE_UTILS_H
#define INCLUDE_UTILS_H

#define TAILLELISTE 1024

#include <stdbool.h>


/* Déclaration des variables globales : flags de ligne de commandes initialisés dans main et nom du tube nommé */

static const char *pipenamed = "tubedecompression.fifo";		//Nom du tube nommé contenant les données décompressées.

int extract;	//Flags pour extraction (option -x)
int listingd;	//Flags pour listing détaillé (option -l)
int decomp;	//Flags pour décompression (option -z)
int logflag; 	//Flags pour logfile (option -e)
int thrd;		//Flags pour parallélisation (option -p)
int nthreads;	//Nombre de threads (option -p)


/*
Structure des header des archives (.tar ou .tar.gz).
Format standard pour les archives tar selon la norme ustar POSIX.1
*/

struct header_posix_ustar {
                   char name[100];
                   char mode[8];
                   char uid[8];
                   char gid[8];
                   char size[12];
                   char mtime[12];
                   char checksum[8];
                   char typeflag[1];
                   char linkname[100];
                   char magic[6];
                   char version[2];
                   char uname[32];
                   char gname[32];
                   char devmajor[8];
                   char devminor[8];
                   char prefix[155];
                   char pad[12];
};


/*
Fonction principale : recueille les header de chaque fichier dans l'archive (compressée ou non) ainsi que les données suivantes chaque header si il y en a.
Appelle ensuite les diverses fonctions utiles au traitement souhaité.
Prend en argument l'emplacement de l'archive, et les 5 flags d'options (ainsi que le nombre de threads)
Retourne 0 si tout s'est bien passé, 1 sinon.
*/

int traitement(char *folder);



/*
Fonction génératrice de logfile. Est appelée si et seulement si l'option -e est spécifiée.
logname : Nom du logfile (normalement : logfile.txt)
option : Options du fopen (normalement "a" pour faire un ajout en fin de fichier et pas l'écraser à chaque fois)
filename : Nom de l'archive passée en paramètre de ptar.
Génère un logfile formaté pour ptar.
*/

FILE *genlogfile(const char *logname, const char *option, char *filename);



/*
Effectue le listing détaillé des informations contenues dans le header d'élément passé en paramètre.
Print : permissions, uid/gid (= propriétaire/groupe), taille, mtime, liens symboliques (fichier linké).
Retourne 0.
*/

int listing(struct header_posix_ustar head);



/*
Pratique l'extraction de l'élément correspondant au header passé en paramètre, en utilisant les données (data) si c'est un fichier.
Ecrit les codes de retour des open/close/write/symlink/mkdir/setuid/setgid/utime/fsync dans un logfile si l'option -e est spécifiée.
Affecte correctement tous les attributs des éléments, sauf le mtime des dossiers qui sont affectés en fin de traitement().
Si on veut forcer un nom de fichier, le spécifier dans le champ name, sinon mettre NULL.
Retourne 0 si tout s'est bien passé, -1 sinon.
*/

int extraction(struct header_posix_ustar head, const char *name, char *data, FILE *logfile);



/*
Décompresse l'archive [.tar].gz (il faut que l'extension .gz soit présente) passée en paramètre avant tout traitement ultérieur.
Ecrit les données décompressées dans un tube nommé créé dans la fonction. Se fork une fois :
Le processus père retourne le nom du tube nommé juste après le fork().
Le processus fils effectue l'écriture des données dans l'entrée du tube nommé, puis est kill (et ne retourne donc rien).
Retourne EXIT_FAILURE si il y eu au moins 1 erreur dans les opérations.
Module optionnel : Gère le cas d'une archive .gz seule (et non .tar.gz) avec le flag isonlygz et le nom du fichier récupéré par checkfile.
Excepté pour le cas d'un fichier compressé au format .gz sans archivage, filenamegz devrait être mis à NULL et isonlygz à false.
*/

const char *decompress(char *folder, FILE *logfile, bool isonlygz, const char *filenamegz);


#endif
