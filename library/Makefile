CC=gcc

CFLAGS+= -O3

%.o: %.c
	$(CC) -g -c -fpic -o $@ $< $(CFLAGS)

text: textinstrumentation.o bitcode
	$(CC) -shared -o libinstrumentation.so $<

null: nullinstrumentation.o bitcode
	$(CC) -shared -o libinstrumentation.so $<

binary: binaryinstrumentation.o dinamite_time.o
	make bitcode
	$(CC) -shared -o libinstrumentation.so $^

bitcode: textinstrumentation.c
	clang -emit-llvm $< -c -g -o instrumentation.bc

clean:
	rm *.o instrumentation.bc

