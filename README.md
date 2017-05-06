# PTAR

**PTAR - Extracteur d'archive tar USTAR POSIX durable et parallèle. Compatible USTAR Pax et UTF-8.**

*Projet de RS TELECOM Nancy 2016-2017*

* **Auteurs**

  - GARCIA Guillaume
  - ZAMBAUX Gauthier

* **Dépendances**

  - zlibc zlib1g zlib1g-dev        (La bibliothèque zlib)
  - groff-utf8                     (optionnel, pour ptar(1))

* **Build & execute:**

    - `make`
    - `./ptar  [opts]  [args]`

* **Documentation**

  - Pour des détails plus précis sur notre programme et sa conception, veuillez consulter notre [rapport de projet au format PDF](docs/rapport.pdf).
  - Vous pouvez également consulter la page de manuel [ptar(1)](manpage/ptar.1.md).

* **Utilisation de ptar**

	* *Lister les éléments contenus dans une archive tar*

	    - `./ptar chemin/vers/archive`

	* *Extraire les éléments d'une archive tar (et listing basique)*

	    - `./ptar -x chemin/vers/archive`

	* *Listing détaillé des métadonnées des éléments d'une archive tar*

	    - `./ptar -l chemin/vers/archive`

	* *Parallélisation et durabilisation des écritures avec NBTHREADS threads POSIX*

	    - `./ptar -p NBTHREADS chemin/vers/archive`

	* *Décompression d'une archive .tar.gz (compressée avec gzip), gère le cas de header désordonnés*

	   - `./ptar -z chemin/vers/archive`

	* *Générer un logfile (pour extraction & décompression) de divers codes retours, pour développeurs*

	   - `./ptar -e chemin/vers/archive`

* **Lancer la suite de test**

	- Compiler les sources si ce n'est pas déjà fait avec `make`
	- `make test`
  - Pour nettoyer le dossier courant lancez `make clean`

* **Exemples :**

	- Les options peuvent être combinées.
	- L'exemple testf.tar fourni dans le git ne contient que des fichiers (dont un vide).
	- L'exemple testall.tar fourni dans le git contient des fichiers, des dossiers et un lien symbolique.
	- L'exemple testfalsearch.tar fourni dans le git n'est pas une archive mais un fichier avec l'extension .tar.
	- Les exemples testf.tar.gz et testall.tar.gz sont les mêmes mais compressés au format gzip.
	- Les exemples vice.tar et vice.tar.gz sont prévues pour déboguer le programme en le stressant.
	- L'exemple bigarch.tar.gz est prévue pour tester le comportement de ptar sur une archive conséquente.


  - `./ptar archives_test/testall.tar`
    - _**Listing basique** des éléments de l'archive tar._

  - `./ptar -x archive_test/testall.tar`
    - _**Listing basique** et **extraction** des éléments de l'archive tar._

  - `./ptar -l archive_test/testall.tar`
    - _**Listing détaillé** des éléments de l'archive tar._

  - `./ptar -xl archive_test/testall.tar`
    - _**Listing détaillé** et **extraction** des éléments de l'archive tar._

  - `./ptar -z archive_test/testall.tar.gz`
    - _**Listing basique** des éléments de l'archive tar.**gz**._

  - `./ptar -xz archive_test/testall.tar.gz`
    - _**Listing basique** et **extraction** des éléments de l'archive tar.**gz**._

  - `./ptar -lz archive_test/testall.tar.gz`
    - _**Listing détaillé** des éléments de l'archive tar.**gz**._

  - `./ptar -zxl archive_test/testall.tar.gz`
    - _**Listing détaillé** et **extraction** des éléments de l'archive tar.**gz**._

  - `./ptar -xp 8 archives_test/testall.tar`
    - _**Listing basique** et **extraction durable** des éléments de l'archive tar sur 8 threads._

  - `./ptar -xlp 3 archive_test/testall.tar`
    - _**Listing détaillé** et **extraction durable** des éléments de l'archive tar sur 3 threads._

  - `./ptar -xzlp 4 archive_test/testall.tar.gz`
    - _**Listing détaillé** et **extraction durable** des éléments de l'archive tar.**gz** sur 4 threads._


  - Exécuter le script rmtest.sh avant chaque test sur les archives pour nettoyer le dossier courant :
    - `make clean`


