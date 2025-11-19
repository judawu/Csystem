/*dynamicmemory.c */
#include <stdio.h>
#include <stdlib.h>


int main() {
    char *name;
   name=malloc(32*sizeof(char));
   printf("what is your name!\n");
   scanf("%31s", name);
    printf("hello, %s\n", name);
    free(name);
    return 0;
}
