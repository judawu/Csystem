/*vm*/
#include "vm.h"
//memory functuon 
    void copy(u8 *dst, u8 *src, size_t size){
    size_t i;
    for(i=size; i; i--){
        *dst++ = *src++;
     }
    return;
    };
    void zero(u8 *p, size_t size){
    size_t i;  
    for(i=size; i; i--){
        *p++ = 0;
    }
    return;
    };

    void printhex(u8 *data, size_t size,char sep){
    size_t i;
    for(i=0; i<size; i++){
        printf("%02x%c", *data++,sep);
    }
    printf("\n");
    return;
    };
  
    

// 向表中添加一条新指令（在组装/执行时调用）
void addInstruction(InstructionTable *instrtab, Instruction *instr) {
    // 需要扩容时（简单策略：每次容量翻倍）
    if (instrtab->count >= instrtab->capacity) {
        size_t new_cap = instrtab->capacity == 0 ? 16 : instrtab->capacity * 2;
        Instruction **new_ptr = realloc(instrtab->instructions,
                                        new_cap * sizeof(Instruction*));
        if (!new_ptr) {
            // 扩容失败，可选择报错或忽略
            return;
        }
        instrtab->instructions = new_ptr;
        instrtab->capacity = new_cap;
    }

    // 添加新指令指针
    instrtab->instructions[instrtab->count++] = instr;
}
// 创建一个空的指令表
InstructionTable *createInstrTable(void) {
    InstructionTable *table = malloc(sizeof(InstructionTable));
    if (!table) return NULL;

    zero((u8*)table,sizeof(InstructionTable));
    return table;
};
// 获取第 idx 条指令（用于反编译遍历）
Instruction *getInstruction(InstructionTable *instrtab, size_t idx) {
    if (idx >= instrtab->count) return NULL;
    return instrtab->instructions[idx];
}

// 释放整个表（包括所有指令内存）
void printInstrTable(InstructionTable *instrtab) {
    Instruction *instr;
    if (!instrtab) return;
     for (size_t i=0;i<instrtab->count;i++ )
       {
        instr=getInstruction(instrtab,i);
        printf("Instruction idx:   %ld\n",i);
        printf("-->Instruction opcode:   %2x\n",instr->o);
        printf("-->Instruction lens:   %d\n",instr->instrlen);
        printf("-->Instruction bytes:");
        for (u8 j=0;j<instr->instrlen;j++)
              printf(" %2x",instr->bytes[j]);
        printf("\n");
       };
       return;
   
};



// 辅助：格式化有效地址 (ModR/M + SIB + disp)
static const char* formatAddress(const u8 *b, int modrm_pos, int *out_bytes_used, u64 iptr) {
    static char addrBuf[128];
    u8 modrm = b[modrm_pos];
    int mod = (modrm >> 6) & 0x03;
    int rm  = (modrm ) & 0x07;
    int reg  = (modrm >> 3) & 0x07;
  
    int bytes_used = 1; // ModR/M 本身
    addrBuf[0] = '\0';

    if (mod == 3) {
        // 寄存器直接模式
         strcat(addrBuf, 
             reg64[reg]);
        return addrBuf;
    }

    strcat(addrBuf, "[");

    int has_sib = 0;
    int base = -1, index = -1, scale = 1;

    if (mod == 0 && rm == 4) { // [SIB] 无 disp
        has_sib = 1;
    } else if (mod == 1 && rm == 4) { // [SIB + disp8]
        has_sib = 1;
    } else if (mod == 2 && rm == 4) { // [SIB + disp32]
        has_sib = 1;
    } else if (rm == 5 && mod == 0) {
        // RIP-relative (64位模式下 mod=00, rm=101)
        u32 disp = *(u32*)(b + modrm_pos + 1);
        u64 target = iptr + (modrm_pos + 1 + 4) + (i32)disp;
        snprintf(addrBuf + strlen(addrBuf), 
        sizeof(addrBuf) - strlen(addrBuf),
                 "rip + 0x%x ; => 0x%llx",
                  disp, 
                  target);
        *out_bytes_used = 5;
        strcat(addrBuf, "]");
        return addrBuf;
    }

    if (has_sib) {
        u8 sib = b[modrm_pos + 1];
        bytes_used++;
        base  = sib & 7;
        index = (sib >> 3) & 7;
        scale = 1 << ((sib >> 6) & 3); // 1,2,4,8
        if (index == 4) index = -1; // 无 index
    } else {
        base = rm;
    }

    // 输出 base
    if (base != -1 && (has_sib || !has_sib)) {
        if (mod == 0 && rm == 5 && !has_sib) {
            // [disp32]
            u32 disp = *(u32*)(b + modrm_pos + 1);
            snprintf(addrBuf + strlen(addrBuf),
                     sizeof(addrBuf) - strlen(addrBuf),
                     "0x%x",
                      disp);
            bytes_used += 4;
        } else {
            strcat(addrBuf, 
                reg64[base]);
        }
    }

    // 输出 index*scale
    if (index != -1) {
        if (base != -1) strcat(addrBuf, " + ");
        snprintf(addrBuf + strlen(addrBuf), 
        sizeof(addrBuf) - strlen(addrBuf),
                 "%s*%d",
                  reg64[index], 
                  scale);
    }

    // disp8 / disp32
    if (mod == 1) {
        int8_t disp8 = b[modrm_pos + bytes_used];
        if (disp8 >= 0)
            snprintf(addrBuf + strlen(addrBuf),
             sizeof(addrBuf) - strlen(addrBuf), 
             " + 0x%x", 
             disp8);
        else
            snprintf(addrBuf + strlen(addrBuf), 
            sizeof(addrBuf) - strlen(addrBuf), 
            " - 0x%x", 
            -disp8);
        bytes_used++;
    } else if (mod == 2) {
        i32 disp32 = *(i32*)(b + modrm_pos + bytes_used);
        if (disp32 >= 0)
            snprintf(addrBuf + strlen(addrBuf), 
            sizeof(addrBuf) - strlen(addrBuf), " + 0x%x", 
            disp32);
        else
            snprintf(addrBuf + strlen(addrBuf), 
            sizeof(addrBuf) - strlen(addrBuf), 
            " - 0x%x" ,
             -disp32);
        bytes_used += 4;
    }

    strcat(addrBuf, "]");
    *out_bytes_used = bytes_used;
    return addrBuf;
};


