#PTAR

Extracteur d'archives durable et parallèle

Projet de RS TELECOM Nancy 2016-2017

Auteurs
    
    GARCIA Guillaume
    ZAMBAUX Gauthier

Dépendances

    groff-utf8   (pour un moyen de lecture de la page man pta(1)
    à venir ...

Build & execute:

    make
    ./ptar       ( [opts] à venir )

Utilisation de ptar

	Lister les dossiers et fichiers (et leur taille) d'une archive tar

	    ./ptar
	    Modifier l'emplacement de l'archive directement dans la variable directory dans main.c
	
	à venir ...
		
	    à venir...

	Exemples :

	    ./ptar
	    L'exemple test.tar fourni dans le git se présente comme suit :
							Dossier test
							/          \
					Dossier test1       Dossier test2
                  			 /                       \
			Fichier lol.txt (vide, 0 octets)    Fichier lol.lol (non-vide, 38 octets)

	    à venir ...

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

	   (Si nécessaire, télécharger le paquet groff-utf8 :)
 	   wget http://www.haible.de/bruno/gnu/groff-utf8.tar.gz
	   (C'est une archive tar non compressée à l'inverse de ce que laisse penser son extension !)
	   tar xvf groff-utf8.tar.gz
 	   cd groff-utf8
	   make
	   make install PREFIX=/usr/local
	   (Veillez à vérifier que votre chemin $PREFIX/bin est contenu dans $PATH).
	   cd <chemin_dossier_rs2016-Garcia-Zambaux>
	   (Si groff-utf8 est déjà installé, passer à:)
	   tar -xvzf ptar.1.gz
	   groff-utf8 -Tutf8 -mandoc ptar.1 | less


Dernière Màj 

	26/10/2016    02:32
