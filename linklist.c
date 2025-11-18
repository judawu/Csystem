#include <stdio.h>
#include <stdlib.h>


typedef struct Node
   {
     int number;
     struct Node *next;
   } node;
int addnode(node *list,int n);
int freelist(node *list);
node* removefirstnode(node *list);
int removelastnode(node *list);
int removenumber(node *list,int n);

int main(void)

{

  // CREATE LIST POINTER
    node *list=NULL;

 //ASGIN MEM FOR TEMP n
    node *n=malloc(sizeof(node));
    if (n==NULL)
    {
        
        return 1;
    }

    //INIT N
    //(*n).number=1;
    n->number=1;
    n->next=NULL;
//update list
 list=n;
    n=malloc(sizeof(node));
    if (n==NULL)
    {
        free(list);
        return 1;
    }

  
    n->number=2;
    n->next=NULL;
    list->next=n;
    
      n=malloc(sizeof(node));
    if (n==NULL)
    {
        free(list->next);
        free(list);
        return 1;
    }

 
    n->number=3;
    n->next=NULL;
    list->next->next=n;
    addnode(list,4);

 //pirnt list
  printf("the list number is %i, and the next list number is %i\n",list->number,list->next->number);
  
      addnode(list,5);
      addnode(list,6);
      addnode(list,7);
      addnode(list,8);
      addnode(list,9);
      addnode(list,10);
      removelastnode(list);
      list= removefirstnode(list);
      removenumber(list,5);
    
 //pinrt node 
  for (node *tmp=list;tmp!=NULL;tmp=tmp->next)
  {
      printf("the list number is %i\n",tmp->number); 
  }
    return 0;
}


int addnode(node *list,int n)
{
    node *p=malloc(sizeof(node));
    if (p==NULL)
    {
        
        return 1;
    }

  
    p->number=n;
    p->next=NULL;

 for (node *tmp=list;tmp!=NULL;tmp=tmp->next)
  {
      if (tmp->next == NULL)
      {
       tmp->next=p;
   
      return 0;
      }
  }
}

int freelist(node *list)

{
    while(list->next==NULL)
    {
        node *tmp=list->next;
        free(list);
        list=tmp;
    }
    return 0;
}

int removenumber(node *list,int n)

{
   
    
    if(list==NULL)
       {return 1;}
    
 
    node *tmp=list;
     node *tmp1;   
        
      
        do
        {
         
             if(tmp->next->number==n)
           { 
               //printf("found %i",tmp->next->number);
          tmp1=tmp->next;
          tmp->next= tmp->next->next;
        free(tmp1);
           return 0;
           }
           tmp=tmp->next;
            
        }
        while(tmp->next!=NULL);   
        
}

node* removefirstnode(node *list)

{
    
    if(list==NULL)
       {return NULL;}
     if(list->next==NULL)
     {
        free(list);
        return NULL;
     }
        node *tmp=list->next;
        free(list);
      //  list=tmp;
       
        return tmp;
        
    
}

int removelastnode(node *list)

{
     
        node *tmp=list;
        if(tmp==NULL)
          {
              return 1;
              
          }
        
        if(tmp->next==NULL)
          {
              free(list);
              
          }
       while(tmp->next->next !=NULL)
         {
             tmp=tmp->next;
            // printf("found");
         }
        free(tmp->next);
         tmp->next=NULL;
         
    return 0;
}
