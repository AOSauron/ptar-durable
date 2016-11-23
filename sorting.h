///////// Pour ptar 1.5 minimum /////////
/*

Fonctions de tri

Utile seulement après la décompression.
Liste et comptent chaque élément de chaque types.
Sert à trier les éléments récupérés après la décompression, en effet ils sont désordonnés post-décompression.
Il arrive alors que certains dossiers arrivent avant leur dossiers parents (qui ne sont pas encore créés) et donc il y a erreur.

*/

#ifndef INCLUDE_SORTING_H
#define INCLUDE_SORTING_H


/* Structure comprenant les 3 tableaux de header de chaque type (fichiers, dossiers, liens symboliques) et les données des fichiers */

struct header_table {
	struct header_posix_ustar *headDir;
	struct header_posix_ustar *headFile;
	struct header_posix_ustar *headSymlink;
	char *datas;
	int nbDir;
	int nbFile;
	int nbSymlink;
};

typedef struct header_table gzHeadertype;


/*
Analyseur d'archive : compte le nombre d'élément de chaque type contenu dans l'archive.
Retourne une structure adaptée aux traitements.
Le paramètre doit être le tube nommé créé après la décompression.
L'ordre des données est le même que celui des fichiers.
*/

gzHeadertype analyse(const char *folder, FILE *logfile);


/*
Trieur : trie les éléments (les dossiers) avec un tri à bulle classique.
Ne retourne rien vu la simplicité de la fonction.
*/

void tribulle(gzHeadertype composition);


/*
Compte le nombre de token séparés par le délimiteur passé en paramètre.
Retourne ce nombre.
*/

int getnbtoken(char *name, const char *delim);


/*
Boucle sur les headers, dans le bon ordre, afin d'extraire/lister tous les éléments de façon satisfaisante.
Retourne 0 si tout s'est bien passé, 1 si il a eu au moins 1 erreur.
*/

int traitepostdecomp(gzHeadertype composition, FILE *logfile);


#endif
