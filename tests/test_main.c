#include <stdio.h>

int test_core(void);
int test_session(void);

int main(void)
{
    int failures = 0;
    failures += test_core();
    failures += test_session();
    if (failures != 0) {
        fprintf(stderr, "C0-PQLink: %d test(s) failed\n", failures);
        return 1;
    }
    puts("C0-PQLink: all tests passed");
    return 0;
}

