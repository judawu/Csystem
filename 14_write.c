/*  prinf function system call */
#include <unistd.h>
int main(){
    /*
    libray call:
    man 3 printf 
    system call:
    man 2 write

     ssize_t write(int fd, const void buf[.count], size_t count);
     fd:
     1 stdin(keybord)
     2 stdout(screen)
     3 stderr(screen)
    debug:
    strace -f 

    
     */
write(2,"hello world\n",12);
return 0;
}