#include "vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
// ───────────────────────────────────────────────
// Helper Functions
// ───────────────────────────────────────────────
static inline u64 read_le64(const u8* p) {
return ((u64)p[0]) | ((u64)p[1] << 8) | ((u64)p[2] << 16) | ((u64)p[3] << 24) |
((u64)p[4] << 32) | ((u64)p[5] << 40) | ((u64)p[6] << 48) | ((u64)p[7] << 56);
}
static inline u32 read_le32(const u8* p) {
return ((u32)p[0]) | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}
static inline u16 read_le16(const u8* p) {
return ((u16)p[0]) | ((u16)p[1] << 8);
}
static inline i8 read_disp8(const u8* p) {
    return (i8)*p;
}

//memory functuon 
static inline u8* copy(void *dst, const void *src, size_t size){
    u8 *d=(u8*)dst;
    u8 *s=(u8*)src;
    if(size==0 || d==s) return dst;
    if (d < s || s >= s + size) {
        // 非重叠区域，直接从前往后复制
        while(size--){
           *d++ = *s++;
        }
    } else {
        // 重叠区域，从后往前复制
        d+=size-1;
        s+=size;
        while(size--){
           *d-- = *s--;
        }
    }
    return dst;
    };
  
static inline void zero(void *src, size_t size){ 
    u8 *p=(u8*)src;
    while(size--){
        *p++=0;
    }
    return;
    };

static inline void printhex(u8 *data, size_t size,char sep){
    size_t i;
    for(i=0; i<size; i++){
        printf("%02x%c", *data++,sep);
    }
    printf("\n");
    return;
    };


// for 16-bit mode physical address calculation (segment:offset)
static inline u32 get_physical_address(const VM* vm, u16 segment, u16 offset) {
    u32 phys = ((u32)segment << 4) + (u32)offset;
    return phys & 0xFFFFF;  // Classic 8086 wrap-around to 20-bit (1 MB)
}

static inline u8* get_memory_ptr(VM* vm, u16 segment, u16 offset, sz size) {
    u32 phys = get_physical_address(vm, segment, offset);
    if (phys + size > vm->mem_size) return NULL;  // Segfault
    return vm->memory + phys;
}

// Get general-purpose register by index (0-15)
static inline u64* get_gpr(VM* vm, u8 idx) {
    if (idx >= 8 && vm->mode != MODE_64BIT) return NULL;
    switch (idx & 0xF) {
    case 0: return &vm->r.rax; case 1: return &vm->r.rcx;
    case 2: return &vm->r.rdx; case 3: return &vm->r.rbx;
    case 4: return &vm->r.rsp; case 5: return &vm->r.rbp;
    case 6: return &vm->r.rsi; case 7: return &vm->r.rdi;
    case 8: return &vm->r.r8;  case 9: return &vm->r.r9;
    case 10: return &vm->r.r10; case 11: return &vm->r.r11;
    case 12: return &vm->r.r12; case 13: return &vm->r.r13;
    case 14: return &vm->r.r14; case 15: return &vm->r.r15;
default: {
    printf("Invalid REG index: %d\n", idx);
    return NULL;}
}
}
static inline u8* get_reg_ptr(VM* vm, u8 idx, sz size) {
    if (vm->mode != MODE_16BIT || size != 1) {
        return (u8*)get_gpr(vm, idx);  // Normal for non-16-bit or word/dword
    }
    // 16-bit mode byte ops: 0-3 = low byte, 4-7 = high byte of A/C/D/B
    u64* base;
    u8 offset = 0;
    if (idx >= 4) {
        idx -= 4;
        offset = 1;  // High byte
    }
    switch (idx) {
        case 0: base = &vm->r.rax; break;
        case 1: base = &vm->r.rcx; break;
        case 2: base = &vm->r.rdx; break;
        case 3: base = &vm->r.rbx; break;
        default: return NULL;
    }
    return ((u8*)base) + offset;
}
// Determine operand size based on mode, prefixes, and REX.W
static inline sz get_op_size(VM* vm, bool is_byte_op, bool operand_override) {
if (is_byte_op) return 1;
i32 bits = (vm->mode == MODE_16BIT) ? 16 : 32;
if (operand_override) {
bits = (bits == 16) ? 32 : 16;
}
if (vm->mode == MODE_64BIT && (vm->rex & 0x8)) {
bits = 64;
}
return bits / 8;
}
// Determine operand size based on mode, prefixes, and REX.W
static inline u64 get_op_value(u8* ptr, sz size) {
    switch (size) {
    case 1: return *(u8*)ptr;
    case 2: return *(u16*)ptr;
    case 4: return *(u32*)ptr;
    case 8: return *(u64*)ptr;
    default: return 0;
    }
}

// Set value to register/memory with size masking (sign/zero extend if needed)
static inline void set_op_value(u8* ptr, u64 val, sz size) {
    switch (size) {
        case 1: *(u8*)ptr = (u8)val; break;
        case 2: *(u16*)ptr = (u16)val; break;
        case 4: *(u32*)ptr = (u32)val; break;
        case 8: *(u64*)ptr = val; break;
    }
}

// Zero upper bits of a register after a sub-64-bit write
static inline void zero_upper_register_bits(VM* vm, u8* ptr, sz size) {
        if (size >= 8) return;
        u8* reg_start = (u8*)&vm->r.rax;
        u8* reg_end   = (u8*)&vm->r.rflags;
        u64 p = (u64)ptr;
        u64 s = (u64)reg_start;
        if (p >= s && p < (u64)reg_end && (p - s) % 8 == 0) {
        u64* reg = (u64*)ptr;
        u64 mask = (1ULL << (size * 8)) - 1;
        *reg &= mask;
}
}

static bool calc_ea_16bit(VM* vm, u8 mod, u8 rm, const u8** pc, sz* len,
                          u16 segment_override, u16* out_segment, u16* out_offset) {
    // Mask to lower 16 bits
    u16 bx = vm->r.rbx & 0xFFFF;
    u16 si = vm->r.rsi & 0xFFFF;
    u16 di = vm->r.rdi & 0xFFFF;
    u16 bp = vm->r.rbp & 0xFFFF;

    bool force_ss = false;
    *out_offset = 0;

    // Default segment (will override if needed)
    if (segment_override != 0) {
        switch (segment_override) {
            case 1: *out_segment = vm->r.es; break;  // Extral Segment
            case 2: *out_segment = vm->r.cs; break;  // CODE Segment
            case 3: *out_segment = vm->r.ss; break;  // STACK Segment
            case 4: *out_segment = vm->r.ds; break;  // DATA Segment
            default: *out_segment = vm->r.ds;
        }
    } else {
        *out_segment = vm->r.ds;
    }

    // Base + index calculation
    switch (rm) {
        case 0: *out_offset = bx + si; break;              // [bx+si]
        case 1: *out_offset = bx + di; break;              // [bx+di]
        case 2: *out_offset = bp + si; force_ss = true; break; // [bp+si]
        case 3: *out_offset = bp + di; force_ss = true; break; // [bp+di]
        case 4: *out_offset = si; break;                   // [si]
        case 5: *out_offset = di; break;                   // [di]
        case 6:                                                    // Special
            if (mod == 0) {                                        // Direct address [disp16]
                *out_offset = read_le16(*pc);
                (*pc) += 2;
                (*len) += 2;
            } else {                                               // [bp] or [bp+disp]
                *out_offset = bp;
                force_ss = true;
            }
            break;
        case 7: *out_offset = bx; break;                   // [bx]
    }

    // Apply SS override if no segment prefix and BP is involved
    if (segment_override == 0 && force_ss) {
        *out_segment = vm->r.ss;
    }

    // Displacement
    if (mod == 1) {  // disp8
        *out_offset += (i8)**pc;
        (*pc)++;
        (*len)++;
    } else if (mod == 2 || (mod == 0 && rm == 6)) {  // disp16
        *out_offset += (i16)read_le16(*pc);
        (*pc) += 2;
        (*len) += 2;
    }

    return true;
}

// Effective address calculation (64-bit addressing always, RIP-relative only in 64-bit)
static u64 get_effective_address(VM* vm, u8 mod, u8 rm, const u8** pc, sz* len) {
        u64 addr = 0;
        u8 rex_b = vm->rex & 1;
        u8 rex_x = (vm->rex >> 1) & 1;
        bool has_sib = (rm == 4);
        u8 sib = 0;
        u8 scale = 1, index_idx = 4, base_idx = rm | (rex_b << 3);
        if (vm->mode != MODE_64BIT) {
        if (rex_b || rex_x) return 0;  // Extended indices/bases invalid
        }
        if (has_sib) {
        sib = **pc;
        (*pc)++;
        (*len)++;
        scale = 1 << ((sib >> 6) & 3);
        index_idx = ((sib >> 3) & 7) | (rex_x << 3);
        if (vm->mode != MODE_64BIT && index_idx >= 8) return 0;
        base_idx = (sib & 7) | (rex_b << 3);
        }
        u64* base_reg = get_gpr(vm, base_idx);
        u64* index_reg = (index_idx != 4) ? get_gpr(vm, index_idx) : NULL;
        if (base_reg) addr += *base_reg;
        if (index_reg) addr += *index_reg * scale;
        bool is_rip_relative = (mod == 0 && rm == 5) ||
        (mod == 0 && has_sib && base_idx == 5);
        if (is_rip_relative) {
            if (vm->mode != MODE_64BIT) return 0;  // RIP-relative only in 64-bit
            i32 disp = read_le32(*pc);
            (*pc) += 4;
            (*len) += 4;
            addr = vm->r.rip + *len + disp;
            return addr;
        }
        i64 disp = 0;
        if (mod == 1) {
            disp = (i8)**pc;
            (*pc)++;
            (*len)++;
        } 
        else if (mod == 2) {
            disp = (i32)read_le32(*pc);
            (*pc) += 4;
            (*len) += 4;
        }
        addr += disp;
        return addr;
}
// Flag helpers (unchanged)
static void set_flags_basic(VM* vm, u64 result, sz size) {
        vm->r.rflags &= ~(FLAG_CF | FLAG_OF | FLAG_SF | FLAG_ZF | FLAG_PF | FLAG_AF);
        if (result == 0) vm->r.rflags |= FLAG_ZF;
        if (result & (1ULL << (size * 8 - 1))) vm->r.rflags |= FLAG_SF;
        u8 low_byte = (u8)result;
        u8 parity = low_byte ^ (low_byte >> 4);
        parity ^= (parity >> 2);
        parity ^= (parity >> 1);
        if (!(parity & 1)) vm->r.rflags |= FLAG_PF;
}
static void set_flags_add_sub(VM* vm, u64 a, u64 b, u64 result, bool is_sub, sz size) {
        set_flags_basic(vm, result, size);
        if (is_sub) {
        if (a < b) vm->r.rflags |= FLAG_CF;
        } else {
        if (result < a) vm->r.rflags |= FLAG_CF;
        }
        u64 sign_bit = 1ULL << (size * 8 - 1);
        bool a_sign = a & sign_bit;
        bool b_sign = is_sub ? !(b & sign_bit) : (b & sign_bit);
        bool res_sign = result & sign_bit;
        if ((a_sign == b_sign) && (a_sign != res_sign)) vm->r.rflags |= FLAG_OF;
        u8 low_a = a & 0xF;
        u8 low_b = b & 0xF;
        u8 low_res = result & 0xF;
        if (is_sub) {
          if (low_a < low_b) vm->r.rflags |= FLAG_AF;
        } else {
          if (low_res < low_a) vm->r.rflags |= FLAG_AF;
        }
        }