const char* disasmInstruction(const Instruction *instr,u64 current_addr) {
    if (!instr || instr->instrlen == 0) 
        return "<invalid>";

    Opcode op = instr->o;
    const u8 *b = instr->bytes;
    int modpos=2;
    if((*b & 0xF0) != 0x40 ){
        if(b[1] == 0x0F){
             modpos=3;
        }else{
            modpos=1;
        }
    }

    u8 len = instr->instrlen;
    // 辅助宏：读取立即数（小端）
    #define U8(n)  ((n) < len ? b[n] : 0)
    #define U32(n) (U8(n) | (U8(n+1)<<8) | (U8(n+2)<<16) | (U8(n+3)<<24))
    #define U64(n) ((u64)U32(n) | ((u64)U32(n+4)<<32))
    #define S8(n)  ((i8)U8(n))
    #define S32(n) ((i32)U32(n))

    switch (op) {
        case NOP:  
            snprintf(disasm_buf, sizeof(disasm_buf), "nop"); 
            break;
        case HLT:  
            snprintf(disasm_buf, sizeof(disasm_buf), "hlt"); 
            break;
        case RET:  
            snprintf(disasm_buf, sizeof(disasm_buf), "ret"); 
            break;
        case INT:  
            snprintf(disasm_buf, 
                sizeof(disasm_buf),
                 "int    0x%02x", 
                  U8(1)); 
            break;

        // 跳转与调用（相对偏移）
        case JZ:   
            snprintf(disasm_buf, 
                sizeof(disasm_buf), 
                "jz/je     0x%llx", 
                current_addr + len + S8(1)); 
            break;
        case JNZ:  
            snprintf(disasm_buf, 
            sizeof(disasm_buf), 
            "jnz/jne    0x%llx" , 
            current_addr + len + S8(1)); 
            break;
        case JC:   
            snprintf(disasm_buf, 
            sizeof(disasm_buf), 
            "jc/jb     0x%llx", 
            current_addr + len + S8(1)); 
            break;
        case JNC:  
            snprintf(disasm_buf, 
                sizeof(disasm_buf), 
                "jnc/jnb    0x%llx", 
                current_addr + len + S8(1)); 
            break;
        case JBE:  
            snprintf(disasm_buf, 
                sizeof(disasm_buf), 
                "jbe/jna    0x%llx", 
                current_addr + len + S8(1)); 
            break;
        case JA:  
            snprintf(disasm_buf, 
                sizeof(disasm_buf), 
                "ja/jbe    0x%llx", 
                current_addr + len + S8(1)); 
            break;
         case JL:  
            snprintf(disasm_buf, 
                sizeof(disasm_buf), 
                "jl/jnge    0x%llx", 
                current_addr + len + S8(1)); 
            break;
         case JGE:  
            snprintf(disasm_buf, 
                sizeof(disasm_buf), 
                "jge/jnl    0x%llx", 
                current_addr + len + S8(1)); 
            break;
        case JLE:  
            snprintf(disasm_buf, 
                sizeof(disasm_buf), 
                "jle/jng    0x%llx", 
                current_addr + len + S8(1)); 
            break;
        case JG:  
            snprintf(disasm_buf, 
                sizeof(disasm_buf), 
                "jg/jnle    0x%llx", 
                current_addr + len + S8(1)); 
            break;
        case JMPR: 
            snprintf(disasm_buf, 
                sizeof(disasm_buf), 
                "jmp    0x%llx", 
                current_addr + len + S8(1)); 
            break;
        case JMP:  
            snprintf(disasm_buf,
                 sizeof(disasm_buf),
                  "jmp    0x%llx", 
                  current_addr + len + S32(1)); 
            break;
        case CALL: 
            snprintf(disasm_buf, 
                sizeof(disasm_buf), 
                "call   0x%llx" , 
                current_addr + len + S32(1)); 
            break;
        
         case LOOP: case LOOPNE: case LOOPE: 
            snprintf(disasm_buf, 
                sizeof(disasm_buf), 
                "loop 0x%llx rcx" , 
                 current_addr+len); 
            break;
        // MOV 立即数到寄存器（B8~BF +rd id64）
        case MOV1: case MOV2: case MOV3: case MOV4:
        case MOV5: case MOV6: case MOV7: case MOV8: {
            int reg = op - MOV1;
            u64 imm = (len >= 10) ? U64(2) : 0;
            snprintf(disasm_buf, 
                     sizeof(disasm_buf), 
                    "mov    %s, 0x%llx", 
                    reg64[reg], imm);
            break;
        }

        // MOV imm32 到 r/m64（C7 /0 id）
       case MOVD: {
            if (len >= 7) {
                int used;
                const char* addr = formatAddress(b, modpos, &used, current_addr + 2);
                if (addr && addr[0] != '[') { // 寄存器模式
                    int reg =((*b & 1)<<3) + (b[modpos] & 7);
                    snprintf(disasm_buf, 
                        sizeof(disasm_buf), 
                        "mov    %s, 0x%x", 
                        reg64[reg], 
                        U32(3));
                } else {
                    snprintf(disasm_buf, 
                        sizeof(disasm_buf), 
                        "mov    %s, 0x%x" ,
                        addr ? addr : "[??]",
                        U32(3 + used - 1));
                }
            } else {
               
                strcpy(disasm_buf, "mov    ??");
            }
            break;
        }
        case 0x81: case 0x83:{
            u8 modrm = b[modpos];
            int subop = (modrm >> 3) & 7;
            u64 imm = U64(3) ;
             int used;
            const char* addr = formatAddress(b, modpos, &used, current_addr + 2);
             switch (subop) {
                case 0:   
                    snprintf(disasm_buf, 
                        sizeof(disasm_buf), 
                        "add    %s, 0x%llx" ,
                        addr ? addr : "[??]",
                        imm);
                
                   break;
                case 2: 
                   snprintf(disasm_buf, 
                        sizeof(disasm_buf), 
                        "or    %s, 0x%llx" ,
                        addr ? addr : "[??]",
                        imm);
                  
                   break;
                case 4: 
                    snprintf(disasm_buf, 
                        sizeof(disasm_buf), 
                        "and    %s, 0x%llx" ,
                        addr ? addr : "[??]",
                        imm);
                 
                   break;
                case 6: 
                    snprintf(disasm_buf, 
                        sizeof(disasm_buf), 
                        "xor    %s, 0x%llx" ,
                        addr ? addr : "[??]",
                        imm);
                 
                   break;
                case 5: case 7:
                    snprintf(disasm_buf, 
                        sizeof(disasm_buf), 
                        "cmp    %s, 0x%llx" ,
                        addr ? addr : "[??]",
                        imm);
                   break;
                default:  
                  break;
            }


            break;
          }
        // 通用寄存器间 MOV / XCHG / CMP / ADD / SUB / AND / OR / XOR
        case MOV: case MOVR: 
        case XCHG: case LEA:
        case CMP: case CMP1:case CMP2:case CMP3:
        case ADD: case ADD1:case ADD2:case ADD3:
        case SUB: case SUB1:case SUB2: case SUB3: 
        case AND: case AND1:case AND2: case AND3: 
        case OR: case OR1:case OR2: case OR3:
        case XOR:case XOR1:case XOR2: case XOR3:  {
           const char* mnem[] = {
                [MOV]="mov", 
                [MOVR]="mov", 
                [XCHG]="xchg",
                [LEA]="lea",
                [ADD]="add", 
                [ADD1]="add",
                [ADD2]="add",
                [ADD3]="add",
                [SUB]="sub",
                [SUB1]="sub", 
                [SUB2]="sub", 
                [SUB3]="sub", 
                [XOR]="xor",
                [XOR1]="xor",
                [XOR2]="xor",
                [XOR3]="xor",
                [CMP]="cmp", 
                [CMP1]="cmp", 
                [CMP2]="cmp", 
                [CMP3]="cmp", 
                [AND]="and",
                [AND1]="and", 
                [AND2]="and", 
                [AND3]="and", 
                [OR]="or",
                [OR1]="or",
                [OR2]="or",
                [OR3]="or"
            };
            const char* name = mnem[op];
            int dir_to_reg = (op & 0x02);
            if (len < 3) { 
                snprintf(disasm_buf, 
                    sizeof(disasm_buf), 
                    "%s ??", 
                    name); 
                    break; 
                }

            u8 modrm = b[modpos];
            int reg =((*b  & 1) <<3) + ((modrm >> 3) & 7);
            int rm  = ((*b  & 1) <<3) +  (modrm & 7);
            int mod = modrm >> 6;

            int addr_bytes = 0;
            const char* mem = formatAddress(b, modpos, &addr_bytes, current_addr + 2);

            if (mod == 3) { // reg, reg
                if (dir_to_reg)
                    snprintf(disasm_buf,
                         sizeof(disasm_buf),
                          "%s    %s, %s", 
                          name,
                          reg64[reg],
                          reg64[rm]);
                else
                    snprintf(disasm_buf,
                         sizeof(disasm_buf), 
                         "%s    %s, %s", 
                         name, 
                         reg64[rm],
                         reg64[reg]);
            } else {
                // 内存操作数
                if (dir_to_reg)
                    snprintf(disasm_buf,
                         sizeof(disasm_buf), 
                         "%s    %s, %s",
                          name, 
                          reg64[reg],
                        mem ? mem : "[??]");
                else
                    snprintf(disasm_buf,
                         sizeof(disasm_buf), 
                         "%s    %s, %s",
                          name, mem ? mem : "[??]", 
                          reg64[reg]);
            }
            break;
        }
        //INC DEC
        case 0xFF:
          u8 modrm = b[modpos];
          u8 subop = (modrm >> 3) & 7;
          int addr_bytes = 0;
          const char* mem = formatAddress(b, modpos, &addr_bytes, current_addr + 2);
          if(subop){
                     snprintf(disasm_buf,
                         sizeof(disasm_buf), 
                         "dec    %s",
                         mem ? mem : "[??]"
                         );   
          }else{

              snprintf(disasm_buf,
                         sizeof(disasm_buf), 
                         "inc    %s",
                          mem ? mem : "[??]"
                         );

          }
           break;
        // CMP rax, imm32 (3D id)
        case CMP4: 
           snprintf(disasm_buf, 
            sizeof(disasm_buf),
             "cmp    rax, 0x%X", 
             U32(1)); 
           break;
        case ADD4: 
            snprintf(disasm_buf, 
                sizeof(disasm_buf),
                "add    rax, 0x%X", 
                U32(1));
            break;
        case XOR4: 
            snprintf(disasm_buf, 
                sizeof(disasm_buf),
                "xor    rax, 0x%X", 
                U32(1));
            break;
        case OR4: 
            snprintf(disasm_buf, 
                sizeof(disasm_buf),
                "or    rax, 0x%X", 
                U32(1));
            break;
        case AND4: 
            snprintf(disasm_buf, 
                sizeof(disasm_buf),
                "and    rax, 0x%X", 
                U32(1));
            break;
        // NOT (F7 /2 r/m64)
        case NOT: {
           if (len >= 3) {
                int used;
                const char* addr = formatAddress(b, modpos, &used, current_addr + 2);
                if (addr && addr[0] != '[')
                    snprintf(disasm_buf, 
                        sizeof(disasm_buf),
                         "not    %s", 
                         reg64[b[2] & 7]);
                else
                    snprintf(disasm_buf,
                         sizeof(disasm_buf), 
                         "not    %s", 
                         addr ? addr : "[??]");
            }
            break;
        }

        // PUSH reg (50~57) 和 PUSH imm32 (68)
        case PUSH: case PUSH1: case PUSH2: case PUSH3:
        case PUSH4: case PUSH5: case PUSH6: case PUSH7:
        case PUSH8: {
            if (op == PUSH8)
                snprintf(disasm_buf, 
                    sizeof(disasm_buf),
                     "push   0x%x", 
                     U32(1));
            else
                snprintf(disasm_buf, 
                    sizeof(disasm_buf),
                     "push   %s", 
                     reg64[op - PUSH]);
            break;
        }

        // POP reg (58~5F)
        case POP: case POP1: case POP2: case POP3:
        case POP4: case POP5: case POP6: case POP7: {
            snprintf(disasm_buf, 
                sizeof(disasm_buf), 
                "pop    %s", 
                reg64[op - POP]);
            break;
        }

        // SHL/SAR 等 (C1 /4 ib 或 /5 ib)
        case SHLR: {
            if (len >= 4) {
                u8 modrm = U8(2);
                int subop = (modrm >> 3) & 7;
                const char* name = (subop == 4) ? "shl" : (subop == 5) ? "sar" : "???";
                uint8_t imm = U8(3);
                if ((modrm & 0xC0) == 0xC0) {
                    int reg = modrm & 7;
                    snprintf(
                        disasm_buf, 
                        sizeof(disasm_buf), 
                        "%s    %s, %d",
                         name, 
                         reg64[reg], 
                         imm);
                } else {
                    snprintf(
                        disasm_buf, 
                        sizeof(disasm_buf), 
                        "%s    [??], %d", 
                        name, 
                        imm);
                }
            } else {
                strcpy(disasm_buf, "shift ??");
            }
            break;
        }
                // lAHF
        case LAF: {
            snprintf(disasm_buf, 
                sizeof(disasm_buf), 
                "laf");
            break;
        }
        case SAF: {
            snprintf(disasm_buf, 
                sizeof(disasm_buf), 
                "saf");
            break;
        }
        case PUSHF: {
            snprintf(disasm_buf, 
                sizeof(disasm_buf), 
                "pushf");
            break;
        }
          case POPF: {
            snprintf(disasm_buf, 
                sizeof(disasm_buf), 
                "popf");
            break;
        }

     

      
        // BT r/m64, r64 (0F A3)
        case BT: case MOVZX:case MOVZY: {
             const char* mnem[] = {
                [BT]="bt", 
                [MOVZX]="movz", 
                [MOVZY]="movz",
             };

             const char* name = mnem[op];
            if (len >= 4 ) {
                u8 modrm = U8(3);
                int reg =((*b  & 1) <<3) + ((modrm >> 3) & 7);
                int rm  = ((*b  & 1) <<3) +  (modrm & 7);
                if ((modrm & 0xC0) == 0xC0)
                    snprintf(disasm_buf, 
                        sizeof(disasm_buf), 
                        "%s     %s, %s", 
                        name,
                        reg64[rm], 
                        reg64[reg]);
                else
                    snprintf(disasm_buf, 
                        sizeof(disasm_buf), 
                        "%s     [%s+...], %s",
                         name,
                         reg64[rm], 
                         reg64[reg]);
            } else {
                snprintf(disasm_buf, 
                    sizeof(disasm_buf), 
                    "%s     ??",
                    name);
            }
            break;
        }

        default:
            snprintf(disasm_buf, 
                sizeof(disasm_buf), 
                ".byte  0x%02x    ; unknown ? Ox0F:SETs/MOVZs/CMOVs... ", 
                op);
            break;
    }

    return disasm_buf;
};
void disasmInstrTable(InstructionTable *table) {
    u64 addr = 0;
    printf("Disassembly:\n");
    for (size_t i = 0; i < table->count; i++) {
        Instruction *instr = table->instructions[i];
        printf("%08llx"  ": ", addr);

        // 打印字节
        for (size_t j = 0; j < instr->instrlen && j < 16; j++)
            printf("%02x ", instr->bytes[j]);
        for (size_t j = instr->instrlen; j < 16; j++)
            printf("   ");

        printf("%s\n", disasmInstruction(instr, addr));
        addr += instr->instrlen;
    }
};



