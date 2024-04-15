#!/bin/bash

if [ "$#" !=  "2" ]; then
    echo "Number of parameters is not two items"
    exit 1
fi

writefile="$1"
writestr="$2"

# Create the directoryif doesn't exist
dir_path=$(dirname "$writefile")
if [ ! -d $dir_path ]; then
    mkdir -p $dir_path
fi

if [ "$?" != 0 ]; then
    echo "The folder containing the file can't be created"
    exit 1
fi

echo $writestr > $writefile

if [ "$?" != 0 ]; then
    echo "The file can't be created"
    exit 1
fi