// ───────────────────────────────────────────────
// VM Creation/Destruction/Mode
// ───────────────────────────────────────────────
VM* vm_create(sz memory_bytes) {
        VM* vm = calloc(1, sizeof(VM));
        if (!vm) return NULL;
        vm->memory = calloc(1, memory_bytes);
        if (!vm->memory) {
        free(vm);
        return NULL;
        }
        vm->mem_size = memory_bytes;
        vm->r.rsp = memory_bytes;
        vm->r.rflags = 0x202;
        vm->mode = MODE_64BIT;
   
        return vm;
}
void vm_destroy(VM* vm) {
            if (vm) {
       
            free(vm->memory);
            free(vm);
            }
}
void vm_set_mode(VM* vm, VmMode mode) {
        vm->mode = mode;
        if(mode == MODE_16BIT){
           
            // Real-mode defaults (common for bootable code / simple programs)
            vm->r.cs = 0x0000;
            vm->r.ds = 0x0000;
            vm->r.es = 0x0000;
            vm->r.ss = 0x0000;
            vm->r.ip = 0x0000;       // Or 0x7C00 for boot sectors
            vm->r.rsp = 0xFFFE;      // Common initial SP
            vm->r.rflags = 0x202;
        }
}
void vm_set_disasm_mode(VM* vm, bool enable) {
        vm->disasm_mode = enable;
}
// ───────────────────────────────────────────────
// Program Loading
// ───────────────────────────────────────────────
VmStatus vm_load_program(VM* vm, const u8* code, sz len) {
        if (len > vm->mem_size) return VM_SEGFAULT;
        memcpy(vm->memory, code, len);
        vm->code_break = len;
        vm->r.rip = 0;
        if(vm->mode == MODE_16BIT){
            vm->r.ip = 0; // Sync IP to RIP for 16-bit mode
            vm->r.rip = vm->r.ip; // Sync IP to RIP for 16-bit mode
        }
        return VM_OK;
}
// ───────────────────────────────────────────────
// Single-Step Execution
// ───────────────────────────────────────────────
static VmStatus step(VM* vm) {
      
        const u8* original_pc;
        const u8* pc;
      

        if (vm->mode == MODE_16BIT) {
            u32 phys_ip = get_physical_address(vm, vm->r.cs, vm->r.ip);
            if (phys_ip >= vm->mem_size) return VM_SEGFAULT;
            pc = original_pc = vm->memory + phys_ip;
        
        } else {
            if (vm->r.rip >= vm->mem_size) return VM_SEGFAULT;
            pc = original_pc = vm->memory + vm->r.rip;
           
        }
        sz len = 0;
        // Prefix handling
        vm->rex = 0;
        bool operand_override = false;
        u16 segment_override = 0;  // 0=none (use default), 1=ES, 2=CS, 3=SS, 4=DS
         while (true) {
            if (pc >= vm->memory + vm->mem_size) return VM_SEGFAULT;
            u8 prefix = *pc;
            if ((prefix & 0xF0) == 0x40) {
                 if (vm->mode == MODE_64BIT) {
                    vm->rex = prefix;
                    pc++;
                    len++;  
                }else if(vm->mode == MODE_32BIT){
                    // In 32-bit mode, 0x40-0x4F are not valid REX prefixes
                    return VM_BAD_OPCODE;
                }
                else{
                    break;
                }

            } else if (prefix == 0x66) {
            operand_override = true;
            pc++;
            len++;
            }
            else if (vm->mode == MODE_16BIT) {  // Only in real-mode
               // printf("16-bit prefix: %02x\n", prefix);
                switch (prefix) {
                    case 0x26: segment_override = 1; pc++; len++; continue;  // ES:
                    case 0x2E: segment_override = 2; pc++; len++; continue;  // CS:
                    case 0x36: segment_override = 3; pc++; len++; continue;  // SS:
                    case 0x3E: segment_override = 4; pc++; len++; continue;  // DS:
                    default: break;
                }
                break;
            }
            else {
            break;
            }
        }
         
        
        u8 opcode = *pc++;
        len++;
        bool is_extended = false;
        if (opcode == 0x0F && vm->mode != MODE_16BIT) {
            is_extended = true;
            opcode = *pc++;
            len++;
            }
            // ModR/M handling


      
        bool has_modrm = false;
        if(opcode == 0x00 || opcode == 0x01 || opcode == 0x02 || opcode == 0x03  //ADD
          || opcode == 0x08 || opcode == 0x09  || opcode == 0x0A || opcode == 0x0B //OR
          || opcode == 0x10 || opcode == 0x11 || opcode == 0x12 || opcode == 0x13 //ADC
          || opcode == 0x18 || opcode == 0x19  || opcode == 0x1A || opcode == 0x1B //SBB
          || opcode == 0x20 || opcode == 0x21 || opcode == 0x22 || opcode == 0x23 //AND
          || opcode == 0x28 || opcode == 0x29  || opcode == 0x2A || opcode == 0x2B //SUB
          || opcode == 0x30 || opcode == 0x31 || opcode == 0x32 || opcode == 0x33 //XOR
          || opcode == 0x38 || opcode == 0x39  || opcode == 0x3A || opcode == 0x3B //CMP
          || opcode == 0x69 || opcode == 0x6B
          || (opcode >= 0x80 &&  opcode <= 0x8f) 
          || opcode == 0xAF
          || (opcode == 0xC0 || opcode == 0xC1 || opcode == 0xC6 || opcode == 0xC7
          || opcode == 0xD0 || opcode == 0xD1 || opcode == 0xD2 || opcode == 0xD3
          || opcode == 0xF6 || opcode == 0xF7 || opcode == 0xFE || opcode == 0xFF)) 
         {
                        has_modrm = true;
         }
         bool is_byte_op = (opcode & 1) == 0 && (opcode <= 0x3F);
        sz op_size;
        if( opcode == 0x6A  || opcode == 0x6B || (opcode >= 0x70 && opcode <= 0x7F)
            ||( opcode >= 0xB0 && opcode <= 0xB7 )|| opcode == 0xC6 || opcode == 0xCD
            || (opcode >= 0xE0 && opcode <= 0xE7)|| opcode == 0xEB ){
                        op_size = 1;
                 
        }
        // else if(opcode == 0x05 || opcode == 0x0D || opcode == 0x15 || opcode == 0x1D
        //                 || opcode == 0x25 || opcode == 0x2D || opcode == 0x35 || opcode == 0x3D
        //                 || opcode == 0xB8 || opcode == 0xB9 || opcode == 0xBA || opcode == 0xBB
        //                 || opcode == 0xBC || opcode == 0xBD || opcode == 0xBE || opcode == 0xBF
        //                 || opcode == 0x68 || opcode == 0xE8 || opcode == 0xE9
        //                 || opcode == 0xCC  || opcode == 0xCD || opcode == 0xCE){
        //                  op_size = operand_override ? 2 : 4;
        //             }
        else if (  opcode== 0x06 || opcode == 0x07 || opcode == 0x0E
                        || opcode == 0x16  || opcode == 0x17 || opcode == 0x1E || opcode == 0x1F 
                        || opcode == 0x26  || opcode == 0x27 || opcode == 0x2E || opcode == 0x2F 
                        || opcode == 0x36  || opcode == 0x37 || opcode == 0x3E || opcode == 0x3F 
                        || (opcode >= 0x40 && opcode <= 0x4F)
                         || (opcode >= 0x60 && opcode <= 0x67)
                        || opcode == 0x90 
                        || opcode == 0xF4 || opcode == 0xF8 || opcode == 0xF9 || opcode == 0xFA 
                        || opcode == 0xFB || opcode == 0xFC || opcode == 0xFD
                        || opcode == 0xC3 || opcode == 0xCB  || opcode == 0xCC || opcode == 0xCF
                        ) {
                        op_size = 0;
                      
                    }
                    
        else {
                    op_size = get_op_size(vm, is_byte_op, operand_override);
                }
                            
           
        u8 modrm = 0, mod = 0, reg = 0, rm = 0;
        if (has_modrm) {
        modrm = *pc++;
        
        len++;
        if(opcode == 0x00 || modrm == 0x00){
            if(*pc++ == 0x00){
                return VM_UNKNOWN_ERROR;
            }
           
        }
        mod = (modrm >> 6) & 3;
        reg = (((vm->rex & 4) >> 1) | ((modrm >> 3) & 7));
        rm  = (((vm->rex & 1) << 3) | (modrm & 7));
           
        if (vm->mode != MODE_64BIT && (reg >= 8 || rm >= 8)) return VM_BAD_OPCODE;
        }
     
        // Operand pointers
        u8* reg_ptr = NULL;
        u8* rm_ptr = NULL;
        if (has_modrm) {
            reg_ptr = get_reg_ptr(vm, reg, op_size);
            if (!reg_ptr) return VM_SEGFAULT;
            if (mod == 3) {
            rm_ptr = (u8*)get_reg_ptr(vm, rm, op_size);
            if (!rm_ptr) return VM_SEGFAULT;
            } else {
           // Memory
                if (vm->mode == MODE_16BIT) {
                    u16 mem_seg = vm->r.ds;   // default, will be overridden
                    u16 mem_off = 0;
                    calc_ea_16bit(vm, mod, rm, &pc, &len, segment_override, &mem_seg, &mem_off);
                    rm_ptr = get_memory_ptr(vm, mem_seg, mem_off, op_size);
                    if (!rm_ptr) return VM_SEGFAULT;

                    if (vm->disasm_mode) {
                        u32 phys = get_physical_address(vm, mem_seg, mem_off);
                        printf("  Effetive Address %04x:%04x (phys 0x%05x)\n", mem_seg, mem_off, phys);
                    }
                } else {
                    u64 ea = get_effective_address(vm, mod, rm, &pc, &len);
                    if (ea + op_size > vm->mem_size) return VM_SEGFAULT;
                    rm_ptr = vm->memory + ea;
                }
            }
        }
          
          
            // Execution
        switch (opcode) {    
            // Arithmetic
            case 0x00: case 0x01:  {  // ADD r/m, r

                u64 a = get_op_value(rm_ptr, op_size);
                u64 b = get_op_value(reg_ptr, op_size);
                u64 res = a + b;
                set_op_value(rm_ptr, res, op_size);
                set_flags_add_sub(vm, a, b, res, false, op_size);
               
                if (vm->disasm_mode){
                    printf("ADD r/m=%lx, reg= %d\n",(mod == 3)?rm:rm_ptr-vm->memory,reg);
                    vm_read_memory(vm, rm_ptr);
                }
                break;
            }
            case 0x02: case 0x03: {  // ADD r, r/m
                u64 a = get_op_value(reg_ptr, op_size);
                u64 b = get_op_value(rm_ptr, op_size);
                u64 res = a + b;
                set_op_value(reg_ptr, res, op_size);
                set_flags_add_sub(vm, a, b, res, false, op_size);
           
                if (vm->disasm_mode){
                    printf("ADD r/m=%lx, reg=%d\n",(mod == 3)?rm:rm_ptr-vm->memory,reg);
                    vm_read_memory(vm, rm_ptr);
                }
                break;
            }
            case 0x04: case 0x05: {  // ADD rax, imm
                u64* dst = &vm->r.rax;
                u64 imm = (op_size == 1) ? *pc :(op_size == 2 ? read_le16(pc) : (op_size == 4 ? read_le32(pc) : read_le64(pc)));
                u64 a = *dst;
                u64 res = a + imm;
                *dst = res;
                set_flags_add_sub(vm, a, imm, res, false, op_size);
                len += op_size;
            
                if (vm->disasm_mode){ 
                    printf("ADD rax, imm=%lx\n",imm);
                }
                break;
            }
           // PUSH segment (0x06 ES, 0x0E CS, 0x16 SS, 0x1E DS)
            case 0x06: {  // PUSH ES
                u64 val = vm->r.es;
                vm->r.rsp = (u16)(vm->r.rsp - 2);
                u8* stack_ptr = get_memory_ptr(vm, vm->r.ss, (u16)vm->r.rsp, 2);
                if (!stack_ptr) return VM_STACK_OVERFLOW;
                set_op_value(stack_ptr, val, 2);
                if (vm->disasm_mode) printf("PUSH ES = %04x\n", (u16)val);
                break;
            }
            // POP segment (0x07 ES, 0x0F CS, 0x17 SS, 0x1F DS)
            case 0x07: {  // POP ES
                u8* stack_ptr = get_memory_ptr(vm, vm->r.es, (u16)vm->r.rsp, 2);
                if (!stack_ptr) return VM_STACK_OVERFLOW;
                vm->r.es = (u16)get_op_value(stack_ptr, 2);
                vm->r.rsp = (u16)(vm->r.rsp + 2);
                if (vm->disasm_mode) printf("POP ES = %04x\n", vm->r.es);
                break;
            }
          
            case 0x08: case 0x09: {  // OR r/m, r
                u64 a = get_op_value(rm_ptr, op_size);
                u64 b = get_op_value(reg_ptr, op_size);
                u64 res = a | b;
                set_op_value(rm_ptr, res, op_size);
                set_flags_basic(vm, res, op_size);
                vm->r.rflags &= ~FLAG_CF & ~FLAG_OF;
           
                if (vm->disasm_mode){
                    printf("OR r/m=%lx, reg=%d\n",(mod == 3)?rm:rm_ptr-vm->memory,reg);
                    vm_read_memory(vm, rm_ptr);
                }
                break;
           }
            case 0x0A: case 0x0B: {  // OR r, r/m
                u64 a = get_op_value(reg_ptr, op_size);
                u64 b = get_op_value(rm_ptr, op_size);
                u64 res = a | b;
                set_op_value(reg_ptr, res, op_size);
                set_flags_basic(vm, res, op_size);
                vm->r.rflags &= ~FLAG_CF & ~FLAG_OF;
              
                if (vm->disasm_mode){
                    printf("OR reg=%d, r/m=%lx\n", reg, (mod == 3)?rm:rm_ptr-vm->memory);
                    vm_read_memory(vm, rm_ptr);
                }
                break;
           }
            case 0x0C: case 0x0D: {  // OR rax, imm
                u64* dst = &vm->r.rax;
                u64 imm = (op_size == 1) ? *pc :(op_size == 2 ? read_le16(pc) : (op_size == 4 ? read_le32(pc) : read_le64(pc)));
                u64 a = *dst;
                u64 res = a | imm;
                *dst = res;
                set_flags_basic(vm, res, op_size);
                len += op_size;
             
                if (vm->disasm_mode){
                    printf("OR rax, imm=%lx\n",imm);
                }
                break;
            }
            case 0x0E: {  // PUSH CS
                u64 val = vm->r.cs;
                vm->r.rsp = (u16)(vm->r.rsp - 2);
                u8* stack_ptr = get_memory_ptr(vm, vm->r.ss, (u16)vm->r.rsp, 2);
                if (!stack_ptr) return VM_STACK_OVERFLOW;
                set_op_value(stack_ptr, val, 2);
                if (vm->disasm_mode) printf("PUSH CS = %04x\n", (u16)val);
                break;
            }
            case 0x0F: {  // POP CS

                u8* stack_ptr = get_memory_ptr(vm, vm->r.cs, (u16)vm->r.rsp, 2);
                if (!stack_ptr) return VM_STACK_OVERFLOW;
                vm->r.cs = (u16)get_op_value(stack_ptr, 2);
                vm->r.rsp = (u16)(vm->r.rsp + 2);
                if (vm->disasm_mode) printf("POP CS = %04x\n", vm->r.cs);
                break;
            }
       
          

            case 0x10: case 0x11:  {  // ADC r/m, r
                u64 carry = (vm->r.rflags & FLAG_CF) ? 1 : 0;
                u64 a = get_op_value(rm_ptr, op_size);
                u64 b = get_op_value(reg_ptr, op_size);
                u64 res = a + b + carry;
                set_op_value(rm_ptr, res, op_size);
                set_flags_add_sub(vm, a, b, res, false, op_size);
             
                if (vm->disasm_mode){
                    printf("ADC r/m=%lx, reg=%d\n",(mod == 3)?rm:rm_ptr-vm->memory,reg);
                    vm_read_memory(vm, rm_ptr);
                }
                break;
            }
             case 0x12: case 0x13:  {  // ADC  r,r/m
                u64 carry = (vm->r.rflags & FLAG_CF) ? 1 : 0;
                u64 a = get_op_value(reg_ptr, op_size);
                u64 b = get_op_value(rm_ptr, op_size);
                u64 res = a + b + carry;
                set_op_value(reg_ptr, res, op_size);
                set_flags_add_sub(vm, a, b, res, false, op_size);
             
                if (vm->disasm_mode){
                    printf("ADC reg=%d, r/m=%lx\n", reg, (mod == 3)?rm:rm_ptr-vm->memory);
                    vm_read_memory(vm, rm_ptr);
                }
                break;
            }

      

            case 0x14: case 0x15: {  // ADC rax, imm
                u64 carry = (vm->r.rflags & FLAG_CF) ? 1 : 0;
                u64* dst = &vm->r.rax;
                u64 imm = (op_size == 1) ? *pc :(op_size == 2 ? read_le16(pc) : (op_size == 4 ? read_le32(pc) : read_le64(pc)));
                u64 a = *dst;
                u64 res = a + imm + carry;
                *dst = res;
                set_flags_add_sub(vm, a, imm, res, false, op_size);
                len += op_size;
           
                if(vm->disasm_mode){
                    printf("ADC rax,imm=%lx\n",imm);
                }
                break;
            }

            case 0x16: {  // PUSH SS
            u64 val = vm->r.ss;
            vm->r.rsp = (u16)(vm->r.rsp - 2);
            u8* stack_ptr = get_memory_ptr(vm, vm->r.ss, (u16)vm->r.rsp, 2);
            if (!stack_ptr) return VM_STACK_OVERFLOW;
            set_op_value(stack_ptr, val, 2);
            if (vm->disasm_mode) printf("PUSH SS = %04x\n", (u16)val);
            break;
            }

            case 0x17: {  // POP SS
                u8* stack_ptr = get_memory_ptr(vm, vm->r.ss, (u16)vm->r.rsp, 2);
                if (!stack_ptr) return VM_STACK_OVERFLOW;
                vm->r.ss = (u16)get_op_value(stack_ptr, 2);
                vm->r.rsp = (u16)(vm->r.rsp + 2);
                if (vm->disasm_mode) printf("POP SS = %04x\n", vm->r.ss);
                break;
            }

            case 0x18: case 0x19: {  // SBB r/m, r
                u64 carry = (vm->r.rflags & FLAG_CF) ? 1 : 0;
                u64 a = get_op_value(rm_ptr, op_size);
                u64 b = get_op_value(reg_ptr, op_size);
                u64 res = a - (b + carry);
                set_op_value(rm_ptr, res, op_size);
                set_flags_add_sub(vm, a, b + carry, res, true, op_size);
             
                if(vm->disasm_mode){
                    printf("SBB r/m=%lx, reg=%d\n",(mod == 3)?rm:rm_ptr-vm->memory,reg);
                    vm_read_memory(vm, rm_ptr);
                }
                break;
            }
            case 0x1A: case 0x1B: {  // SBB  r,r/m
            u64 carry = (vm->r.rflags & FLAG_CF) ? 1 : 0;
                u64 a = get_op_value(reg_ptr, op_size);
                u64 b = get_op_value(rm_ptr, op_size);
                u64 res = a - (b + carry);
                set_op_value(reg_ptr, res, op_size);
                set_flags_add_sub(vm, a, b + carry, res, true, op_size);
         
                if(vm->disasm_mode){
                    printf("SBB reg=%d, r/m=%lx\n", reg, (mod == 3)?rm:rm_ptr-vm->memory);
                    vm_read_memory(vm, rm_ptr);
                }
                break;
           }
           case 0x1C: case 0x1D: {  // SBB rax, imm
                u64 carry = (vm->r.rflags & FLAG_CF) ? 1 : 0;
                u64* dst = &vm->r.rax;
                u64 imm = (op_size == 1) ? *pc :(op_size == 2 ? read_le16(pc) : (op_size == 4 ? read_le32(pc) : read_le64(pc)));
                u64 a = *dst;
                u64 res = a -(imm + carry);
                *dst = res;
                set_flags_add_sub(vm, a, imm, res, true, op_size);
                len += op_size;
          
                if(vm->disasm_mode){
                    printf("SBB rax, imm=%lx\n",imm);
                }
                break;
            }
                case 0x1E: {  // PUSH DS
                u64 val = vm->r.ds;
                vm->r.rsp = (u16)(vm->r.rsp - 2);
                u8* stack_ptr = get_memory_ptr(vm, vm->r.ss, (u16)vm->r.rsp, 2);
                if (!stack_ptr) return VM_STACK_OVERFLOW;
                set_op_value(stack_ptr, val, 2);
                if (vm->disasm_mode) printf("PUSH DS = %04x\n", (u16)val);
                break;
            }
               case 0x1F: {  // POP dS
                u8* stack_ptr = get_memory_ptr(vm, vm->r.ds, (u16)vm->r.rsp, 2);
                if (!stack_ptr) return VM_STACK_OVERFLOW;
                vm->r.ds = (u16)get_op_value(stack_ptr, 2);
                vm->r.rsp = (u16)(vm->r.rsp + 2);
                if (vm->disasm_mode) printf("POP DS = %04x\n", vm->r.ds);
                break;
            }

            case 0x20: case 0x21: {  // AND r/m, r
                u64 a = get_op_value(rm_ptr, op_size);
                u64 b = get_op_value(reg_ptr, op_size);
                u64 res = a & b;
                set_op_value(rm_ptr, res, op_size);
                set_flags_basic(vm, res, op_size);
                vm->r.rflags &= ~FLAG_CF & ~FLAG_OF;
            
                if(vm->disasm_mode){
                    printf("AND r/m=%lx, reg=%d\n",(mod == 3)?rm:rm_ptr-vm->memory,reg);
                    vm_read_memory(vm, rm_ptr);
                }
                break;
            }
            case 0x22: case 0x23: {  // AND r，  r/m,
                u64 a = get_op_value(reg_ptr, op_size);
                u64 b = get_op_value(rm_ptr, op_size);
                u64 res = a & b;
                set_op_value(reg_ptr, res, op_size);
                set_flags_basic(vm, res, op_size);
                vm->r.rflags &= ~FLAG_CF & ~FLAG_OF;
          
                if(vm->disasm_mode){
                    printf("AND reg=%d, r/m=%lx\n", reg, (mod == 3)?rm:rm_ptr-vm->memory);
                    vm_read_memory(vm, rm_ptr);
                }
                break;
            }

           case 0x24: case 0x25: {  // And rax, imm    
                u64* dst = &vm->r.rax;
                u64 imm = (op_size == 1) ? *pc :(op_size == 2 ? read_le16(pc) : (op_size == 4 ? read_le32(pc) : read_le64(pc)));
                u64 a = *dst;
                u64 res = a & imm;          
                *dst = res;
                set_flags_add_sub(vm, a, imm, res, false, op_size);
                len += op_size;
              
                if(vm->disasm_mode){
                    printf("AND rax, imm=%lx\n",imm);
                }
                break;
            }
            case 0x26: {
                    printf("ES PREFIX (ignored)\n");
                
                    break;  // PUSH DS (ignored)
                    }
            case 0x27: { //DAA
                    printf("DAA (ignored)\n");
                  
                    break;  // DAA (ignored)
                    }
            case 0x28: case 0x29: {  // SUB r/m, r
                u64 a = get_op_value(rm_ptr, op_size);
                u64 b = get_op_value(reg_ptr, op_size);
                u64 res = a - b;
                set_op_value(rm_ptr, res, op_size);
                zero_upper_register_bits(vm, rm_ptr, op_size);
                set_flags_add_sub(vm, a, b, res, true, op_size);
            
                if(vm->disasm_mode){
                    printf("SUB r/m=%lx, reg=%d\n",(mod == 3)?rm:rm_ptr-vm->memory,reg);
                    vm_read_memory(vm, rm_ptr);
                }
                break;
            }

            case 0x2A: case 0x2B: {  // SUB r, r/m
                u64 a = get_op_value(reg_ptr, op_size);
                u64 b = get_op_value(rm_ptr, op_size);
                u64 res = a - b;
                set_op_value(reg_ptr, res, op_size);
                zero_upper_register_bits(vm, reg_ptr, op_size);
                set_flags_add_sub(vm, a, b, res, true, op_size);
           
                if(vm->disasm_mode){
                    printf("SUB reg=%d, r/m=%lx\n", reg, (mod == 3)?rm:rm_ptr-vm->memory);
                    vm_read_memory(vm, rm_ptr);
                }
                break;
            }
            case 0x2C: case 0x2D: {  // SUB rax, imm
                u64* dst = &vm->r.rax;
                u64 imm = (op_size == 1) ? *pc :(op_size == 2 ? read_le16(pc) : (op_size == 4 ? read_le32(pc) : read_le64(pc)));
                u64 a = *dst;
                u64 res = a - imm;
                *dst = res;
                set_flags_add_sub(vm, a, imm, res, true, op_size);
                len += op_size;
            
                if(vm->disasm_mode){
                    printf("SUB rax, imm=%lx\n",imm);
                }
                break;
            }
             case 0x2E: {
                    printf("CS PREFIX (ignored)\n");
                  
                    break;  
                    }
            case 0x2F: { //DAS
                    printf("DAS (ignored)\n");
               
                    break;  // DAS (ignored)
                    }
              // ─── Bitwise: XOR/AND/OR/NOT ───
            case 0x30: case 0x31: {  // XOR r/m, r
                u64 a = get_op_value(rm_ptr, op_size);
                u64 b = get_op_value(reg_ptr, op_size);
                u64 res = a ^ b;
                set_op_value(rm_ptr, res, op_size);
                set_flags_basic(vm, res, op_size);
                vm->r.rflags &= ~FLAG_CF & ~FLAG_OF;  // Clear CF/OF for logic ops
              
                if(vm->disasm_mode){
                    printf("XOR r/m=%lx, reg=%d\n",(mod == 3)?rm:rm_ptr-vm->memory,reg);
                    vm_read_memory(vm, rm_ptr);
                }
                break;
            }

          case 0x32: case 0x33: {  // XOR r, r/m
                u64 a = get_op_value(reg_ptr, op_size);
                u64 b = get_op_value(rm_ptr, op_size);
                u64 res = a ^ b;
                set_op_value(reg_ptr, res, op_size);
                set_flags_basic(vm, res, op_size);
                vm->r.rflags &= ~FLAG_CF & ~FLAG_OF;  
            
                if(vm->disasm_mode){
                    printf("XOR reg=%d, r/m=%lx\n", reg, (mod == 3)?rm:rm_ptr-vm->memory);
                    vm_read_memory(vm, rm_ptr);
                }
                
                break;
            }
            case 0x34: case 0x35: {  // XOR rax, imm
                u64* dst = &vm->r.rax;
                u64 imm = (op_size == 1) ? *pc :(op_size == 2 ? read_le16(pc) : (op_size == 4 ? read_le32(pc) : read_le64(pc)));
                u64 a = *dst;
                u64 res = a ^ imm;
                *dst = res;
                set_flags_basic(vm, res, op_size);
                vm->r.rflags &= ~FLAG_CF & ~FLAG_OF;  
                len += op_size;
          
                if(vm->disasm_mode){
                    printf("XOR rax, imm=%lx\n",imm);
                }
                break;
            }
            case 0x36: {
                printf("SS PREFIX (ignored)\n");
             
                break;  
                }
            case 0x37: {
                printf("AAA (ignored)\n");
           
                break;  
                }
          // ─── Compare: CMP ───
            case 0x38: case 0x39: {  // CMP r/m, r
                u64 a = get_op_value(rm_ptr, op_size);
                u64 b = get_op_value(reg_ptr, op_size);
                u64 res = a - b;
                set_flags_add_sub(vm, a, b, res, true, op_size);
           
                
                if(vm->disasm_mode){
                    printf("CMP r/m=%lx, reg=%d\n",(mod == 3)?rm:rm_ptr-vm->memory,reg);
                    vm_read_memory(vm, rm_ptr);
                }
                break;
            }
            case 0x3A: case 0x3B: {  // CMP r, r/m      
                u64 a = get_op_value(reg_ptr, op_size);
                u64 b = get_op_value(rm_ptr, op_size);
                u64 res = a - b;
                set_flags_add_sub(vm, a, b, res, true, op_size);
              
                if(vm->disasm_mode){
                    printf("CMP reg=%d, r/m=%lx\n", reg, (mod == 3)?rm:rm_ptr-vm->memory);
                    vm_read_memory(vm, rm_ptr);
                }
                
                break;          
            }
            case 0x3C: case 0x3D: {  // CMP rax, imm
                u64 a = vm->r.rax;
                u64 imm = (op_size == 1) ? *pc :(op_size == 2 ? read_le16(pc) : (op_size == 4 ? read_le32(pc) : read_le64(pc)));
                u64 res = a - imm;
                set_flags_add_sub(vm, a, imm, res, true, op_size);
                len += op_size;
           
                if(vm->disasm_mode){
                    printf("CMP rax, imm=%lx\n",imm);
                }
                break;
            }
            case 0x3E: {
                printf("DS PREFIX (ignored)\n");
            
                break;  
                }
            case 0x3F: {
                printf("AAS (ignored)\n");
           
                break;  
                }
            case 0x40 ... 0x4F: { //INC/DEC r64 or REX prefix
                if (vm->mode == MODE_16BIT) {
                    u8 reg_idx = (opcode - 0x40) & 0x7;
                  
                    u64* reg = get_gpr(vm, reg_idx);
                    if (!reg) return VM_SEGFAULT;
                    if (opcode <= 0x47) {  // INC r64
                        u64 a = *reg;
                        u64 res = a + 1;
                        *reg = res;
                        set_flags_add_sub(vm, a, 1, res, false, 8);
                        if(vm->disasm_mode){
                            printf("INC  reg=%d\n", reg_idx);
                        }
                    } else {  // DEC r64
                        u64 a = *reg;
                        u64 res = a - 1;       
                        *reg = res;
                        set_flags_add_sub(vm, a, 1, res, true, 8);  
                        if(vm->disasm_mode){
                            printf("DEC reg=%d\n", reg_idx);
                        }
                    }
             
                printf("INC/DEC in 16-bit mode reg=%d = %lx\n", reg_idx, *reg);
                 
                }else { 
                    printf("REX PREFIX (unexpected in opcode stream, ignored)\n");
                 
                    }

                break;
            }
             // ─── Stack: PUSH/POP ───
            case 0x50 ... 0x57: {  // PUSH r64
                u8 reg_idx = (opcode - 0x50) | ((vm->rex & 1) << 3);
                u64* src = get_gpr(vm, reg_idx);
                u64 val = get_op_value((u8*)src, op_size);
                if (vm->mode == MODE_16BIT) {
                    vm->r.rsp = (u16)(vm->r.rsp - op_size);  // SP -= 2 (wraps)
                    u8* stack_ptr = get_memory_ptr(vm, vm->r.ss, (u16)vm->r.rsp, op_size);
                    if (!stack_ptr) return VM_STACK_OVERFLOW;
                    set_op_value(stack_ptr, val, op_size);
                } else {
                    vm->r.rsp -= op_size;
                    if (vm->r.rsp + op_size > vm->mem_size) return VM_STACK_OVERFLOW;
                    set_op_value((u8*)&vm->memory[vm->r.rsp], val, op_size);
                }
                if (vm->disasm_mode) {
                    printf("PUSH r%d = %lx\n", reg, val);
                }
                break;
            }
            case 0x58 ... 0x5F: {  // POP r64
                u8 reg_idx = (opcode - 0x58) | ((vm->rex & 1) << 3);
               
                u64 val;
                if (vm->mode == MODE_16BIT) {
                        u8* stack_ptr = get_memory_ptr(vm, vm->r.ss, (u16)vm->r.rsp, op_size);
                        if (!stack_ptr) return VM_STACK_OVERFLOW;
                        val = get_op_value(stack_ptr, op_size);
                        vm->r.rsp = (u16)(vm->r.rsp + op_size);  // SP += 2
                } else {
                        if (vm->r.rsp + op_size > vm->mem_size) return VM_STACK_OVERFLOW;
                        val = get_op_value((u8*)&vm->memory[vm->r.rsp], op_size);
                        vm->r.rsp += op_size;
                    }
                    u8* reg_ptr = (u8*)get_gpr(vm, reg_idx);
                    set_op_value(reg_ptr, val, op_size);
                    zero_upper_register_bits(vm, reg_ptr, op_size);
                    if (vm->disasm_mode) {
                        printf("POP r%d = %lx\n", reg, val);
                    }
                break;
            }
            case 0x60: {
                memcpy(vm->memory + vm->r.rsp - sizeof(Registers), &vm->r, sizeof(Registers));
                vm->r.rsp -= sizeof(Registers);
                if(vm->disasm_mode){
                    printf("PUSHAD (push all general-purpose registers)\n");
                }
                break;  
                }
            case 0x61: {
                if (vm->r.rsp + sizeof(Registers) > vm->mem_size) return VM_SEGFAULT;
                memcpy(&vm->r, vm->memory + vm->r.rsp, sizeof(Registers));
                vm->r.rsp += sizeof(Registers);
                if(vm->disasm_mode){
                    printf("POPAD (pop all general-purpose registers)\n");
                }
                break;  
                }
            case 0x62: {
                printf("BOUND (ignored)\n");
            
                break;  
                }
            case 0x63: {
                printf("ARPL (ignored)\n");
          
                break;  
                }
            case 0x64: {
                printf("FS PREFIX (ignored)\n");
           
                break;  
                }
            case 0x65: {
                printf("GS PREFIX (ignored)\n");
                 
                    break;      

            }
            case 0x66: {
                printf("Operand-size override prefix in opcode stream (ignored)\n");
              
                break;  
                }
            case 0x67: {
                printf("Address-size override prefix in opcode stream (ignored)\n");
           
                break;  
                }
     

            case 0x68: {  // PUSH imm32 (sign-extend to 64)
                if (vm->r.rip + len + 4 > vm->mem_size) return VM_SEGFAULT;
                i64 imm = *pc;
                len += 1;
                if (vm->r.rsp < 8) return VM_STACK_OVERFLOW;
                vm->r.rsp -= 8;
                if (vm->r.rsp >= vm->mem_size) return VM_SEGFAULT;
                *(u64*)(vm->memory + vm->r.rsp) = imm;
        
                if(vm->disasm_mode){
                    printf("PUSH imm32=%lx\n", imm);
                }
                break;
            }
            case 0x69: {  // IMUL r64, r/m, imm32
                if (mod == 3) return VM_BAD_OPCODE;  // No reg-reg IMUL with imm
                u64* dst = get_gpr(vm, reg);
                if (!dst) return VM_SEGFAULT;
                u64 val = get_op_value(rm_ptr, op_size);
                i64 imm = (i32)read_le32(pc);
                len += 4;
                i64 res = (i64)val * imm;
                *dst = res;
                set_flags_add_sub(vm, val, imm, res,false, op_size);
            
                if(vm->disasm_mode){
                    printf("IMUL reg=%d, r/m=%lx  = %lx * %lx = %lx\n", reg, (mod == 3)?rm:rm_ptr-vm->memory, val, imm, res);
                }
                break;
            }

            case 0x6A: {  // PUSH imm8 
                if (vm->r.rip + len + 1 > vm->mem_size) return VM_SEGFAULT;
                i64 imm = (i32)read_le32(pc);
                len += 4;
                if (vm->r.rsp < 8) return VM_STACK_OVERFLOW;
                vm->r.rsp -= 8;
                if (vm->r.rsp >= vm->mem_size) return VM_SEGFAULT;
                *(u64*)(vm->memory + vm->r.rsp) = imm;
          
                if(vm->disasm_mode){
                    printf("PUSH imm8=%lx\n", imm);
                }
                break;
            }
            case 0x6B: {  // IMUL r64, r/m, imm8
                if (mod == 3) return VM_BAD_OPCODE;  // No reg-reg IMUL with imm
                u64* dst = get_gpr(vm, reg);
                if (!dst) return VM_SEGFAULT;
                u64 val = get_op_value(rm_ptr, op_size);
                i64 imm = (i8)*pc;
                len += 1;
                i64 res = (i64)val * imm;
                *dst = res;
                set_flags_add_sub(vm, val, imm, res, false, op_size);
          
                if(vm->disasm_mode){
                    printf("IMUL reg=%d, r/m=%lx  = %lx * %lx = %lx\n", reg, (mod == 3)?rm:rm_ptr-vm->memory, val, imm, res);
                }
                break;
            }
            case 0x6C: case 0x6D: case 0x6E: case 0x6F: {
                printf("INS/OUTS (ignored)\n");
                vm->r.rip += len;
                break;  
                }   

           // ─── Jumps: Conditional and Unconditional ───
            case 0x70 ... 0x7F: {  // Short conditional jumps
                if (vm->r.rip + len + 1 >= vm->mem_size) return VM_SEGFAULT;
                i8 disp = read_disp8(pc);
                len++;
                bool take = false;
                u8 cond = opcode & 0xF;
                switch (cond) {
                    case 0x0: take = (vm->r.rflags & FLAG_OF); break;  // JO
                    case 0x1: take = !(vm->r.rflags & FLAG_OF); break; // JNO
                    case 0x2: take = (vm->r.rflags & FLAG_CF); break;  // JB
                    case 0x3: take = !(vm->r.rflags & FLAG_CF); break; // JAE
                    case 0x4: take = (vm->r.rflags & FLAG_ZF); break;  // JE
                    case 0x5: take = !(vm->r.rflags & FLAG_ZF); break; // JNE
                    case 0x6: take = (vm->r.rflags & (FLAG_CF | FLAG_ZF)); break;  // JBE
                    case 0x7: take = !(vm->r.rflags & (FLAG_CF | FLAG_ZF)); break; // JA
                    case 0x8: take = (vm->r.rflags & FLAG_SF); break;  // JS
                    case 0x9: take = !(vm->r.rflags & FLAG_SF); break; // JNS
                    case 0xA: take = (vm->r.rflags & FLAG_PF); break;  // JP
                    case 0xB: take = !(vm->r.rflags & FLAG_PF); break; // JNP
                    case 0xC: take = ((vm->r.rflags & FLAG_SF) != (vm->r.rflags & FLAG_OF)); break;  // JL
                    case 0xD: take = ((vm->r.rflags & FLAG_SF) == (vm->r.rflags & FLAG_OF)); break;  // JGE
                    case 0xE: take = (vm->r.rflags & FLAG_ZF) || ((vm->r.rflags & FLAG_SF) != (vm->r.rflags & FLAG_OF)); break;  // JLE
                    case 0xF: take = !(vm->r.rflags & FLAG_ZF) && ((vm->r.rflags & FLAG_SF) == (vm->r.rflags & FLAG_OF)); break;  // JG
                }
              
                if (take) vm->r.rip += (i64)disp;
                if(vm->disasm_mode){
                    printf("Short conditional jump with disp %d, take=%d\n", disp, take);
                }
                break;
            }
            case 0x80: case 0x81: case 0x82: case 0x83: {  // CMP r/m, imm
                sz imm_size = (opcode == 0x83) ? 1 : op_size;
                if (vm->r.rip + len + imm_size > vm->mem_size) return VM_SEGFAULT;
                u64 a = get_op_value(rm_ptr, op_size);
                i64 imm = (imm_size == 1) ? (i8)*pc : (imm_size == 2 ? (i16)read_le16(pc) : (i32)read_le32(pc));
                u64 res = a - imm;
                set_flags_add_sub(vm, a, imm, res, true, op_size);
                len += imm_size;
            
                if(vm->disasm_mode){
                    printf("CMP r/m=%lx, imm=%lx\n",(mod == 3)?rm:rm_ptr-vm->memory, imm);
                    vm_read_memory(vm, rm_ptr);
                }
                break;
            }
            case 0x84: case 0x85: {  // TEST r/m, r
                u64 a = get_op_value(rm_ptr, op_size);
                u64 b = get_op_value(reg_ptr, op_size);
                u64 res = a & b;
                set_flags_basic(vm, res, op_size);
                vm->r.rflags &= ~FLAG_CF & ~FLAG_OF;  // Clear CF/OF for logic ops
              
                if(vm->disasm_mode){
                    printf("TEST r/m=%lx, reg=%d\n", (mod == 3)?rm:rm_ptr-vm->memory,reg);
                    vm_read_memory(vm, rm_ptr);
                }
                break;
            }
            case 0x86: case 0x87: {  // XCHG r/m, r
                u64 a = get_op_value(rm_ptr, op_size);
                u64 b = get_op_value(reg_ptr, op_size);
                set_op_value(rm_ptr, b, op_size);   
                set_op_value(reg_ptr, a, op_size);      
           
                if(vm->disasm_mode){
                    printf("XCHG r/m=%lx, reg=%d\n",(mod == 3)?rm:rm_ptr-vm->memory,reg);
                    vm_read_memory(vm, rm_ptr);    
                }
                break;
            }

            case 0x88: case 0x89: {  // MOV r/m, r
                u64 val = get_op_value(reg_ptr, op_size);
                set_op_value(rm_ptr, val, op_size);
                zero_upper_register_bits(vm, rm_ptr, op_size);
            
                if(vm->disasm_mode){
                    printf("MOV r/m=%lx, reg=%d\n",(mod == 3)?rm:rm_ptr-vm->memory,reg);
                    vm_read_memory(vm, rm_ptr);

                }
                break;
                }
            case 0x8A: case 0x8B: {  // MOV r, r/m
                u64 val = get_op_value(rm_ptr, op_size);
                set_op_value(reg_ptr, val, op_size);
                zero_upper_register_bits(vm, reg_ptr, op_size);
            
                if(vm->disasm_mode){
                    printf("MOV reg=%d, r/m=%lx\n", reg, (mod == 3)?rm:rm_ptr-vm->memory);
                    vm_read_memory(vm, rm_ptr);
                }
                break;
                }
            case 0x8C:  {  // MOV r/m, Sreg
                printf("MOV r/m, Sreg (ignored)\n");
         
                break;  
                }
            // ─── LEA ───
            case 0x8D: {  // LEA r, m
                if (mod == 3) return VM_BAD_OPCODE;  // No reg-reg LEA
                u64 addr = get_effective_address(vm, mod, rm, &pc, &len);  // Recalculate if needed
                u64* dst = get_gpr(vm, reg);
                if (!dst) return VM_SEGFAULT;
                *dst = addr;
            
                if(vm->disasm_mode){
                    printf("LEA reg=%d, r/m=%lx\n", reg, addr);
                    vm_read_memory(vm, vm->memory+addr);
                }
                break;
            }
            case 0x8E:  {  // MOV  Sreg r/m
                printf("MOV Sreg,r/m  (ignored)\n");
              
                break;  
                }
        
            case 0x8F: {  // POP r/m
                if (reg == 0) {  // /0 is POP
                u64 val;
                if (vm->mode == MODE_16BIT) {
                    u8* stack_ptr = get_memory_ptr(vm, vm->r.ss, (u16)vm->r.rsp, op_size);
                    if (!stack_ptr) return VM_STACK_OVERFLOW;
                    val = get_op_value(stack_ptr, op_size);
                    vm->r.rsp = (u16)(vm->r.rsp + op_size);
                } else {
                    val = get_op_value(vm->memory + vm->r.rsp, op_size);
                    vm->r.rsp += op_size;
                }
                set_op_value(rm_ptr, val, op_size);
                zero_upper_register_bits(vm, rm_ptr, op_size);
                if (vm->disasm_mode) printf("POP r/m=%d = %lx\n", rm, val);
                } 
                else return VM_BAD_OPCODE;
                break;
            }
            case 0x90: {  // NOP
              
                if(vm->disasm_mode){
                    printf("NOP\n");
                }
                break;
            }
            case 0x91 ... 0x97: {  // XCHG rax, r64
                u8 reg_idx = (opcode - 0x90) | ((vm->rex & 1) << 3);
                u64* reg = get_gpr(vm, reg_idx);
                if (!reg) return VM_SEGFAULT;
                u64 temp = vm->r.rax;
                vm->r.rax = *reg;
                *reg = temp;
             
                if(vm->disasm_mode){
                    printf("XCHG rax, reg=%d\n", reg_idx);
                }
                break;
            }
            case 0x98: {  // CBW/CWDE/CDQE
                if (op_size == 1) vm->r.rax = (i8)vm->r.rax;
                else if (op_size == 2) vm->r.rax = (i16)vm->r.rax;
                else if (op_size == 4) vm->r.rax = (i32)vm->r.rax;
              
                if(vm->disasm_mode){
                    printf("CBW/CWDE/CDQE (op size %ld)\n", op_size);
                }
                break;
            }
            case 0x99: {  // CWD/CDQ/CQO
                if (op_size == 2) {
                    i16 ax = (i16)vm->r.rax;
                    vm->r.rdx = (ax < 0) ? 0xFFFF : 0x0000;
                } else if (op_size == 4) {
                    i32 eax = (i32)vm->r.rax;
                    vm->r.rdx = (eax < 0) ? 0xFFFFFFFF : 0x00000000;
                } else if (op_size == 8) {
                    i64 rax = (i64)vm->r.rax;
                    vm->r.rdx = (rax < 0) ? 0xFFFFFFFFFFFFFFFF : 0x0000000000000000;
                }
          
                if(vm->disasm_mode){
                    printf("CWD/CDQ/CQO (op size %ld)\n", op_size);
                }
                break;
            
            }
            case 0x9A: {  // CALL ptr16:16/ptr16:32/ptr16:64
                printf("CALL ptr16:XX (ignored)\n");
           

                break;  
            }
            case 0x9B: {  // WAIT/FWAIT
                printf("WAIT/FWAIT (ignored)\n");
              
                break;  
            }
            // ─── Flags: PUSHF/POPF ───
            case 0x9C: {  // PUSHF
                if (vm->r.rsp < 8) return VM_STACK_OVERFLOW;
                vm->r.rsp -= 8;
                if (vm->r.rsp >= vm->mem_size) return VM_SEGFAULT;
                *(u64*)(vm->memory + vm->r.rsp) = vm->r.rflags;
             
                if(vm->disasm_mode){
                    printf("PUSHF (push rflags %lx)\n", vm->r.rflags);
                }
                break;
            }
            case 0x9D: {  // POPF
                if (vm->r.rsp + 8 > vm->mem_size) return VM_SEGFAULT;
                vm->r.rflags = *(u64*)(vm->memory + vm->r.rsp);
                vm->r.rsp += 8;
           
                if(vm->disasm_mode){
                    printf("POPF (pop rflags %lx)\n", vm->r.rflags);
                }
                break;
            }
            case 0x9E: {  // SAHF
                vm->r.rflags = (vm->r.rflags & 0xFFFFFFFFFFFFFF00) | (vm->r.rax & 0xFF);
          
                if(vm->disasm_mode){
                    printf("SAHF (set rflags from AL %02x)\n", (u8)vm->r.rax);
                }
                break;
            }
            case 0x9F: {  // LAHF
                vm->r.rax = (vm->r.rax & 0xFFFFFFFFFFFFFF00) | (vm->r.rflags & 0xFF);
          
                if(vm->disasm_mode){
                    printf("LAHF (load AL from rflags %02x)\n", (u8)vm->r.rflags);
                }
                break; 
            }         
       
           case 0xA0: case 0xA1: {  // MOV AL/AX/EAX/RAX, moffs
                u64 addr = (op_size == 1) ? read_le16(pc) : (op_size == 4) ? read_le32(pc) : read_le64(pc);
                if (addr + op_size > vm->mem_size) return VM_SEGFAULT;
                u64 val = *(u64*)(vm->memory + addr);
                set_op_value((u8*)&vm->r.rax, val, op_size);
                zero_upper_register_bits(vm, (u8*)&vm->r.rax, op_size);
                len += (op_size == 1) ? 2 : (op_size == 4) ? 5 : 9;  // opcode + offset
             
                if(vm->disasm_mode){
                    printf("MOV AL/AX/EAX/RAX, moffs (addr %lx) = %lx\n", addr, val);
                }
                break;
            }
            case 0xA2: case 0xA3: {  // MOV moffs, AL/AX/EAX/RAX
                u64 addr = (op_size == 1) ? read_le16(pc) : (op_size == 4) ? read_le32(pc) : read_le64(pc);
                if (addr + op_size > vm->mem_size) return VM_SEGFAULT;
                u64 val = get_op_value((u8*)&vm ->r.rax, op_size);
                *(u64*)(vm->memory + addr) = val;
                len += (op_size == 1) ? 2 : (op_size == 4) ? 5 : 9;  // opcode + offset
            
                if(vm->disasm_mode){
                    printf("MOV moffs, AL/AX/EAX/RAX (addr %lx) = %lx\n", addr, val);
                }
                break;
            }
            case 0xA4: case 0xA5: { // MOVS BYTE/WORD
                bool byte_op = (opcode == 0xA4);
                sz opsz = byte_op ? 1 : 2;
                bool rep = false; // we'll handle REP later

                u16 src_seg = vm->r.ds;
                u16 src_off = vm->r.rsi & 0xFFFF;
                u16 dst_seg = vm->r.es;
                u16 dst_off = vm->r.rdi & 0xFFFF;

                u8* src_ptr = get_memory_ptr(vm, src_seg, src_off, opsz);
                u8* dst_ptr = get_memory_ptr(vm, dst_seg, dst_off, opsz);
                if (!src_ptr || !dst_ptr) return VM_SEGFAULT;

                u64 val = get_op_value(src_ptr, opsz);
                set_op_value(dst_ptr, val, opsz);

                // Direction flag
                i16 delta = (vm->r.rflags & FLAG_DF) ? -opsz : opsz;
                vm->r.rsi = (vm->r.rsi & ~0xFFFF) | ((vm->r.rsi + delta) & 0xFFFF);
                vm->r.rdi = (vm->r.rdi & ~0xFFFF) | ((vm->r.rdi + delta) & 0xFFFF);

                if (vm->disasm_mode) {
                    printf("MOVS%s [%04x:%04x] -> [%04x:%04x]\n",
                        byte_op ? "B" : "W", src_seg, src_off, dst_seg, dst_off);
                }
                break;
            }
            case 0xA6: case 0xA7: {
                printf("MOVS (ignored)\n");
            
                break;  
                }
            case 0xA8: case 0xA9: {  // TEST AL/AX/EAX/RAX, imm
                u64 a = get_op_value((u8*)&vm->r.rax, op_size);
                u64 imm = (op_size == 1) ? *pc :(op_size == 2 ? read_le16(pc) : (op_size == 4 ? read_le32(pc) : read_le64(pc)));
                u64 res = a & imm;      
                set_flags_basic(vm, res, op_size);
                vm->r.rflags &= ~FLAG_CF & ~FLAG_OF;    
                len += op_size;
         
                if(vm->disasm_mode){
                    printf(" TEST AL/AX/EAX/RAX, imm=%lx\n", imm);
                }
                break;
            }
            case 0xAA: case 0xAB: case 0xAC: case 0xAD: {
                printf("STOS (ignored)\n");
           
                break;  
                }   
            case 0xAE: {
                printf("SCAS (ignored)\n");
        
                break;  
                }
    
             // ─── Multiplication: IMUL (extended opcode) ───
            case 0xAF: {
                if (is_extended) {  // IMUL r, r/m
                    u64 a = get_op_value(reg_ptr, op_size);  // src_ptr is reg
                    u64 b = get_op_value(rm_ptr, op_size);  // dst_ptr is r/m
                    i64 res = (i64)a * (i64)b;  // Signed multiply
                    set_op_value(reg_ptr, res, op_size);  // Result to reg
                    // Flags: OF/CF if result doesn't fit in operand size
                    if ((i64)res != (i64)(res & ((1ULL << (op_size * 8)) - 1))) {
                        vm->r.rflags |= FLAG_OF | FLAG_CF;
                    } else {
                        vm->r.rflags &= ~(FLAG_OF | FLAG_CF);
                    }
                    if(vm->disasm_mode){
                        printf("IMUL reg=%d, r/m=%lx:  = %lx * %lx = %lx\n",reg, (mod == 3)?rm:rm_ptr-vm->memory,  a, b, res);
                    }
                }else{
                    printf("SCASW, ignored)\n");
                };
                 
                
                    break;
                }


            case 0xB0 ... 0xB7: {  //B0–B7  (mov r8b–r15b / al–bh, imm8)
                u8 reg_idx = (opcode - 0xB0) | ((vm->rex & 1) << 3);
                u64* dst = get_gpr(vm, reg_idx);
                if (!dst) return VM_SEGFAULT;
                u64 imm = *pc++;
                len += 1;
                *(u8*)dst = (u8)imm;
                // No zero-upper for 8-bit writes (matches real x86 behavior for low 8-bit regs)
             
                if(vm->disasm_mode){
                    printf("MOV  reg=%d,imm=%02x\n", reg_idx, (u8)imm);
                }
                break;
            }
            case 0xB8 ... 0xBF: {  // MOV reg, imm
               u8 reg_idx = (opcode - 0xB8) | ((vm->rex & 1) << 3);
                u64* dst = get_gpr(vm, reg_idx);
                if (!dst) return VM_SEGFAULT;

                // Use full operand size logic (respects mode, 0x66 prefix, and REX.W)
                sz op_size = get_op_size(vm, false, operand_override);

                u64 imm = 0;
                if (op_size == 1) imm = *pc++;
                else if (op_size == 2) imm = read_le16(pc), pc += 2;
                else if (op_size == 4) imm = read_le32(pc), pc += 4;
                else if (op_size == 8) imm = read_le64(pc), pc += 8;

                len += op_size;
                *dst = imm;
                zero_upper_register_bits(vm, (u8*)dst, op_size);
              
                if(vm->disasm_mode){
                    printf("MOV  reg=%d,imm=%lx\n", reg_idx, imm);
                }
                break;
            }
           case 0xC0: case 0xC1: {  // ROL/ROR r/m8, imm8
                if (!is_extended || vm->mode == MODE_16BIT) {
                    u64 val = get_op_value(rm_ptr, 1);
                    u8 shift = *pc % 8;
                    u64 res;
                    if (opcode == 0xC0) {  // ROL
                        res = (val << shift) | (val >> (8 - shift));
                    } else {  // ROR
                        res = (val >> shift) | (val << (8 - shift));
                    }
                    set_op_value(rm_ptr, res, 1);
                    vm->r.rflags = (vm->r.rflags & ~FLAG_CF) | ((res & 1) ? FLAG_CF : 0);
                    len += 1;
               
                    if(vm->disasm_mode){
                        printf("ROL/ROR r/m8=%lx, imm8: val=%02x shift=%d res=%02x\n", (mod == 3)?rm:rm_ptr-vm->memory, (u8)val, shift, (u8)res);
                    }
                } else {
                    printf("RCL/RCR r/m8, imm8 (ignored)\n");
                    vm->r.rip +=  1;  // Still consume imm8
                }
                break;
            }
            case 0xC2: {  // RET imm16
                if (vm->r.rsp + 8 > vm->mem_size) return VM_SEGFAULT;
                vm->r.rip = *(u64*)(vm->memory + vm->r.rsp);
                vm->r.rsp += 8 + read_le16(pc);  // Pop return address + imm16
                len += 2;  // imm16
                if(vm->disasm_mode){
                    printf("RET imm16 with imm=%d\n", read_le16(pc));
                }
                break;
            }
            case 0xC3: {  // RET
                    if (vm->r.rsp + 8 > vm->mem_size) return VM_SEGFAULT;
                    vm->r.rip = *(u64*)(vm->memory + vm->r.rsp);
                    len-=1;
                    vm->r.rsp += 8;
                    if(vm->disasm_mode){
                        printf("RET\n");
                    }
                    break;
                }
            case 0xC4: case 0xC5: {  // LES/LDS r16/32, m
                printf("LES/LDS (ignored)\n");
             
                break;  
                }

            case 0xC6: case 0xC7: {  // MOV r/m, imm
                sz imm_size = (op_size == 8) ? 4 : op_size;
                u64 raw = (imm_size == 1) ? *pc : (imm_size == 2 ? read_le16(pc) : read_le32(pc));
                i64 imm = (i64)raw;
                if (imm_size < 4) imm = (imm_size == 1) ? (i64)(i8)raw : (i64)(i16)raw;
                else imm = (i64)(i32)raw;
                set_op_value(rm_ptr, imm, op_size);
                zero_upper_register_bits(vm, rm_ptr, op_size);
                len += imm_size;
           
                if(vm->disasm_mode){
                    printf("MOV r/m=%lx, imm=%lx\n", (mod == 3)?rm:rm_ptr-vm->memory, imm);
                    vm_read_memory(vm, rm_ptr);
                }
                break;
                }
            case 0XC8: {  // ENTER imm16, imm8
                if (vm->r.rsp < 16) return VM_STACK_OVERFLOW;
                vm->r.rsp -= 8;
                if (vm->r.rsp >= vm->mem_size) return VM_SEGFAULT;
                *(u64*)(vm->memory + vm->r.rsp) = vm->r.rbp;  // Push old RBP
                u16 alloc_size = read_le16(pc);
                u8 nesting = *pc;
                len += 3;  // imm16 + imm8
                if (nesting > 0) {
                    for (u8 i = 1; i < nesting; i++) {
                        if (vm->r.rsp < 8) return VM_STACK_OVERFLOW;
                        vm->r.rsp -= 8;
                        if (vm->r.rsp >= vm->mem_size) return VM_SEGFAULT;
                        *(u64*)(vm->memory + vm->r.rsp) = *(u64*)(vm->memory + vm->r.rsp + 8);  // Push previous frame pointer
                    }
                }
                vm->r.rbp = vm->r.rsp;
                if (alloc_size > 0) {
                    if (vm->r.rsp < alloc_size) return VM_STACK_OVERFLOW;
                    vm->r.rsp -= alloc_size;
                    if (vm->r.rsp >= vm->mem_size) return VM_SEGFAULT;
                }
                vm->r.rip += len;
                if(vm->disasm_mode){
                    printf("ENTER imm16 %d, imm8 %d\n", alloc_size, nesting);
                }
                break;
            }
            case 0xC9: {  // LEAVE
                vm->r.rsp = vm->r.rbp;
                if (vm->r.rsp + 8 > vm->mem_size) return VM_SEGFAULT;
                vm->r.rbp = *(u64*)(vm->memory + vm->r.rsp);
                vm->r.rsp += 8;
            
                if(vm->disasm_mode){
                    printf("LEAVE\n");
                }
                break;
            }
            case 0xCA: {  // RETF imm16
                if (vm->r.rsp + 16 > vm->mem_size) return VM_SEGFAULT;
                u64 new_rip = *(u64*)(vm->memory + vm->r.rsp);
               // u64 new_cs = *(u64*)(vm->memory + vm->r.rsp + 8);
                // For simplicity, we won't actually change segments in this VM
                vm->r.rip = new_rip;
                vm->r.rsp += 16 + read_le16(pc);  // Pop return address + imm16
                len += 2;  // imm16
                if(vm->disasm_mode){
                    printf("RETF imm16 with imm %d\n", read_le16(pc));
                }
                break;
            }
            case 0xCB: {  // RETF
                if (vm->r.rsp + 16 > vm->mem_size) return VM_SEGFAULT;
                u64 new_rip = *(u64*)(vm->memory + vm->r.rsp);
               // u64 new_cs = *(u64*)(vm->memory + vm->r.rsp + 8);
                // For simplicity, we won't actually change segments in this VM
                vm->r.rip = new_rip;
               
                vm->r.rsp += 16;
                if(vm->disasm_mode){
                    printf("RETF\n");
                }
                break;
            }
            case 0xCC: {  // INT3
                printf("INT3 (breakpoint) encountered\n");
       
                break;
            }
            case 0xCD: {  // INT imm8
                u8 int_num = *pc;
                printf("INT %d (ignored)\n", int_num);
                len += 1;
           
                break;
            }
            case 0xCE: {  // INTO
                printf("INTO (ignored)\n");
            
                break;  
                }
     
            case 0xCF:{
                    if (is_extended || vm->mode == MODE_16BIT) {  // IRET
                        if (vm->r.rsp + 24 > vm->mem_size) return VM_SEGFAULT;
                        u64 new_rip = *(u64*)(vm->memory + vm->r.rsp);
                       // u64 new_cs = *(u64*)(vm->memory + vm->r.rsp + 8);
                        u64 new_rflags = *(u64*)(vm->memory + vm->r.rsp + 16);
                        // For simplicity, we won't actually change segments in this VM
                        vm->r.rip = new_rip;
                        vm->r.rflags = new_rflags;
                        vm->r.rsp += 24;
                        if(vm->disasm_mode){
                            printf("IRET: new RIP %lx, new RFLAGS %lx\n", new_rip, new_rflags);
                        }
                       
                    }else{
                     
                    }
                     break;
            }
            case 0xD0:{
                if (is_extended || vm->mode == MODE_16BIT) {  // ROL r/m8, 1
                    u64 val = get_op_value(rm_ptr, 1);
                    u64 res = (val << 1) | (val >> 7);
                    set_op_value(rm_ptr, res, 1);
                    vm->r.rflags = (vm->r.rflags & ~FLAG_CF) | ((res & 1) ? FLAG_CF : 0);
                   if(vm->disasm_mode){
                        printf("ROL r/m=%lx, 1: val=%02x res=%02x\n", (mod == 3)?rm:rm_ptr-vm->memory, (u8)val, (u8)res);
                    }
                  }
                 
                     break;
            }
        case 0xD1: {
            if (is_extended || vm->mode == MODE_16BIT) {  // ROL r/m16/32/64, 1
                u64 val = get_op_value(rm_ptr, op_size);
                u64 res = (val << 1) | (val >> ((op_size * 8) - 1));
                set_op_value(rm_ptr, res, op_size);
                vm->r.rflags = (vm->r.rflags & ~FLAG_CF) | ((res & 1) ? FLAG_CF : 0);
               if(vm->disasm_mode){
                    printf("ROL r/m16/32/64=%lx, 1: val=%lx res=%lx\n", (mod == 3)?rm:rm_ptr-vm->memory, val, res);
                }
            }
        
             break;
        }   
        case 0xD2: {
            if (is_extended || vm->mode == MODE_16BIT) {  // ROL r/m8, CL
                u64 val = get_op_value(rm_ptr, 1);
                u8 shift = get_op_value((u8*)&vm->r.rcx, 1) % 8;
                u64 res = (val << shift) | (val >> (8 - shift));
                set_op_value(rm_ptr, res, 1);
                vm->r.rflags = (vm->r.rflags & ~FLAG_CF) | ((res & 1) ? FLAG_CF : 0);
               if(vm->disasm_mode){
                    printf("ROL r/m=%lx, CL: val=%02x shift=%d res=%02x\n", (mod == 3)?rm:rm_ptr-vm->memory, (u8)val, shift, (u8)res);
                }
            }
           
            break;
        }
        case 0xD3: {
            if (is_extended || vm->mode == MODE_16BIT) {  // ROL r/m16/32/64, CL
                u64 val = get_op_value(rm_ptr, op_size);
                u8 shift = get_op_value((u8*)&vm->r.rcx, 1) % (op_size * 8);
                u64 res = (val << shift     ) | (val >> ((op_size * 8) - shift));
                set_op_value(rm_ptr, res, op_size);
                vm->r.rflags = (vm->r.rflags & ~FLAG_CF) | ((res & 1) ? FLAG_CF : 0);
                if(vm->disasm_mode){
                      printf("ROL r/m=%lx, CL: val=%lx shift=%d res=%lx\n", (mod == 3)?rm:rm_ptr-vm->memory, val, shift, res);
                 }
            }
             
                break;
            }
        case 0xD4: case 0xD5: {
            printf("AAM/AAD (ignored)\n");
            vm->r.rip +=   1;  // Still consume imm8
            break;  
            }
        case 0xD6: {
            printf("SALC (ignored)\n");
         
            break;  
            }
        case 0xD7: {
            printf("XLAT (ignored)\n");
        
            break;  
            }
        case 0xD8 ... 0xDF: {
            printf("x87 FPU instructions (ignored)\n");
       
            break;  
            }
        
            

        // ─── Loops (from vm_v1) ───
        case 0xE0: {  // LOOPNE rel8
            vm->r.rcx--;
            i8 disp = read_disp8(pc);
            len++;
            if (vm->r.rcx != 0 && !(vm->r.rflags & FLAG_ZF)) vm->r.rip += (i64)disp;
        
            if(vm->disasm_mode){
                printf("LOOPNE rel8 with disp %d, RCX now %ld\n", disp, vm->r.rcx);
            }
            break;
        }
        case 0xE1: {  // LOOPE rel8
            vm->r.rcx--;
            i8 disp = read_disp8(pc);
            len++;
            if (vm->r.rcx != 0 && (vm->r.rflags & FLAG_ZF)) vm->r.rip += (i64)disp;
         
            if(vm->disasm_mode){
                printf("LOOPE rel8 with disp %d, RCX now %ld\n", disp, vm->r.rcx);
            }
            break;
        }
        case 0xE2: {  // LOOP rel8
            vm->r.rcx--;
            i8 disp = read_disp8(pc);
            len++;
            if (vm->r.rcx != 0) vm->r.rip += (i64)disp;
 
            if(vm->disasm_mode){
                printf("LOOP rel8 with disp %d,  RCX is %ld\n", disp, vm->r.rcx);
            }
            break;
        }
        case 0xE3: {  // JECXZ rel8
            i8 disp = read_disp8(pc);
            len++;
            if (vm->r.rcx == 0) vm->r.rip += (i64)disp;
           
            if(vm->disasm_mode){
                printf("JECXZ rel8 with disp %d, RCX is %ld\n", disp, vm->r.rcx);
            }
            break;
        }
        case 0xE4: case 0xE5: {  // IN AL/AX, imm8
            u8 port = *pc;
            len++;
            // Stub: Just print for now (can expand later)
            printf("IN from port %02x\n", port);
            
            if(vm->disasm_mode){
                printf("IN AL/AX, imm8 with port %02x\n", port);
            }
            break;
        }
        case 0xE6: case 0xE7: {  // OUT imm8, AL/AX
            u8 port = *pc;
            len++;
            // Stub: Just print for now (can expand later)
            printf("OUT to port %02x\n", port);
       
            if(vm->disasm_mode){
                printf("OUT imm8, AL/AX with port %02x\n", port);
            }
            break;
        }
           // ─── Call/Ret ───
        case 0xE8: {  // CALL rel32
            if (vm->r.rip + len + 4 > vm->mem_size) return VM_SEGFAULT;
            i32 disp = (i32)read_le32(pc);
            len += 4;
            if (vm->r.rsp < 8) return VM_STACK_OVERFLOW;
            vm->r.rsp -= 8;
            if (vm->r.rsp >= vm->mem_size) return VM_SEGFAULT;
            *(u64*)(vm->memory + vm->r.rsp) = vm->r.rip + len;  // Push return addr
            
            if(vm->disasm_mode){
                printf("CALL rel32 with disp=%d, return addr=0x%lx\n", disp, vm->r.rip + len);
            }
            vm->r.rip += (i64)disp;
            
      
            break;
        }
        case 0xE9: {  // JMP rel32
            if (vm->r.rip + len + 4 > vm->mem_size) return VM_SEGFAULT;
            i32 disp = (i32)read_le32(pc);
            len += 4;
            vm->r.rip +=  (i64)disp;
            if(vm->disasm_mode){
                printf("JMP rel32 with disp=%d\n", disp);
            }
            break;
        }
        case 0xEA: {  // JMP ptr16:16/ptr16:32/ptr16:64
            printf("JMP ptr16:XX (ignored)\n");
         
            if(vm->disasm_mode){
                printf("JMP ptr16:XX (ignored)\n");
            }
            break;  
        }
        case 0xEB: {  // JMP short rel8
            if (vm->r.rip + len + 1 >= vm->mem_size) return VM_SEGFAULT;
            i8 disp = read_disp8(pc);
            len++;
            vm->r.rip +=  (i64)disp;
            if(vm->disasm_mode){
                printf("JMP short rel8 with disp=%d\n", disp);
            }
            break;
        }
        case 0xEC: case 0xED: {  // IN AL/AX, DX
            // Stub: Just print for now (can expand later)
            printf("IN from port DX\n");
       
            break;
        }
        case 0xEE: case 0xEF: {  // OUT DX, AL/AX
            // Stub: Just print for now (can expand later)
            printf("OUT to port DX\n");
          
            break;
        }
        case 0xF0: {  // LOCK prefix
            printf("LOCK prefix (ignored)\n");
    
            break;  
            }
        case 0xF1: // INT1   
            {  
            printf("INT1 (ignored)\n");
      
            break;
            }
        case 0xF2: {  // REPNE/REPNZ prefix
            printf("REPNE/REPNZ prefix (ignored)\n");       
 
            break;  
            }
        case 0xF3: {  // REP/REPE/REPZ prefix
            printf("REP/REPE/REPZ prefix (ignored)\n");         
          
            break;  
            }   
        case 0xF4: {  // HLT
            if(vm->disasm_mode){
             printf("HLT encountered, halting VM\n"); 
            }
            return VM_HALT;
         }
        case 0xF5: {  // CMC
            vm->r.rflags ^= FLAG_CF;
          
            if(vm->disasm_mode){
                printf("CMC, new CF=%d\n", (vm->r.rflags & FLAG_CF) ? 1 : 0);
            }
            break;
        } 
        case 0xF6: {  // TEST/NOT/NEG/MUL/IMUL/DIV/IDIV r/m8 (reg field determines specific op)
            switch (reg) {
                case 0: {  // TEST r/m8, imm8
                    u64 val = get_op_value(rm_ptr, 1);
                    u8 imm = *pc;
                    len++;
                    u64 res = val & imm;
                    set_flags_basic(vm, res, 1);
                    vm->r.rflags &= ~FLAG_CF & ~FLAG_OF;  
                
                    if(vm->disasm_mode){
                        printf("TEST r/m8=%lx, imm8: val=%02x imm=%02x res=%02x\n", (mod == 3)?rm:rm_ptr-vm->memory, (u8)val, imm, (u8)res);
                    }
                    break;
                }
                case 1: {  // TEST r/m8, imm8 (same as reg=0 but sign-extended imm)
                    u64 val = get_op_value(rm_ptr, 1);
                    i8 imm = (i8)*pc;
                    len++;
                    u64 res = val & (u64)(i64)imm;
                    set_flags_basic(vm, res, 1);
                    vm->r.rflags &= ~FLAG_CF & ~FLAG_OF;  
                
                    if(vm->disasm_mode){
                        printf("TEST r/m8=%lx, imm8 (sign-extended): val=%02x imm=%02x res=%02x\n", (mod == 3)?rm:rm_ptr-vm->memory, (u8)val, (u8)imm, (u8)res);
                    }
                    break;
                }
                // Other reg values (2-7) would be for NOT/NEG/MUL/IMUL/DIV/IDIV
                default: return VM_BAD_OPCODE;
            }
            break;
        }
        case 0xF7: {  // NOT r/m (reg=2)
            if (reg != 2) return VM_BAD_OPCODE;
            u64 val = get_op_value(rm_ptr, op_size);
            u64 res = ~val;
            set_op_value(rm_ptr, res, op_size);
      
            if(vm->disasm_mode){
                printf("NOT r/m=%lx: val=%lx res=%lx\n", (mod == 3)?rm:rm_ptr-vm->memory, val, res);
            }
            break;
        }
        case 0xF8: {  // CLC
            vm->r.rflags &= ~FLAG_CF;
        
            if(vm->disasm_mode){
                printf("CLC, new CF=0\n");
            }
            break;
        }
        case 0xF9: {  // STC
            vm->r.rflags |= FLAG_CF;
        
            if(vm->disasm_mode){
                printf("STC, new CF=1\n");
            }
            break; }
        case 0xFA: {  // CLI
            vm->r.rflags &= ~FLAG_IF;
        
            if(vm->disasm_mode){
                printf("CLI, new IF=0\n");
            }
            break; }
        case 0xFB: {  // STI
            vm->r.rflags |= FLAG_IF;    
         
            if(vm->disasm_mode){
                printf("STI, new IF=1\n");
            }
            break; }
        case 0xFC: {  // CLD
            vm->r.rflags &= ~FLAG_DF;
          
            if(vm->disasm_mode){
                printf("CLD, new DF=0\n");
            }
            break; }
        case 0xFD: {  // STD
            vm->r.rflags |= FLAG_DF;
       
            if(vm->disasm_mode){
                printf("STD, new DF=1\n");
            }
            break; } 
        case 0xFE: {  // INC/DEC r/m8 (reg=0 for INC, 1 for DEC)
            if (reg == 0 || reg == 1) {
                u64 val = get_op_value(rm_ptr, 1);
                u64 res = (reg == 0) ? val + 1 : val - 1;
                set_op_value(rm_ptr, res, 1);
                set_flags_add_sub(vm, val, 1, res, (reg == 1), 1);
            
                if(vm->disasm_mode){
                    printf("%s r/m8=%lx: val=%02x res=%02x\n", (reg == 0) ? "INC" : "DEC", (mod == 3)?rm:rm_ptr-vm->memory  , (u8)val, (u8)res);
                    vm_read_memory(vm, rm_ptr);  // Read memory value at rm_ptr
                }
            } else return VM_BAD_OPCODE;
            break;
        }   
        case 0xFF: {  // INC/DEC
                   // printf("0xFF opcode with reg=%d\n", reg);
                    if (reg == 6) {  // /6 is PUSH
                        u64 val = get_op_value(rm_ptr, op_size);
                        if (vm->mode == MODE_16BIT) {
                            vm->r.rsp = (u16)(vm->r.rsp - op_size);
                            u8* stack_ptr = get_memory_ptr(vm, vm->r.ss, (u16)vm->r.rsp, op_size);
                            if (!stack_ptr) return VM_STACK_OVERFLOW;
                            set_op_value(stack_ptr, val, op_size);
                        } else {
                            vm->r.rsp -= op_size;
                            set_op_value(vm->memory + vm->r.rsp, val, op_size);
                        }
                        if (vm->disasm_mode) printf("PUSH r/m=%d = %lx\n", rm, val);
                    }
                    else if (reg == 0 || reg == 1) {
                    u64 val = get_op_value(rm_ptr, op_size);
                    u64 res = (reg == 0) ? val + 1 : val - 1;
                    set_op_value(rm_ptr, res, op_size);
                    zero_upper_register_bits(vm, rm_ptr, op_size);
                    set_flags_add_sub(vm, val, 1, res, (reg == 1), op_size);
                  
                    if(vm->disasm_mode){
                        printf("%s r/m16/32/64=%lx: val=%lx res=%lx\n", (reg == 0) ? "INC" : "DEC", (mod == 3)?rm:rm_ptr-vm->memory, val, res);
                    } 
                   }
                    else return VM_BAD_OPCODE;
                    break;
                }
        default: return VM_BAD_OPCODE;
        }
        if (vm->mode == MODE_16BIT) {
                vm->r.ip += len;
                vm->r.rip = get_physical_address(vm, vm->r.cs, vm->r.ip);
            } else {
                vm->r.rip += len;
            }
        if(vm->disasm_mode){
            if (vm->mode == MODE_16BIT) {
                printf("DISASM %x:%lxx [%s]: ", vm->r.cs, vm->r.ip-len, "16");
            } else {
                printf("DISASM @ 0x%lx [%s]: ", vm->r.rip-len,
                    vm->mode == MODE_32BIT ? "32" : "64");
            }
            for (sz i = 0; i < len; i++) printf("%02x ", original_pc[i]);
            printf(" , Executed opcode %02x at len %ld\n", opcode, len);
            vm_dump_registers(vm);
            printf("\n");
        }
     
        return VM_OK;
}
// vm_run, dumps unchanged
VmStatus vm_run(VM* vm) {
    VmStatus status;
    while ((status = step(vm)) == VM_OK) {}
        return status;
    }
