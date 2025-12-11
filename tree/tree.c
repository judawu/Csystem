/*tree.c*/
#include "tree.h"


Tree root={
    .n={
     .tag= (TagRoot|TagNode),
     .north=(Node*)&root,
     .west=0,
     .east=0,
     .path="/"
    }

};


char *indent(int8 n){
    static char buf[256];
    if (n<1)
       return "";
    assert(n<128);
    memset(buf,0,256);  
    memset(buf, ' ', n);
    return buf;
};

void print_tree(int fd,Tree *_root){

    int8 indentation;
    char buf[256];
    int16 size;
    Node *n;
    Leaf *l;
    indentation=0;
    for (n=(Node*)_root;n;n=n->west){
        Print(indent(indentation++));
        Print(n->path);
        Print("\n");
        
        if(n->east){      
             for(l=find_last(n);l!=(Leaf*)n ;l=l->west){ 
           // for(l=n->east;l;l=l->east){
                    Print(indent(indentation));
                    Print(n->path);
                    Print("/");
                    Print(l->key);
                    Print("->");       
                    write(fd,(char*)l->value,(int)l->size);
                    Print("\n");
            };
         
          
        }
         
    }

};
Node *creat_node(Node *parent,int8 *path){
    Node *n;
    int16 size;
    errno=NoError;
    assert(parent);
    size= sizeof(Node);
    n=(Node*)malloc(size);
    memset((int8*)n,0,size);
    parent->west=n;
    n->tag=TagNode;
    n->north=parent;
    strncpy((char*)n->path,(char*)path,255);
    return n;   

};
Node *find_node_linear(int8 *path){
    Node *p,*ret;
    ret=(Node*)0;
    for(p=(Node*)&root;p;p=p->west){
        if(!strcmp((char*)p->path,(char*)path)){
            ret=p;
            break;
        };
    };
    return ret;
};

Leaf *find_leaf_linear(int8 *path,int8 *key){
   Node *n;
   Leaf *l,*ret;
   n=find_node(path);
   ret=(Leaf*)0;
   if(n){
     for(l=n->east;l;l=l->east){
    if(!strcmp((char*)l->key,(char*)key)){
         ret=l;
         break;
      }
     }
       };
   
   return ret;
};
int8 *lookup_linear(int8 *path,int8 *key){
    Leaf *p;
    p=find_leaf(path,key);
    return (p)?p->value:(int8*)0;
};
Leaf *find_last_leaf_linear(Node *parent){
    Leaf *l;
    errno=NoError;
    assert(parent);
    if(!parent->east){
       return (Leaf*)0;
    }
    for(l=parent->east;l->east;l=l->east);
    assert(l);
    return l;

};

Leaf *creat_leaf(Node *parent,int8 *key,int8 *value,int16 count){
    Leaf *l,*new;
    int16 size;
    assert(parent);
    l=find_last(parent);
    size=sizeof(Leaf);

    new=(Leaf*)malloc(size);
    assert(new);
    memset((int8*)new,0,size);
    new->tag=TagLeaf;
    
    if(!l){
         parent->east=new;
         new->west=(Tree*)parent;
    }else{
        l->east=new;
        new->west=(Tree*)l;
    };
    strncpy((char*)new->key,(char*)key,127);
    new->value=(int8*)malloc(count);
    assert(new->value);
    memset(new->value,0,count);
    strncpy((char*)new->value,(char*)value,count);
    new->size=count;

    return new; 
};


Tree *example_tree(){
    int8 c;
    Node *n,*p;
    char path[256];
    int x;
    memset(path,0,256);
    x=0;
    
    for(n=(Node*)&root,c='a';c<='z';c++){
         x=strlen(path);
         *(path+ x++)='/';
         *(path+x)=c;
        // printf("%s\n",path);
         p=n;
         n=creat_node(p,(int8*)path);

    };
    return (Tree *)&root;

};
char *example_path(char path){
    static char buf[256];
    char c;
    int32 x;
    memset(buf,0,256);
    for(c='a';c<=path;c++){    
      x=(int32)strlen(buf);
      *(buf+x++)='/';
      
      *(buf+x)=c;
    }
    return buf;
};
char *stringduplicate(char *str){
    static char buf[256];
    size_t n;
    memset(buf,0,256);
    n=strlen(str);
    if(n>128)
        n=128;
    strncpy(buf,str,n);
    strncpy(buf+n,str,n);
    memset(buf+255,'\0',1);
    return buf;
}


