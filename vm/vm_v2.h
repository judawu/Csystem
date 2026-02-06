#ifndef VM_H
#define VM_H

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

typedef uint8_t   u8;
typedef uint16_t  u16;
typedef uint32_t  u32;
typedef uint64_t  u64;
typedef  int8_t   i8;
typedef  int16_t  i16;
typedef  int32_t  i32;
typedef  int64_t  i64;
typedef  size_t  sz;

// ───────────────────────────────────────────────
// Flags (only the most important ones in phase 1)
// ───────────────────────────────────────────────
#define FLAG_CF     (1ULL <<  0)
#define FLAG_PF     (1ULL <<  2)
#define FLAG_AF     (1ULL <<  4)
#define FLAG_ZF     (1ULL <<  6)
#define FLAG_SF     (1ULL <<  7)
#define FLAG_OF     (1ULL << 11)

#define CF  (vm->r.rflags & FLAG_CF)
#define ZF  (vm->r.rflags & FLAG_ZF)
#define SF  (vm->r.rflags & FLAG_SF)
#define OF  (vm->r.rflags & FLAG_OF)

// ───────────────────────────────────────────────
// Core structures
// ───────────────────────────────────────────────
typedef struct {
    u64 rax, rbx, rcx, rdx;
    u64 rsi, rdi, rbp, rsp;
    u64 r8,  r9,  r10, r11;
    u64 r12, r13, r14, r15;
    u64 rflags;
    u64 rip;
} Registers;

typedef struct {
    Registers r;
    u8       *memory;
    sz    mem_size;         // total allocated bytes
    sz    code_break;       // end of loaded code
    u8        rex;              // current REX prefix state
} VM;

// ───────────────────────────────────────────────
// Error / halt codes
// ───────────────────────────────────────────────
typedef enum {
    VM_OK       = 0,
    VM_HALT     = 1,
    VM_SEGFAULT = 2,
    VM_BAD_OPCODE = 3,
} VmStatus;

// ───────────────────────────────────────────────
// API
// ───────────────────────────────────────────────
VM*     vm_create(sz memory_bytes);
void    vm_destroy(VM *vm);
VmStatus vm_load_program(VM *vm, const u8 *code, sz len);
VmStatus vm_run(VM *vm);
void    vm_dump_registers(const VM *vm);
void    vm_dump_memory(const VM *vm, sz bytes);

#endif