#!/bin/bash

if [ "$#" !=  "2" ]; then
    echo "Number of parameters is not two items"
    exit 1
fi

if [ ! -d "$1" ]; then
    echo "The first argument is not a valid directory"
    exit 1
fi

filesdir="$1"
searchstr="$2"

file_count=$(find $filesdir -type f | wc -l)
match_count=$(grep -r $searchstr $filesdir | wc -l)
echo The number of files are $file_count and the number of matching lines are $match_count
