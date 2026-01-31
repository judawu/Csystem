/*vm*/
#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <errno.h>
#include <inttypes.h>

#define NoErr   0x00
#define Syshlt  0x01
#define ErrMem  0x02
#define ErrSegv 0x04



typedef unsigned char u8;
typedef unsigned short int u16;
typedef unsigned int u32;
typedef unsigned long long int u64;

typedef char i8;
typedef short int i16;
typedef int i32;
typedef long long int i64;

/*
16bit VM

Register:AX,BX,CX,DX,SP,IP...
65KB Memory
No Serial COM port
No fLoppy drive

*/
typedef u8 Errorcode;
#define segfault(x) errorVM((x),ErrSegv)
#define Syshltfault(x) errorVM((x),Syshlt)

// 标志位宏（简化）
#define CF (vm->rflags & (1ULL << 0)) //Carry flag
#define PF (vm->rflags & (1ULL << 2)) //Parity Flag
#define AF (vm->rflags & (1ULL << 4)) //Auxiliary Carry Flag
#define ZF (vm->rflags & (1ULL << 6)) // Zero Flag
#define SF (vm->rflags & (1ULL << 7)) // Sign Flag
#define TF (vm->rflags & (1ULL << 8)) // Trap Flag
#define IF (vm->rflags & (1ULL << 9)) // Interrupt Enable Flag
#define DF (vm->rflags & (1ULL << 10)) // Direction Flag
#define OF (vm->rflags & (1ULL << 11)) // Overflow Flag
#define IOPL (vm->rflags & (3ULL << 12)) //I/O Privilege Level
#define NT (vm->rflags & (3ULL << 14)) //Nested Task
#define RF (vm->rflags & (1ULL << 16)) //Resume Flag
#define VMODE (vm->rflags & (1ULL << 17)) //Virtual-8086 Mode
#define AC (vm->rflags & (1ULL << 18)) //Alignment Check
#define VIF (vm->rflags & (1ULL << 19)) //Virtual Interrupt Flag
#define VIP (vm->rflags & (1ULL << 20)) //Virtual Interrupt Pending
#define ID (vm->rflags & (1ULL << 21)) //ID Flag

// 寄存器名字表（64位）
static const char* reg64[16] = {
    "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
    "r8",  "r9",  "r10", "r11", "r12", "r13", "r14", "r15"
};

// 静态缓冲区，用于返回字符串（线程不安全，如果多线程请改用线程局部或动态分配）
static char disasm_buf[256];

typedef u64 Reg;

struct s_registers{
    Reg RAX;
    Reg RBX;
    Reg RCX;
    Reg RDX;
    Reg RSI; //Source Index
    Reg RDI; //Destination Index
    Reg RBP; //Base Pointer/frame pointer
    Reg RSP; //Stack Pointer
    Reg R8;
    Reg R9;
    Reg R10;
    Reg R11;
    Reg R12; 
    Reg R13; 
    Reg R14; 
    Reg R15;
    Reg RFLAGS;
    Reg RIP;  //Instrction Pointer
};
typedef struct s_registers Registers;
#define rax c.r.RAX
#define rbx c.r.RBX
#define rcx c.r.RCX
#define rdx c.r.RDX
#define rsi c.r.RSI
#define rdi c.r.RDI
#define rflags c.r.RFLAGS
#define rbp c.r.RBP
#define rsp c.r.RSP
#define rip c.r.RIP
#define r8 c.r.R8
#define r9 c.r.R9
#define r10 c.r.R10
#define r11 c.r.R11
#define r12 c.r.R12
#define r13 c.r.R13
#define r14 c.r.R14
#define r15 c.r.R15

struct s_cpu
{
    Registers r;
};
typedef struct s_cpu CPU;


typedef u8 Memory[(u16)(-1)];
typedef Memory *Stack;

typedef u8 Program;

