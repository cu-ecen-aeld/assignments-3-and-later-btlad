#!/bin/sh

if [ $# -ne 2 ]
then
    echo "fail: The script $0 requires two arguments"
    exit 1
elif [ ! -d "$1" ]
then
    echo " The path $1 does not exist "
    exit 1
else
    DIRN=$1
    SEARCH=$2
    X=$(find $DIRN -type f | wc -l)
    Y=$(grep $SEARCH $DIRN/* | wc -l)
    echo "The number of files are $X and the number of matching lines are $Y"
    exit 0
fi
