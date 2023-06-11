#!/bin/bash

# Boucler sur tous les fichiers du dossier
for fichier in ./install/bin/*
do
    echo ""
    echo "EXECUTING $fichier"
    echo ""
    "./$fichier" " 5" " 7"
done
