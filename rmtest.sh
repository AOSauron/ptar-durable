#!/bin/bash

#Nettoie le répértoire courant des fichiers extraits des archives test pour ptar. Enlève également les fichiers cachés xxx.xxx~
#Supprime également le fichier logfile.txt
#Supprime les éventuels tubes nommés dans le dossier courant.
#Adapté pour la version 1.3.1.0 de ptar (et de ses exemples) minimum

rm -rf test/
rm -rf include/
rm fichierc
rm f1.*
rm *.jpg

rm logfile.txt

rm *~
rm archives_test/*~

rm *.fifo
