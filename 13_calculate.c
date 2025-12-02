/*calculate.c*/
#include <stdio.h>

#define ADD 1
#define SUB 2
#define MUL 3
#define DIV 4

void add(int *result,int a,int b){
   *result=a+b;
   return;
};
void sub(int *result,int a,int b){
   *result=a-b;
   return;
};
void mul(int *result,int a,int b){
   *result=a*b;
   return;
};
void div(int *result,int a,int b){
   *result=a/b;
   return;
};

int main(){
    int x,y,op,result;
    void (*fp)();

    printf("please press 1 for addition,2 for subtraction,3 for multplication,4 for divsion and 0 for quit\n");
    scanf("%d",&op);
    printf("Number 1: ");
    scanf("%d",&x);
    printf("Number 2: ");
    scanf("%d",&y);

    switch(op)
    {
        case ADD:
        fp=add;
        break;
        case SUB:
        fp=sub;
        break;
        case DIV:
        fp=div;
        break;
        case MUL:
        fp=mul;
        break;
        default:
        return -1;
        
    };

    fp(&result,x,y);
   printf("result is : %d\n",result);

   return 0;
   
}