int32 example_leaves(int8 *file){
   // int fd;
     FILE *fd;
    char buf[256];
    char *path,*val;
    int32 x,y;

    Node *n;
  

 //   fd=open(ExampleFile,O_RDONLY);
    fd= fopen(file,"r");
    assert(fd);
    memset(buf,0,256);
    y=0;
    //while((x=read(fd,buf,255))>0){
       while (fgets(buf,255,fd)){
        x=(int32)strlen(buf);
        *(buf+x-1)=0;
         path=example_path(*buf);
         n=find_node((int8*)path);
         if(!n){
            memset(buf,0,256);
            continue;
         }
         val=stringduplicate(buf);
/*
         printf("\n");
         printf("node==='%s'\n",n->path);
         printf("buf==='%s'\n",buf);
         printf("val==='%s'\n",val);
          printf("len==='%d'\n",(int16)strlen(val));
          printf("\n");
*/
         creat_leaf(n,(int8*)buf,(int8*)val,(int16)strlen(val));
           
         y++;
         memset(buf,0,256);

    };
    fclose(fd);
   return y;

  
};
int32 example_searches(int8 *file){
    FILE *fd;
    char buf[64],*path;
    int32 n;
    int16 size;
    int8  *value;
   
    n=0;
    fd=fopen(file,"r");
    assert(fd);
    memset(buf,0,64);
   while(fgets(buf,63,fd)>0){
     size=(int16)strlen(buf);
     assert(size);
     size--;
     buf[size]=0;
     path=example_path(*buf);
     value=lookup((int8*)path,(int8*)buf);
     if(value){
               printf("%s->'%s'\n",buf,value);
               fflush(stdout);
               n++;
     };  
    memset(buf,0,64);
   };
    printf("trst");
    fclose(fd);
    return n;
};
int test(){
     Node *n, *n2;
     Leaf *l1,*l2, *l3;
     int8 *key, *value;
     int16 size;
   


    n=creat_node((Node*)&root,(int8*)"/Users");
    assert(n);
    n2=creat_node(n,(int8*)"/Users/login");
    assert(n2);


    key=(int8*)"user1";
    value=(int8*)"youareluck";
    size=(int16)strlen((char*)value);
    l1=creat_leaf(n2,key,value,size);
    assert(l1);
 // printf("%s\n",l1->value);
     key=(int8*)"user2";
     value=(int8*)"youarenotluck";
     size=(int16)strlen((char*)value);
     l2=creat_leaf(n2,key,value,size);
     assert(l2);

     key=(int8*)"user3";
     value=(int8*)"luck";
     size=(int16)strlen((char*)value);
     l3=creat_leaf(n2,key,value,size);
     assert(l3);
  
     print_tree(1,&root);


   // printf("%p",find_node((int8*)"/Users/login"));
   int8 *test=lookup((int8*)"/Users/login",(int8*)"user3");
   printf("%s",test);
    free(l2);
    free(l1);
    free(n2);
    free(n);
   return 0;
}

int example(int argc,char *argv[]){
    Tree *example;
    int32 x,y;
    float ms;

        /*int getrusage(int who, struct rusage *usage);*/
     struct rusage usage;
    // struct timeval time;
    int *file,*subfile;

    if(argc <3){
        fprintf(stderr,"usage %s FILE SUBFILE <./tree wl.50000.txt  wl.5000.txt>\n",*argv);
         return -1;
    }
      
    
    file=(int8*)argv[1];
    subfile=(int8*)argv[2];

    example=example_tree();
    x=example_leaves(file);
   // printf("total line : %d\n",x);
   (void)x;
   (void)example;
 
   if(fork())
      wait(0);
    else{
        example_searches(subfile);
        exit(0);
    }
 
  

   // print_tree(1,example);
    y=getrusage(RUSAGE_SELF,&usage);
    if(y)
      perror("getruseage()");
     else{
     printf("ret:\t%d\n",y);
     ms=(float)usage.ru_utime.tv_sec+(float)usage.ru_utime.tv_usec/1000000;
     printf("useage time:\t%.4f seconds\n",ms);

     };
    return 0;
};
int main(int argc,char *argv[]){
    /* ./tree | more*/
    //test();
    example(argc,argv);
    return 0;
}