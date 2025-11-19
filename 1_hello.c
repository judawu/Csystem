

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main()
{
    char *s="hello world";
     
    char *t=malloc(strlen(s)+1);
    
    char *r=s;
      
       if(t==NULL)
       {return 1;};
       
       for (int i=0;i<strlen(s)+1;i++)
         {
             t[i]=s[i];
             
         }
         printf("%s\n",s);
         printf("%s\n",t);
         printf("%s\n",r);
         printf("%c\n",r[6]);
          printf("%c\n",*(r+6));
         printf("%s\n",(r+6));
         t[6]='W';
          printf("%s\n",t);
         strcpy(t,r);
       
         printf("%s\n",t);
         
         free(t);
         
       
    return 0;
}
