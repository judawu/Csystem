/*tree.h*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/time.h>

typedef unsigned char int8;
typedef unsigned short int16;
typedef unsigned int int32;
typedef unsigned long long int64;
typedef unsigned char Tag;

#define  TagRoot 1
#define  TagNode 2
#define  TagLeaf 4

#define NoError 0

/*sort -R wl.txt | head -1000 > wl.1000.txt */
/*
#define ExampleFile "./wl.txt"
#define ExampleFile2 "./wl.1000.txt" 
#define ExampleFile3 "./wl.5000.txt"
#define ExampleFile4 "./wl.10000.txt"
#define ExampleFile5 "./wl.50000.txt"
#define ExampleFile6 "./wl.100000.txt"
#define ExampleFile7 "./wl.200000.txt"
#define ExampleFile8 "./wl.350000.txt"
*/
typedef void* Nullptr;

Nullptr nullpointer=0;
#define find_node(x) find_node_linear(x)
#define find_last(x) find_last_leaf_linear(x)
#define find_leaf(x,y) find_leaf_linear(x,y)
#define lookup(x,y) lookup_linear(x,y)
#define reterr(x)\
 errno=(x);\
 return nullpointer;
#define Print(x) \
        memset(buf,0,256);\
        strncpy(buf,(char*)(x),255);\
        size=(int16)strlen(buf);\
        if(size) \
          write(fd,buf,size);


struct s_node{
    Tag tag;
    struct s_node *north;
    struct s_node *west;
    struct s_leaf *east;
    int8 path[256];  
};

typedef struct s_node Node;

struct s_leaf {
    Tag tag;
    union u_tree *west;
    struct s_leaf *east;
    int8 key[128];
    int8 *value;
    int16 size;
};

typedef struct s_leaf Leaf;

union u_tree{
   Node n;
   Leaf l;
};
typedef union u_tree Tree;




char *indent(int8);
Node *creat_node(Node*,int8*);
Node *find_node_linear(int8*);
Leaf *find_last_leaf_linear(Node*);
Leaf *find_leaf_linear(int8*,int8*);
Leaf *creat_leaf(Node*,int8*,int8*,int16);
int8 *lookup_linear(int8*,int8*);
void print_tree(int,Tree*);

int32 example_searches(int8*);
Tree *example_tree(void);
char *example_path(char);
int32 example_leaves(int8*);
char *stringduplicate(char*);

int test();
int example(int,char**);
int main(int,char**);