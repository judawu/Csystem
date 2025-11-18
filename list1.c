

#include <stdio.h>
#include <stdlib.h>


int main(void)
{
    
    printf("start:\n");
    // allocate a dynamic meomry for 3 ints array
    int *list = malloc(3*sizeof(int));
    // protect memory 
    if (list==NULL)
    {
        return 1;
        
    };
    // assign value to array
    *list=1;
    *(list+1)=2;
    list[2]=3;
  
    
//time passed   

// asiign a mew dynamic memory for incease 4 int array
    int *tmp=malloc(4*sizeof(int));
    
    if(tmp==NULL)
    {
        free(list);
        return 1;
    }

    //copy from old array to new array
    for(int i=0;i<3;i++)
    {
        tmp[i]=list[i];
        
    }
    // assign the forth int in the new array
       tmp[3]=4;
     // relase the address for old pointer  
       free(list);

    // older pointer point to the new array
       list=tmp;
       
    for(int i=0;i<4;i++)
    {
       printf("list position  %i is %i\n",i,list[i]); 
       
        
    }
    //relase the address for old pointer
    free(list);
    return 0;
    
}
    





