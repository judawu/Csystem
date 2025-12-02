bits 32
section .data

 str: db "hello world",0xa,0

section .text
global _start
_start:
;write(fd,buffer,size);
;nasm -f elf64 14_write.asm;
;ld 14_write.o -o test;
;./test &;
;./test;
;sudo strace -f ./test;


write:
 mov eax,4  ;write;
 mov ebx,1  ;fd;
 mov ecx,str ;buffer;
 mov edx,12  ;size;
 int 0x80
exit:
 mov eax,1
 mov ebx,5
 int 0x80