// 释放整个表（包括所有指令内存）
void freeInstrTable(InstructionTable *table) {
    if (!table) return;
    for (size_t i = 0; i < table->count; i++) {
        free(table->instructions[i]);  // 释放每条指令的内存
    }
    free(table->instructions);
    free(table);
};
VM *creatVM(){
    VM *vm;
    InstructionTable *itab=createInstrTable();;
    size_t size;
    size=sizeof(VM);
    vm=(VM*)malloc(size);
    if(!vm){
        errno=ErrMem;
        return (VM*)0;
    }
    zero((u8*)vm,size);
    vm->itab=itab;
    vm->rip=(Reg)(vm->m);
    vm->rsp=(Reg)(vm->m+sizeof(vm->m));
    return vm;
};

void removeVM(VM* vm){
  
    if(!vm){
        errno=ErrMem;
        return;
    }
    freeInstrTable(vm->itab);
    free(vm);
    return;
};
void errorVM(VM* vm,Errorcode e){
    int exitcode;
    exitcode= -1;

    if(vm){
      printVM(vm);
      removeVM(vm);
      };

    switch(e){
       case ErrSegv:
        fprintf(stderr,"%s\n","Vitural Macchine Segmentation fault");
        break;
       case Syshlt:
        fprintf(stderr,"%s\n","Vitural Macchine System halted");

        exitcode = 0 ;
        break;
       default:
        break;
    }
    exit(exitcode);
};



// 根据寄存器编号获取 vm 中的对应寄存器指针
Reg* getRegister(VM* vm, u8 reg_num, u8 ostype) {
    // size: 1=64bit, 2=32bit, 3=16bit, 4=8bit high (AH等), 5=8bit low (AL等)
    Reg *base = NULL;
  
    switch (reg_num & 7) {  // 低3位决定基础寄存器
        case 0: base = &vm->rax; break;
        case 1: base = &vm->rcx; break;
        case 2: base = &vm->rdx; break;
        case 3: base = &vm->rbx; break;
        case 4: base = &vm->rsp; break;
        case 5: base = &vm->rbp; break;
        case 6: base = &vm->rsi; break;
        case 7: base = &vm->rdi; break;
    }
    if (reg_num >= 8) {  // r8~r15
        base = &(vm->r8) + (reg_num - 8) * 8 ;
    }

    if (!base) return NULL;

    
    if (ostype == 1) return base;                    // 64位
    if (ostype == 0) return base;                    // 32位（低32位自动截断）
    if (ostype == 2) return base;                    // 16位（低16位）
    if (ostype == 4) return base;                    // 8位低：AL/CL/DL/BL等
    if (ostype == 3) {                               // 8位高：AH/CH/DH/BH (仅低4个寄存器)
        if ((reg_num & 7) >= 3) return NULL;       // SPL/BPL/SIL/DIL 在 REX 下特殊，但高8位不可用
        return (Reg*)((u8*)base + 1);
    }
    return NULL;
}

// 返回操作数的实际内存地址（64位模式下）
// 如果是寄存器，直接返回寄存器指针；如果是内存，返回内存地址
// 如果不支持的寻址方式，返回 NULL 并可设置错误

/*

Mod   Reg   R/M
 11    000   011
   └─────┬─────┘
         ↓
      11000011 （二进制）
      = C3h    （十六进制）
Mod = 11：寻址模式（Addressing Mode），决定 R/M 字段是表示寄存器还是内存寻址的方式
    00:无位移的内存寻址：[基址] 或 [SIB] 如果 R/M=100，则有 SIB
    01:有 8 位有符号位移的内存寻址：[基址 + disp8] 或 [SIB + disp8]
    10:有 32 位有符号位移的内存寻址：[基址 + disp32] 或 [SIB + disp32]
    11:寄存器到寄存器（Register-to-Register）无 SIB

Reg = 000：源寄存器是 EAX（32 位模式下 EAX 的基本编号是 000）
    Reg 值（二进制）基本寄存器（无 REX.R）扩展寄存器（REX.R=1）备注
    000     RAX / EAX / AX / AL     R8                      最常用
    001     RCX / ECX / CX / CL     R9                      常用于参数/循环计数
    010     RDX / EDX / DX / DL     R10                     数据寄存器
    011     RBX / EBX / BX / BL     R11                     基址寄存器
    100     RSP / ESP / SP / AH     R12                     栈指针（特殊）
    101     RBP / EBP / BP / CH     R13                     帧指针
    110     RSI / ESI / SI / DH     R14                     源索引
    111     RDI / EDI / DI / BH     R15                     目的索引

R/M = 011：表示源操作数或目的操作数，可以是寄存器编号，也可以是内存寻址的基址寄存器）
    当 Mod = 11,R/M 直接表示寄存器编号（000~111，对应 RAX~RDI，或扩展到 R8~R15 需 REX.B
    当 Mod = 00/01/10（内存模式）,R/M 表示基址寄存器（Base Register），或者特殊情况
      R/M = 100（二进制 100）：表示使用 SIB 字节（Scale-Index-Base），后面会跟 1 字节 SIB
      R/M = 101 且 Mod = 00：表示无基址 + 32 位位移（通常是 [RIP + disp32]
在 64 位模式下，操作 32 位寄存器（EAX、EBX 等）时，不需要 REX 前缀

SIB
 7  6    5  4  3    2  1  0
┌──┬──┬───┬───┬───┬───┬───┬───┐
│ Scale │   Index   │   Base    │
└──┴──┴───┴───┴───┴───┴───┴───┘
   ↑         ↑          ↑
   │         │          └─ Base 字段（基址寄存器，3位）
   │         └──────────── Index 字段（变址寄存器，3位）
   └──────────────────────── Scale 字段（比例因子，2位）

只有当 ModR/M 中的 R/M = 100（二进制 100）时，才会有 SIB 字节,地址 = Base + (Index × Scale) + Displacement（位移，由 Mod 决定）
    IB 字节（十六进制）Scale        Index           Base        对应汇编写法        实际地址计算                    备注
    00                  00 (×1)     000 (RAX)   000 (RAX)       [RAX + RAX*1]       RAX + RAX                   很少用
    04                  00 (×1)     000 (RAX)   100 (RSP)       [RSP + RAX]         RSP + RAX                   常见于栈操作
    44                  01 (×2)     001 (RCX)   100 (RSP)       [RSP + RCX*2        ]RSP + RCX*2                常见于数组访问
    84                  10 (×4)     010 (RDX)   011 (RBX)       [RBX + RDX*4]       RBX + RDX*4                 最经典的数组索引（int 数组）
    C4                  11 (×8)     100 (无)    100 (RSP)       [RSP + 0]           只用 RSP（无变址）           常见于局部变量（[RSP + disp]）
*/

