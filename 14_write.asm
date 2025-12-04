bits 32
section .data

 str: db "hello world",0xa,0
 strsize: dd 0x12
 returnaddress: resd 1 ;recieve 1 unit of 32bit ;

section .text
global _start
_start:
;cd usr/include/x86_64-linux-gnu/asm;
;vi unistd_32.h;
;write(fd,buffer,size);
;nasm -f elf64 14_write.asm;
;objdump -d 14_write.o;
;ld 14_write.o -o test;
;./test &;
;./test;
;sudo strace -f ./test;
;gdb ./test;
;gdb>disassemble _start  :access the _start;
;gdb>x/i 0x000000000040100a  :access the memory instruction x/s x/x x/b...;
;gdb>b _start  :Breakpoin;
;gdb>r         :Starting program;register;
;gdb>i r       :information of register;
;objdump -M intel  -D ./test | more;
;objdump -M intel --disassemble=main ./test;

nop
mov eax,0
mov ebx,0
mov esi,returnaddress ;before jmp, save the addresss;
mov DWORD [esi],$+0x8          ;save current address+8(after jump) to [esi]=returnaddress;
jmp write ;call write;

mov ecx,0
call print ;the same as write, but push the crrent address and return back;
mov edx,0
jmp exit ;or call exit;
nop

write:
 mov eax,4  ;write;
 mov ebx,1  ;fd;
 mov ecx,str ;buffer;
 mov esi,strsize
 mov edx,[esi];= mov edx,12 size;
 int 0x80
 
return:
 mov esi,returnaddress
 jmp [esi]  ;return back;

print:
 mov eax,4  ;write;
 mov ebx,1  ;fd;
 mov ecx,str ;buffer;
 mov esi,strsize
 mov edx,[esi];= mov edx,12 size;
 int 0x80
 ret
exit:
 mov eax,1
 xor ebx,ebx ;=mov ebx,0 ,faster, quit with 0;
 int 0x80
