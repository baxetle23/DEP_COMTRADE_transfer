#!/bin/bash

#create array files and possible dates

echo "SCRIPT SORT FILE OSCIL STARTS WORK"

ROOT_DIR=$1

const_ext1="cfg" #const extencion files which moveing
const_ext2="dat" #const extencion files which moveing

cd $ROOT_DIR

for each in * #name file - may be use regular expressionsa
do
       extension="${each##*.}";
       if [[ $extension == $const_ext1 || $extension == $const_ext2 ]]; then
         date_file=$(date +%d-%m-%Y -r "$each");   #get data create file
         if [ ! -d "$date_file" ]; then
                mkdir "$date_file"
         fi
         mv $each $date_file/&each
       fi
done
