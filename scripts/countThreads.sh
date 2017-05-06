#!/bin/bash

#Compte les threads lancé par ptar
#S'arrete avec controle+C !

echo "Contrôle + C pour arrêter le script après effet."
while [ 1 ];do ps -T -C ptar; echo "----";done >>trace

