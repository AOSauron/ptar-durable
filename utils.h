///////// Pour ptar 1.3 minimum /////////
/*

Fonctions utils

Fonctions principales du programme ptar :
Correspondantes aux options suivantes :
-x : Extraction.
-l : Listing détaillé.
-z : Décompression gzip avec la bibliothèque zlib.
-p NBTHREADS : (forcément couplée avec -x au moins) Durabilité et parallélisation de l'opération avec un nombre de threads choisi.

*/


/*
Pratique l'extraction de l'élément correspondant au header passé en paramètre, en utilisant les données si c'est un fichier.
Ecrit les codes de retour des open/close/write/symlink/mkdir/setuid/setgid dans un logfile
Retourne 0 si tout s'est bien passé, -1 sinon.
*/
int extraction(struct header_posix_ustar head, char *data, FILE *logfile);

/*
Effectue le listing détaillé des éléments passés en paramètre.
Print : permissions, uid/gid (= propriétaire/groupe), taille, mtime, liens symboliques (fichier linké).
Retourne 0.
*/
int listing(struct header_posix_ustar head);