## Page de manuel ptar(1)

* **Lire la page de manuel de ptar sans manipulations/droits super-utilisateurs au préalable**

	- `man ./manpage/ptar.1.gz`

* **Lire la page de manuel avec `man ptar`, nécessite d'avoir les droits super-utilisateurs**

  - Faites une sauvegarde de votre config man : `sudo mv /etc/man.config /etc/man.config.save`
	- `sudo cp ./manpage/man.config /etc`
	- `sudo mkdir -p /usr/local/man/man1/`
	- `sudo install -g 0 -o 0 -m 0644 ./manpage/ptar.1.gz /usr/local/man/man1/`
	- `man ptar`

	- (La commande `man ptar` devrait alors afficher la page man du bon programme
	   et pas celle du programme ptar potentiellement préexistant "tar-like program written in perl")

* **Lire la page de manuel avec groff-utf8**

	- (Si nécessaire, télécharger le paquet groff-utf8, sinon passer à (1) )
 	- `wget http://www.haible.de/bruno/gnu/groff-utf8.tar.gz`
	- (C'est une archive tar non compressée à l'inverse de ce que laisse penser son extension !)
	- `tar xvf groff-utf8.tar.gz`
 	- `cd groff-utf8`
	- `make`
	- `make install PREFIX=/usr/local`
	- (Veillez à vérifier que votre chemin $PREFIX/bin est contenu dans $PATH).
	- `cd chemin/dossier/rs2016-Garcia-Zambaux`
	- (1) `tar -xvzf ./manpage/ptar.1.gz`
	- `groff-utf8 -Tutf8 -mandoc ptar.1 | less`


## Bibliotheque dynamique : zlib

  * **Installer la zlib**

      - `sudo apt-get install zlibc zlib1g zlib1g-dev`

  * **Configurer la variable LD_LIBRARY_PATH**

      - `export LD_LIBRARY_PATH= path_du_raccourci_vers_zlib.so`


## Liste des corrections

  * **Dernière Màj**

  	**16/12/2016**  -  14:00  - **version 1.7.5.1** : *Version stable de ptar rapide, bugs mineurs corrigés.*

  * Pour voir la liste détaillées des corrections, consultez le [changelog](docs/CHANGELOG.md).


## Debug

* **Pour observer le code brute d'une fichier et son affichage**

	- `hexdump -C testfalsearch.tar`
	- `hexdump -C testf.tar`
	- `hexdump -C testall.tar`

* **Pour observer le contenu d'un .o (table des symboles)**

	- `nm main.o`
	- `nm utils.o`
	- `nm checkfile.o`

* **Pour voir les includes à la compilation, ajouter aux CFLAGS du Makefile**

	- -I\<path\>

* **Pour déboguer avec GDB, il est préférable d'ajouter aux CFLAGS du Makefile (génère une table des symboles pour débug)**

	- -g

* **Le fichier logfile.txt généré lors de l'extraction si l'option -e est spécifiée contient les codes de retours des fonctions utilisées pendant l'extraction. Générer un logfile lors de l'extraction/décompression d'une archive**

	- `./ptar -xzep 3 archive_test/testall.tar`

* **Pour compter les threads utilisés dans le programme, lancer ce dans un terminal parallèle**

	- `./scripts/countThreads`

* **Pour compter le temps d'éxecution du programme, utiliser time**

	- `time ./ptar -lxzp 8 archive.tar.gz`

* **Pour bien charger la bibliotheque dynamique, il faut parfois bien set la variable d'environnement LD_LIBRARY_PATH**

	- `export LD_LIBRARY_PATH = <path_du_raccourci.so>`

* **ptar vérifie la somme de contrôle (checksum) des fichiers de l'archive, si l'un des éléments est corrompu, ptar termine et renvoie une erreur.**
