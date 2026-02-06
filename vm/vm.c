#include "vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// ───────────────────────────────────────────────
// Helper Functions
// ───────────────────────────────────────────────

// Read little-endian values from memory
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

// Get general-purpose register by index (0-15)
static inline u64* get_gpr(VM* vm, u8 idx) {
    switch (idx & 0xF) {
        case 0: return &vm->r.rax; case 1: return &vm->r.rcx;
        case 2: return &vm->r.rdx; case 3: return &vm->r.rbx;
        case 4: return &vm->r.rsp; case 5: return &vm->r.rbp;
        case 6: return &vm->r.rsi; case 7: return &vm->r.rdi;
        case 8: return &vm->r.r8;  case 9: return &vm->r.r9;
        case 10: return &vm->r.r10; case 11: return &vm->r.r11;
        case 12: return &vm->r.r12; case 13: return &vm->r.r13;
        case 14: return &vm->r.r14; case 15: return &vm->r.r15;
        default: 
          {
            printf("Invalid GPR index: %d\n", idx);
            return NULL;
          }
    }
}

// Determine operand size (1, 4, or 8 bytes) based on REX.W and byte-op flag
static inline sz get_op_size(VM* vm, bool is_byte_op) {
    if (is_byte_op) return 1;
    return (vm->rex & 0x8) ? 8 : 4;  // REX.W=1 -> 64-bit, else 32-bit
}

// Get value from register/memory with size masking
static inline u64 get_op_value(u8* ptr, sz size) {
    switch (size) {
        case 1: return *(u8*)ptr;
        case 4: return *(u32*)ptr;
        case 8: return *(u64*)ptr;
        default: return 0;
    }
}

// Set value to register/memory with size masking (sign/zero extend if needed)
static inline void set_op_value(u8* ptr, u64 val, sz size) {
    switch (size) {
        case 1: *(u8*)ptr = (u8)val; break;
        case 4: *(u32*)ptr = (u32)val; break;
        case 8: *(u64*)ptr = val; break;
    }
}

// Calculate effective address (handles ModR/M, SIB, displacements, RIP-relative)
static u64 get_effective_address(VM* vm, u8 mod, u8 rm, const u8** pc, sz* len) {
    u64 addr = 0;
    u8 rex_b = vm->rex & 1;        // REX.B extends base
    u8 rex_x = (vm->rex >> 1) & 1; // REX.X extends index
    bool has_sib = (rm == 4);      // SIB if R/M=100
    u64* base_reg = NULL;
    u64* index_reg = NULL;
    u8 scale = 1;
    u8 sib = 0;
    u8 base_idx = rm | (rex_b << 3);

    if (has_sib) {
        if (*pc >= vm->memory + vm->mem_size) return 0;
        sib = **pc;
        (*pc)++;
        (*len)++;
        scale = 1 << ((sib >> 6) & 3);  // Scale: 1,2,4,8
        u8 index_idx = ((sib >> 3) & 7) | (rex_x << 3);
        if (index_idx != 4) {  // Index=4 means no index
            index_reg = get_gpr(vm, index_idx);
            if (!index_reg) return 0;
        }
        base_idx = (sib & 7) | (rex_b << 3);
    }

    if (base_idx != 5 || mod != 0 || !has_sib) {  // Base=5 mod=0 is special (no base or RIP)
        base_reg = get_gpr(vm, base_idx);
        if (base_reg) addr += *base_reg;
    }

    if (index_reg) addr += *index_reg * scale;

    // Add displacement based on Mod
    i64 disp = 0;
    bool is_rip_relative = (mod == 0 && rm == 5 && !has_sib) ||
                           (mod == 0 && has_sib && (base_idx == 5));
    if (mod == 0 && is_rip_relative) {
        if (*pc + 4 > vm->memory + vm->mem_size) return 0;
        disp = (i32)read_le32(*pc);
        (*pc) += 4;
        (*len) += 4;
        addr = vm->r.rip + *len + disp;  // RIP-relative
    } else if (mod == 1) {  // disp8
        if (*pc >= vm->memory + vm->mem_size) return 0;
        disp = (i8)**pc;
        (*pc)++;
        (*len)++;
        addr += disp;
    } else if (mod == 2) {  // disp32
        if (*pc + 4 > vm->memory + vm->mem_size) return 0;
        disp = (i32)read_le32(*pc);
        (*pc) += 4;
        (*len) += 4;
        addr += disp;
    }
    printf("Effective address calculation: mod=%d, rm=%d, has_sib=%d, base_idx=%d, scale=%d, disp=%lld, final_addr=0x%lx\n",
           mod, rm, has_sib, base_idx, scale, disp, addr);
    return addr;
}