void* getRmOperand(VM* vm, u8 mod, u8 rm, u8* instr, u8* instr_len, u8 ostype) {
    Reg* reg_ptr = NULL;
    u64 address = 0;

    // 先处理 R/M 字段的扩展（REX.B）
    u8 base_reg = rm;
    if (vm->rex & 0x01) base_reg |= 0x08;  // REX.B 扩展 R8~R15

    // 1. Mod == 11：寄存器直接操作
    if (mod == 3) {
        reg_ptr = getRegister(vm, base_reg, ostype);
        if (reg_ptr) {
            return (void*)reg_ptr;  // 返回寄存器本身的地址
        }
        return NULL;  // 非法寄存器
    }

    // 2. Mod == 00, 01, 10：内存寻址（无位移 / 8位位移 / 32位位移）

    // 特殊情况：rm == 4 (100) 表示使用 SIB 字节
    if (rm == 4) {
        // 读取 SIB 字节
        u8 sib = *instr;
        instr++;
        (*instr_len)++;

        u8 scale = (sib >> 6) & 0x03;    // scale: 00=1, 01=2, 10=4, 11=8
        u8 index = (sib >> 3) & 0x07;    // 变址寄存器
        u8 base  = (sib >> 0) & 0x07;    // 基址寄存器

        if (vm->rex & 0x04) index |= 0x08;  // REX.X 扩展变址
        if (vm->rex & 0x01) base  |= 0x08;  // REX.B 扩展基址

        // 计算基址
        Reg* base_ptr = getRegister(vm, base, ostype);
        if (base_ptr) {
            address += *base_ptr;
        } else if (base == 5 && mod == 0) {
            // [RIP + disp32] 或 [disp32]（无基址时 mod=00, rm=5）
            // 特殊：无基址 + mod=00 + rm=5 → RIP 相对
            i32 disp = *(i32*)instr;
            instr += 4;
            *instr_len += 4;
            address += (u64)(instr + disp);  // RIP 是下一条指令地址
        } else {
            return NULL;  // 非法基址
        }

        // 计算变址
        if (index != 4) {  // index=4 表示无变址
            Reg* index_ptr = getRegister(vm, index, ostype);
            if (index_ptr) {
                u64 scale_factor[] = {1, 2, 4, 8};
                address += *index_ptr * scale_factor[scale];
            }
        }
    }
    else {
        // 普通基址寄存器（无 SIB）
        Reg* base_ptr = getRegister(vm, base_reg, ostype);
        if (base_ptr) {
            address += *base_ptr;
        } else {
            // 特殊情况：mod=00, rm=5 → [RIP + disp32]
            if (mod == 0 && base_reg == 5) {
                i32 disp = *(i32*)instr;
                instr += 4;
                *instr_len += 4;
                address += (u64)(instr + disp);  // RIP 相对
            } else {
                return NULL;  // 非法
            }
        }
    }

    // 3. 处理位移（displacement）
    i64 disp = 0;
    if (mod == 1) {           // 8位位移（有符号）
        disp = (i8)*instr;
        instr++;
        (*instr_len)++;
    }
    else if (mod == 2 || (mod == 0 && base_reg == 5)) {  // 32位位移
        disp = *(i32*)instr;
        instr += 4;
        *instr_len += 4;
    }

    address += disp;

    // 4. 返回最终内存地址
    return (void*)address;
};
void __cle(VM* vm){
   vm->rflags &= ~(1ULL << 6);
}
void __clc(VM* vm){
   vm->rflags &= ~(1ULL << 0);
}
void __cla(VM* vm){
   vm->rflags &= ~(1ULL << 4);
}
void __cls(VM* vm){
   vm->rflags &= ~(1ULL << 7);
}
void __clt(VM* vm){
   vm->rflags &= ~(1ULL << 8);
}

void __cli(VM* vm){
   vm->rflags &= ~(1ULL << 9);
}
void __cld(VM* vm){
   vm->rflags &= ~(1ULL << 10);
}
void __clo(VM* vm){
   vm->rflags &= ~(1ULL << 11);
}
void __cl(VM* vm){
      vm->rflags &= ~((1ULL<<0)|(1ULL<<2)|(1ULL<<4)|(1ULL<<6)|(1ULL<<7)|(1ULL<<11)); // 清 CF PF AF ZF SF OF

}
void __ste(VM* vm){
   vm->rflags |= (1ULL << 6);
}
void __stc(VM* vm){
   vm->rflags |= (1ULL << 0);
}
void __stp(VM* vm){
   vm->rflags |= (1ULL << 2);
}
void __sta(VM* vm){
   vm->rflags |= (1ULL << 4);
}
void __sts(VM* vm){
   vm->rflags |= (1ULL << 7);
}
void __stt(VM* vm){
   vm->rflags |= (1ULL << 8);
}

