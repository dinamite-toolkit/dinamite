#!/bin/bash

if [ -z $1 ]; then
    TEST=main
else
    TEST=$1
fi

if [ "$OSTYPE" == 'darwin' ]; then
    LIBNAME='AccessInstrument.dylib'
else
    LIBNAME='AccessInstrument.so'
fi

if [ -z $CLANG ]; then
    CLANG='clang'
fi

#$CLANG -O3 -g -v -Xclang -load -Xclang \
DIN_FILTERS="function_filter.json" $CLANG -O3 -g -v -Xclang -load -Xclang \
	   ../../Release+Asserts/lib/${LIBNAME} \
	   tests/${TEST}.cpp -L./library/ -linstrumentation -lpthread -o ${TEST}

#clang -g -v -Xclang -load -Xclang ../../Release+Asserts/lib/AccessInstrument.so tests/main.c -L./library/ -linstrumentation
#clang++ -g -v -Xclang -load -Xclang ../../Release+Asserts/lib/AccessInstrument.so main.cpp -L./library/ -linstrumentation
