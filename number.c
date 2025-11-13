#include <stdio.h>

int main() {
    int num1,num2;

    printf("Enter an integer: ");
    scanf("%d", &num1);
    printf("Enter an integer: ");
    scanf("%d", &num2);
    if (num1 > num2) {
        printf("The number %d is greater than %d.\n", num1,num2);
    } else if (num1 < num2) {
        printf("The number %d is less than %d.\n", num1,num2);
    } else {
        printf("The number %d is equal to  %d.\n", num1,num2);
    }
    float radius, area;
    const float pi = 3.14;  
    printf("Enter the radius of the circle: ");
    scanf("%f", &radius);
    area = pi * radius * radius;
    printf("The area of the circle with radius %.2f is %.4f\n", radius, area);
    return 0;
}