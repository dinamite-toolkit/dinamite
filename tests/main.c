#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int a;
    int b;
} simplestruct;

typedef struct s_compositestruct {
    simplestruct child;
    int c;
    int d;
    float e;
} compositestruct;

int abc(int i, void *second) {
    if (i%2) {
        return 2 * (int)((int)second & 0xf);
    } else {
        return 1;
    }
    return 0;
}

int main(int argc, char **argv) {
    compositestruct cs;
    simplestruct ss;
    simplestruct ssarr[50000];

    struct {
        int test1;
        int test2;

    } testliteral;

	int i, j;
    int a = 0;
    cs.e = 0;

    testliteral.test1 = 0;

    simplestruct *test = (simplestruct *) malloc(42 * sizeof(simplestruct));

 for (j = 0; j < 50; j++) {
    compositestruct *temp;
        for (i = 0; i < 5; i++) {
            abc(i, (void*)(i+0xff));
            a += 2;
            cs.child.a = i;
            cs.e += 3.14;
            ss.a = a;
            ss.a++;
            ss.a++;
            ss.a++;
            ssarr[i].b = 4;

            temp = (compositestruct *) malloc(sizeof(compositestruct));
            temp = (compositestruct *) calloc(2, sizeof(compositestruct));

            temp->e = cs.e;
            temp->c = 3;
            temp->c += 1;
            //free(temp);
        }
    }
    exit(0);

	return 0;

}
