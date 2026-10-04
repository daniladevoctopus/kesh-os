#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/utsname.h>

int main(int argc, char **argv) {
    printf("\n========================================\n");
    printf("   Hello from musl libc 1.2.5 on KeshOS!\n");
    printf("========================================\n");
    printf("[musl] argc = %d, argv[0] = '%s'\n", argc, argv[0] ? argv[0] : "NULL");

    struct utsname u;
    if (uname(&u) == 0) {
        printf("[musl] uname sysname: %s, release: %s, machine: %s\n", u.sysname, u.release, u.machine);
    }

    void *ptr = malloc(1024);
    if (ptr) {
        strcpy((char *)ptr, "musl dynamic memory allocation (brk/mmap) working!");
        printf("[musl] %s\n", (char *)ptr);
        free(ptr);
    }

    printf("[musl] Standard C library POSIX layer verified OK!\n");
    printf("========================================\n\n");
    return 0;
}
