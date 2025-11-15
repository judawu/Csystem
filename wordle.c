/*wordlwt.c */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>

#define ResultGreen 1
#define ResultYellow 2
#define ResultRed 4


typedef char Result; 

struct s_words {
    char **arr;
    int size;
}

typedef struct s_words Words;

Result cc(char,int,char*);
Result *cw(char*,char*);
void Example_print_results(Result*) ;
bool isin(char, char*);
Words readfile(char*, int);

Words **readfile(char *filename, int maxline){
    char buf[8];
    int i,size;
    FILE *fd;
    char *ret[5];
    Words words;
    fd=fopen(filename,"r");
    if(!fd){
        perror("fopen");
        words= {
            .arr = (char **)0,
            .size = 0
        };
        return words;
    }
    size=maxline*5;
    ret=(char **)malloc(size);
    if(!ret){
        perror("malloc");
        fclose(fd);
        words= {
            .arr = (char **)0,
            .size = 0
        };
        return words;
    }
    memset(buf,0,8);
    i=0;
    while (fget(buf,7,fd)){
         size=strlen(buf);
         if(size<1)
         { 
            memset(buf,0,8);    
            continue; 
         } 
                 //mylon\n\0
         //012345
         //strlen=>6
         size--;
         buf[size]='\0';
         if(size!=5){
             memset(buf,0,8);
             continue;
         }
         ret[i][0]=buf[0];  
         ret[i][1]=buf[1];
         ret[i][2]=buf[2];
         ret[i][3]=buf[3];
         ret[i][4]=buf[4];
         memset(buf,0,8);
         n++;
         if(maxline <=n){
             break;
         }


    }
    fclose(fd);
     words= {
            .arr = ret,
            .size = i   
        };
        return words;
}

bool isin(char c, char* word){
    int i;
    bool ret;
    ret = false;
    for(i=0;i<strlen(word);i++){
        if(c==word[i]){
            ret= true;
        }
    }
    return ret;
}
int main(int, char**);

void Example_print_results(Result *results) {
   int i;
   for(i=0;i<5;i++){
       switch(results[i]){
           case ResultGreen:
               printf("%s\n","Green");
               break;
           case ResultYellow:
                printf("%s\n","Yellow");
               break;
           case ResultRed:
                printf("%s\n","Red");
               break;
            default:
                printf("Unknown Result: %d\n",results[i]);
               break;
       }
   }
   return;
}

Result cc(char guess, int idx, char *word) {
    char correct ;
    correct= word[idx];
    if(guess==correct) {   
            return ResultGreen;
    }
    if(isin(guess,word)){
            return ResultYellow;
          }
   return ResultRed; 
   
   
}
Result *cw(char *guess,char *word){
    static Result res[5];
    int i;
    for(i=0;i<5;i++){
        res[i]=cc(guess[i],i,word);
    }
    return res;
}
int main(int argc, char *argv[]) {
    char *correct, *guess;
    Result *res;
    if(argc != 3) {
        fprintf(stderr, "Usage: %s <guess> <word>\n", argv[0]);
        return -1;
    }
    correct = argv[2];
    guess = argv[1];
    res=cw(guess, correct);
    Example_print_results(res);
    return 0;
}

