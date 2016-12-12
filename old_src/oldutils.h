///////// Pour ptar 1.3 minimum /////////
/*

Fonctions utils

Fonctions principales du programme ptar :
Correspondantes aux options suivantes :
-x : Extraction.
-l : Listing détaillé.
-z : Décompression gzip avec la bibliothèque zlib.
-p NBTHREADS : Durabilité et parallélisation de l'opération avec un nombre de threads choisi.
-e : Ecriture dans un logfile.txt. Utile pour -z et -x.

*/

#ifndef INCLUDE_UTILS_H
#define INCLUDE_UTILS_H

#define MAXDIR 2048        //Nombre maximum de dossiers contenus dans une archive.
#define PATHLENGTH 255     //Taille maximale pour un pathname fixée à 255 caractère (voir struct header_posix_ustar)

#include <stdbool.h>
#include <zlib.h>


/* Déclaration des variables globales : flags de ligne de commandes initialisés dans main et nom du tube nommé */

static const char *pipenamed = "tubedecompression.fifo";    //Nom du tube nommé contenant les données décompressées.

int extract;	                                              //Flags pour extraction (option -x)
int listingd;	                                              //Flags pour listing détaillé (option -l)
int decomp;	                                                //Flags pour décompression (option -z)
int logflag; 	                                              //Flags pour logfile (option -e)
int thrd;		                                                //Flags pour parallélisation (option -p)
int nthreads;	                                              //Nombre de threads (option -p)

int file;                                                   //Descripteur de fichier de l'open primordial.
gzFile filez;                                               //

FILE *logfile; 			                                        //Logfile pour l'option -e.

static pthread_mutex_t MutexRead;                           //Mutex pour le read dans traitement().
static pthread_mutex_t MutexWrite;                          //Mutex pour les actions sur disque (write, mkdir..) dans extraction().


/*
Structure des header des archives (.tar ou .tar.gz).
Format standard pour les archives tar selon la norme ustar POSIX.1
*/

struct header_posix_ustar {
          char name[100];        //Nom de l'élément.
          char mode[8];          //Permissions de l'élément.
          char uid[8];           //User ID de l'élément.
          char gid[8];           //Group ID de l'élément.
          char size[12];         //Taille des données suivant le header. Est différent de 0 seulement si il s'agit d'un fichier non vide.
          char mtime[12];        //Date de modification de l'élément.
          char checksum[8];      //Somme de contrôle du header.
          char typeflag[1];      //Type d'élément : on ne gère que 0 (fichier), 2 (lien symbolique) et 5 (dossier).
          char linkname[100];    //Nom du fichier auquel fait référence le lien symbolique.
          char magic[6];         //Champs contenant "ustar\0" si il s'agit d'une archive ustar (sert à vérifier si il s'agit d'une archive tar).
          char version[2];       //Version. Normalement valant "00" ASCII pour des archives POSIX tar standardes.
          char uname[32];        //User name lié au User ID de l'élément. (habituellement le nom d'utilisateur propriétaire de l'élément)
          char gname[32];        //Groupe name lié au Groupe ID de l'élément.
          char devmajor[8];      //Inutile ici. (pour typeflag 4).
          char devminor[8];      //Inutile ici. (pour typeflag 4).
          char prefix[155];      //Espace supplémentaire pour le nom (qui contient également le chemin d'accès) si il est trop long. Les octets inutilés valent NUL.
          char pad[12];          //Padding pour prefix, valant aussi NULL si il n'y a pas de besoin d'espace supplémentaire.
};

//On crée un alias de cette structure pour éviter de répéter le mot-clé struct à chaque fois.
typedef struct header_posix_ustar headerTar;


/*
Fonction appelée par les threads !
Fonction principale : recueille les header de chaque fichier dans l'archive (compressée ou non) ainsi que les données suivantes chaque header si il y en a.
Appelle ensuite les diverses fonctions utiles au traitement souhaité.
Prend en argument l'emplacement de l'archive, et les 5 flags d'options (ainsi que le nombre de threads)
Retourne 0 (EXIT_SUCCES) si tout s'est bien passé, 1 (EXIT_FAILURE) sinon.
*/

void *traitement(char *folder);



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

int listing(headerTar head);



/*
Pratique l'extraction de l'élément correspondant au header passé en paramètre, en utilisant les données (data) si c'est un fichier.
Ecrit les codes de retour des open/close/write/symlink/mkdir/setuid/setgid/utime/fsync dans un logfile si l'option -e est spécifiée.
Affecte correctement tous les attributs des éléments, sauf le mtime des dossiers qui sont affectés en fin de traitement().
Si on veut forcer un nom de fichier, le spécifier dans le champ name, sinon mettre NULL.
Retourne 0 si tout s'est bien passé, -1 sinon.
*/

int extraction(headerTar *head, char *namex, char *data, FILE *logfile);



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



/*
Sert à vérifier la non-corruption d'une archive tar après son téléchargement.
Vérifie la somme de contrôle stockée dans le header passé en paramètre :
Pour cela, recalcule le checksum du header et le compare au champs checksum du header.
L'algorithme de calcul est le suivant : fait la somme de tous les bytes du header en remplaçant le champ header.checksum par
une suite de 8 espaces ASCII de valeur décimale 32.
Avant la comparaison, on applique un masque 0x3FFFF au checksum calculé car seul les 18 bits de poids faible
nous importent ici pourla comparaison. Pour cela on utilise un ET bit-à-bit. Ce n'est pas nécessaire mais
c'est plus sûr pour la comparaison. (les autres bits pouvant changer la valeur)
Retourne true si le checksum est bon, false sinon.
*/

bool checksum(headerTar *head);



#endif
