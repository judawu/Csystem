/*  printf2  */
#include <unistd.h>
#include <stdarg.h>

#define putchar2(x) write(1, chardup2((char)(x)), 1)
#define Wait4char 1
#define Wait4fmt  2
#define Wait4dot 3
typedef unsigned char State;


char *chardup2(char c) {
    static char buf[2];
    char *p = buf;
    *p++=c;
    *p--=0;
    return p;
}

unsigned int strlen2(const char *s) {
    unsigned int n = 0;
    while (s[n]) n++;
    return n;
}

int puts2(const char *s) {
    if (!s) return write(1, "(null)", 6);
    unsigned int n = strlen2(s);
    return write(1, s, n);
}

void printf2(const char *fmt, ...) {
    va_list ap;
    int n,i, precision;
    va_start(ap, fmt);   
    char rev[20];       
    const char *p = fmt;
    State s = Wait4char;        

    do {               
        switch (s) {
            case Wait4char:
                if (*p == '%') {
                    precision=9;
                    s = Wait4fmt;
                } else {
                    putchar2(*p);
                }
                break;
            case Wait4dot:
                if (*p =='0') p++;
                if (*p > '0' && *p <= '9') {
                    precision = *p - '0';
                    s = Wait4fmt;
                } else {
                    
                    s = Wait4char;
                }
                break;  

            case Wait4fmt:
                switch (*p) {
                    case 's':
                        puts2(va_arg(ap, const char *));
                        break;
                    case 'c':
                        putchar2(va_arg(ap, int));  
                        break;
                    case '.':  
                        
                        s = Wait4dot;
                        break;  
                        
                    case 'f':
                        {
                            double f = va_arg(ap, double);
                            if (f < 0) { putchar2('-'); f = -f; }
                            n = (int)f;
                            float frac = f - n;
                            if (n == 0) putchar2('0');                    
                            i = 0;
                            while (n > 0) {
                                rev[i++] = '0' + (n % 10);
                                n /= 10;
                            }
                            while (i--) putchar2(rev[i]);
                            putchar2('.');

                            frac = frac * 1000000;  
                            n = (int)(frac + 0.5);  
                            i = 0;
                            if (n == 0) {
                                putchar2('0');
                                putchar2('0');
                                putchar2('0');
                                putchar2('0');
                                putchar2('0');
                                putchar2('0');
                            } else {
                                while (n > 0) {
                                    rev[i++] = '0' + (n % 10);
                                    n /= 10;
                                }
                                while (i < 6) {  
                                    putchar2('0');
                                    i++;
                                }
                           
                                while (i--) {
                                    putchar2(rev[i]);
                                     precision--;
                                     if(precision<=0) 
                                        break;
                                    }
                            }
                        }
                        break;
                    case 'b':
                        putchar2('0');
                        putchar2('b');  
                        n = va_arg(ap, int);
                        if (n == 0) putchar2('0');                    
                        i = 0;
                        while (n > 0) {
                            rev[i++] = '0' + (n % 2);
                            n /= 2;
                        }
                        while (i--) putchar2(rev[i]);
                        break;
                    case 'o': {
                        putchar2('0');
                        putchar2('o');  
                        n = va_arg(ap, int);
                        if (n == 0) putchar2('0');                    
                        i = 0;
                        while (n > 0) {
                            rev[i++] = '0' + (n % 8);
                            n /= 8;
                        }
                        while (i--) putchar2(rev[i]);
                        break;
                    }
                    case 'x': {
                        putchar2('0');
                        putchar2('x');  
                        n = va_arg(ap, int);
                        if (n == 0) putchar2('0');                    
                        i = 0;
                        while (n > 0) {
                            int digit = n % 16;         
                            if (digit < 10)
                                rev[i++] = '0' + digit;
                            else
                                rev[i++] = 'a' + (digit - 10);
                            n /= 16;
                        }
                        while (i--) putchar2(rev[i]);
                        break;
                    }
                    
      
                    case 'd': {
                        n = va_arg(ap, int);
                        if (n < 0) { putchar2('-'); n = -n; }
                        if (n == 0) putchar2('0');                    
                        i = 0;
                        while (n > 0) {
                            rev[i++] = '0' + (n % 10);
                            n /= 10;
                        }
                        while (i--) putchar2(rev[i]);
                        break;
                    }
                    case '%':
                        putchar2('%');
                        break;
                    default:
                        putchar2('%');
                        putchar2(*p);
                        break;
                }
                if(s != Wait4dot) 
                    s = Wait4char;
               
                break;
        }
    }while (*p++) ;

    va_end(ap);                
}

int main() {
    printf2("Hello %s! You are %c years old? Only %d!\n", "Alice", '2', 15);
    printf2("Percent: 65%%\n");
    printf2("Null: %s\n", (char*)0);
    char *name = "Bob";
    int age = 30;
    float height = 175.001234455; 
    float score = -0.000346; 
    printf2("Name: %s, Age: %d,height: %.5f,scores:%f\n", name, age,height,score);
    printf2("number %d = %b = %o = %x \n", age, age, age, age);
    return 0;
}