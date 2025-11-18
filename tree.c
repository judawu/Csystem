// Online C compiler to run C program online
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

typedef struct Node
   {
     int number;
     struct Node *left;
     struct Node *right;
   } node;

void freenode(node *root);

void printnode(node *root);
void removeleftnode(node *root);
void removerightnode(node *root);
node* addnode(node *root,int n);
void printtree(node *root);
void freetree(node *root);
int addtree(node **root,int n);
bool searchtree(node *root,int n);
int main(void)
{

  node *tree=NULL;
   addtree(&tree,1);
   addtree(&tree,3);
  
   tree=addnode(tree,2);
     printtree(tree);
    
   tree=addnode(tree,5);

   tree=addnode(tree,7);
 
   tree=addnode(tree,9);
  tree=addnode(tree,20);
   tree=addnode(tree,2);
   tree=addnode(tree,4);
  tree=addnode(tree,11);
   tree=addnode(tree,8); 
   tree=addnode(tree,15);
   tree=addnode(tree,6);
//  printnode(tree);
   searchtree(tree,5);
   searchtree(tree,7);
  tree=addnode(tree,6);
 // freenode(tree);
 // tree=NULL;
  freetree(tree);
  tree=addnode(tree,1);
  tree=addnode(tree,8);
  tree=addnode(tree,5);
  tree=addnode(tree,7);
  tree=addnode(tree,4);
  tree=addnode(tree,10);
  tree=addnode(tree,9);
tree=addnode(tree,11);
  tree=addnode(tree,6);
 // printnode(tree);
//removeleftnode(tree->right);
 removerightnode(tree);
 // printnode(tree);
  printtree(tree);
    return 0;
}


node* addnode(node *root,int n)
{
    node *p=malloc(sizeof(node));
        if (p==NULL)
        {
            return NULL;
        }
       p->number=n;
       p->left=NULL;
       p->right=NULL; 
    if (root==NULL)
      {
    
       return p;
      }
    else
     {
         if(root->number < n)
          {   
            root->right= addnode(root->right,n);
          
            
           
             
            
             
          }
         else
          { 
              root->left= addnode(root->left,n);
           
            
           
          }
       return root;   
        
     }
}

void freenode(node *root)
{
       if (root==NULL)
      { return;}
     if(root->left!=NULL)
     {   
         root=root->left;
         freenode(root->left);
     }
     if(root->right!=NULL)
     {  
        root=root->right;
        freenode(root->right);
     }
  //   printf("free");
     free(root);
   
   
    return;
}

int addtree(node **root,int n)
{
      node *p=malloc(sizeof(node));
        if (p==NULL)
        {
            return 1;
        }
       p->number=n;
       p->left=NULL;
       p->right=NULL; 
       if (*root==NULL)
      { *root=p;
          return 0;
      }
      if ((*root)->number<n)
       {return addtree(&(*root)->left,n);}
       else
        {return addtree(&(*root)->right,n);}
        
     
}

bool searchtree(node *root,int n)
{
       if (root==NULL)
      { return false;}
      else if (n<root->number)
        {return searchtree(root->left,n);}
      else if (n>root->number)
        {return searchtree(root->right,n);}
     else if (n==root->number)
        {  
            printf("%i found in tree\n",n);
            return true;}
     else
        {  printf("%i not found in tree\n",n);
            return false;
            
        }
}
void freetree(node *root)
{
       if (root==NULL)
      { return;}
        freetree(root->left);
        freetree(root->right);
        free(root);
       return;
}

void printnode(node *root)

{
       if (root==NULL)
      {
          printf("NULL");
          return;}
   
    
     if(root->left!=NULL)
     { //  printf("parent is %i left child is %i\n",root->number,root->left->number);
         printnode(root->left);
       
     }
      if(root->right!=NULL)
     {     
         //printf("parent is %i right child is %i\n",root->number,root->right->number);
         printnode(root->right);
        
     }
    
     
    return;
}

void printtree(node *root)

{
       if (root==NULL)
      {
          
          return;}
   
    
 
         printtree(root->left);
         printf("number is %i\n",root->number);
         printtree(root->right);
        
    
    
     
    return;
}

void removeleftnode(node *root)

{
    
    if (root==NULL)
      { return;}
     if(root->left!=NULL)
     {   
        node *p=root->left;
        root->left=NULL;
        if(p->right!=NULL)
          {root->left=p->right;
              node *q=p->right;
              while(q->left !=NULL)
              {
                  q=q->left;
                  
              }
              q->left=p->left;
              
          }
         else if (p->left!=NULL)
         {root->left=p->left;}
           
        free(p);
     }
      return;
}
void removerightnode(node *root)

{
    
    if (root==NULL)
      { return;}
     if(root->right!=NULL)
     {   
        node *p=root->right;
        root->right=p->left;
        node *q=p->left;
        while (q->right!=NULL)
        {q=q->right;}
        q->right=p->right;
        free(p);
     }
      return;
}