void __sti(VM* vm){
   vm->rflags |= (1ULL << 9);
}
void __std(VM* vm){
   vm->rflags |= (1ULL << 10);
}
void __sto(VM* vm){
   vm->rflags |= (1ULL << 11);
}
void executeInstruction(VM* vm) {
    u8  *pc = (u8*)vm->rip;           // 当前指令指针  
    u8  *orgpc=pc;
    u8 instr_len = 0;
    vm->rex = 0;
    // 1. REX 前缀（可选）
    u8 extended_opcode = 0;  // 如果是 0x0F 开头，保存第二个字节
    bool is_two_byte = false;  // 是否是 0x0F 开头的指令
    for(;;) {
        if ((*pc & 0xF0) == 0x40) {               // REX 前缀 0x40-0x4F
            vm->rex = *pc;
            pc++;
            instr_len++;
        } else if (*pc == 0x0F) {                 // 两字节 opcode 前缀
            pc++;
            instr_len++;
            extended_opcode = *pc;                // 保存第二个字节
          
            is_two_byte = true;
            break;  // 0x0F 后紧跟扩展 opcode，不再循环
        } else {
            break;  // 非前缀，退出
        }
    }
    
    u8 opcode = *pc;
    if (is_two_byte) {
        opcode = extended_opcode;
    }
    pc++;
    instr_len++;
    u8 is_64bit = (vm->rex & 0x08) != 0;  // REX.W = 64位操作数
   
  // 是否需要 ModR/M（根据真实 opcode 判断）

    bool needs_modrm = false;
 
    if (is_two_byte) {
        // 两字节指令中需要 ModR/M 的
        needs_modrm = (opcode == 0xA3 ||                           // BT
                       (opcode >= 0x40 && opcode <= 0x4F) ||         // CMOVcc
                       (opcode >= 0x90 && opcode <= 0x9F) ||         // SETcc
                       opcode == 0xB6 || opcode == 0xB7 );               
    } else {
        // 单字节指令
        needs_modrm = ( opcode == 0xC7 ||
         opcode ==  0x00 || opcode == 0x01|| opcode == 0x02|| opcode == 0x03  // ADD
        || opcode == 0x08 || opcode == 0x09|| opcode == 0x0A|| opcode == 0x0B  // OR
        || opcode == 0x20 || opcode == 0x21|| opcode == 0x22|| opcode == 0x23  // AND
        || opcode == 0x28 || opcode == 0x29|| opcode == 0x2A|| opcode == 0x2B  // SUB
        || opcode == 0x30 || opcode == 0x31|| opcode == 0x32|| opcode == 0x33  // XOR
        || opcode == 0x38 || opcode ==  0x39|| opcode == 0x3A|| opcode == 0x3B
        || opcode == 0x81 || opcode == 0x83 || opcode == 0x87 ||opcode == 0x89 
        || opcode == 0x8B || opcode == 0x8D 
        || opcode == 0xC1 || opcode == 0xC7
        ||  opcode == 0xF7 ||opcode == 0xFF);  // INC/DEC/LEA
    }

    u8 modrm = 0, mod = 0, reg = 0, rm = 0;
    if (needs_modrm) {
        modrm = *pc;
        pc++;
        instr_len++;
        mod = (modrm >> 6) & 0x03;
        reg = (modrm >> 3) & 0x07;
        rm  = modrm & 0x07;
        if (vm->rex & 0x04) 
            reg |= 0x08;
        if (vm->rex & 0x01) 
            rm  |= 0x08;
    }
   
   
  
    // 主解码

    if (!is_two_byte){
        switch (opcode) {
        
      // ==================== NOP / HLT ==================== 
        case 0x90:  // NOP
             {
                vm->rip += instr_len;
                break;
            }
       

        case 0xF4:  // HLT
        {
            vm->rip += instr_len;
            Syshltfault(vm);
            return;
        }

        case 0xCD:  // INT n
        {
            u8 int_num = *pc;
            pc++;
            instr_len++;
            // 简单处理：INT 3 为断点
            vm->rip += instr_len;
            if (int_num == 3) 
               segfault(vm);
            else 
              errorVM(vm, int_num);
            return;
        }
       // ==================== MOV r64, imm64 (B8+rd io) ==================== 
        case 0xB8: case 0xB9: case 0xBA: case 0xBB:
        case 0xBC: case 0xBD: case 0xBE: case 0xBF: {
            u8 reg_num = (opcode & 0x07) | ((vm->rex & 0x01) ? 8 : 0);
            Reg imm64=*(Reg*)pc;
            pc += 8;
            instr_len += 8;
            Reg *dst = getRegister(vm, reg_num, is_64bit);
            if (!dst) { segfault(vm);  return; }
            *dst = imm64;
            vm->rip += instr_len;
            break;
        }
        // ==================== MOV r/m64, imm32 (C7 /0 id) ==================== 
        case 0xC7: {
            if ((modrm & 0x38) == 0x00) {  // /0
                void* dst_ptr = getRmOperand(vm, mod, rm, pc, &instr_len, is_64bit);
                if (dst_ptr) {
                    i32 imm = *(i32*)pc;
                    pc += 4;
                    instr_len += 4;
                    *(Reg*)dst_ptr = (is_64bit) ? (i64)imm : (u32)imm;
                    vm->rip += instr_len;
               
                } else {
                   
                    segfault(vm);
                    return;
                }
            }
            break;
        }
       //==================== MOV r/m64, r64  (89) / MOV r64, r/m64 (8B) ==================== 
        case 0x89:  // MOV r/m64, r64
        case 0x8B:  // MOV r64, r/m64
       {
            void* src_ptr = (opcode == 0x89) ? getRegister(vm, reg, is_64bit)
                                             : getRmOperand(vm, mod, rm, pc, &instr_len, is_64bit);
            void* dst_ptr = (opcode == 0x89) ? getRmOperand(vm, mod, rm, pc, &instr_len, is_64bit)
                                             : getRegister(vm, reg, is_64bit);
            if (src_ptr && dst_ptr) {
                *(Reg*)dst_ptr = *(Reg*)src_ptr;   
                  
            } else {
                segfault(vm);
                return;
            }
            vm->rip += instr_len;
            break;
        }

      

      // ==================== 通用算术/逻辑指令 (ADD OR AND SUB XOR CMP) ==================== 
        case 0x00: case 0x01: case 0x02: case 0x03:  // ADD
        case 0x08: case 0x09: case 0x0A: case 0x0B:  // OR
        case 0x20: case 0x21: case 0x22: case 0x23:  // AND
        case 0x28: case 0x29: case 0x2A: case 0x2B:  // SUB
        case 0x30: case 0x31: case 0x32: case 0x33:  // XOR
        case 0x38: case 0x39: case 0x3A: case 0x3B: { // CMP
            bool is_add = (opcode & ~0x07) == 0x00;
            bool is_or  = (opcode & ~0x07) == 0x08;
            bool is_and = (opcode & ~0x07) == 0x20;
            bool is_sub = (opcode & ~0x07) == 0x28;
            bool is_xor = (opcode & ~0x07) == 0x30;
            bool is_cmp = (opcode & ~0x07) == 0x38;

            bool dir_to_rm = (opcode & 0x02);   // D=1: reg → r/m
            void *rm_ptr = getRmOperand(vm, mod, rm, pc, &instr_len, is_64bit);
            Reg *reg_ptr = getRegister(vm, reg, is_64bit);
            if (!rm_ptr || !reg_ptr) { segfault(vm);  return; }

            u64 dst_val = *(u64*)rm_ptr;
            u64 src_val = *reg_ptr;
            u64 src = dir_to_rm ? src_val : dst_val;
            u64 dst = dir_to_rm ? dst_val : src_val;
            u64 result;

            if (is_add) result = dst + src;
            else if (is_sub || is_cmp) result = dst - src;
            else if (is_and) result = dst & src;
            else if (is_or)  result = dst | src;
            else if (is_xor) result = dst ^ src;
            else result = 0;

            // 标志位完整更新
            __cl(vm);
            if (result == 0) 
              __ste(vm);                                    // ZF
            if ((i64)result < 0) 
              __sts(vm);                                // SF

            // CF
            if (is_add && result < dst)
              __stc(vm);
            if ((is_sub || is_cmp) && dst < src) 
              __stc(vm);

            // OF
            if (is_add && (((dst ^ result) & (src ^ result)) & (1ULL<<63))) 
              __sto(vm);
            if ((is_sub || is_cmp) && (((dst ^ src) & (dst ^ result)) & (1ULL<<63))) 
              __sto(vm);

            // PF
            u8 parity_byte = result & 0xFF;
            parity_byte ^= parity_byte >> 4;
            parity_byte ^= parity_byte >> 2;
            parity_byte ^= parity_byte >> 1;
            if ((parity_byte & 1) == 0) 
              __stp(vm);

            // AF
            if (((dst ^ src) ^ result) & 0x10) 
               __sta(vm);

            if (!is_cmp) {
                if (dir_to_rm)
                    *(u64*)rm_ptr = result;
                else
                    *reg_ptr = result;
            }
            vm->rip += instr_len;
            break;
        }

       // ==================== 立即数形式 (83 /x ib, 81 /x id, 05 id 等) ==================== 
        
        case 0x83: case 0x81:{
           
            u8 subop = (modrm >> 3) & 7;
            i64 imm;
            bool sign_ext = (opcode == 0x83);
            void *dst = getRmOperand(vm, mod, rm, pc, &instr_len, is_64bit);
                if (!dst) { segfault(vm);  return; }
            if (sign_ext) {
                    imm = (i8)*pc; 
                    pc++; 
                    instr_len++;
                } else {
                    imm = *(i32*)pc; 
                    pc += 4; 
                    instr_len += 4;
                }

            u64 val = *(u64*)dst;
            u64 result;
            bool is_cmp = (subop == 7);

            switch (subop) {
                case 0: result = val + imm; break;
                case 2: result = val | imm; break;
                case 4: result = val & imm; break;
                case 6: result = val ^ imm; break;
                case 5: case 7: result = val - imm; break;
                default: segfault(vm); return;
            }

             
            __cl(vm);
            if (result == 0) 
                __ste(vm);
            if ((i64)result < 0) 
                __sts(vm);
            if (subop == 0 && result < val) 
                __stc(vm);
            if ((subop == 5 || subop == 7) && val < (u64)imm) 
                __stc(vm);;
            
                // OF
            if (subop==0 && (((val ^ result) & (imm ^ result)) & (1ULL<<63))) 
                __sto(vm);;
            if ((subop==5 || is_cmp) && (((val ^ imm) & (val ^ result)) & (1ULL<<63))) 
                __sto(vm); 

            // PF
            u8 parity_byte = result & 0xFF;
            parity_byte ^= parity_byte >> 4;
            parity_byte ^= parity_byte >> 2;
            parity_byte ^= parity_byte >> 1;
            if ((parity_byte & 1) == 0) 
               __stp(vm);;

            // AF
            if (((val ^ imm) ^ result) & 0x10) 
               __sta(vm);
            if (!is_cmp) 
                *(u64*)dst = result;
            vm->rip += instr_len;
            break;
        
        }
       
        case 0x05: case 0x0D: case 0x25: case 0x35: case 0x3D: 
        {     
            u8 subop;
            i64 imm;
            subop = (opcode == 0x05 ? 0 : opcode == 0x0D ? 2 : opcode == 0x25 ? 4 : opcode == 0x35 ? 6 : 7);
            imm = *(i32*)pc; 
            pc += 4; 
            instr_len += 4;
          
            u64 val = vm->rax;
            u64 result;
            bool is_cmp = (subop == 7);
            switch (subop) {
                case 0: 
                    result = val + imm; 
                    break;
                case 2: 
                    result = val | imm; 
                    break;
                case 4: 
                    result = val & imm; 
                    break;
                case 6: 
                    result = val ^ imm; 
                    break;
                case 5: case 7: 
                    result = val - imm; 
                    break;
                default: result = val;
            }

            __cl(vm);
            if (result == 0) 
              __ste(vm);                                    // ZF
            if ((i64)result < 0) 
              __sts(vm);                                // SF

            // CF
            if (subop==0 && result < val) 
                __stc(vm);
            if ((subop==5  || is_cmp) && val < imm)
                __stc(vm);

            // OF
            if (subop==0 && (((val ^ result) & (imm ^ result)) & (1ULL<<63))) 
                __sto(vm);
            if ((subop==5 || is_cmp) && (((val ^ imm) & (val ^ result)) & (1ULL<<63))) 
                __sto(vm);

            // PF
            u8 parity_byte = result & 0xFF;
            parity_byte ^= parity_byte >> 4;
            parity_byte ^= parity_byte >> 2;
            parity_byte ^= parity_byte >> 1;
            if ((parity_byte & 1) == 0) 
              __stp(vm);

            // AF
            if (((val ^ imm) ^ result) & 0x10) 
                __sta(vm);

            if (!is_cmp) vm->rax = result;
            vm->rip += instr_len;
            break;
        }

       //==================== NOT (F7 /2) ==================== 
        case 0xF7: {
           
            if ((modrm & 0x38) == 0x10) {  // /2 = NOT
             
                void *dst = getRmOperand(vm, mod, rm, pc, &instr_len, is_64bit);
                if (!dst) { segfault(vm); return; }
                *(Reg*)dst = ~*(Reg*)dst;
            }else{
                segfault(vm);
            }
            vm->rip += instr_len;
            break;
        }

       // ==================== 移位 (C1 /4 ib, /5 ib, /7 ib) ==================== 
        case 0xC1: { 
            u8 subop = (modrm >> 3) & 7;
            if (subop == 4 || subop == 5 || subop == 7) {  // SHL, SHR, SAR
             
                void *dst = getRmOperand(vm, mod, rm, pc, &instr_len, is_64bit);
                if (!dst) { 
                    segfault(vm); 
                     return; }
                u8 shift = *pc; 
                pc++; 
                instr_len++;
                Reg val = *(Reg*)dst;
                if (subop == 4) *(Reg*)dst = val << shift;
                else if (subop == 5) *(Reg*)dst = val >> shift;
                else if (subop == 7) *(Reg*)dst = (u64)((i64)val >> shift);
                __cl(vm);
               
                if (*(Reg*)dst == 0) 
                  __ste(vm);
                if ((i64)*(Reg*)dst < 0) 
                  __sts(vm);
                // PF
                u8 parity_byte = *(Reg*)dst & 0xFF;
                parity_byte ^= parity_byte >> 4;
                parity_byte ^= parity_byte >> 2;
                parity_byte ^= parity_byte >> 1;
                if ((parity_byte & 1) == 0) 
                    __stp(vm);
            }else{
                 segfault(vm);
            }
            vm->rip += instr_len;
            break;
        }

        // ==================== PUSH / POP r64 ==================== 
        case 0x50 ... 0x57: {  // PUSH r64
            u8 reg_num = (opcode & 7) | ((vm->rex & 1) ? 8 : 0);
            Reg *r = getRegister(vm, reg_num, is_64bit);
            if (!r) { segfault(vm); 
                 return; }
            vm->rsp -= 8;
            *(u64*)vm->rsp = *r;
            vm->rip += instr_len;
            break;
        }
        case 0x58 ... 0x5F: {  // POP r64
            u8 reg_num = (opcode & 7) | ((vm->rex & 1) ? 8 : 0);
            Reg *r = getRegister(vm, reg_num, is_64bit);
            if (!r) { segfault(vm); 
                 return; }
            *r = *(u64*)vm->rsp;
            vm->rsp += 8;
            vm->rip += instr_len;
            break;
        }

      //==================== CALL / JMP / RET ==================== 
        case 0xE8: {  // CALL rel32
            i32 offset = *(i32*)pc; 
            pc += 4; 
            instr_len += 4;
            vm->rsp -= 8;
            *(Reg*)vm->rsp = vm->rip + instr_len;
            vm->rip += instr_len + offset;
            goto record_and_return;
        }
        case 0xE9: {  // JMP rel32
            i32 offset = *(i32*)pc; 
            pc += 4; 
            instr_len += 4;
            vm->rip += instr_len  + offset;
            goto record_and_return;
        }
        case 0xC3: {  // RET
            vm->rip = *(Reg*)vm->rsp;
            vm->rsp += 8;
            goto record_and_return;
        }

       //==================== 短条件跳转 ==================== 
        case 0x70 ... 0x7F:
        case 0xEB: {
        
           i8 offset = *(i8*)pc; 
    
           pc++; 
           instr_len++;
           bool taken = false;

            switch (opcode) {
                case 0xEB: taken = true;                                      // JMP
                    break;
                case 0x74: taken = ZF;                                        // JE/JZ
                    break;
                case 0x75: taken = !ZF;                                       // JNE/JNZ
                    break;
                case 0x72: taken = CF;                                        // JB/JC (below/carry)
                    break;
                case 0x73: taken = !CF;                                       // JAE/JNC (above or equal/no carry)
                    break;
                case 0x77: taken = !CF && !ZF;                                 // JA/JBE (above)
                    break;
                case 0x76: taken = CF || ZF;                                  // JBE/JNA (below or equal)
                    break;

                // 有符号跳转
                case 0x7C: taken = (SF != OF);                                 // JL/JNGE (less)
                    break;
                case 0x7D: taken = (SF == OF);                                 // JGE/JNL (greater or equal)
                    break;
                case 0x7E: taken = ZF || (SF != OF);                           // JLE/JNG (less or equal)
                    break;
                case 0x7F: taken = !ZF && (SF == OF);                          // JG/JNLE (greater)
                    break;
            }

            if (taken) {
                vm->rip += instr_len + offset;
                goto record_and_return;
            }else{
                 vm->rip += instr_len;
            }
         
            break;
        }

        case 0x9F: {  // LAF - Load from Flags
           
            vm->rax = vm->rflags ;  
            vm->rip += instr_len;
            break;
        }

        case 0x9E: {  // SAF - Store RAX into Flags
            
            vm->rflags = vm->rax;
            vm->rip += instr_len;
            break;
        }

        // ==================== LEA r64====================
        case 0x8D: {  // LEA r64, m
            void *src_addr = getRmOperand(vm, mod, rm, pc, &instr_len, is_64bit);
            Reg *dst_ptr = getRegister(vm, reg, is_64bit);
            if (!src_addr || !dst_ptr) { segfault(vm); return; }

            // LEA 的特殊之处：取地址本身，而不是内存内容
            *dst_ptr = (u64)src_addr;

            vm->rip += instr_len;
            break;
        }

        // ==================== PUSHF / POPF (9C / 9D) ====================
        case 0x9C: {  // PUSHF - Push RFLAGS
            vm->rsp -= 8;
            *(u64*)vm->rsp = vm->rflags;
            vm->rip += instr_len;
            break;
        }

        case 0x9D: {  // POPF - Pop RFLAGS
            vm->rflags = *(u64*)vm->rsp;
            vm->rsp += 8;
            vm->rip += instr_len;
            break;
        }



       // ==================== INC / DEC r/m64 (FF /0 或 /1) ====================
        case 0xFF: {
            u8 subop = (modrm >> 3) & 7;
            if (subop == 0 || subop == 1) {  // /0 = INC, /1 = DEC
                void *dst_ptr = getRmOperand(vm, mod, rm, pc, &instr_len, is_64bit);
                if (!dst_ptr) { segfault(vm); return; }

                u64 val = *(u64*)dst_ptr;
                u64 result = (subop == 0) ? (val + 1) : (val - 1);

                // 标志位更新（INC/DEC 不影响 CF）
                __cl(vm);
                if (result == 0) __ste(vm);                 // ZF
                if ((i64)result < 0) __sts(vm);             // SF

                // OF：有符号溢出检测（关键！）
                if (subop == 0 && val == 0x7FFFFFFFFFFFFFFFULL)             // INC: 最大正数 +1
                    __sto(vm);
                if (subop == 1 && val == 0x8000000000000000ULL)             // DEC: 最小负数 -1
                    __sto(vm);

                // PF
                u8 p = result & 0xFF;
                p ^= p >> 4; p ^= p >> 2; p ^= p >> 1;
                if ((p & 1) == 0) __stp(vm);

                // AF（低4位进位/借位）
                if (((val ^ result) ^ 1) & 0x10)  // 通用写法
                    __sta(vm);

                *(u64*)dst_ptr = result;
                vm->rip += instr_len;
            } else {
                // FF 的其他子操作码（如 /2 CALL, /4 JMP 等）未实现
                segfault(vm);
                return;
            }
            break;
        }
        // ==================== LOOP / LOOPE / LOOPNE (E0/E1/E2) ====================
        case 0xE0:  // LOOPNE / LOOPNZ
        case 0xE1:  // LOOPE / LOOPZ
        case 0xE2: { // LOOP
            i8 offset = *(i8*)pc;
            pc++;
            instr_len++;

            // RCX -= 1（64位模式下使用 RCX）
            vm->rcx -= 1;
            bool take_jump = false;
            if (opcode == 0xE2) {                    // LOOP
                take_jump = (vm->rcx != 0);
            } else if (opcode == 0xE1) {             // LOOPE / LOOPZ
                take_jump = (vm->rcx != 0) && ZF;
            } else if (opcode == 0xE0) {             // LOOPNE / LOOPNZ
                take_jump = (vm->rcx != 0) && !ZF;
            }

            if (take_jump) {
                vm->rip += instr_len + offset;
            } else {
                vm->rip += instr_len;
            }
            goto record_and_return;
        }


        default:
            segfault(vm);     
            return;
    }
    }else{

     
         switch (opcode) {
         // SETcc r/m8 (0x0F 0x90 ~ 0x9F)
        case 0x90 ... 0x9F: {
        
                void *dst_ptr = getRmOperand(vm, mod, rm, pc, &instr_len, is_64bit);
                if (!dst_ptr) { segfault(vm); return; }

                bool condition = false;
                switch (opcode) {
                    case 0x90: condition = OF; break;                    // SETO
                    case 0x91: condition = !OF; break;                   // SETNO
                    case 0x92: condition = CF; break;                    // SETB
                    case 0x93: condition = !CF; break;                   // SETAE
                    case 0x94: condition = ZF; break;                    // SETE
                    case 0x95: condition = !ZF; break;                   // SETNE
                    case 0x96: condition = CF || ZF; break;              // SETBE
                    case 0x97: condition = !CF && !ZF; break;            // SETA
                    case 0x98: condition = SF; break;                    // SETS
                    case 0x99: condition = !SF; break;                   // SETNS
                    case 0x9A: condition = PF; break;                    // SETP
                    case 0x9B: condition = !PF; break;                   // SETNP
                    case 0x9C: condition = (SF != OF); break;            // SETL
                    case 0x9D: condition = (SF == OF); break;            // SETGE
                    case 0x9E: condition = ZF || (SF != OF); break;      // SETLE
                    case 0x9F: condition = !ZF && (SF == OF); break;     // SETG
                }

                *(u8*)dst_ptr = condition ? 1 : 0;
                vm->rip += instr_len;
                break;
          
        }
           // ==================== 两字节指令 (0x0F 前缀) ====================
        
       
        case 0xA3: {  // BT r/m64, r64
            void *rm_ptr = getRmOperand(vm, mod, rm, pc, &instr_len, is_64bit);
            Reg *reg_ptr = getRegister(vm, reg, is_64bit);
            if (!rm_ptr || !reg_ptr) { segfault(vm); return; }

            u64 bit_offset = *reg_ptr;
            u64 value = *(u64*)rm_ptr;

            __clc(vm);  // 清 CF
            if (value & (1ULL << (bit_offset & 63))) {
                __stc(vm);  // CF = 1
            }
            vm->rip += instr_len;
            break;
        }

        // MOVZX r64, r/m8 (0x0F B6) / MOVZX r64, r/m16 (0x0F B7)
        case 0xB6:  // MOVZX r64, r/m8
        case 0xB7: { // MOVZX r64, r/m16
            void *src_ptr = getRmOperand(vm, mod, rm, pc, &instr_len, is_64bit);
            Reg *dst_ptr = getRegister(vm, reg, is_64bit);
            if (!src_ptr || !dst_ptr) { segfault(vm); return; }

            if (opcode == 0xB6) {
                *dst_ptr = *(u8*)src_ptr;           // 零扩展 8 位
            } else {
                *dst_ptr = *(u16*)src_ptr;          // 零扩展 16 位
            }
            vm->rip += instr_len;
            break;
        }
       
        // 长条件跳转：0x0F 0x80 ~ 0x0F 0x8F (32位相对偏移)
        case 0x80 ... 0x8F: {
        
            i32 offset = *(i32*)pc;
            pc += 4;
            instr_len += 4;

            bool taken = false;
            switch (opcode) {
                case 0x80: taken = (SF != OF); break;           // JO
                case 0x81: taken = (SF == OF); break;           // JNO
                case 0x82: taken = CF; break;                   // JB
                case 0x83: taken = !CF; break;                  // JAE
                case 0x84: taken = ZF; break;                   // JE
                case 0x85: taken = !ZF; break;                  // JNE
                case 0x86: taken = CF || ZF; break;             // JBE
                case 0x87: taken = !CF && !ZF; break;           // JA
                case 0x88: taken = SF; break;                   // JS
                case 0x89: taken = !SF; break;                  // JNS
                case 0x8A: taken = PF; break;                   // JP
                case 0x8B: taken = !PF; break;                  // JNP
                case 0x8C: taken = (SF != OF); break;           // JL
                case 0x8D: taken = (SF == OF); break;           // JGE
                case 0x8E: taken = ZF || (SF != OF); break;     // JLE
                case 0x8F: taken = !ZF && (SF == OF); break;    // JG
            }

            if (taken) {
                vm->rip += instr_len + offset;
            } else {
                vm->rip += instr_len;
            }
            goto record_and_return;
        }

        // CMOVcc r64, r/m64 (0x0F 0x40 ~ 0x4F)
        case 0x40 ... 0x4F: {
            void *src_ptr = getRmOperand(vm, mod, rm, pc, &instr_len, is_64bit);
            Reg *dst_ptr = getRegister(vm, reg, is_64bit);
            if (!src_ptr || !dst_ptr) { segfault(vm); return; }

            bool condition = false;
            switch (opcode) {
                case 0x40: condition = OF; break;
                case 0x41: condition = !OF; break;
                case 0x42: condition = CF; break;
                case 0x43: condition = !CF; break;
                case 0x44: condition = ZF; break;
                case 0x45: condition = !ZF; break;
                case 0x46: condition = CF || ZF; break;
                case 0x47: condition = !CF && !ZF; break;
                case 0x48: condition = SF; break;
                case 0x49: condition = !SF; break;
                case 0x4C: condition = (SF != OF); break;
                case 0x4D: condition = (SF == OF); break;
                case 0x4E: condition = ZF || (SF != OF); break;
                case 0x4F: condition = !ZF && (SF == OF); break;
                default: condition = false;
            }

            if (condition) {
                *dst_ptr = *(u64*)src_ptr;
            }
            vm->rip += instr_len;
            break;
        }

        
        default:
            segfault(vm);     
            return;
      }
      
    }
    

    // 正常指令结束：记录完整字节并前进 RIP
record_and_return:
    Instruction *instr;
    size_t size=sizeof(Instruction)+instr_len;
    instr=(Instruction*)malloc(size);
    zero((u8*)instr,size);
    instr->o=opcode;
    instr->instrlen = instr_len;
    copy(instr->bytes, orgpc, instr_len);
    addInstruction(vm->itab, instr);
    
}
void executeVM(VM *vm){
   
    u64 brkaddr;
    assert(vm && *vm->m);
    brkaddr=(u64)(vm->m + vm->b);
    //printf("break addr is 0x%llx\n",brkaddr);
    /*
    instr1 arg1 instr2 instr3
    mov ax,0x05;nop;hlt;
    0x01 0x00 0x95
    0x02
    0x03
    
    */
    do{         
          executeInstruction(vm);
    }
    while(vm->rip< brkaddr);
  //  while(vm->rip< brkaddr && (Opcode)(*(u8*)(vm->rip)) != HLT );

    if(vm->rip>brkaddr) 
         
         segfault(vm);
    return;
};


