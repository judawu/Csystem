/*malloc2.c*/
#include <unistd.h>
#include <stdio.h>
#define malloc2(x) sbrk(x);_alloc+=x;
#define free2(x) sbrk(-_alloc);_alloc=0;

unsigned int _alloc;
int main() {
    char *name;
   name=malloc2(32*sizeof(char));
   printf("what is your name!\n");
   scanf("%31s", name);
    printf("hello, %s\n", name);
    free2(name);
    return 0;
}