void vm_dump_registers(const VM* vm) {
    // unchanged, but add mode
    const Registers *r = &vm->r;
    if (vm->mode == MODE_16BIT) {
    printf("CS=%04x DS=%04x ES=%04x SS=%04x IP=%04x\n",
           r->cs, r->ds, r->es, r->ss, r->ip);
    printf("AX=%04x BX=%04x CX=%04x DX=%04x\n",
           (u16)r->rax, (u16)r->rbx, (u16)r->rcx, (u16)r->rdx);
    printf("SI=%04x DI=%04x BP=%04x SP=%04x\n",
           (u16)r->rsi, (u16)r->rdi, (u16)r->rbp, (u16)r->rsp);
   } else {
    printf("Mode: %s\n", vm->mode == MODE_16BIT ? "16-bit" : vm->mode == MODE_32BIT ? "32-bit" : "64-bit");
      
    printf("RAX=%016lx  RBX=%016lx  RCX=%016lx  RDX=%016lx\n",
           r->rax, r->rbx, r->rcx, r->rdx);
    printf("RSI=%016lx  RDI=%016lx  RBP=%016lx  RSP=%016lx\n",
           r->rsi, r->rdi, r->rbp, r->rsp);
    printf(" R8=%016lx   R9=%016lx  R10=%016lx  R11=%016lx\n",
           r->r8,  r->r9,  r->r10,  r->r11);
   
    }
     printf("RIP=%016lx RFLAGS=%016lx  [ CF=%d  ZF=%d  SF=%d  OF=%d ]\n",
           r->rip, r->rflags,
           (int)!!(r->rflags & FLAG_CF),
           (int)!!(r->rflags & FLAG_ZF),
           (int)!!(r->rflags & FLAG_SF),
           (int)!!(r->rflags & FLAG_OF));
}
void vm_dump_memory(const VM* vm, sz bytes) {
    sz lim = bytes < vm->mem_size ? bytes : vm->mem_size;
        printf("Memory first %zu bytes:\n", lim);
        for (sz i = 0; i < lim; i++) {
            printf("%02x ", vm->memory[i]);
            if ((i & 15) == 15) printf("\n");
        }
        printf("\n");
}

