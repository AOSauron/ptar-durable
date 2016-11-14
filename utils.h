///////// Pour ptar 1.3 minimum /////////
/*

Fonctions utils

Fonctions principales du programme ptar :
Correspondantes aux options suivantes :
-x : Extraction.
-l : Listing détaillé.
-z : Décompression gzip avec la bibliothèque zlib.
-p NBTHREADS : (forcément couplée avec -x au moins) Durabilité et parallélisation de l'opération avec un nombre de threads choisi.
-e : Ecriture d'un logfile.txt. Seulement compatible avec l'extraction (-x) pour le 

*/


/*
Pratique l'extraction de l'élément correspondant au header passé en paramètre, en utilisant les données si c'est un fichier.
Ecrit les codes de retour des open/close/write/symlink/mkdir/setuid/setgid/utime/fsync dans un logfile si l'option -e est spécifiée.
Retourne 0 si tout s'est bien passé, -1 sinon.
*/
int extraction(struct header_posix_ustar head, char *data, FILE *logfile, int log);

/*
Effectue le listing détaillé des éléments passés en paramètre.
Print : permissions, uid/gid (= propriétaire/groupe), taille, mtime, liens symboliques (fichier linké).
Retourne 0.
*/
int listing(struct header_posix_ustar head);

/*
Décompresse l'archive .tar.gz (il faut que l'extension .gz soit présente) passée en paramètre avant tout traitement ultérieur.
Retourne 0 si la décompression s'est bien passée, -1 sinon.
*/
int decompress(char *directory);