enum e_opcode{
    NOP = 0x90,
    HLT = 0xF4,
    RET = 0xC3,
    CALL = 0xE8,
    INT = 0xCD,
    JC = 0x72,
    JNC = 0x73,
    JZ = 0x74,
    JNZ = 0x75,
    JBE = 0x76,
    JA = 0x77,
    JL = 0x7C,
    JGE = 0x7D,
    JLE = 0x7E,
    JG = 0x7F,
    JMP = 0xE9,
    JMPR = 0xEB,
    MOV = 0x89,//0x8B
    MOVR = 0x8B,//0x8B
    MOVD= 0xC7,
    MOV1=0xB8,
    MOV2=0xB9,
    MOV3=0xBA,
    MOV4=0xBB,
    MOV5=0xBC,
    MOV6=0xBD,
    MOV7=0xBE,
    MOV8=0xBF,
    CMP = 0x38, 
    CMP1 = 0x39,
    CMP2 = 0x3A, 
    CMP3 = 0x3B,
    CMP4 = 0x3D,
    ADD = 0x00, 
    ADD1 = 0x01, 
    ADD2 = 0x02, 
    ADD3 = 0x03,
    ADD4 = 0x05,
    SUB = 0x28, 
    SUB1 = 0x29, 
    SUB2=0x2B,
    SUB3=0x2D,
    XOR = 0x30,
    XOR1 = 0x31,
    XOR2 = 0x32,
    XOR3 = 0x33,
    XOR4 = 0x35,
    AND = 0x20, 
    AND1 = 0x21, 
    AND2=0x22,
    AND3=0x23,
    AND4=0x25,
    OR = 0x08,
    OR1 = 0x09,
    OR2 = 0x0A,
    OR3= 0x0B,
    OR4=0x0D,
    NOT = 0xF7, 
    PUSH = 0x50,
    PUSH1=0x51,
    PUSH2=0x52,
    PUSH3=0x53,
    PUSH4=0x54,
    PUSH5=0x55,
    PUSH6=0x56,
    PUSH7=0x57,
    PUSH8=0x68,
    POP = 0x58, //58~5F）
    POP1= 0x59,
    POP2= 0x5A,
    POP3= 0x5B,
    POP4= 0x5C,
    POP5= 0x5D,
    POP6= 0x5E,
    POP7= 0x5F,
    XCHG = 0x87,
    SHLR= 0xC1, //C1
    LEA = 0x8D,
    PUSHF=0x9C,
    POPF=0x9D,
    SAF=0x9E,
    LAF=0X9F,
    INC = 0xFF,
    LOOPNE = 0xE0,
    LOOPE = 0xE1,
    LOOP = 0xE2,
    IMMOP1 = 0x81,
    IMMOP2 = 0x83,
    BT= 0xA3,

    MOVZX = 0xB6,
    MOVZY = 0xB7,
   
};
typedef enum e_opcode Opcode;

struct s_instruction{
    Opcode  o;
    u8 instrlen;
    u8 bytes[]; 
};
typedef struct s_instruction Instruction;


// 定义一个指令表结构(用于反编译）)
typedef struct {
    Instruction **instructions;  // 指针数组
    size_t count;                // 当前指令数量
    size_t capacity;             // 已分配容量
} InstructionTable;



struct s_vm {
    CPU c;
    Memory m;
    size_t b;  /*break line  offset*/
    u8 rex; //rex flags
    InstructionTable *itab;
};
typedef struct s_vm VM;


void zero(u8*, size_t);
void copy(u8*, u8*, size_t);
void printhex(u8*, size_t,char);


InstructionTable *createInstrTable(void);
void addInstruction(InstructionTable*, Instruction*);
Instruction *getInstruction(InstructionTable*, size_t);
void printInstrTable(InstructionTable*);
const char* disasmInstruction(const Instruction*,u64);
void disasmInstrTable(InstructionTable*);
void freeInstrTable(InstructionTable*);

VM *creatVM();
void removeVM(VM*);
void errorVM(VM*,Errorcode);
Reg* getRegister(VM*, u8, u8);
void* getRmOperand(VM*, u8, u8, u8*, u8*, u8); 
void executeInstruction(VM*);
void executeVM(VM*);
void printReg(VM*);
void printVM(VM*);


Program *exampleprogram(VM*);
int main(int,char**);