// Flag setters (expanded for more accuracy)
static void set_flags_basic(VM* vm, u64 result, sz size) {
    vm->r.rflags &= ~(FLAG_CF | FLAG_OF | FLAG_SF | FLAG_ZF | FLAG_PF | FLAG_AF);
    if (result == 0) vm->r.rflags |= FLAG_ZF;
    if (result & (1ULL << (size * 8 - 1))) vm->r.rflags |= FLAG_SF;

    // Parity Flag (PF): even number of 1s in low byte
    u8 low_byte = (u8)result;
    u8 parity = low_byte ^ (low_byte >> 4);
    parity ^= (parity >> 2);
    parity ^= (parity >> 1);
    if (!(parity & 1)) vm->r.rflags |= FLAG_PF;
}

static void set_flags_add_sub(VM* vm, u64 a, u64 b, u64 result, bool is_sub, sz size) {
    set_flags_basic(vm, result, size);

    // CF: Carry/Borrow
    if (is_sub) {
        if (a < b) vm->r.rflags |= FLAG_CF;
    } else {
        if (result < a) vm->r.rflags |= FLAG_CF;
    }

    // OF: Overflow (signed)
    u64 sign_bit = 1ULL << (size * 8 - 1);
    bool a_sign = a & sign_bit;
    bool b_sign = is_sub ? !(b & sign_bit) : (b & sign_bit);  // Flip for sub
    bool res_sign = result & sign_bit;
    if ((a_sign == b_sign) && (a_sign != res_sign)) vm->r.rflags |= FLAG_OF;

    // AF: Auxiliary Carry (low nibble carry)
    u8 low_a = a & 0xF;
    u8 low_b = b & 0xF;
    u8 low_res = result & 0xF;
    if (is_sub) {
        if (low_a < low_b) vm->r.rflags |= FLAG_AF;
    } else {
        if (low_res < low_a) vm->r.rflags |= FLAG_AF;
    }
}

// Simple disassembly for debug (inspired by vm_v1, simplified)
static void disasm_instruction(VM* vm, const u8* pc, sz len) {
    if (!vm->disasm_mode) return;
    printf("DISASM @ 0x%lx: ", vm->r.rip);
    for (sz i = 0; i < len; i++) printf("%02x ", pc[i]);
    printf("\n");
}

// ───────────────────────────────────────────────
// VM Creation/Destruction
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
    vm->r.rsp = memory_bytes;  // Start stack at top of memory
    vm->r.rflags = 0x202;      // Default flags (IF=1, reserved)
    vm->disasm_mode = false;   // Disasm off by default
    return vm;
}

