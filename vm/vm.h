#ifndef VM_V3_H
#define VM_V3_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Type aliases for clarity
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;
typedef size_t   sz;

// Flag bit positions (expanded for completeness)
#define FLAG_CF     (1ULL <<  0)  // Carry Flag
#define FLAG_PF     (1ULL <<  2)  // Parity Flag
#define FLAG_AF     (1ULL <<  4)  // Auxiliary Carry Flag
#define FLAG_ZF     (1ULL <<  6)  // Zero Flag
#define FLAG_SF     (1ULL <<  7)  // Sign Flag
#define FLAG_TF     (1ULL <<  8)  // Trap Flag
#define FLAG_IF     (1ULL <<  9)  // Interrupt Enable Flag
#define FLAG_DF     (1ULL << 10)  // Direction Flag
#define FLAG_OF     (1ULL << 11)  // Overflow Flag

// Register structure (64-bit)
typedef struct {
    u64 rax, rbx, rcx, rdx;
    u64 rsi, rdi, rbp, rsp;
    u64 r8,  r9,  r10, r11;
    u64 r12, r13, r14, r15;
    u64 rflags;
    u64 rip;
} Registers;

// VM structure
typedef struct {
    Registers r;       // Registers
    u8*       memory;  // Dynamic memory buffer
    sz        mem_size; // Total allocated memory size
    sz        code_break; // End of loaded code section
    u8        rex;     // Current REX prefix (cleared after instruction)
    bool      disasm_mode; // Optional disassembly mode (for debugging)
} VM;

// Status codes for VM operations
typedef enum {
    VM_OK          = 0,
    VM_HALT        = 1,  // HLT encountered
    VM_SEGFAULT    = 2,  // Memory access violation
    VM_BAD_OPCODE  = 3,  // Invalid opcode
    VM_STACK_OVERFLOW = 4, // Added for robustness
} VmStatus;

// API Functions
VM*      vm_create(sz memory_bytes);
void     vm_destroy(VM* vm);
VmStatus vm_load_program(VM* vm, const u8* code, sz len);
VmStatus vm_run(VM* vm);
void     vm_dump_registers(const VM* vm);
void     vm_dump_memory(const VM* vm, sz bytes);
void     vm_set_disasm_mode(VM* vm, bool enable);  // Toggle disassembly

#endif // VM_V3_H