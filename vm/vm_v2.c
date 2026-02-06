#include "vm.h"

// ───────────────────────────────────────────────
// Helpers
// ───────────────────────────────────────────────

static inline u64 read_le64(const u8 *p) {
    return  (u64)p[0]       | ((u64)p[1] <<  8) |
           ((u64)p[2] << 16) | ((u64)p[3] << 24) |
           ((u64)p[4] << 32) | ((u64)p[5] << 40) |
           ((u64)p[6] << 48) | ((u64)p[7] << 56);
}

static inline u32 read_le32(const u8 *p) {
    return  (u32)p[0]       | ((u32)p[1] <<  8) |
           ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

static inline u16 read_le16(const u8 *p) {
    return (u16)p[0] | ((u16)p[1] << 8);
}

// Helper: read signed 8-bit displacement
static inline i8 read_disp8(const u8 *p) {
    return (i8)*p;
}

// Get 64-bit register by 4-bit index (0..15)
static u64* get_gpr(VM *vm, u8 idx) {
    switch (idx & 0xF) {
        case  0: return &vm->r.rax;
        case  1: return &vm->r.rcx;
        case  2: return &vm->r.rdx;
        case  3: return &vm->r.rbx;
        case  4: return &vm->r.rsp;
        case  5: return &vm->r.rbp;
        case  6: return &vm->r.rsi;
        case  7: return &vm->r.rdi;
        case  8: return &vm->r.r8;
        case  9: return &vm->r.r9;
        case 10: return &vm->r.r10;
        case 11: return &vm->r.r11;
        case 12: return &vm->r.r12;
        case 13: return &vm->r.r13;
        case 14: return &vm->r.r14;
        case 15: return &vm->r.r15;
        default: return NULL;
    }
}
// Helper to get operand size and mask
static inline int get_op_size(VM *vm, bool is_byte_op) {
    if (is_byte_op) return 1;
    return (vm->rex & 8) ? 8 : 4;  // REX.W=1 → 64-bit, else 32-bit
}

// Helper to get value from register/memory with correct size
static u64 get_op_value(VM *vm, u8 *mem_ptr, int size) {
    switch (size) {
        case 1: return *mem_ptr;
        case 4: return *(u32*)mem_ptr;
        case 8: return *(u64*)mem_ptr;
        default: return 0;
    }
}

// Helper to write value back with correct size
static void set_op_value(VM *vm, u8 *mem_ptr, u64 val, int size) {
    switch (size) {
        case 1: *mem_ptr = (u8)val; break;
        case 4: *(u32*)mem_ptr = (u32)val; break;
        case 8: *(u64*)mem_ptr = val; break;
    }
}
// Full effective address calculation — handles all common modes
// static u64 get_effective_address(VM *vm, u8 mod, u8 rm, const u8 **pc, size_t *len) {
//     u64 addr = 0;

//     // REX extensions
//     u8 rex_b = vm->rex & 1;           // extends base register
//     u8 rex_x = (vm->rex & 2) >> 1;    // extends index register

//     u64 *base_reg  = NULL;
//     u64 *index_reg = NULL;
//     u8 scale       = 1;
//     bool sib_present = (rm == 4);

//     u8 base_idx = rm | (rex_b << 3);

//     // Handle SIB if present
//     u8 sib = 0;
//     if (sib_present) {
//         // Read SIB byte
//         if (*pc >= vm->memory + vm->mem_size) return 0;
//         sib = **pc;
//         (*pc)++;
//         (*len)++;

//         // Scale (1, 2, 4, 8)
//         scale = 1 << ((sib >> 6) & 3);

//         // Index register (4 = none)
//         u8 index_idx = ((sib >> 3) & 7) | (rex_x << 3);
//         if (index_idx != 4) {
//             index_reg = get_gpr(vm, index_idx);
//             if (!index_reg) return 0;
//         }

//         // Base register (may be special case 5)
//         base_idx = (sib & 7) | (rex_b << 3);
//     }

//     // Get base register
//     if (base_idx <= 15) {
//         base_reg = get_gpr(vm, base_idx);
//         if (!base_reg && !(sib_present && base_idx == 5 && mod == 0)) {
//             // Invalid register — only allowed if SIB base=5 mod=0 (no base)
//             return 0;
//         }
//     }

//     // Add base if present
//     if (base_reg) {
//         addr += *base_reg;
//     }

//     // Add scaled index if present
//     if (index_reg) {
//         addr += *index_reg * scale;
//     }

//     // Displacement
//     i64 disp = 0;
//     if (mod == 0) {
//         // mod=0: no disp except special cases
//         bool is_rip_relative = false;
//         bool is_absolute_disp = false;

//         if (!sib_present) {
//             if (rm == 5) is_rip_relative = true;
//         } else {
//             u8 sib_base = (sib & 7) | (rex_b << 3);
//             if (sib_base == 5) is_absolute_disp = true;  // SIB no base → absolute disp32
//         }

//         if (is_rip_relative || is_absolute_disp) {
//             if (*pc + 4 > vm->memory + vm->mem_size) return 0;
//             disp = (i32)read_le32(*pc);
//             (*pc) += 4;
//             (*len) += 4;

//             if (is_rip_relative) {
//                 addr = vm->r.rip + *len + disp;
//             } else {
//                 addr += disp;
//             }
//         }
//         // else: no displacement — just base + index*scale
//     } else if (mod == 1) {
//         // disp8
//         if (*pc >= vm->memory + vm->mem_size) return 0;
//         disp = (i8)**pc;
//         (*pc)++;
//         (*len)++;
//         addr += disp;
//     } else if (mod == 2) {
//         // disp32
//         if (*pc + 4 > vm->memory + vm->mem_size) return 0;
//         disp = (i32)read_le32(*pc);
//         (*pc) += 4;
//         (*len) += 4;
//         addr += disp;
//     }
//     // mod==3: register — no address (handled by caller)
//   printf("DEBUG GET ADDRESS: mod=%d rm=%d sib=%d scale=%d base_idx=%d index=%d addr=0x%lx\n",
//        mod, rm, sib_present, scale, base_idx, index_reg ? index_reg - vm->r.rax : -1, addr);
//     return addr;
// }
static u64 get_effective_address(VM *vm, u8 mod, u8 rm, const u8 **pc, size_t *len) {
    u64 addr = 0;

    u8 rex_b = vm->rex & 1;
    u8 rex_x = (vm->rex & 2) >> 1;

    u64 *base = NULL;
    u64 *index = NULL;
    u8 scale = 1;
    bool has_sib = (rm == 4);

    u8 sib = 0;
    u8 base_idx = rm | (rex_b << 3);

    if (has_sib) {
        if (*pc >= vm->memory + vm->mem_size) return 0;
        sib = **pc;
        (*pc)++;
        (*len)++;

        scale = 1 << ((sib >> 6) & 3);

        u8 index_idx = ((sib >> 3) & 7) | (rex_x << 3);
        if (index_idx != 4) {
            index = get_gpr(vm, index_idx);
            if (!index) return 0;
        }

        base_idx = (sib & 7) | (rex_b << 3);
    }

    base = get_gpr(vm, base_idx);

    if (base) addr += *base;
    if (index) addr += *index * scale;

    // Displacement
    if (mod == 0) {
        bool rip_relative = (!has_sib && rm == 5) ||
                            (has_sib && ((sib & 7) | (rex_b << 3)) == 5);

        if (rip_relative) {
            if (*pc + 4 > vm->memory + vm->mem_size) return 0;
            i32 disp = (i32)read_le32(*pc);
            (*pc) += 4;
            (*len) += 4;
            addr = vm->r.rip + *len + disp;
        } else if (has_sib && !base) {
            if (*pc + 4 > vm->memory + vm->mem_size) return 0;
            i32 disp = (i32)read_le32(*pc);
            (*pc) += 4;
            (*len) += 4;
            addr += (u64)(i64)disp;
        }
    } else if (mod == 1) {
        if (*pc >= vm->memory + vm->mem_size) return 0;
        i8 disp = (i8)**pc;
        (*pc)++;
        (*len)++;
        addr += (i64)disp;
    } else if (mod == 2) {
        if (*pc + 4 > vm->memory + vm->mem_size) return 0;
        i32 disp = (i32)read_le32(*pc);
        (*pc) += 4;
        (*len) += 4;
        addr += (i64)disp;
    }

    return addr;
}
// ───────────────────────────────────────────────
// Flag helpers (phase 1 — basic but correct)
// ───────────────────────────────────────────────
static void set_flags(VM *vm, u64 result) {
    vm->r.rflags &= ~(FLAG_CF | FLAG_OF | FLAG_SF | FLAG_ZF | FLAG_PF);
    if (result == 0) vm->r.rflags |= FLAG_ZF;
    if (result & (1ULL << 63)) vm->r.rflags |= FLAG_SF;
    // PF (parity of low byte) - optional
     u8 parity = result ^ (result >> 4);
    parity ^= (parity >> 2);
    parity ^= (parity >> 1);
    if (!(parity & 1)) vm->r.rflags |= FLAG_PF;
}
static void set_flags_add_sub(VM *vm, u64 a, u64 b, u64 result, bool is_sub) {
    vm->r.rflags &= ~(FLAG_CF | FLAG_PF | FLAG_AF | FLAG_ZF | FLAG_SF | FLAG_OF);
    // ZF
    if (result == 0) vm->r.rflags |= FLAG_ZF;

    // SF
    if (result & (1ULL << 63)) vm->r.rflags |= FLAG_SF;

    // CF (borrow / carry)
    if (is_sub) {
        if (a < b) vm->r.rflags |= FLAG_CF;
    } else {
        if (result < a) vm->r.rflags |= FLAG_CF;
    }

    // OF (signed overflow)
    if (!is_sub) {
        bool of = ((a ^ result) & (b ^ result) & (1ULL << 63)) != 0;
        if (of) vm->r.rflags |= FLAG_OF;
    } else {
        bool of = ((a ^ b) & (a ^ result) & (1ULL << 63)) != 0;
        if (of) vm->r.rflags |= FLAG_OF;
    }

    // PF (parity of low 8 bits)
    u8 parity = result ^ (result >> 4);
    parity ^= (parity >> 2);
    parity ^= (parity >> 1);
    if (!(parity & 1)) vm->r.rflags |= FLAG_PF;
}



// ───────────────────────────────────────────────
// Create / destroy
// ───────────────────────────────────────────────
VM* vm_create(sz memory_bytes) {
    VM *vm = calloc(1, sizeof(VM));
    if (!vm) return NULL;

    vm->memory = calloc(1, memory_bytes);
    if (!vm->memory) {
        free(vm);
        return NULL;
    }

    vm->mem_size   = memory_bytes;
    vm->r.rflags   = 0x202;   // IF=1 by convention
    vm->r.rsp      = memory_bytes - 8;  // grow down from top

    return vm;
}

void vm_destroy(VM *vm) {
    if (vm) {
        free(vm->memory);
        free(vm);
    }
}

// ───────────────────────────────────────────────
// Load flat binary
// ───────────────────────────────────────────────
VmStatus vm_load_program(VM *vm, const u8 *code, sz len) {
    if (len >= vm->mem_size) return VM_SEGFAULT;
    memcpy(vm->memory, code, len);
    vm->code_break = len;
    vm->r.rip = 0;
    return VM_OK;
}


// ───────────────────────────────────────────────
// Core decoder & executor — Phase 1 subset only
// ───────────────────────────────────────────────
static VmStatus step(VM *vm) {
    const u8 *pc, *p;
    pc = p= vm->memory + vm->r.rip;
    if (vm->r.rip >= vm->code_break) return VM_HALT;
    
    vm->rex = 0;
    sz len = 1;
 
    // REX prefix?
    if ((*pc & 0xF0) == 0x40) {
        vm->rex = *pc++;
        len++;
    }
    
    u8 opcode = *pc++;
    printf("Debug step rip=%016lx, opcode=%02x, rex=%02x\n",
       vm->r.rip, opcode, vm->rex);
    switch (opcode) {
  
    // opcode base: [your_base] (e.g. 00 for ADD r/m8, r8)
//     00 /r → ADD r/m8, r8
    // 01 /r → ADD r/m16/32/64, r16/32/64 (already have?)
    // 02 /r → ADD r8, r/m8
    // 03 /r → ADD r16/32/64, r/m16/32/64
    // OR (08–0B) ADC (10–13) SBB (18–1B) AND (20–23)
    // SUB (28–2B) XOR (30–33) CMP (38–3B)
   // ─── 8/32/64-bit arithmetic (ADD/OR/ADC/SBB/AND/SUB/XOR/CMP) ───
    case 0x00: case 0x01: case 0x02: case 0x03:  // ADD
    case 0x08: case 0x09: case 0x0A: case 0x0B:  // OR
    case 0x10: case 0x11: case 0x12: case 0x13:  // ADC
    case 0x18: case 0x19: case 0x1A: case 0x1B:  // SBB
    case 0x20: case 0x21: case 0x22: case 0x23:  // AND
    case 0x28: case 0x29: case 0x2A: case 0x2B:  // SUB
    case 0x30: case 0x31: case 0x32: case 0x33:  // XOR
     { 
        if (vm->r.rip + len >= vm->mem_size) return VM_SEGFAULT;

        u8 modrm = *pc++;
        len++;

        u8 mod = modrm >> 6;
        u8 reg_idx = ((vm->rex & 4) >> 1) | ((modrm >> 3) & 7);   // REX.R
        u8 rm_idx  = ((vm->rex & 1) << 3) | (modrm & 7);          // REX.B

        // Determine operation type from opcode
        int base = opcode & ~3;  // group base (00,08,10,18,20,28,30,38)
        bool is_dest_rm = (opcode & 2) == 0;  // 0x00/08/... = r/m OP r    0x02/0A/... = r OP r/m
        bool is_byte = (opcode & 1) == 0;     // 00/08/... byte, 01/09/... full size

        int op_size = get_op_size(vm, is_byte);

        u64 *reg_ptr = get_gpr(vm, reg_idx);
        if (!reg_ptr) return VM_BAD_OPCODE;
        u64 reg_val = *reg_ptr & ((1ULL << (op_size*8)) - 1);

        u64 left_val, right_val;
        bool is_cmp = (base == 0x38);
        u64 addr=0;
        if (mod == 3) {
            u64 *rm_ptr = get_gpr(vm, rm_idx);
            if (!rm_ptr) return VM_BAD_OPCODE;
            u64 rm_val = *rm_ptr & ((1ULL << (op_size*8)) - 1);

            left_val = is_dest_rm ? rm_val : reg_val;
            right_val = is_dest_rm ? reg_val : rm_val;
        } else {
            addr = get_effective_address(vm, mod, rm_idx, &pc, &len);
            if (addr == 0 || addr + op_size > vm->mem_size) return VM_SEGFAULT;

            left_val = get_op_value(vm, vm->memory + addr, op_size);
            right_val = reg_val;
            if (!is_dest_rm) {
                // swap for r OP r/m
                u64 tmp = left_val;
                left_val = right_val;
                right_val = tmp;
            }
        }

        u64 result = 0;
        bool update_flags = true;
        bool write_back = !is_cmp;

        switch (base) {
            case 0x00:  // ADD
                result = left_val + right_val;
                set_flags_add_sub(vm, left_val, right_val, result, false);
                break;
            case 0x08:  // OR
                result = left_val | right_val;
                set_flags(vm, result);
                break;
            case 0x10:  // ADC
                u64 carry = (vm->r.rflags & FLAG_CF) ? 1 : 0;
                result = left_val + right_val + carry;
                set_flags_add_sub(vm, left_val, right_val + carry, result, false);
                break;
            case 0x18:  // SBB
                u64 borrow = (vm->r.rflags & FLAG_CF) ? 1 : 0;
                result = left_val - right_val - borrow;
                set_flags_add_sub(vm, left_val, right_val + borrow, result, true);
                break;
            case 0x20:  // AND
                result = left_val & right_val;
                set_flags(vm, result);
                break;
            case 0x28:  // SUB
                result = left_val - right_val;
                set_flags_add_sub(vm, left_val, right_val, result, true);
                break;
            case 0x30:  // XOR
                result = left_val ^ right_val;
                set_flags(vm, result);
                break;
           
            default:
                return VM_BAD_OPCODE;
        }

        if (write_back) {
            if (mod == 3) {
                u64 *dst_ptr = get_gpr(vm, is_dest_rm ? rm_idx : reg_idx);
                *dst_ptr = (*dst_ptr & ~((1ULL << (op_size*8)) - 1)) | (result & ((1ULL << (op_size*8)) - 1));
            } else {
                set_op_value(vm, vm->memory + addr, result, op_size);
            }
        }

        vm->r.rip += len;
        break;
    } 
    case 0x38: case 0x39: case 0x3A: case 0x3B: { // CMP
        if (vm->r.rip + len >= vm->mem_size) return VM_SEGFAULT;

        u8 modrm = *pc++;
        len++;

        u8 mod = modrm >> 6;
        u8 reg_idx = ((vm->rex & 4) >> 1) | ((modrm >> 3) & 7);
        u8 rm_idx  = ((vm->rex & 1) << 3) | (modrm & 7);

        bool is_byte = (opcode == 0x38 || opcode == 0x3A);

        u64 left, right;

        // Left operand (r/m)
        if (mod == 3) {
            u64 *left_reg = get_gpr(vm, rm_idx);
            if (!left_reg) return VM_BAD_OPCODE;
            left = *left_reg;
        } else {
            u64 addr = get_effective_address(vm, mod, rm_idx, &pc, &len);
            if (addr == 0 || addr + (is_byte ? 0 : 7) >= vm->mem_size) return VM_SEGFAULT;
            left = is_byte ? *(u8*)(vm->memory + addr) : *(u64*)(vm->memory + addr);
        }

        // Right operand (r)
        u64 *right_reg = get_gpr(vm, reg_idx);
        if (!right_reg) return VM_BAD_OPCODE;
        right = *right_reg;

        u64 result = left - right;
        set_flags_add_sub(vm, left, right, result, true);

        // CMP does NOT write back

        vm->r.rip += len;
        return VM_OK;
    }


        // ─── ADD rax, imm32 (05 id) ───
        case 0x05: {
            if (vm->r.rip + len + 4 > vm->mem_size) return VM_SEGFAULT;
            i32 imm = (i32)read_le32(pc);
            u64 old = vm->r.rax;
            vm->r.rax += (u64)imm;
            set_flags_add_sub(vm, old, (u64)imm, vm->r.rax, false);
            len+=4;
            vm->r.rip += len ;
            break;
        }
        case 0x0F: {
            if (vm->r.rip + len >= vm->mem_size) return VM_SEGFAULT;
            u8 op2 = *pc++;
            len++;

            switch (op2) {
                case 0xAF: { // imul r32/64, r/m32/64
                    if (vm->r.rip + len >= vm->mem_size) return VM_SEGFAULT;
                    u8 modrm = *pc++;
                    len++;

                    u8 mod = modrm >> 6;
                    u8 reg = ((vm->rex & 4) >> 1) | ((modrm >> 3) & 7);
                    u8 rm  = ((vm->rex & 1) << 3) | (modrm & 7);

                    u64 *dst = get_gpr(vm, reg);
                    if (!dst) return VM_BAD_OPCODE;

                    u64 left = *dst;
                    u64 right;

                    if (mod == 3) {
                        u64 *src = get_gpr(vm, rm);
                        if (!src) return VM_BAD_OPCODE;
                        right = *src;
                    } else {
                        u64 addr = get_effective_address(vm, mod, rm, &pc, &len);
                        if (addr == 0 || addr + 7 >= vm->mem_size) return VM_SEGFAULT;
                        right = *(u64*)(vm->memory + addr);
                    }

                    // Signed multiply (64×64 → 128, but we truncate to 64)
                    __int128 product = (__int128)(int64_t)left * (__int128)(int64_t)right;
                    *dst = (u64)product;

                    // Flags: CF and OF set if high 64 bits != sign extension of low 64
                    bool overflow = (product >> 64) != ((int64_t)product >> 63);
                    if (overflow) vm->r.rflags |= (FLAG_CF | FLAG_OF);
                    else vm->r.rflags &= ~(FLAG_CF | FLAG_OF);

                    vm->r.rip += len;
                    return VM_OK;
                }
                default:
                    return VM_BAD_OPCODE;
            }
        }

       // ─── Group 1 immediate: ADD/OR/ADC/SBB/AND/SUB/XOR/CMP r/m, imm ───
        // 80 /r   8-bit
        // 81 /r   32/64-bit imm32 sign-extended
        // 82 /r   8-bit imm8 (same as 80)
        // 83 /r   32/64-bit imm8 sign-extended
        case 0x80: case 0x81: case 0x82: case 0x83: {
            if (vm->r.rip + len >= vm->mem_size) return VM_SEGFAULT;

            u8 modrm = *pc++;
            len++;

            u8 subop = (modrm >> 3) & 7;
            u8 mod   = modrm >> 6;
            u8 rm    = ((vm->rex & 1) << 3) | (modrm & 7);

            // Determine operand size
            bool is_byte = (opcode == 0x80 || opcode == 0x82);
            bool imm_is_byte = is_byte || (opcode == 0x83);

            // Read immediate
            u64 imm;
            if (imm_is_byte) {
                if (*pc >= vm->memory + vm->mem_size) return VM_SEGFAULT;
                imm = (u8)*pc++;
      
                len++;
            } else {
                if (vm->r.rip + len + 4 > vm->mem_size) return VM_SEGFAULT;
                i32 imm32 = (i32)read_le32(pc);
                pc += 4;
                len += 4;
                imm = (u64)(i64)imm32;
            }

            // Get left operand (r/m)
            u64 left;
            bool left_is_mem = (mod != 3);
            u64 addr = 0;
            if (left_is_mem) {
                addr = get_effective_address(vm, mod, rm, &pc, &len);
                if (addr == 0 || addr + (is_byte ? 0 : 7) >= vm->mem_size) return VM_SEGFAULT;
                left = is_byte ? *(u8*)(vm->memory + addr) : *(u64*)(vm->memory + addr);
            } else {
                u64 *reg = get_gpr(vm, rm);
                if (!reg) return VM_BAD_OPCODE;
                left = *reg;
            }

            u64 result = 0;
            bool write_back = true;

            switch (subop) {
                case 0: // ADD
                    result = left + imm;
                    set_flags_add_sub(vm, left, imm, result, false);
                    break;

                case 1: // OR
                    result = left | imm;
                    vm->r.rflags &= ~(FLAG_CF | FLAG_OF);
                    set_flags(vm, result);
                    break;

                case 2: // ADC
                    {
                        u64 carry = (vm->r.rflags & FLAG_CF) ? 1 : 0;
                        result = left + imm + carry;
                        set_flags_add_sub(vm, left, imm + carry, result, false);
                    }
                    break;

                case 3: // SBB
                    {
                        u64 borrow = (vm->r.rflags & FLAG_CF) ? 1 : 0;
                        result = left - imm - borrow;
                        set_flags_add_sub(vm, left, imm + borrow, result, true);
                    }
                    break;

                case 4: // AND
                    result = left & imm;
                    vm->r.rflags &= ~(FLAG_CF | FLAG_OF);
                    set_flags(vm, result);
                    break;

                case 5: // SUB
                    result = left - imm;
                    set_flags_add_sub(vm, left, imm, result, true);
                    break;

                case 6: // XOR
                    result = left ^ imm;
                    vm->r.rflags &= ~(FLAG_CF | FLAG_OF);
                    set_flags(vm, result);
                    break;

                case 7: // CMP
                    result = left - imm;
                    set_flags_add_sub(vm, left, imm, result, true);
                    write_back = false;
                    break;

                default:
                    return VM_BAD_OPCODE;
            }

            // Write back (except CMP)
            if (write_back) {
                if (left_is_mem) {
                    if (is_byte) {
                        *(u8*)(vm->memory + addr) = (u8)result;
                    } else {
                        *(u64*)(vm->memory + addr) = result;
                    }
                } else {
                    u64 *dst_reg = get_gpr(vm, rm);
                    *dst_reg = result;
                }
            }

            vm->r.rip += len;
            break;
        }
      case 0x89: {
        if (vm->r.rip + len >= vm->mem_size) return VM_SEGFAULT;
        u8 modrm = *pc++;
        len++;

        u8 mod = modrm >> 6;
        u8 reg = ((vm->rex & 4) >> 1) | ((modrm >> 3) & 7);
        u8 rm  = ((vm->rex & 1) << 3) | (modrm & 7);

        u64 *src = get_gpr(vm, reg);
        if (!src) return VM_BAD_OPCODE;

        if (mod == 3) {
            u64 *dst = get_gpr(vm, rm);
            if (!dst) return VM_BAD_OPCODE;
            *dst = *src;
        } 
   
        else {
            u64 addr = get_effective_address(vm, mod, rm, &pc, &len);
            if (addr == 0 || addr + 7 >= vm->mem_size) return VM_SEGFAULT;
            *(u64*)(vm->memory + addr) = *src;
        }

        vm->r.rip += len;
        break;
    }
        case 0x8B: {
        if (vm->r.rip + len >= vm->mem_size) return VM_SEGFAULT;
        u8 modrm = *pc++;
        len++;

        u8 mod = modrm >> 6;
        u8 reg = ((vm->rex & 4) >> 1) | ((modrm >> 3) & 7);
        u8 rm  = ((vm->rex & 1) << 3) | (modrm & 7);

        u64 *dst = get_gpr(vm, reg);
        if (!dst) return VM_BAD_OPCODE;

        if (mod == 3) {
            u64 *src = get_gpr(vm, rm);
            if (!src) return VM_BAD_OPCODE;
            *dst = *src;
        } 
        
        
        else {
            u64 addr = get_effective_address(vm, mod, rm, &pc, &len);
            if (addr == 0 || addr + 7 >= vm->mem_size) return VM_SEGFAULT;
            *dst = *(u64*)(vm->memory + addr);
        }

        vm->r.rip += len;
        break;
    }
  

        // ─── MOV reg64, imm64 ───
        case 0xB8: case 0xB9: case 0xBA: case 0xBB:
        case 0xBC: case 0xBD: case 0xBE: case 0xBF: {
            u8 reg = (opcode & 7) | ((vm->rex & 1) << 3);
            u64 *dst = get_gpr(vm, reg);
            if (!dst) return VM_BAD_OPCODE;
            if (vm->r.rip + len + 8 > vm->mem_size) return VM_SEGFAULT;
            *dst = read_le64(pc);
            len+=8;
            vm->r.rip += len;
            break;
        }

    

        // ─── MOV r8, imm8 (B0–B7) ───
       // Moves 8-bit immediate to AL/CL/DL/BL/AH/CH/DH/BH
        case 0xB0: case 0xB1: case 0xB2: case 0xB3:
        case 0xB4: case 0xB5: case 0xB6: case 0xB7: {
            if (vm->r.rip + len >= vm->mem_size) return VM_SEGFAULT;

            // imm8 follows immediately
            if (pc >= vm->memory + vm->mem_size) return VM_SEGFAULT;
            u8 imm8 = *pc++;
          
            len++;

            // Determine which 8-bit register
            u8 reg_idx = opcode & 7;  // 0=al, 1=cl, 2=dl, 3=bl, 4=ah, 5=ch, 6=dh, 7=bh

            // Get pointer to the byte register
            u8 *dst_byte = NULL;
            switch (reg_idx) {
                printf("%d",reg_idx);
                case 0: dst_byte = (u8*)&vm->r.rax; break;      // al
                case 1: dst_byte = (u8*)&vm->r.rcx; break;      // cl
                case 2: dst_byte = (u8*)&vm->r.rdx; break;      // dl
                case 3: dst_byte = (u8*)&vm->r.rbx; break;      // bl
                case 4: dst_byte = ((u8*)&vm->r.rax) + 1; break; // ah
                case 5: dst_byte = ((u8*)&vm->r.rcx) + 1; break; // ch
                case 6: dst_byte = ((u8*)&vm->r.rdx) + 1; break; // dh
                case 7: dst_byte = ((u8*)&vm->r.rbx) + 1; break; // bh
                default: return VM_BAD_OPCODE;
            }

            *dst_byte = imm8;

            // No flags affected by MOV
            vm->r.rip += len;
            break;
        }
        // ─── MOV r/m32/64, imm32 (C7 /0) ───
       case 0xC7: {
            if (vm->r.rip + len >= vm->mem_size) return VM_SEGFAULT;
            u8 modrm = *pc++;
            len++;

            u8 subop = (modrm >> 3) & 7;
            if (subop != 0) return VM_BAD_OPCODE;  // only /0 = mov imm

            u8 mod = modrm >> 6;
            u8 rm  = ((vm->rex & 1) << 3) | (modrm & 7);
         //   printf("subop= %02x, mod= %02x, rm=%02x\n", subop, mod, rm);
            bool is_64bit = (vm->rex & 8) != 0;

            if (mod == 3) {
                // Register destination
                if (vm->r.rip + len + 4 > vm->mem_size) return VM_SEGFAULT;
                i32 imm32 = (i32)read_le32(pc);
                len += 4;
                u64 value = is_64bit ? (u64)(i64)imm32 : (u64)(u32)imm32;
                u64 *dst = get_gpr(vm, rm);
                if (!dst) return VM_BAD_OPCODE;
                *dst = value;
            } else {
                u64 addr = get_effective_address(vm, mod, rm, &pc, &len);
                if (addr == 0 || addr + (is_64bit ? 7 : 3) >= vm->mem_size) {
                    return VM_SEGFAULT;
                }
                i32 imm32 = (i32)read_le32(pc);
                len += 4;
                u64 value = is_64bit ? (u64)(i64)imm32 : (u64)(u32)imm32;
                if (is_64bit) {
                    *(u64*)(vm->memory + addr) = value;
                } else {
                    *(u32*)(vm->memory + addr) = (u32)value;
                }
            }

            vm->r.rip += len;
            break;  // MUST have break here!
        }

        // ─── INC / DEC r64   FF /0 /1 ───
        case 0xFF: {
            if (vm->r.rip + len >= vm->mem_size) return VM_SEGFAULT;
            u8 modrm = *pc++;
            len++;
            u8 subop = (modrm >> 3) & 7;
            u8 mod = modrm >> 6;
            u8 rm  = ((vm->rex & 1) << 3) | (modrm & 7);
          
            if (mod != 3) return VM_BAD_OPCODE; // only reg for now

            u64 *dst = get_gpr(vm, rm);
            if (!dst) return VM_BAD_OPCODE;

            u64 old = *dst;

            if (subop == 0) { // INC
                (*dst)++;
                set_flags_add_sub(vm, old, 1, *dst, false);
            } else if (subop == 1) { // DEC
                (*dst)--;
                set_flags_add_sub(vm, old, 1, *dst, true);
            } else {
                return VM_BAD_OPCODE;
            }

            vm->r.rip += len;
            break;
        }

        // ─── PUSH r64   50+rd ───
        case 0x50: case 0x51: case 0x52: case 0x53:
        case 0x54: case 0x55: case 0x56: case 0x57: {
            u8 reg = (opcode & 7) | ((vm->rex & 1) << 3);
            u64 *src = get_gpr(vm, reg);
            if (!src) return VM_BAD_OPCODE;

            vm->r.rsp -= 8;
            if (vm->r.rsp + 8 > vm->mem_size || vm->r.rsp >= vm->mem_size) return VM_SEGFAULT;

            *(u64*)(vm->memory + vm->r.rsp) = *src;
            vm->r.rip += len;
            break;
        }

        // ─── POP r64   58+rd ───
        case 0x58: case 0x59: case 0x5A: case 0x5B:
        case 0x5C: case 0x5D: case 0x5E: case 0x5F: {
            u8 reg = (opcode & 7) | ((vm->rex & 1) << 3);
            u64 *dst = get_gpr(vm, reg);
            if (!dst) return VM_BAD_OPCODE;

            if (vm->r.rsp >= vm->mem_size - 7) return VM_SEGFAULT;

            *dst = *(u64*)(vm->memory + vm->r.rsp);
            vm->r.rsp += 8;
            vm->r.rip += len;
            break;
        }

        // ─── Short conditional jumps (70–7F family) ───
        case 0x70: case 0x71: case 0x72: case 0x73:
        case 0x74: case 0x75: case 0x76: case 0x77:
        case 0x78: case 0x79: case 0x7A: case 0x7B:
        case 0x7C: case 0x7D: case 0x7E: case 0x7F: {
            if (vm->r.rip + len >= vm->mem_size) return VM_SEGFAULT;

            i8 disp = read_disp8(pc);
            len++;

            bool take = false;

            switch (opcode & 0x0F) {
                case 0x0: take = (vm->r.rflags & FLAG_OF) != 0; break;  // jo
                case 0x1: take = (vm->r.rflags & FLAG_OF) == 0; break;  // jno
                case 0x2: take = (vm->r.rflags & FLAG_CF) != 0; break;  // jb
                case 0x3: take = (vm->r.rflags & FLAG_CF) == 0; break;  // jnb
                case 0x4: take = (vm->r.rflags & FLAG_ZF) != 0; break;  // jz
                case 0x5: take = (vm->r.rflags & FLAG_ZF) == 0; break;  // jnz
                case 0x6: take = (vm->r.rflags & (FLAG_CF | FLAG_ZF)) != 0; break;  // jbe
                case 0x7: take = (vm->r.rflags & (FLAG_CF | FLAG_ZF)) == 0; break;  // ja
                case 0xC: take = ((vm->r.rflags & FLAG_SF) != 0) != ((vm->r.rflags & FLAG_OF) != 0); break;  // jl
                case 0xD: take = ((vm->r.rflags & FLAG_SF) != 0) == ((vm->r.rflags & FLAG_OF) != 0); break;  // jge
                case 0xE: take = (vm->r.rflags & FLAG_ZF) != 0 || (((vm->r.rflags & FLAG_SF) != 0) != ((vm->r.rflags & FLAG_OF) != 0)); break;  // jle
                case 0xF: take = (vm->r.rflags & FLAG_ZF) == 0 && (((vm->r.rflags & FLAG_SF) != 0) == ((vm->r.rflags & FLAG_OF) != 0)); break;  // jg
                default: return VM_BAD_OPCODE;
            }
      
        if (take) {
            vm->r.rip += (i64)disp;
        }
        vm->r.rip += len;
        break;
    }

        case 0xE8: {  // call rel32
        if (vm->r.rip + len + 4 > vm->mem_size) return VM_SEGFAULT;
        i32 disp = (i32)read_le32(pc);
        pc += 4;
        len += 4;

        // Push return address (rip after this instruction)
        u64 ret_addr = vm->r.rip + len;
        printf("DEBUG CALL: pushing return addr 0x%llx, jumping +%d to 0x%llx\n", 
           ret_addr, disp, vm->r.rip + (i64)disp + len);
        vm->r.rsp -= 8;
        if (vm->r.rsp + 8 > vm->mem_size || vm->r.rsp >= vm->mem_size) {
            vm->r.rsp += 8;  // rollback
            return VM_SEGFAULT;
        }
        *(u64*)(vm->memory + vm->r.rsp) = ret_addr;

        // Jump
        vm->r.rip += (i64)disp;
        vm->r.rip += len;  // already includes the disp32

        break;
    }
    case 0xC3: {  // ret
    if (vm->r.rsp >= vm->mem_size - 7) return VM_SEGFAULT;
        u64 ret_addr = *(u64*)(vm->memory + vm->r.rsp);
         printf("DEBUG RET: popping return addr 0x%llx, jumping to 0x%llx\n", ret_addr, ret_addr);
        vm->r.rsp += 8;
        vm->r.rip = ret_addr;
        break;
    }

        // ─── HLT ───
        case 0xF4:
            return VM_HALT;

        // ─── NOP ───
        case 0x90:
            vm->r.rip += len;
            break;

        default:
            return VM_BAD_OPCODE;
      }
      printf("Debug step instruction= ");
      for(sz i=0; i<len; i++){
           printf("%02x ", p[i]);
       }
       printf("\n");
       return VM_OK;
    
}



VmStatus vm_run(VM *vm) {
    VmStatus st;
    while ((st = step(vm)) == VM_OK) {
        // single step — can add breakpoint later
    }
    return st;
}

// ───────────────────────────────────────────────
// Debug print
// ───────────────────────────────────────────────
void vm_dump_registers(const VM *vm) {
    const Registers *r = &vm->r;
    printf("RAX=%016lx  RBX=%016lx  RCX=%016lx  RDX=%016lx\n",
           r->rax, r->rbx, r->rcx, r->rdx);
    printf("RSI=%016lx  RDI=%016lx  RBP=%016lx  RSP=%016lx\n",
           r->rsi, r->rdi, r->rbp, r->rsp);
    printf(" R8=%016lx   R9=%016lx  R10=%016lx  R11=%016lx\n",
           r->r8,  r->r9,  r->r10,  r->r11);
    printf("RIP=%016lx RFLAGS=%016lx  [ CF=%d  ZF=%d  SF=%d  OF=%d ]\n",
           r->rip, r->rflags,
           (int)!!(r->rflags & FLAG_CF),
           (int)!!(r->rflags & FLAG_ZF),
           (int)!!(r->rflags & FLAG_SF),
           (int)!!(r->rflags & FLAG_OF));
}

void vm_dump_memory(const VM *vm, sz bytes) {
    sz lim = bytes < vm->mem_size ? bytes : vm->mem_size;
    printf("Memory first %zu bytes:\n", lim);
    for (sz i = 0; i < lim; i++) {
        printf("%02x ", vm->memory[i]);
        if ((i & 15) == 15) printf("\n");
    }
    printf("\n");
}

// ───────────────────────────────────────────────
// Example usage (replace your main)
// ───────────────────────────────────────────────
int main(void) {
    VM *vm = vm_create(1ULL << 20);  // 1 MiB
    if (!vm) return 1;

    // You can later generate this with your exampleprogram()
    // For now — hand-written tiny test
 static const u8 tiny[] = {
    
    0xB0, 0xAB,   // mov al, 0xAB
    0xB4, 0xCD,   // mov ah, 0xCD
    0xB1, 0x12,   // mov cl, 0x12
        // Test byte ADD r/m8, r8
    0xB0, 0x05,                 // mov al, 5
    0xB1, 0x03,                 // mov cl, 3
    0x00, 0xC8,                 // add al, cl   → al=8, no carry


    0x48, 0xB0, 0xAB,   // mov r8b, 0xAB   (with REX.B=1)
    0x48, 0xB1, 0xCD,   // mov r9b, 0xCD
  
   // 0xF4,        // hlt
    
    0x48, 0xB8,0x88,0x77,0x66,0x55,0x44,0x33,0x22,0x11,     // mov rax, 0x1122334455667788
    0x48, 0x89, 0xC3,                                           // mov rbx, rax
    0x48, 0x31, 0xC0,                                           // xor rax, rax
    0x48, 0x05, 0x00,0x22,0x11,0x44,                            // add rax, 0x44112200
    0x48, 0x81,0xEB,0x78,0x56,0x34,0x12,                      // sub rbx, 0x12345678
    0x48, 0xFF, 0xC0,                                           // inc rax

    0x48, 0xBB, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // mov rbx, 8

    // mov qword [0x1000], 0x1122334455667788 (sign-extended from imm32)
    0x48, 0xC7, 0x04, 0x25, 0x00, 0x10, 0x00, 0x00,
    0x88, 0x77, 0x66, 0x55,   // imm32 low 32 bits (high bits will be sign-extended)

    // mov qword [0x1008], 0xAABBCCDD11223344 (sign-extended from imm32)
    0x48, 0xC7, 0x04, 0x25, 0x08, 0x10, 0x00, 0x00,
    0x44, 0x33, 0x22, 0x11,   // imm32 = 0x11223344 → becomes 0xFFFFFFFF11223344

    0x48, 0x8B, 0x83, 0x00, 0x10, 0x00, 0x00,           // mov rax, [rbx + 0x1000]

          // mov rax, 0x1122334455667788
    0x48, 0xB8, 0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11,
    // mov qword [0x1000], rax
    0x48, 0x89, 0x04, 0x25, 0x00, 0x10, 0x00, 0x00,

    // mov rax, 0xAABBCCDD11223344
    0x48, 0xB8, 0x44, 0x33, 0x22, 0x11, 0xDD, 0xCC, 0xBB, 0xAA,
    // mov qword [0x1008], rax
    0x48, 0x89, 0x04, 0x25, 0x08, 0x10, 0x00, 0x00,

   // RIP-relative test
    0x48, 0x8B, 0x05, 0x00, 0x00, 0x00, 0x00,   // mov rax, [rip + 0]

    // SIB test
    0x48, 0xBB, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // mov rbx, 0x1000      
    0x48, 0xC7, 0xC1, 0x02, 0x00, 0x00, 0x00, // mov rcx, 2
    0x48, 0x8B, 0x44, 0x8B, 0x08,                                 // mov rax, [rbx + rcx*4 + 8]


    0x48, 0x8B, 0x05, 0x08, 0x00, 0x00, 0x00,   // mov rax, [rip + 8]
    0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,   // 8 nops
    0x48, 0xB8, 0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x00, 0x00, 0x00,  // known value
   
   
    //loop
    // 0x48, 0x8B, 0x83, 0x00, 0x10, 0x00, 0x00,
    // 0x50,
    // 0x48, 0xFF, 0xCB,
    // 0x75, 0xF3,

    0x48, 0x89, 0xC1,
    0x58,
    

     // call/ret test
    0xE8, 0x04, 0x00, 0x00, 0x00,   // call +8
    0x90, 0x90, 0x90,               // 3 nops
    0xC3,                           // ret
    0x90, 0x90,                     // padding
       // Example: set ZF=1, then jz forward, jnz backward
    0x48, 0x31, 0xC0,                       // xor rax, rax   → ZF=1
    0x74, 0x05,                             // jz +5 (skip over jnz)
    0x75, 0xFC,                             // jnz -4 (infinite loop if ZF=0)
    0x90, 0x90, 0x90,                       // landing pad
    // Factorial-like (very simple)
      0xB9, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,0x00,             // mov ecx, 5
      0xBA, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,0x00,            // mov edx, 1
      0xE8, 0x08, 0x00, 0x00, 0x00,             // call mul_loop
   
      0xF4,                                     // hlt

    // // mul_loop: edx *= ecx; dec ecx; jnz mul_loop; ret
     0x0F, 0xAF, 0xD1,                         // imul edx, ecx
     0xFF, 0xC9,                               // dec ecx
     0x75, 0xF9,                               // jnz -7 (back to imul)
     0xC3,                                     // ret

     0xF4                                              // hlt
};

    vm_load_program(vm, tiny, sizeof(tiny));
    VmStatus st = vm_run(vm);

    printf("VM stopped with status: %d\n", st);
   // printf("Final RSP = 0x%llx\n", vm->r.rsp);
   // vm_dump_memory(vm,0x100);
 //   vm_dump_registers(vm);

    vm_destroy(vm);
    return 0;
}