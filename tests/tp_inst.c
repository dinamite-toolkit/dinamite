#include <stdio.h>

int a = 0;

void __dinamite_tracepoint(char *msg, int value) {
}

void increment_a() {
    void *testptr = (void *) &a;
    a++;
}

int main(int argc, char **argv) {
    int i;

    for (i = 0; i < 10000; i++) {
        __dinamite_tracepoint("<log> Before", a);
        increment_a();
        __dinamite_tracepoint("<log> After", a);
    }

    return 0;
}

