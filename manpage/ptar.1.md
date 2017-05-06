#### PTAR(1)  &nbsp;&nbsp;&nbsp;&nbsp; Commandes&nbsp;&nbsp;&nbsp; &nbsp;PTAR(1)



###### NOM
       ptar - Listing et extraction d'archives TAR

###### SYNOPSIS
       ptar [OPTION]... [FICHIER]

###### DESCRIPTION
       Affiche le contenu (si aucun paramètre n'est fourni) ou, plus utile, extrait l'archive TAR nommée FICHIER,
       à partir du répertoire courant.

###### OPTIONS
       Aucune option n'est obligatoire.


  * -x
            En plus de lister le contenu de l’archive TAR passée en paramètre,extrait  son  contenu  (crée  les
            répertoires, fichiers, définit les permissions, les propriétaires, les dates de modification, etc.)


  * -l
            Listing  détaillé.  Par  défault,  ptar  n'affiche  que  les noms des répertoires et fichiers. Avec
            l'option -l, ptar affiche, séparés par un espace à chaque fois:
             - les permissions sous la forme drwxr-xr-x (comme dans la sortie de ls -l)
             - l'uid et le gid du fichier, sous la forme 1001/1001
             - la taille du fichier (en décimal)
             - la date de modification, sous la forme 2016-10-13 16:44:12
             - le nom du fichier ou du répertoire (suivi d'un / si répertoire)
             - si le fichier est un lien symbolique, "-> destination"


  * -p NBTHREADS
            Utilise NBTHREADS threads pour réaliser les écritures en parallèle.

  * -z
            L'archive passée en paramètre est compressée avec gzip; demande à ptar de la décompresser avant  de
            la  traiter.  ptar  chargera  dynamiquement  la  bibliothèque  zlib (avec dlopen) pour effectuer la
            décompression.

  * -e
            Option pour développeurs, génère un logfile dans le répertoire courant  contenant  les  valeurs  de
            retour des diverses fonctions utilisées (notemment pendant l'extraction).

       Toutes les options peuvent être combinées.

###### VALEUR DE RETOUR
       ptar  s'arrête  en renvoyant la valeur de retour 0 si tous les traitements se sont déroulés avec succès; 1
       sinon.


###### EXEMPLE
       ptar -lxzp 8 fichier.tar.gz
              Liste le contenu détaillé de fichier.tar.gz (une archive tar compressée avec gzip) et  extrait  son
              contenu dans le répertoire courant. L'extraction se fait en utilisant 8 threads.

###### VOIR AUSSI
       tar(1), tar(5), zlib(5)



1.0.0.0  &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;2016-10-23                                     &nbsp;&nbsp; &nbsp;&nbsp;&nbsp;PTAR(1)