void printReg(VM* vm){
    printf("the virtual machine registers is:\n");
    printf("REX=0x%x\n",vm->rex);
    printf("RAX=0x%llx\n",vm->rax);
    printf("RBX=0x%llx\n",vm->rbx);
    printf("RCX=0x%llx\n",vm->rcx);
    printf("RDX=0x%llx\n",vm->rdx);
    printf("RIP=0x%llx\n",vm->rip);
    printf("RSP=0x%llx\n",vm->rsp);
    printf("RSI=0x%llx\n",vm->rsi);
    printf("RDI=0x%llx\n",vm->rdi);
    printf("R8=0x%llx\n",vm->r8);
    printf("R9=0x%llx\n",vm->r9);
    printf("R10=0x%llx\n",vm->r10);
    printf("R11=0x%llx\n",vm->r11);
    printf("R12=0x%llx\n",vm->r12);
    printf("R13=0x%llx\n",vm->r13);
    printf("R14=0x%llx\n",vm->r14);
    printf("R15=0x%llx\n",vm->r15);
    printf("RFLAGS= 0x%llx :\n CF=%lld,PF=%lld,AF=%lld,\nZF=%lld,SF=%lld,TF=%lld,IF=%lld,\nDF=%lld,OF=%lld,IOPL=%lld,NT=%lld,\nRF=%lld,VMODE=%lld,AC=%lld,\nVIF=%lld,VIP=%lld,ID=%lld\n",
        vm->rflags,CF,PF,AF,ZF,SF,TF,IF,DF,OF,IOPL,NT,RF,VMODE,AC,VIF,VIP,ID);
    
}
void printVM(VM* vm){
    printf("virtual machine address = %p(size:%ld KB)\n",vm,sizeof(VM)/1024);
    printReg(vm);
    printf("program  address = %p\n",vm->m);
    printf("program offset is %ld,and the break line address is %p\n",vm->b,vm->m+vm->b);
   
    printf("the program section is (Instruction sets):\n");
    printhex((u8*)vm->m,vm->b,' ');
        /*
    printf("the virtual machine exective instruction sequences are:\n");
    printInstrTable(vm->itab);
*/
    printf("the virtual machine Disassembly instruction sequences are:\n");
    disasmInstrTable(vm->itab);
};



