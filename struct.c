/*struct.c */
#include <stdio.h>
#include <string.h>

struct Person {
    char title[8];
    char lastname[32];
    char firstname[32];
    char email[64];
    int age;
};

int main() {
    struct Person Juda;
    strncpy(Juda.title, "President.",8);    
    strncpy(Juda.lastname, "JUDA",32);    
    strncpy(Juda.firstname, "US",32);    
    strncpy(Juda.email, "juda@us,com",64);    
    Juda.age = 45;  
    
    printf("Name: %s %s %s\n", Juda.title, Juda.firstname, Juda.lastname);
    printf("Email: %s\n", Juda.email);
    printf("Age: %d\n", Juda.age); 
    
    return 0;
}