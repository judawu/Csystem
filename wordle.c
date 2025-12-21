/*wordle.c
 cut -d " " -f1 hash.txt | sort -u >  hashno.txt
 grep -E '^.....$' wl.txt > wl.5lw.txt
sort -R wl.5lw.txt > 5l.txt
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

#define ResultGreen 1
#define ResultYellow 2
#define ResultRed 4
#define max 5000
#define ValidationOK 0
#define ValidationBadInput 1
#define ValidationNoSuchWord 2

#define ClrGreen "\033[1;32m"
#define ClrYellow "\033[1;33m"
#define ClrRed "\033[1;31m"
#define ClrStop "\033[0m"
typedef char Result;
typedef char Validation;
bool continuation,win;
int rounds;
static char words[max][64];
bool corrects[64];
typedef struct s_words Words;
bool isin(char,char*);
void prompt(bool*,unsigned char);
void print_results(Result*,char*,unsigned char);
Result cc(char, int,char*);
Result *cw(char*,char*);
char *randomword(int,unsigned char);
void seed();

Validation validator(char*,unsigned char);
char *readline(void);
void gameloop(char*,unsigned char);
int readfile(char*,unsigned char);
int test(int,char**);
int main(int,char**);



bool isin(char c,char*str){
    bool ret;
    int i,size;
    ret=false;
    size=strlen(str);
    for(i=0;i<size;i++)
      if(str[i]==c){
             ret=true;
             break;
      };  
   return ret;

};
void prompt(bool *correctchar,unsigned char charnum){
   int i;
    for(i=0;i<charnum;i++){
      switch(correctchar[i]){
        case false:    
          printf("-");
          break;
        case true:
           printf("+");
            break;
      }};
      printf("\n\n%d> ",charnum-rounds);
      fflush(stdout);
      return;
};
void print_results(Result *res,char *guess,unsigned char charnum){
    int i;
    for(i=0;i<charnum;i++){
      switch(res[i]){
        case ResultGreen:
        //  printf("%s\n","Green");
          printf("%s%c%s",ClrGreen,guess[i],ClrStop);
          break;
        case ResultYellow:
         // printf("%s\n","Yellow");
          printf("%s%c%s",ClrYellow,guess[i],ClrStop);
          break;
        case ResultRed:
        //  printf("%s\n","Red");
          printf("%s%c%s",ClrRed,guess[i],ClrStop);
          break;
        default:
         printf("Unknow: %d\n",res[i]);
         break;
      }};
    printf("\n");
     return;
};


Result cc(char guess, int idx,char *word){
    char correct;
    correct=word[idx];
    if (guess==correct){
        corrects[idx]=true;
         return ResultGreen;
    }  
    else if(isin(guess,word))
         return ResultYellow; 
    else 
          return ResultRed;
};

Result *cw(char *guess,char *word){
   static Result res[64];
   int i,size;
   size=strlen(word);
   for(i=0;i<size;i++){
     res[i]=cc(guess[i],i,word);
   };
   return res;
};

char *readline(){
  static char  buf[64];
  int size;
  memset(buf,0,64);
  fgets(buf,63,stdin);
  size=strlen(buf);
  assert(size);
  size--;
  buf[size]=0;
  return buf;
};

Validation validator(char *word,unsigned char charnum){
  int n,i;
  bool ret;
  bool strcmp_(char *str1,char *str2){
        int i;
        bool ret=true;
        for(i=0;i<charnum;i++,str1++,str2++){
            if(*str1!=*str2){
                     ret=false;
                     break;
            }             
        }
        return ret;

  };
  n=strlen(word);
  if(n!=charnum)
    return ValidationBadInput;

  ret=false;
  for(i=0;i<max;i++){
     n=strcmp_(words[i],word);
     if(n){
      ret=true;
      break;
     }
  }
  if(ret)
   return ValidationOK;
  else
   return ValidationNoSuchWord;
    
};

int readfile(char *filename,unsigned char charnum){
    char buf[64];
    int i,size;
    FILE *fd;
    fd=fopen(filename,"r");
    if(!fd)
      {
        perror("fopen()"); 
        return -1;
      };
    i=0;
    memset(buf,0,8);
    while(fgets(buf,63,fd)){
         size=strlen(buf);
         if(size<1){
            memset(buf,0,64);
            continue;
         }
         size--;
         if(size != charnum){
            memset(buf,0,64);
            continue;
         }
         buf[size]=0;
         strncpy(words[i],buf,size);
         memset(buf,0,64);
         i++;
         if(max<=i){
            break;
         };
    };
   fclose(fd);

    return i;

};
void seed(){
  int x;
  x=getpid();
  srand(x);
  return;
};

char *randomword(int maxnum,unsigned char charnum){
  int  x,i;
  static char ret[64];
  assert(charnum<64);
  x=rand() % maxnum;
  assert(x>0);
  for(i=0;i<charnum;i++){
     ret[i]=words[x][i];
  }
  ret[charnum]=0;
  return ret;
};


void gameloop(char *correct,unsigned char charnum){
     char *input;
     Result *res;
     Validation valres;
     int i,c;
   
     prompt(corrects,charnum);
     input=readline();
     valres=validator(input,charnum);
     switch (valres)
     {
     case ValidationBadInput:
      printf("bad input,only support %s%d%s letters\n",ClrGreen,charnum,ClrStop);
      return;
     case ValidationNoSuchWord:
      printf("No such word,only support %s%d%s letters in the lists\n",ClrGreen,charnum,ClrStop);
      rounds++;
      return;
     default:
      break;
     };

     res=cw(input,correct);
     for(i=c=0;i<charnum;i++)
       if(corrects[i]==true)
         c++;
    print_results(res,input,charnum);
    if (c==charnum){
      win =true;
      continuation=false;
      return;
    }
    rounds++;
    if(rounds>charnum)
    {
       win =false;
       continuation=false;
       return;
    };
     };



int main(int argc,char *argv[]){
    char *infile,*p;
    int n,charnum;
    seed();
    rounds=0;
    win=false;
    
    for(n=0;n<64;n++)
       corrects[n]=false;
    
     if (argc<2){
        fprintf(stderr,"usgage: %s%s infile%s\n ",ClrRed,*argv,ClrStop);
        return -1;
    }
    infile = argv[1];
    printf("%sLet's play the game\n\n12the game is to guess a password from the password book%s\n",ClrGreen,ClrStop);
 
    printf("%splease input the number of letters your want to guess%s\n",ClrYellow,ClrStop);
    charnum=atoi(readline());
    assert(charnum>0);
    assert(charnum<64);
    n=readfile(infile,charnum);
   
   
    assert(!(n<=0));
    printf("you have crete a passowrd list for %d letters length from the file of %d words\n",charnum,n);
    printf("let's random pick a word from the list\n");
    p=randomword(n,charnum);
    printf("%sDebug:correct word is %s%s\n",ClrRed,p,ClrStop);
    n=strlen(p);
    printf("the word is %s%d%s letters, please guess\n\n\n> ",ClrYellow,n,ClrStop);
    fflush(stdout);
    continuation=true;
    while(continuation){
      gameloop(p,charnum);
    };
  

    printf("Aha, the correct word was '%s%s%s'\n",ClrGreen,p,ClrStop);
    if(win)
        printf("%sCongratulations,you won the game.%s\n",ClrGreen,ClrStop);
    else
        printf("%sUnluckly,you lose the game.%s\n",ClrRed,ClrStop);
    return 0;
}