Program *exampleprogram(VM *vm) {
    u8 *p = vm->m;           // 指向虚拟机内存起始
    Program *prog = (Program *)p;

    // 1. MOV RAX, 0x12345678   (64位寄存器 + 32位立即数，符号扩展)
    *p++ = 0x48;             // REX.W=1 (64位操作)
    *p++ = 0xC7;             // 操作码：立即数 → 寄存器/内存 (32位立即数符号扩展到64位)
    *p++ = 0xC0;             // ModR/M: Mod=11, Reg=000(/0=MOV), R/M=000(RAX)
    *p++ = 0x78;             // 立即数：0x12345678 (小端序)
    *p++ = 0x56;
    *p++ = 0x34;
    *p++ = 0x12;
    *p++ = 0x90;             // NOP
    // 2. MOV RBX, RAX          (64位寄存器间移动)
    *p++ = 0x48;             // REX.W=1
    *p++ = 0x89;             // 操作码：寄存器 → 寄存器
    *p++ = 0xC3;             // ModR/M: Mod=11, Reg=000(RAX), R/M=011(RBX)
    *p++ = 0x90;             // NOP
    // 3. XOR RAX, RAX          (清零 RAX，最快方式)
    *p++ = 0x48;             // REX.W=1
    *p++ = 0x31;             // XOR 操作码
    *p++ = 0xC0;             // ModR/M: Mod=11, Reg=000(RAX), R/M=000(RAX)

    *p++ = 0x90;             // NOP

    // 4. MOV RAX, RBX          (方向相反，64位寄存器)
    *p++ = 0x48;             // REX.W=1
    *p++ = 0x8B;             // 操作码：寄存器 ← 寄存器
    *p++ = 0xC3;             // ModR/M: Mod=11, Reg=000(RAX), R/M=011(RBX)

    *p++ = 0x90;             // NOP

    // 5. MOV RAX, &vm->rbx     (把 vm->rbx 这个变量的地址加载到 RAX)
    *p++ = 0x48;             // REX.W=1
    *p++ = 0xB8;             // 操作码：立即数 → 寄存器/内存 (32位立即数符号扩展到64位)
    u64 rbx_addr = (u64)&(vm->rbx);   // 取 vm->rbx 的地址（64位指针）
    memcpy(p, &rbx_addr, 8);          // 拷贝 8 字节地址
    p += 8;

    *p++ = 0x90;             // NOP

    // 6. MOV RCX, [RAX]        (从 RAX 指向的内存读取 64 位到 RCX)
    *p++ = 0x48;             // REX.W=1 (64位操作)
    *p++ = 0x8B;             // 操作码：寄存器 ← 内存
    *p++ = 0x08;             // ModR/M: Mod=00, Reg=001(RCX), R/M=000(RAX)
   

    
 
    *p++ = 0x90;             // NOP
    *p++ = 0x48;             // REX.W=1
    *p++ = 0x8B;             // 操作码：寄存器 → 寄存器
    *p++ = 0xD1;             
    *p++ = 0x90;             // NOP

 

    *p++ = 0x48;            
    *p++ = 0xC1;           
    *p++ = 0xEB;  
    *p++ = 0x03; 
    *p++ = 0x90; 
 
       // MOV [RAX], RCX        (从 RAX 指向的内存读取 64 位到 RCX)
    *p++ = 0x48;             // REX.W=1 (64位操作)
    *p++ = 0x89;             // 操作码：寄存器 -> 内存
    *p++ = 0x10;             // ModR/M: Mod=00, Reg=001(RDX), R/M=000(RAX)
   

    *p++ = 0x9F;             // laf

    *p++ = 0x90;             // NOP
 
    *p++ = 0x48;             // REX.W=1 (64位操作)
    *p++ = 0x01;   //or
    *p++ = 0xFF;   
   

    *p++ = 0x9E;    //saf
    *p++ = 0x90;    // NOP
    

    *p++ = 0x48;             
    *p++ = 0xB8;           
    u64 rax_addr = (u64)&(vm->rax);   
    memcpy(p, &rax_addr, 8);         
    p += 8;

   // 测试 inc rax
   *p++ = 0x48; 
   *p++ = 0xFF; 
   *p++ = 0xC0;  // inc rax
   

// 测试 inc rbx
    *p++ = 0x48; 
    *p++ = 0xFF; 
    *p++ = 0xC3;  // inc rbx
    // 测试 dec rax
    *p++ = 0x48; 
    *p++ = 0xFF; 
    *p++ = 0xC8;  // dec rax
    *p++ = 0x90;             // NOP
   
    *p++ = 0x48;             // REX.W=1
    *p++ = 0x89;             // 操作码：寄存器 → 寄存器
    *p++ = 0xC3;             // ModR/M: Mod=11, Reg=000(RAX), R/M=011(RBX)

     

    *p++ = 0x90;             // NOP
// 测试 inc qword ptr [rax]

  *p++ = 0x48; *p++ = 0xFF; *p++ = 0x00;  // inc qword ptr [rax]

  // cmp rBx, rAx
 *p++ = 0x48; *p++ = 0x39; *p++ = 0xC3;
  // cmp rax, rbx
// jg label (如果 rbx > rax 跳转)
*p++ = 0x7E; 
*p++ = 0x05;
*p++ = 0x48; *p++ = 0x39; *p++ = 0xD8;
// jg label (如果 rax > rbx 跳转)
*p++ = 0x7E; 
*p++ = 0x08;
      
       *p++ = 0x90; 
       *p++ = 0x90; 
       *p++ = 0x90; 
        *p++ = 0x90; 
        *p++ = 0x90; 
        *p++ = 0x90; 
        *p++ = 0x90; 
        *p++ = 0x90; 
        *p++ = 0x90; 
        *p++ = 0x90; 
        // PUSHF
        *p++ = 0x9C;
        //jmp
        *p++ = 0xE9; 
        *p++ = 0x01;
        *p++ = 0x00; 
        *p++ = 0x00; 
        *p++ = 0x00; 
        *p++ = 0x90; 
        *p++ = 0x90; 
        //popf
        *p++ = 0x9D;
        *p++ = 0x90; 
        *p++ = 0x90; 
        *p++ = 0x90; 
        *p++ = 0x90; 




   // 测试 LEA
        *p++ = 0x48; *p++ = 0x8D; *p++ = 0x45; *p++ = 0x08;  // lea rax, [rbp + 8]


// 测试 LOOP
    *p++ = 0x48; *p++ = 0xC7; *p++ = 0xC1; *(u32*)p = 5; p += 4;  // mov rcx, 5
//label_loop:
    *p++ = 0x48; *p++ = 0xFF; *p++ = 0xC8;  // dec rcx
    *p++ = 0xFF; *p++ = 0xC3;  // inc rbx
    *p++ = 0xE2; *p++ = (i8)-7;  // loop label_loop


    // 测试长跳转
    *p++ = 0x48; *p++ = 0x31; *p++ = 0xC0;  // xor rax, rax
    *p++ = 0x48; *p++ = 0xC7; *p++ = 0xC0; *(u32*)p = 0x10; p += 4;  // mov rax, 16
    *p++ = 0x48; *p++ = 0x83; *p++ = 0xEA; *p++ = 0x0A;  // cmp rax, 10
  //  *p++ = 0x0F; *p++ = 0x8F; *(i32*)p = 0x08; p += 4;  // jg +8 (不跳)
 
  
    // 测试 BT
   *p++ = 0x49; *p++ = 0xC7; *p++ = 0xC0; *(u32*)p = 0x12345678; p += 4;  // mov r8, 0x12345678
 


   *p++ = 0x49;
    *p++ = 0xC7; 
    *p++ = 0xC1; 

    memcpy(p, &(rbx_addr), 4);          // 拷贝 8 字节地址
     p += 4;  // mov r9, &rax
    *p++ = 0x90;  
    *p++ = 0x4D; *p++ = 0x0F;  *p++ = 0xA3; *p++ = 0xC8;  // bt r8, r9   (测试第65位，应为0)

   // 测试 MOVZX
   *p++ = 0x41; *p++ = 0x0F;  *p++ = 0xB6; *p++ = 0xC8;  // movzx .。。


    //HLT
    *p++ = 0xF4;  
    *p++ = 0x90;             // NOP

    *p++ = 0x90;             // NOP
    // 记录程序总长度（字节数）
    vm->b = p - (u8*)prog;
   
    return prog;
}




int main(int argc,char *argv[]){


    /*
    Section .text(code) ( +read + exec - write)
    #############
    ###########
    #########

    ----------------------- break line

    *********
    ***********
    *************
    Section .data   (+read - exec + write)
    */
    printf("crete the vm\n");
    VM *vm;
    vm=creatVM();
    printf("load the program\n");
    Program *program;
    program=exampleprogram(vm);
    printf("excutive the program\n");
    executeVM(vm);
    
    if(vm){

       printVM(vm);
       removeVM(vm);
    };
    return 0;

};