void vm_destroy(VM* vm) {
    if (vm) {
        free(vm->memory);
        free(vm);
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
    vm->r.rip = 0;  // Start at beginning
    return VM_OK;
}

// ───────────────────────────────────────────────
// Single-Step Execution
// ───────────────────────────────────────────────
static VmStatus step(VM* vm) {
    if (vm->r.rip >= vm->mem_size) return VM_SEGFAULT;
    const u8 *pc,*p;
    p=pc = vm->memory + vm->r.rip;  // Current instruction pointer
    sz len = 0;  // Instruction length

    // Check for REX prefix (0x40-0x4F)
    vm->rex = 0;
    if ((*pc & 0xF0) == 0x40) {
        vm->rex = *pc;
        pc++;
        len++;
    }

    u8 opcode = *pc;
    pc++;
    len++;

    // Optional 0x0F prefix for extended opcodes
    bool is_extended = false;
    if (opcode == 0x0F) {
        is_extended = true;
        opcode = *pc;
        pc++;
        len++;
    }
     printf("Debug step rip=%016lx, opcode=%02x, rex=%02x, is_extended=%d\n",
       vm->r.rip, opcode, vm->rex,is_extended);
    // Decode ModR/M if present (common for many ops)
    bool has_modrm = false;
    u8 modrm = 0, mod = 0, reg = 0, rm = 0;
    if ((opcode >= 0x00 && opcode <= 0x3F) || (opcode >= 0x80 && opcode <= 0x8F) ||
        (opcode >= 0xF6 && opcode <= 0xFF) || (is_extended && opcode >= 0x40 && opcode <= 0x4F) ||
        opcode == 0x8B || opcode == 0x89 || opcode == 0x8D || opcode == 0xA3) {
        has_modrm = true;
        if (pc >= vm->memory + vm->mem_size) return VM_SEGFAULT;
        modrm = *pc;
        mod = (modrm >> 6) & 3;
        reg = (modrm >> 3) & 7;
        rm = modrm & 7;
        if (vm->rex & 0x4) reg |= 8;   // REX.R extends reg
        if (vm->rex & 0x1) rm |= 8;    // REX.B extends rm
        printf("Debug step  modrm=%02x, mod=%d, reg=%d, rm=%d\n",
        modrm, mod, reg, rm);
        pc++;   
        len++;
       
    }

    // Get operand pointers (reg or mem)
    u8* dst_ptr = NULL;
    u8* src_ptr = NULL;
    bool is_byte_op = (opcode & 1) == 0 && (opcode <= 0x3F || opcode == 0xF6);  // Byte ops end with even opcode
    sz op_size = get_op_size(vm, is_byte_op);
   
    if (has_modrm) {
        if (mod == 3) {  // Register mode
            dst_ptr = (u8*)get_gpr(vm, rm);
        } else {  // Memory mode
            u64 addr = get_effective_address(vm, mod, rm, &pc, &len);
            if (addr + op_size > vm->mem_size) return VM_SEGFAULT;
            dst_ptr = vm->memory + addr;
        }
        src_ptr = (u8*)get_gpr(vm, reg);  // Source is usually reg
        printf("Debug step dst_ptr=%p, src_ptr=%p, op_size=%d\n", dst_ptr, src_ptr, op_size);
    }

   

    // Handle extended opcodes first
    if (is_extended) {
        if (opcode == 0xAF) {
            // IMUL r, r/m
            u64 a = get_op_value(src_ptr, op_size);  // src_ptr is reg
            u64 b = get_op_value(dst_ptr, op_size);  // dst_ptr is r/m
            i64 res = (i64)a * (i64)b;  // Signed multiply
            set_op_value((u8*)get_gpr(vm, reg), res, op_size);  // Result to reg
            // Flags: OF/CF if result doesn't fit in operand size
            if ((i64)res != (i64)(res & ((1ULL << (op_size * 8)) - 1))) {
                vm->r.rflags |= FLAG_OF | FLAG_CF;
            } else {
                vm->r.rflags &= ~(FLAG_OF | FLAG_CF);
            }
            vm->r.rip += len;
            goto step_end;
        } else if (opcode == 0xA3) {
            // BT r/m, r
            u64 val = get_op_value(dst_ptr, op_size);
            u64 bit = get_op_value(src_ptr, op_size) % (op_size * 8);
            if (val & (1ULL << bit)) vm->r.rflags |= FLAG_CF;
            else vm->r.rflags &= ~FLAG_CF;
            vm->r.rip += len;
            goto step_end;
        } else if (opcode == 0xB6) {
            // MOVZX r32/64, r/m8
            u64 val = get_op_value(dst_ptr, 1);  // Source byte
            u64* dst = get_gpr(vm, reg);
            if (!dst) return VM_SEGFAULT;
            *dst = val;  // Zero-extend to 32/64
            vm->r.rip += len;
            goto step_end;
        } else if (opcode == 0xB7) {
            // MOVZX r32/64, r/m16
            u64 val = get_op_value(dst_ptr, 2);  // Source word
            u64* dst = get_gpr(vm, reg);
            if (!dst) return VM_SEGFAULT;
            *dst = val;
            vm->r.rip += len;
            goto step_end;
        } else {
            return VM_BAD_OPCODE;
        }
    }

    // Execute based on opcode (non-extended)
    switch (opcode) {
        // ─── MOV Variants ───
        case 0x88: case 0x89: {  // MOV r/m, r (byte/32/64)
            if (!dst_ptr || !src_ptr) return VM_SEGFAULT;
            u64 val = get_op_value(src_ptr, op_size);
            set_op_value(dst_ptr, val, op_size);
            vm->r.rip += len;
            break;
        }
        case 0x8A: case 0x8B: {  // MOV r, r/m
            if (!dst_ptr || !src_ptr) return VM_SEGFAULT;
            u8* temp = dst_ptr;
            dst_ptr = src_ptr;
            src_ptr = temp;
            u64 val = get_op_value(src_ptr, op_size);
            set_op_value(dst_ptr, val, op_size);
            vm->r.rip += len;
            break;
        }
        case 0xB0 ... 0xBF: {  // MOV r8/r16/r32/r64, imm
          
            u8 reg_idx = (opcode - 0xB0) | ((vm->rex & 1) << 3);
            u64* dst = get_gpr(vm, reg_idx);
            printf("Debug MOV imm to reg: opcode=%02x, reg_idx=%d, rex_b=%d\n", opcode, reg_idx, vm->rex & 1);
        
            if (!dst) return VM_SEGFAULT;
            u64 imm = (op_size == 8) ? read_le64(pc) : (op_size == 4 ? read_le32(pc) : (op_size == 2 ? read_le16(pc) : *pc));
            *dst = imm;
            len += op_size;
            vm->r.rip += len;
            break;
        }
        case 0xC6: case 0xC7: {  // MOV r/m, imm (byte/32)
            if (!dst_ptr) return VM_SEGFAULT;
            u64 imm = (op_size == 1) ? *pc : read_le32(pc);
            if (op_size == 8) imm = (i64)(i32)imm;  // Sign-extend imm32 to 64
            set_op_value(dst_ptr, imm, op_size);
            len += (op_size == 1 ? 1 : 4);
            vm->r.rip += len;
            break;
        }

        // ─── Arithmetic: ADD/SUB/INC/DEC ───
        case 0x00: case 0x01: {  // ADD r/m, r
            u64 a = get_op_value(dst_ptr, op_size);
            u64 b = get_op_value(src_ptr, op_size);
            u64 res = a + b;
            set_op_value(dst_ptr, res, op_size);
            set_flags_add_sub(vm, a, b, res, false, op_size);
            vm->r.rip += len;
            break;
        }
        case 0x28: case 0x29: {  // SUB r/m, r
            u64 a = get_op_value(dst_ptr, op_size);
            u64 b = get_op_value(src_ptr, op_size);
            u64 res = a - b;
            set_op_value(dst_ptr, res, op_size);
            set_flags_add_sub(vm, a, b, res, true, op_size);
            vm->r.rip += len;
            break;
        }
        case 0x04: case 0x05: {  // ADD rax, imm
            u64* dst = &vm->r.rax;
            u64 imm = (op_size == 1) ? *pc : (op_size == 4 ? read_le32(pc) : read_le64(pc));
            printf("Debug ADD imm to rax: opcode=%02x, imm=0x%lx\n", opcode, imm);
            u64 a = *dst;
            u64 res = a + imm;
            *dst = res;
            set_flags_add_sub(vm, a, imm, res, false, op_size);
            len += op_size;
            vm->r.rip += len;
            break;
        }
        case 0x2C: case 0x2D: {  // SUB rax, imm
            u64* dst = &vm->r.rax;
            u64 imm = (op_size == 1) ? *pc : (op_size == 4 ? read_le32(pc) : read_le64(pc));
            u64 a = *dst;
            u64 res = a - imm;
            *dst = res;
            set_flags_add_sub(vm, a, imm, res, true, op_size);
            len += op_size;
            vm->r.rip += len;
            break;
        }
        case 0xFF: {  // INC/DEC r/m (sub-op in reg field)
            if (!has_modrm) return VM_BAD_OPCODE;
            u64 val = get_op_value(dst_ptr, op_size);
            u64 res;
            if (reg == 0) {  // INC
                res = val + 1;
            } else if (reg == 1) {  // DEC
                res = val - 1;
            } else return VM_BAD_OPCODE;
            set_op_value(dst_ptr, res, op_size);
            set_flags_add_sub(vm, val, 1, res, (reg == 1), op_size);
            vm->r.rip += len;
            break;
        }

        // ─── Bitwise: XOR/AND/OR/NOT ───
        case 0x30: case 0x31: {  // XOR r/m, r
            u64 a = get_op_value(dst_ptr, op_size);
            u64 b = get_op_value(src_ptr, op_size);
            u64 res = a ^ b;
            set_op_value(dst_ptr, res, op_size);
            set_flags_basic(vm, res, op_size);
            vm->r.rflags &= ~FLAG_CF & ~FLAG_OF;  // Clear CF/OF for logic ops
            vm->r.rip += len;
            break;
        }
        case 0x20: case 0x21: {  // AND r/m, r
            u64 a = get_op_value(dst_ptr, op_size);
            u64 b = get_op_value(src_ptr, op_size);
            u64 res = a & b;
            set_op_value(dst_ptr, res, op_size);
            set_flags_basic(vm, res, op_size);
            vm->r.rflags &= ~FLAG_CF & ~FLAG_OF;
            vm->r.rip += len;
            break;
        }
        case 0x08: case 0x09: {  // OR r/m, r
            u64 a = get_op_value(dst_ptr, op_size);
            u64 b = get_op_value(src_ptr, op_size);
            u64 res = a | b;
            set_op_value(dst_ptr, res, op_size);
            set_flags_basic(vm, res, op_size);
            vm->r.rflags &= ~FLAG_CF & ~FLAG_OF;
            vm->r.rip += len;
            break;
        }
        case 0xF7: {  // NOT r/m (reg=2)
            if (reg != 2) return VM_BAD_OPCODE;
            u64 val = get_op_value(dst_ptr, op_size);
            u64 res = ~val;
            set_op_value(dst_ptr, res, op_size);
            vm->r.rip += len;
            break;
        }

        // ─── Compare: CMP ───
        case 0x38: case 0x39: {  // CMP r/m, r
            u64 a = get_op_value(dst_ptr, op_size);
            u64 b = get_op_value(src_ptr, op_size);
            u64 res = a - b;
            set_flags_add_sub(vm, a, b, res, true, op_size);
            vm->r.rip += len;
            break;
        }
        case 0x3C: case 0x3D: {  // CMP rax, imm
            u64 a = vm->r.rax;
            u64 imm = (op_size == 1) ? *pc : (op_size == 4 ? read_le32(pc) : read_le64(pc));
            u64 res = a - imm;
            set_flags_add_sub(vm, a, imm, res, true, op_size);
            len += op_size;
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
            vm->r.rip += len;
            if (take) vm->r.rip += (i64)disp;
            break;
        }
        case 0xEB: {  // JMP short rel8
            if (vm->r.rip + len + 1 >= vm->mem_size) return VM_SEGFAULT;
            i8 disp = read_disp8(pc);
            len++;
            vm->r.rip += len + (i64)disp;
            break;
        }
        case 0xE9: {  // JMP rel32
            if (vm->r.rip + len + 4 > vm->mem_size) return VM_SEGFAULT;
            i32 disp = (i32)read_le32(pc);
            len += 4;
            vm->r.rip += len + (i64)disp;
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
            vm->r.rip += len + (i64)disp;
            break;
        }
        case 0xC3: {  // RET
            if (vm->r.rsp + 8 > vm->mem_size) return VM_SEGFAULT;
            vm->r.rip = *(u64*)(vm->memory + vm->r.rsp);
            vm->r.rsp += 8;
            break;
        }

        // ─── Stack: PUSH/POP ───
        case 0x50 ... 0x57: {  // PUSH r64
            u8 reg_idx = (opcode - 0x50) | ((vm->rex & 1) << 3);
            u64* src = get_gpr(vm, reg_idx);
            if (!src || vm->r.rsp < 8) return VM_STACK_OVERFLOW;
            vm->r.rsp -= 8;
            if (vm->r.rsp >= vm->mem_size) return VM_SEGFAULT;
            *(u64*)(vm->memory + vm->r.rsp) = *src;
            vm->r.rip += len;
            break;
        }
        case 0x58 ... 0x5F: {  // POP r64
            u8 reg_idx = (opcode - 0x58) | ((vm->rex & 1) << 3);
            u64* dst = get_gpr(vm, reg_idx);
            if (!dst || vm->r.rsp + 8 > vm->mem_size) return VM_SEGFAULT;
            *dst = *(u64*)(vm->memory + vm->r.rsp);
            vm->r.rsp += 8;
            vm->r.rip += len;
            break;
        }
        case 0x68: {  // PUSH imm32 (sign-extend to 64)
            if (vm->r.rip + len + 4 > vm->mem_size) return VM_SEGFAULT;
            i64 imm = (i32)read_le32(pc);
            len += 4;
            if (vm->r.rsp < 8) return VM_STACK_OVERFLOW;
            vm->r.rsp -= 8;
            if (vm->r.rsp >= vm->mem_size) return VM_SEGFAULT;
            *(u64*)(vm->memory + vm->r.rsp) = imm;
            vm->r.rip += len;
            break;
        }

        // ─── Flags: PUSHF/POPF ───
        case 0x9C: {  // PUSHF
            if (vm->r.rsp < 8) return VM_STACK_OVERFLOW;
            vm->r.rsp -= 8;
            if (vm->r.rsp >= vm->mem_size) return VM_SEGFAULT;
            *(u64*)(vm->memory + vm->r.rsp) = vm->r.rflags;
            vm->r.rip += len;
            break;
        }
        case 0x9D: {  // POPF
            if (vm->r.rsp + 8 > vm->mem_size) return VM_SEGFAULT;
            vm->r.rflags = *(u64*)(vm->memory + vm->r.rsp);
            vm->r.rsp += 8;
            vm->r.rip += len;
            break;
        }

        // ─── LEA ───
        case 0x8D: {  // LEA r, m
            if (mod == 3) return VM_BAD_OPCODE;  // No reg-reg LEA
            u64 addr = get_effective_address(vm, mod, rm, &pc, &len);  // Recalculate if needed
            u64* dst = get_gpr(vm, reg);
            if (!dst) return VM_SEGFAULT;
            *dst = addr;
            vm->r.rip += len;
            break;
        }

        // ─── Loops (from vm_v1) ───
        case 0xE0: {  // LOOPNE rel8
            vm->r.rcx--;
            i8 disp = read_disp8(pc);
            len++;
            if (vm->r.rcx != 0 && !(vm->r.rflags & FLAG_ZF)) vm->r.rip += (i64)disp;
            vm->r.rip += len;
            break;
        }
        case 0xE1: {  // LOOPE rel8
            vm->r.rcx--;
            i8 disp = read_disp8(pc);
            len++;
            if (vm->r.rcx != 0 && (vm->r.rflags & FLAG_ZF)) vm->r.rip += (i64)disp;
            vm->r.rip += len;
            break;
        }
        case 0xE2: {  // LOOP rel8
            vm->r.rcx--;
            i8 disp = read_disp8(pc);
            len++;
            if (vm->r.rcx != 0) vm->r.rip += (i64)disp;
            vm->r.rip += len;
            break;
        }

        // ─── HLT/NOP ───
        case 0xF4: return VM_HALT;
        case 0x90: vm->r.rip += len; break;

        // ─── Interrupt (stub) ───
        case 0xCD: {  // INT imm8
            u8 int_num = *pc;
            len++;
            // Stub: Just print for now (can expand later)
            printf("INT %02x encountered\n", int_num);
            vm->r.rip += len;
            break;
        }

        default: return VM_BAD_OPCODE;
    }
step_end:
     // Disassemble if enabled
    disasm_instruction(vm, p, len);
    return VM_OK;
}

// ───────────────────────────────────────────────
// Run the VM
// ───────────────────────────────────────────────
VmStatus vm_run(VM* vm) {
    VmStatus status;
    while ((status = step(vm)) == VM_OK) {
        if (vm->r.rip >= vm->code_break) break;  // Optional: stop at code end
    }
    return status;
}

// ───────────────────────────────────────────────
// Debug Dumps
// ───────────────────────────────────────────────
void vm_dump_registers(const VM* vm) {
    const Registers* r = &vm->r;
    printf("RAX=%016lx RBX=%016lx RCX=%016lx RDX=%016lx\n", r->rax, r->rbx, r->rcx, r->rdx);
    printf("RSI=%016lx RDI=%016lx RBP=%016lx RSP=%016lx\n", r->rsi, r->rdi, r->rbp, r->rsp);
    printf("R8 =%016lx R9 =%016lx R10=%016lx R11=%016lx\n", r->r8, r->r9, r->r10, r->r11);
    printf("R12=%016lx R13=%016lx R14=%016lx R15=%016lx\n", r->r12, r->r13, r->r14, r->r15);
    printf("RIP=%016lx RFLAGS=%016lx [CF=%d PF=%d AF=%d ZF=%d SF=%d OF=%d]\n",
           r->rip, r->rflags,
           !!(r->rflags & FLAG_CF), !!(r->rflags & FLAG_PF), !!(r->rflags & FLAG_AF),
           !!(r->rflags & FLAG_ZF), !!(r->rflags & FLAG_SF), !!(r->rflags & FLAG_OF));
}

void vm_dump_memory(const VM* vm, sz bytes) {
    sz lim = bytes < vm->mem_size ? bytes : vm->mem_size;
    printf("Memory dump (first %zu bytes):\n", lim);
    for (sz i = 0; i < lim; i++) {
        printf("%02x ", vm->memory[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    printf("\n");
}

// ───────────────────────────────────────────────
// Example Main (expanded test program)
// ───────────────────────────────────────────────
int main(void) {
    VM* vm = vm_create(1 << 20);  // 1 MiB memory
    if (!vm) return 1;

    // Enable disassembly for debugging
    vm_set_disasm_mode(vm, true);

    // Expanded test program (combines vm_v1 and vm_v2 examples)
    static const u8 test_program[] = {
        // Basic MOV and arithmetic
        0x48, 0xB8, 0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11,  // mov rax, 0x1122334455667788
        0x48, 0x89, 0xC3,                                            // mov rbx, rax
        0x48, 0x31, 0xC0,                                            // xor rax, rax
        0x05, 0x00, 0x00, 0x22, 0x11, 0x44,                    // add rax, 0x44112200
      //  0x81, 0xEB, 0x78, 0x56, 0x34, 0x12,                   // sub rbx, 0x12345678
        0x48, 0xFF, 0xC0,                                            // inc rax

        // // Memory store/load
       // 0x48, 0xC7, 0x04, 0x25, 0x00, 0x10, 0x00, 0x00, 0x88, 0x77, 0x66, 0x55,  // mov qword [0x1000], 0x55667788 (imm32 sign-ext)
        0x48, 0x8B, 0x04, 0x25, 0x00, 0x10, 0x00, 0x00,              // mov rax, [0x1000]

        // // RIP-relative
        0x48, 0x8B, 0x05, 0x00, 0x00, 0x00, 0x00,                    // mov rax, [rip + 0]

        // // SIB addressing
         0x48, 0xBB, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // mov rbx, 0x1000
        // 0x48, 0xC7, 0xC1, 0x02, 0x00, 0x00, 0x00,                    // mov rcx, 2
         0x48, 0x8B, 0x44, 0x8B, 0x08,                                // mov rax, [rbx + rcx*4 + 8]

        // // Conditional jumps
        0x48, 0x31, 0xC0,                                            // xor rax, rax  → ZF=1
        0x74, 0x05,                                                  // jz +5
      //  0x75, 0xFC,                                                  // jnz -4 (should not loop)

        // // Call/Ret with factorial example
       // 0x48, 0xC7, 0xC1, 0x05, 0x00, 0x00, 0x00,                    // mov rcx, 5
      //  0x48, 0xC7, 0xC2, 0x01, 0x00, 0x00, 0x00,                    // mov rdx, 1
        0xE8, 0x0A, 0x00, 0x00, 0x00,                                // call +10 (mul_loop)
        0xF4,                                                        // hlt

        // // mul_loop: imul rdx, rcx; dec rcx; jnz mul_loop; ret
        0x48, 0x0F, 0xAF, 0xD1,                                      // imul rdx, rcx
        0x48, 0xFF, 0xC9,                                            // dec rcx
        0x75, 0xF7,                                                  // jnz -9
        0xC3,                                                        // ret

        // // Loop test (from vm_v1)
        // 0x48, 0xC7, 0xC1, 0x05, 0x00, 0x00, 0x00,                    // mov rcx, 5
        // 0xE2, 0xFC,                                                  // loop -4 (dec rcx, jump if !=0)

        // // BT test
        // 0x48, 0xC7, 0xC0, 0x01, 0x00, 0x00, 0x00,                    // mov rax, 1
        // 0x0F, 0xA3, 0xC0,                                            // bt rax, rax (bit 1, sets CF if 1)

        // // MOVZX test
        // 0xB0, 0xFF,                                                  // mov al, 0xFF
        // 0x0F, 0xB6, 0xC0,                                            // movzx eax, al (zero-extend to 0x000000FF)

        0xF4                                                         // hlt
    };

    VmStatus status = vm_load_program(vm, test_program, sizeof(test_program));
    if (status != VM_OK) {
        printf("Load failed: %d\n", status);
        vm_destroy(vm);
        return 1;
    }

    status = vm_run(vm);
    printf("VM stopped with status: %d\n", status);
 //   vm_dump_registers(vm);
   // vm_dump_memory(vm, 0x100);  // Dump first 256 bytes

    vm_destroy(vm);
    return 0;
}