#!/bin/bash

#Nettoie le répértoire courant des fichiers extraits des archives test pour ptar. Enlève également les fichiers cachés xxx.xxx~
#Adapté pour la version 1.3.1.0 de ptar (et de ses exemples)

rm -rf test/
rm f1.*

rm *~
