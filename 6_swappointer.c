

#include <stdio.h>
#include <stdlib.h>


int main(void)
{
    int f_swap(int a,int b) ;
    int t_swap(int *a,int *b);
   
    int x,y;
    char *z=malloc(1*sizeof(char));
    printf("input x and  y and string z:\n");
   scanf("%i",&x);
   scanf("%i",&y);
    scanf("%s",z);
    printf("x is %i,y is %i\n",x,y);
    f_swap(x,y);
    printf("x is %i,y is %i\n",x,y);
    t_swap(&x,&y);
     printf("x is %i,y is %i\n",x,y);
    
      printf("z is %s\n",z);
     
    return 0;
}

int f_swap(int a,int b)
{
    int temp=a;
    a=b;
    b=temp;
}

int t_swap(int *a,int *b)
{
    int temp=*a;
    *a=*b;
    *b=temp;
}





