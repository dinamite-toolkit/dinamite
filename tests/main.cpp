#include <stdio.h>
#include <iostream>


unsigned int global_var_one;
unsigned int global_var_two;

class Class1 {
    public:
    int a;
    int b;
    int c;
    int d;
    int e;
    int f;
    int g;
    int h;
    int i;
};

class Class2 {
    public:
    Class1 child;
    float x;

    void increaseX() {
        for (int i = 0; i < 10; i++) {
            this->x += 1.2;
        }
    }

};

int test_function() {
    int fn_a = 1;
    int fn_b = 3;

    if (fn_a < fn_b) {
        fn_a += fn_b;
        global_var_one += fn_a;
    } else {
        fn_a += fn_b;
        global_var_one += fn_a;
    }

    return fn_a + fn_b;
}
int case_test(int a) {
    int retval;
    switch (a) {
        case 1: retval = 6;
                break;
        case 2: retval = 3;
                break;
        case 3: retval = 4;
                break;
        case 4: retval = 1;
                break;
        case 5: retval = 9;
                break;
        case 6: retval = 10;
                break;
        case 7: retval = 11;
                break;
        case 8: retval = 15;
                retval += 2 * retval;
                break;
        default: retval = 0;
                 break;
    }
    return retval;
}

int main() {
    Class1 test1;
    Class2 test2;

    global_var_one = 5;

    int *pointer;
    int someint;

    for (int i = 0; i < 1000000; i++) {
        case_test(i);
        test1.a = i;
        pointer = (int*)(&test1);
        test1.c = i+1;
        test2.child.a = i+2;
        test2.child.b = i+3;
        (&test2)->x = 3.14;
        global_var_two = test_function();
    }
    for (int i = 0; i < 1000000; i++) {
        test1.d = i;
        test1.e = i+1;
        test2.child.a = i+2;
        test2.child.b = i+3;
        (&test2)->x = 3.14;
        test2.increaseX();
        global_var_two = test_function();
    }
    for (int i = 0; i < 1000000; i++) {
        test1.f = i;
        test1.i = i+1;
        test2.child.a = i+2;
        test2.child.b = i+3;
        (&test2)->x = 3.14;
    }


}