void vm_read_memory(const VM* vm, u8* addr) {
    if (addr < vm->memory+vm->mem_size) {
        printf("Memory at %p: 0x%lx\n", addr, *(u64*)addr);
    } else {
        printf("Memory read out of bounds at %p\n", addr);
    }   
}

// ───────────────────────────────────────────────
// Extended test main demonstrating all modes
// ───────────────────────────────────────────────
int main(void) {
sz mem = 1 << 20;
// 64-bit test (includes original complex program)
VM* vm64 = vm_create(mem);
vm_set_disasm_mode(vm64, false);
static const u8 prog64[] = {
// Original complex test with 64-bit features
0x48, 0xB8, 0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11,
0x90,
0x05, 0x00, 0x22, 0x11, 0x44,
0x48, 0xFF, 0xC0,
0x48, 0x8B, 0x05, 0x00, 0x00, 0x00, 0x00,  // RIP-relative
0x48, 0xBB, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x48, 0xC7, 0xC1, 0x02, 0x00, 0x00, 0x00,
0x48, 0x88, 0x44, 0x8B, 0x08, 
0x48, 0x31, 0xC0, //XOR rax, rax
0x48, 0x8B, 0x44, 0x8B, 0x08,  // SIB
0x48, 0xC7, 0xC2, 0x0A, 0x00, 0x00, 0x00,  // MOV RDX 10
0x48, 0xC7, 0xC1, 0x05, 0x00, 0x00, 0x00,  // factorial setup mov RCX ,5
0xE8, 0x0A, 0x00, 0x00, 0x00, //CALL    
0xF4,
0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90, //nop
0x48, 0x0F, 0xAF, 0xD1, //IMUL RDX, RCX
0x48, 0xFF, 0xC9, //DEC RCX
0x75, 0xF7, //JNZ back to IMUL
0xC3  //RET
};
vm_load_program(vm64, prog64, sizeof(prog64));
printf("=== 64-bit mode ===\n");
VmStatus s64 = vm_run(vm64);
printf("Status: %d\n", s64);
//vm_dump_memory(vm64, 64);
vm_destroy(vm64);
// 32-bit mode
VM* vm32 = vm_create(mem);
vm_set_mode(vm32, MODE_32BIT);
vm_set_disasm_mode(vm32, false);
static const u8 prog32[] = {
0xB8, 0x88, 0x77, 0x66, 0x55,        // mov eax, 0x55667788
0x05, 0x00, 0x22, 0x11, 0x44,        // add eax, 0x44112200
0xFF, 0xC0,                          // inc eax
0xBB, 0x00, 0x10, 0x00, 0x00,        // mov ebx, 0x1000
0xC7, 0x03, 0x11, 0x22, 0x33, 0x44,  // mov dword [ebx], 0x44332211
0x8B, 0x03,                          // mov eax, [ebx]
0xF4
};
vm_load_program(vm32, prog32, sizeof(prog32));
printf("\n=== 32-bit mode ===\n");
VmStatus s32 = vm_run(vm32);
printf("Status: %d\n", s32);
//vm_dump_registers(vm32);
//vm_dump_memory(vm32, 32);
vm_destroy(vm32);
// 16-bit mode
VM* vm16 = vm_create(mem);
vm_set_mode(vm16, MODE_16BIT);
vm_set_disasm_mode(vm16, true);
vm16->r.ds = 0x1000;  // Data at physical 0x10000+
vm16->r.ss = 0x2000;  // Stack at 0x20000+
vm16->r.rsp = 0xFFFE;  // SP top
static const u8 prog16[] = {
0xB8, 0x88, 0x77,                    // mov ax, 0x7788
0x05, 0x00, 0x22,                    // add ax, 0x2200
 
0xB2, 0x30,  // mov dl, 0x30
0x8E, 0xD2,  // mov ss, dx
0xFF, 0xC0,                          // inc ax
0xBB, 0x00, 0x10,                    // mov bx, 0x1000
0xC7, 0x03, 0x11, 0x22,              // mov word [bx], 0x2211
0x8B, 0x03,                          // mov ax, [bx]
0x50,  // push ax
//xor ax, ax
0x33, 0xC0,  // xor ax, ax
0x58,  // pop ax
// Test [bp]: set bp=0x10, mov [bp], ax (should use SS)
0xBD, 0x10, 0x00,  // mov bp, 0x10
0x89, 0x46, 0x00,  // mov [bp], ax
//xor bx, bx
0x33, 0xDB,  // xor bx, bx
0x8B, 0x5E, 0x00,  // mov bx, [bp]
0x89, 0x44, 0x1E,  // mov [bx+bp], ax
0x8B, 0x74, 0x1E,  // mov si, [bx+bp]
0x4E,  // dec si    
0x47,  // inc di
0x89, 0x7C, 0x34,   // mov [bx+si], di
0xF4
};
vm_load_program(vm16, prog16, sizeof(prog16));
printf("\n=== 16-bit mode ===\n");
VmStatus s16 = vm_run(vm16);
printf("Status: %d\n", s16);
//vm_dump_registers(vm16);
vm_dump_memory(vm16, 32);
vm_destroy(vm16);
// Operand override in 64-bit
VM* vm_ov = vm_create(mem);
vm_set_disasm_mode(vm_ov, true);
static const u8 prog_ov[] = {
0x66, 0xB8, 0x88, 0x77,               // mov ax, 0x7788
0x66, 0x05, 0x00, 0x22,               // add ax, 0x2200
0x66, 0xFF, 0xC0,                     // inc ax
0xF4
};
vm_load_program(vm_ov, prog_ov, sizeof(prog_ov));
printf("\n=== 64-bit with 0x66 override ===\n");
VmStatus sov = vm_run(vm_ov);
printf("Status: %d\n", sov);
//vm_dump_registers(vm_ov);
vm_destroy(vm_ov);
// Bad REX in 32-bit (should hit VM_BAD_OPCODE)
VM* vm_bad = vm_create(mem);
vm_set_mode(vm_bad, MODE_32BIT);
vm_set_disasm_mode(vm_bad, true);
static const u8 bad_prog[] = {
0x49,0xB8,0xF4, 0x90, 0x49, 0x90,0x90, 0xB8,0x90,0x00,0xB8,0x90,
0xF4
};
vm_load_program(vm_bad, bad_prog, sizeof(bad_prog));
printf("\n=== Bad REX in 32-bit (expect BAD_OPCODE) ===\n");
vm_dump_memory(vm_bad, 16);
VmStatus sbad = vm_run(vm_bad);
printf("Status: %d %s\n", sbad, sbad == VM_BAD_OPCODE ? "(expected)" : "");
printf("\n=== Same opcode in 16-bit ===\n");
vm_set_mode(vm_bad, MODE_16BIT);
sbad = vm_run(vm_bad);
printf("Status: %d %s\n", sbad, sbad == VM_UNKNOWN_ERROR ? "(expected,opcode end wtih 0X00(should be 0xF4))" : "");
vm_destroy(vm_bad);
return 0;
}