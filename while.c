#include <stdio.h>

int main() {
   int input;
   input = 0;
   while(1) {
        printf("Do you want to quit enter 1,please input a int numberto.\n");
        scanf("%d", &input);
        if(input == 1) {
            break;
        }
    }
    printf("Exited the loop.\n");
    return 0;
}