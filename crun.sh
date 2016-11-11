#!/bin/bash

if [ -z $1 ]; then
    TEST=main
else
    TEST=$1
fi

if [ "$OSTYPE" == 'darwin' ]; then
    DYLD_LIBRARY_PATH=./library ./${TEST}
else
    LD_LIBRARY_PATH=./library ./${TEST}
fi
