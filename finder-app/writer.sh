#!/bin/sh

if [ $# -ne 2 ]
then
    echo "fail: The script $0 requires two arguments"
    exit 1
fi

FILE=$1
STRING=$2

mkdir -p "$(dirname $FILE)" && touch "$FILE"

if [ $? -eq 0 ]; then
    echo $STRING > $FILE
    exit 0
else
    echo "failed: The file $FILE could not be created"
    exit 1
fi
