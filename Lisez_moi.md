#PTAR

Extracteur durable et parallèle d'archives ustar POSIX

Projet de RS TELECOM Nancy 2016-2017

Auteurs
    
    GARCIA Guillaume
    ZAMBAUX Gauthier

Dépendances

    groff-utf8   (pour un moyen de lecture de la page man ptar(1)

Build & execute:

    make
    ./ptar  [opts]  [args]

Utilisation de ptar

	Lister les éléments contenus dans une archive tar -FONCTIONNEL-

	    ./ptar <chemin/vers/archive>
	
	Extraire les éléments d'une archive tar (et listing basique) -FONCTIONNEL-
		
	    ./ptar -x <chemin/vers/archive>

	Listing détaillé des métadonnées des éléments d'une archive tar -NON IMPLEMENTE-

	    ./ptar -l <chemin/vers/archive>

	Parallélisation et durabilisation de l'extraction avec NBTHREADS threads -NON IMPLEMENTE-

	    ./ptar -xp <NBTHREADS> <chemin/vers/archive>

	Décompression d'une archive .tar.gz (compressée avec gzip) -NON IMPLEMENTE-

	   ./ptar -z <chemin/vers/archive>


	Exemples :

	    L'exemple testf.tar fourni dans le git ne contient que des fichiers (dont un vide).

	    L'exemple testall.tar fourni dans le git contient des fichiers, des dossiers et un lien symbolique.

	    ./ptar archives_test/testf.tar
	    ./ptar -x archive_test/testf.tar
	    ./ptar archives_test/testall.tar
	    ./ptar -x archive_test/testall.tar

	    à venir :
	    ./ptar -l archive_test/testall.tar
	    ./ptar -xl archive_test/testall.tar
	    ./ptar -xlp 3 archive_test/testall.tar
	    ./ptar -xzlp 4 archive_test/testall.tar.gz
	    ./ptar -zx 4 archive_test/testall.tar
	    ./ptar -z archive_test/testall.tar.gz

		
	    Exécuter le script rmtest.sh avant chaque test sur les archives pour nettoyer le dossier courant.
	    ./rmtest.sh
	   

Page de manuel ptar(1)
	
	Lire la page de manuel de ptar sans manipulations/droits super-utilisateurs au préalable
	
	   man ./ptar.1.gz

	Lire la page de manuel avec `man ptar`, nécessite d'avoir les droits super-utilisateurs

	   sudo cp man.config /etc
	   sudo mkdir -p /usr/local/man/man1/
	   sudo install -g 0 -o 0 -m 0644 ptar.1.gz /usr/local/man/man1/
	   man ptar

	   (La commande `man ptar` devrait alors afficher la page man du bon programme 
	   et pas celle du programme ptar potentiellement préexistant "tar-like program written in perl")

	Lire la page de manuel avec groff-utf8

	   (Si nécessaire, télécharger le paquet groff-utf8, sinon passer à (***) )
 	   wget http://www.haible.de/bruno/gnu/groff-utf8.tar.gz
	   (C'est une archive tar non compressée à l'inverse de ce que laisse penser son extension !)
	   tar xvf groff-utf8.tar.gz
 	   cd groff-utf8
	   make
	   make install PREFIX=/usr/local
	   (Veillez à vérifier que votre chemin $PREFIX/bin est contenu dans $PATH).
	   cd <chemin/dossier/rs2016-Garcia-Zambaux>
	   (***)
	   tar -xvzf ptar.1.gz
	   groff-utf8 -Tutf8 -mandoc ptar.1 | less


Dernière Màj 

	30/10/2016    19:41     version 1.3.1.0 (étape 3 achevée + adapation tests blancs & corrections)


Debug 

	Pour observer le code brute d'une fichier et son affichage
		hexdump -C testf.tar
		hexdump -C testall.tar
