#!/bin/bash

DIRECTORIES=`find -name '\.branch' -printf '%h\n' | sort -u`

for dir in $DIRECTORIES
do
        FILES_TO_DELETE=`ls -1 $dir | head -n -3`

        for file in $FILES_TO_DELETE
        do
                echo "Deleting directory $dir/$file"
                rm -rf $dir/$file
        done
done

