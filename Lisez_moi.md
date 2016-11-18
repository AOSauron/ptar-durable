#PTAR

Extracteur durable et parallèle d'archives ustar POSIX

Projet de RS TELECOM Nancy 2016-2017

Auteurs
    
    GARCIA Guillaume
    ZAMBAUX Gauthier

Dépendances

    groff-utf8   (pour un des moyens de lecture de la page man ptar(1)

Build & execute:

    make
    ./ptar  [opts]  [args]

Utilisation de ptar

	Lister les éléments contenus dans une archive tar -FONCTIONNEL-

	    ./ptar <chemin/vers/archive>
	
	Extraire les éléments d'une archive tar (et listing basique) -FONCTIONNEL-
		
	    ./ptar -x <chemin/vers/archive>

	Listing détaillé des métadonnées des éléments d'une archive tar -FONCTIONNEL-

	    ./ptar -l <chemin/vers/archive>

	Parallélisation et durabilisation de l'extraction avec NBTHREADS threads -NON IMPLEMENTE-

	    ./ptar -xp <NBTHREADS> <chemin/vers/archive>

	Décompression d'une archive .tar.gz (compressée avec gzip) -FONCTIONNEL-

	   ./ptar -z <chemin/vers/archive>

	Générer un logfile (pour extraction & décompression) de divers codes retours, pour développeurs -FONCTIONNEL-

	   ./ptar -e <chemin/vers/archive>


	Exemples :
		
	    Les options peuvent être combinées.

	    L'exemple testf.tar fourni dans le git ne contient que des fichiers (dont un vide).

	    L'exemple testall.tar fourni dans le git contient des fichiers, des dossiers et un lien symbolique.

	    L'exemple testfalsearch.tar fourni dans le git n'est pas une archive mais un fichier avec l'extension .tar.

	    Les exemples testf.tar.gz et testall.tar.gz sont les mêmes mais compressés au format gzip.

	    ./ptar archives_test/testall.tar
	    ./ptar -x archive_test/testall.tar
	    ./ptar -l archive_test/testall.tar
	    ./ptar -xl archive_test/testall.tar
	    ./ptar -xe archive_test/testall.tar
	    ./ptar -z archive_test/testall.tar
	    ./ptar -xz archive_test/testall.tar.gz
	    ./ptar -zxl archive_test/testall.tar.gz
	    ./ptar -xzel archive_test/testall.tar.gz

	    à venir :
	    ./ptar -xlp 3 archive_test/testall.tar
	    ./ptar -xzlp 4 archive_test/testall.tar.gz


	    Exécuter le script rmtest.sh avant chaque test sur les archives pour nettoyer le dossier courant (enlève également logfile.txt).
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

	17/11/2016    08:04     version 1.6.0.0 (étape 6 sans étape 5)


Debug 

	Pour observer le code brute d'une fichier et son affichage
		hexdump -C testfalsearch.tar
		hexdump -C testf.tar
		hexdump -C testall.tar

	Le fichier logfile.txt généré lors de l'extraction si l'option -e est spécifiée contient les codes de retours des fonctions utilisées pendant l'extraction.
	Générer un logfile lors de l'extraction d'une archive :
		./ptar -xe archive_test/testall.tar

	Il est possible d'également décompresser un fichier.gz (sans archivage), mais les attributs (tels que premissions, gid et uid) ne sont pas pas traités.
	Les permissions sont forcées : Lecture ecriture et recherche pour l'utilisateur.
	Cette fonction est purement optionnelle et en bêta.
		./ptar -z ptar.1.gz
