#PTAR

Extracteur d'archives durable et parallèle

Projet de RS TELECOM Nancy 2016-2017

Auteurs
    
    GARCIA Guillaume
    ZAMBAUX Gauthier

Dépendances

    groff-utf8   (pour un moyen de lecture de la page man pta(1)

Build & execute:

    make
    ./ptar  [opts]

Utilisation de ptar

	Lister les éléments (et leur taille) compris dans une archive tar -Fonctionnel-

	    ./ptar <chemin/vers/archive>
	
	Extraire les éléments d'une archive tar (et listing basique) -Fonctionnel-
		
	    ./ptar -x <chemin/vers/archive>

	Listing détaillé des métadonnées des éléments d'une archive tar -Non implémenté-

	    ./ptar -l <chemin/vers/archive>

	Parallélisation et durabilisation de l'extraction avec NBTHREADS threads -Non implémenté-

	    ./ptar -xp <NBTHREADS> <chemin/vers/archive>

	Décompression d'une archive .tar.gz (compressée avec gzip) -Non implémenté-

	   ./ptar -z <chemin/vers/archive>

	Exemples :

	    L'exemple test.tar fourni dans le git se présente comme suit :

							Dossier test --- lien_symbo1 (lien symbolique lol.lol)
							/          \
					Dossier test1       Dossier test2
                  			 /                       \
			Fichier lol.txt (vide, 0 octets)    Fichier lol.lol (non-vide, 38 octets)

	    ./ptar archives_test/test.tar
	    ./ptar -x archive_test/test.tar

	    à venir :
	    ./ptar -l archive_test/test.tar
	    ./ptar -xl archive_test/test.tar
	    ./ptar -xlp 3 archive_test/test.tar
	    ./ptar -xzlp 4 archive_test/test.tar.gz
	    ./ptar -zx 4 archive_test/test.tar
	    ./ptar -z archive_test/test.tar.gz

		
	    Utiliser `rm- rf test/` avant chaque test sur l'archive archives_test/test.tar.
	   

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

	27/10/2016    07:29     version 1.3.0.0 (étape 3 achevée)


Debug 

	Pour observer le code brute d'une fichier et son affichage
		hexdump -C test.tar
