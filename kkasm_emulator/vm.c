#define _CRT_SECURE_NO_WARNINGS

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <windows.h>

#include "vm.h"
#include "vmvideo.h"

#pragma warning(disable : 4996)

#define _VM_DEBUG

#ifdef _VM_DEBUG
#define VM_DEBUG(command)   command
#else
#define VM_DEBUG(command)
#endif

#define vm_read  _read
#define vm_write _write

#define TIMER_1		260
//----------------------------------------

// Virtual machine file descriptor
typedef int vmfd_t;

typedef unsigned char vmopcode_t;
// значения опкодов
// With no operands
#define     VM_NOP      0
#define		VM_RET		1
#define		VM_HLT		2
// With one operand
#define		VM_INC		3
#define		VM_DEC		4
#define		VM_POP		5
#define		VM_PUSH		6
#define		VM_IN		7
#define		VM_OUT		8
// With two operands
#define		VM_MOV		9
#define		VM_ADD		10
#define		VM_SUB		11
#define		VM_MUL		12
#define		VM_DIV		13
#define		VM_AND		14
#define		VM_OR		15
#define		VM_XOR		16
#define		VM_CMP		17
#define		VM_SHL		18
#define		VM_SHR		19
#define		VM_LEA		20
// Jump commands
#define		VM_JMP		21
#define		VM_JA		22
#define		VM_JAE		23
#define		VM_JB		24
#define		VM_JBE		25
#define		VM_JE		26
#define		VM_JNE		27
#define		VM_CALL		28
#define		VM_GET		29
#define		VM_RAND		30
#define		VM_WAIT		31
#define		VM_END		32
// Finalize instruction
#define     VM_LAST_INS 32

#define VM_COUNT_INST   (VM_LAST_INS + 1)

static char *vm_mnem[] = {"nop", "ret", "hlt", 
"inc", "dec", "pop", "push", "in", "out", 
"mov", "add", "sub", "mul", "div", "and", "or", "xor", "cmp", "shl", "shr", "lea", 
"jmp", "ja", "jae", "jb", "jbe", "je", "jne", "call", "get", "rand", "wait", "END"};


typedef vm_ins_result (*vm_handler)(vm_struct *vm);

typedef unsigned char vm_operand_type;

// Addressing modes
#define     VM_OPTYPE_NONE			0
#define     VM_OPTYPE_REG_DWORD		1
#define     VM_OPTYPE_REG_WORD		2
#define		VM_OPTYPE_REG_BYTE		3
#define		VM_OPTYPE_REG_MEM_DWORD	4
#define		VM_OPTYPE_REG_MEM_WORD	5
#define		VM_OPTYPE_REG_MEM_BYTE	6
#define		VM_OPTYPE_CONST			7
#define		VM_OPTYPE_VAR			8
#define		VM_OPTYPE_MEM_DWORD		9
#define		VM_OPTYPE_MEM_WORD		10
#define		VM_OPTYPE_MEM_BYTE		11
#define		VM_OPTYPE_LABEL			15

#pragma pack(1)
typedef struct _vm_instruction {
    vmopcode_t opcode;          // опкод
	vm_operand_type op1Type;	//РА1
	vm_operand_type op2Type;	//PA2
	vmopvalue_t op1;             // первый операнд
	vmopvalue_t op2;             // второй операнд
} vm_instruction;
#pragma pack()


static char *vm_state_message[] = {
    "VM_STATE_OK", "VM_STATE_UNKNOW_INSTRUCTION", "VM_STATE_INVALID_OPERAND",
    "VM_STATE_IO_ERROR", "VM_STATE_GENERAL_ERROR", "VM_STATE_UNKNOW_ERROR",
    "VM_STATE_HALT"};



typedef struct _vm_struct {
    vm_state state;
    vmfd_t fdin;
    vmfd_t fdout;

    vmopvalue_t regs[VM_REG_COUNT];
    vmvideo_struct *vmvideo;
    unsigned char memory[VM_MEM_SIZE];
	HWND hwnd;
} vm_struct;


unsigned int currentKey;

//----------------------------------------

static unsigned int vm_disas_operand(vm_operand_type opType, vmopvalue_t opValue, char *buf);

static unsigned int vm_get_dword(vm_struct *vm);
static unsigned int vm_get_word(vm_struct *vm);
static unsigned char vm_get_byte(vm_struct *vm);
static void vm_put_dword(vm_struct *vm, unsigned int value);
static void vm_put_word(vm_struct *vm, unsigned int value);
static void vm_put_byte(vm_struct *vm, unsigned char value);

static unsigned int vm_get_opsize(vm_operand_type optype);

static vm_ins_result vm_get_operand(vm_struct *vm, vm_operand_type opType, vmopvalue_t opValue, vmopvalue_t *value);
static vm_ins_result vm_set_operand(vm_struct *vm, vm_operand_type opType, vmopvalue_t opValue, vmopvalue_t value);

static void vm_set_flags(vm_struct *vm, int command);
static BOOL vm_check_flags(vm_struct *vm, int command);

// Commands handlers
// With no operands
static vm_ins_result vm_nop(vm_struct *vm);
static vm_ins_result vm_ret(vm_struct *vm);
static vm_ins_result vm_hlt (vm_struct *vm);
// With one operand
static vm_ins_result vm_inc (vm_struct *vm);
static vm_ins_result vm_dec (vm_struct *vm);
static vm_ins_result vm_push (vm_struct *vm);
static vm_ins_result vm_pop (vm_struct *vm);
static vm_ins_result vm_in(vm_struct *vm);
static vm_ins_result vm_out(vm_struct *vm);
// With two operands
static vm_ins_result vm_mov(vm_struct *vm);
static vm_ins_result vm_add(vm_struct *vm);
static vm_ins_result vm_sub(vm_struct *vm);
static vm_ins_result vm_mul(vm_struct *vm);
static vm_ins_result vm_div(vm_struct *vm);
static vm_ins_result vm_xor(vm_struct *vm);
static vm_ins_result vm_and(vm_struct *vm);
static vm_ins_result vm_or(vm_struct *vm);
static vm_ins_result vm_cmp(vm_struct *vm);
static vm_ins_result vm_shl(vm_struct *vm);
static vm_ins_result vm_shr(vm_struct *vm);
static vm_ins_result vm_lea(vm_struct *vm);
// Jump commands
static vm_ins_result vm_jmp(vm_struct *vm);
static vm_ins_result vm_ja(vm_struct *vm);
static vm_ins_result vm_jae(vm_struct *vm);
static vm_ins_result vm_jb(vm_struct *vm);
static vm_ins_result vm_jbe(vm_struct *vm);
static vm_ins_result vm_je(vm_struct *vm);
static vm_ins_result vm_jne(vm_struct *vm);
static vm_ins_result vm_call (vm_struct *vm);
static vm_ins_result vm_get(vm_struct *vm);
static vm_ins_result vm_rand(vm_struct *vm);
static vm_ins_result vm_wait(vm_struct *vm);
//END (Finalize)
static vm_ins_result vm_end(vm_struct *vm);

static void vm_init (vm_struct *vm);
static void vm_disas_current_ins (vm_struct *vm);

//----------------------------------------

vm_handler vm_handlers[VM_COUNT_INST] = {vm_nop, vm_ret, vm_hlt, 
vm_inc, vm_dec, vm_pop, vm_push, vm_in, vm_out,
vm_mov, vm_add, vm_sub, vm_mul, vm_div, vm_and, vm_or, vm_xor, vm_cmp, vm_shl, vm_shr, vm_lea,
vm_jmp, vm_ja, vm_jae, vm_jb, vm_jbe, vm_je, vm_jne, vm_call, vm_get, vm_rand, vm_wait, vm_end};

//----------------------------------------

void vm_set_key(vm_struct *vm, int keyCode) {
	currentKey = keyCode;
	return;
}


unsigned int vm_get_instruction_size(vm_instruction *ins) {
	unsigned int size = 0;
    switch (ins->opcode) {
    case VM_NOP:
    case VM_RET:
    case VM_HLT:
    case VM_END:
    case VM_GET:
    case VM_WAIT:
        return 1;
        break;

    case VM_INC:
    case VM_DEC:
    case VM_POP:
    case VM_PUSH:
    case VM_IN:
    case VM_OUT:

    case VM_JMP:
    case VM_JA:
    case VM_JAE:
    case VM_JB:
    case VM_JBE:
    case VM_JE:
    case VM_JNE:
    case VM_CALL:
        return 7;
        break;

    default:
        return 11;
        break;
    }

	return size;
}


static void vm_set_flags(vm_struct *vm, int command) {
	//4294967294 = 1...1 110 = USNIG_INT_MAX - 1
	//‭4294967293‬ = 1...1 101 = UNSIG_INT_MAX - 2

    switch (command) {
    case ZF_UP:
        vm->regs[VM_REG_FLAGS] = vm->regs[VM_REG_FLAGS] | 2;
        break;

    case ZF_DOWN:
        vm->regs[VM_REG_FLAGS] = vm->regs[VM_REG_FLAGS] & 4294967293;
        break;

    case CF_UP:
        vm->regs[VM_REG_FLAGS] = vm->regs[VM_REG_FLAGS] | 1;
        break;

    case CF_DOWN:
        vm->regs[VM_REG_FLAGS] = vm->regs[VM_REG_FLAGS] & 4294967294;
        break;
    }

	return;
}


static BOOL vm_check_flags(vm_struct *vm, int flag) {
	vmopvalue_t res;
    switch (flag) {
    case CF:
        res = vm->regs[VM_REG_FLAGS] & 1;
        if (res == 0) {
            return FALSE;
        }
        else {
            return TRUE;
        }

        break;

    case ZF:
        res = vm->regs[VM_REG_FLAGS] & 2;
        if (res == 0) {
            return FALSE;
        }
        else {
            return TRUE;
        }

        break;

    default:
        return FALSE;
    }
}


vm_instruction * vm_get_current_instruction (vm_struct *vm) {    
    return ((vm_instruction*)((unsigned int)vm->regs[VM_REG_IP] + (unsigned int)vm->memory));
}

//--------------------
vmopvalue_t vm_get_ip (vm_struct *vm) {
    return vm->regs[VM_REG_IP];
}

vm_ins_result vm_set_ip (vm_struct *vm, vmopvalue_t value) {
    if ((unsigned int)value < ((unsigned int)VM_MEM_SIZE - 10)) {
        (unsigned int)vm->regs[VM_REG_IP] = value;
        return VM_RESULT_OK;
    }
    else {
        vm->state = VM_STATE_GENERAL_ERROR;
        return VM_RESULT_GENERAL_ERROR;
    }
}


static unsigned int vm_get_dword(vm_struct *vm) {
	unsigned int tmp;
	if (vm_read(vm->fdin, &tmp, 8) == 8) {
		return tmp;
	}
	else {
		//vm->state = VM_RESULT_IO_ERROR;
		return 0;
	}
}


static unsigned int vm_get_word(vm_struct *vm) {
    unsigned int tmp;
    if (vm_read(vm->fdin, &tmp, 4) == 4) {
        return tmp;
    }
    else {
        //vm->state = VM_RESULT_IO_ERROR;
        return 0;
    }
}


static unsigned char vm_get_byte (vm_struct *vm) { 
    unsigned char tmp;

    if (vm_read(vm->fdin, &tmp, 1) == 1) {
        return tmp;
    }
    else {
        //vm->state = VM_STATE_IO_ERROR;
        return 0;
    }
}

//--------------------

static void vm_put_dword(vm_struct *vm, unsigned int value) {
	if (vm_write(vm->fdout, &value, 8) != 8) {
		//vm->state = VM_RESULT_IO_ERROR;
	}

	return;
}


static void vm_put_word (vm_struct *vm, unsigned int value) {
    if (vm_write(vm->fdout, &value, 4) != 4) {
        //vm->state = VM_RESULT_IO_ERROR;
    }

    return;
}


static void vm_put_byte(vm_struct *vm, unsigned char value) {
    if (vm_write(vm->fdout, &value, 1) != 1) {
        //vm->state = VM_RESULT_IO_ERROR;
    }

    return;
}

//--------------------
static unsigned int vm_get_opsize (vm_operand_type optype) {
    switch (optype) {

    case VM_OPTYPE_NONE:
        return 0;

    case VM_OPTYPE_REG_BYTE:
    case VM_OPTYPE_MEM_BYTE:
    case VM_OPTYPE_REG_MEM_BYTE:
        return 1;

    case VM_OPTYPE_REG_WORD:
    case VM_OPTYPE_MEM_WORD:
    case VM_OPTYPE_REG_MEM_WORD:
        return 2;

    case VM_OPTYPE_REG_DWORD:
    case VM_OPTYPE_MEM_DWORD:
    case VM_OPTYPE_REG_MEM_DWORD:
    case VM_OPTYPE_CONST:
    case VM_OPTYPE_VAR:
    case VM_OPTYPE_LABEL:
        return 4;
    }

    return 0;
} 

//--------------------
BOOL vm_get_memory(vm_struct *vm, unsigned int addr, unsigned int count, vmopvalue_t *value) {
    if (addr < VM_MEM_SIZE) {
        memcpy(value, vm->memory + addr, count);
        return TRUE;
    }

    return FALSE;
}


BOOL vm_get_memory_by_reg(vm_struct *vm, unsigned int reg, unsigned int count, vmopvalue_t *value) {
    if (reg < VM_REG_COUNT) {
        return vm_get_memory(vm, vm->regs[reg], count, value);
    }

    return FALSE;
}


BOOL vm_get_reg_byte(vm_struct *vm, unsigned int reg, unsigned int count, vmopvalue_t *value) {
    if (reg < VM_REG_COUNT) {
        memcpy(value, &vm->regs[reg], count);
        return TRUE;
    }

    return FALSE;
}


BOOL vm_put_memory(vm_struct *vm, unsigned int addr, unsigned int count, vmopvalue_t *value) {
    if (VM_VIDEOMEM_START <= addr && addr <= VM_VIDEOMEM_START + VM_VIDEOMEM_SIZE) {
        vmvideo_write_videomem(vm->vmvideo, addr - VM_VIDEOMEM_START, count, (unsigned char*)value);
    }

    if (addr < VM_MEM_SIZE) {
        memcpy(vm->memory + addr, value, count);
        return TRUE;
    }

    return FALSE;
}


BOOL vm_put_memory_by_reg(vm_struct *vm, unsigned int reg, unsigned int count, vmopvalue_t *value) {
    if (reg < VM_REG_COUNT) {
        return vm_put_memory(vm, vm->regs[reg], count, value);
    }

    return FALSE;
}


BOOL vm_put_reg_bytes(vm_struct *vm, unsigned int reg, unsigned int count, vmopvalue_t *value) {
    if (reg < VM_REG_COUNT) {
        memcpy(&vm->regs[reg], value, count);
        return TRUE;
    }

    return FALSE;
}

//--------------------
static vm_ins_result vm_get_operand(vm_struct *vm, vm_operand_type opType, vmopvalue_t opValue, vmopvalue_t *value) {
    unsigned int count;
    BOOL res = FALSE;

    *value = 0;
    count = vm_get_opsize(opType);

    switch (opType) {
    case VM_OPTYPE_REG_BYTE:
    case VM_OPTYPE_REG_WORD:
    case VM_OPTYPE_REG_DWORD:
        res = vm_get_reg_byte(vm, opValue, count, value);
        break;

    case VM_OPTYPE_CONST:
    case VM_OPTYPE_VAR:
        *value = opValue;
        return VM_RESULT_OK;

    case VM_OPTYPE_LABEL:
        *value = VM_CODE_START + opValue;
        return VM_RESULT_OK;

    case VM_OPTYPE_MEM_BYTE:
    case VM_OPTYPE_MEM_WORD:
    case VM_OPTYPE_MEM_DWORD:
        res = vm_get_memory(vm, opValue, count, value);
        break;

    case VM_OPTYPE_REG_MEM_BYTE:
    case VM_OPTYPE_REG_MEM_WORD:
    case VM_OPTYPE_REG_MEM_DWORD:
        res = vm_get_memory_by_reg(vm, opValue, count, value);
        break;

    default:
        break;
    }

    if (!res) {
        return VM_RESULT_INVALID_OPERAND;
    }
    else {
        return VM_RESULT_OK;
    }
}


static vm_ins_result vm_set_operand(vm_struct *vm, vm_operand_type opType, vmopvalue_t opValue, vmopvalue_t value) {
    unsigned int count;
    BOOL res = FALSE;
    
    count = vm_get_opsize(opType);

    switch (opType) {
    case VM_OPTYPE_REG_BYTE:
    case VM_OPTYPE_REG_WORD:
    case VM_OPTYPE_REG_DWORD:
        res = vm_put_reg_bytes(vm, opValue, count, &value);
        break;

    case VM_OPTYPE_MEM_BYTE:
    case VM_OPTYPE_MEM_WORD:
    case VM_OPTYPE_MEM_DWORD:
        res = vm_put_memory(vm, opValue, count, &value);
        break;

    case VM_OPTYPE_REG_MEM_BYTE:
    case VM_OPTYPE_REG_MEM_WORD:
    case VM_OPTYPE_REG_MEM_DWORD:
        res = vm_put_memory_by_reg(vm, opValue, count, &value);
        break;

    default:
        break;
    }

    if (!res) {
        return VM_RESULT_INVALID_OPERAND;
    }
    else {
        return VM_RESULT_OK;
    }
}

//--------------------
static vm_ins_result vm_hlt(vm_struct *vm) {
    vm->state = VM_STATE_OK;
    return VM_RESULT_BREAK;
}


static vm_ins_result vm_wait(vm_struct *vm) {
    SetTimer(vm->hwnd, TIMER_1, 3000, NULL);
    return VM_RESULT_BREAK;
}


static vm_ins_result vm_rand(vm_struct *vm) {
    vm_instruction *ip = vm_get_current_instruction(vm);
    vmopvalue_t value;
    vmopvalue_t res;

    if (vm_get_operand(vm, ip->op2Type, ip->op2, &value) == VM_RESULT_OK) {
        res = rand() % value;
        vm_set_operand(vm, ip->op1Type, ip->op1, res);
    }

    return VM_RESULT_OK;
}


static vm_ins_result vm_get(vm_struct *vm) {
    if (currentKey == 0x25 || currentKey == 0x26 || currentKey == 0x27 || currentKey == 0x28) {
        vm->regs[VM_REG_KEY] = currentKey;
    }
    else {
        vm->regs[VM_REG_KEY] = 0;
    }

	vm->state = VM_STATE_OK;
	return VM_RESULT_OK;
}


static vm_ins_result vm_end(vm_struct *vm) {
	vm->state = VM_STATE_HALT;
	return VM_RESULT_OK;
}


static vm_ins_result vm_nop (vm_struct *vm) {
    return VM_RESULT_OK;
}


static vm_ins_result vm_in (vm_struct *vm) {
    unsigned int size;
    vmopvalue_t value;

    vm_instruction *ip = vm_get_current_instruction(vm);
    size = vm_get_opsize (ip->op1Type);

    if (size == 1) {
        value = vm_get_byte(vm);
        vm_set_operand(vm, ip->op1Type, ip->op1, value);
    }
    else if (size == 4) {
        value = vm_get_word(vm);
        vm_set_operand(vm, ip->op1Type, ip->op1, value);
    }
    else if (size == 8) {
        value = vm_get_dword(vm);
        vm_set_operand(vm, ip->op1Type, ip->op1, value);
    }

    return VM_RESULT_OK;
}


static vm_ins_result vm_out (vm_struct *vm) {
    unsigned int size;
    vmopvalue_t value;

    vm_instruction *ip = vm_get_current_instruction(vm);
    size = vm_get_opsize (ip->op1Type);

    if (size == 1) {
        vm_set_operand(vm, ip->op1Type, ip->op1, &value);
        vm_put_byte(vm, value);
    }
    else if (size == 4) {
        vm_set_operand(vm, ip->op1Type, ip->op1, &value);
        vm_put_word(vm, value);
    }
	else if (size == 8) {
		vm_set_operand(vm, ip->op1Type, ip->op1, &value);
		vm_put_dword(vm, value);
	}

    return VM_RESULT_OK;
}


static vm_ins_result vm_mov (vm_struct *vm) {
    vm_instruction *ip = vm_get_current_instruction(vm);
    vmopvalue_t value;

    if (vm_get_operand(vm, ip->op2Type, ip->op2, &value) == VM_RESULT_OK) {
        vm_set_operand(vm, ip->op1Type, ip->op1, value);
    } 

    return VM_RESULT_OK;
}


static vm_ins_result vm_add (vm_struct *vm) {
    vmopvalue_t value1;
    vmopvalue_t value2;
	vmopvalue_t res;

    vm_instruction *ip = vm_get_current_instruction(vm);

    if (vm_get_operand(vm, ip->op1Type, ip->op1, &value1) == VM_RESULT_OK &&
        vm_get_operand(vm, ip->op2Type, ip->op2, &value2) == VM_RESULT_OK) {

        res = value1 + value2;

        if (res < value1 || res < value2) {
            vm_set_flags(vm, CF_UP);
        }
        else {
            vm_set_flags(vm, CF_DOWN);
        }

        if (res == 0) {
            vm_set_flags(vm, ZF_UP);
        }
        else {
            vm_set_flags(vm, ZF_DOWN);
        }

        vm_set_operand(vm, ip->op1Type, ip->op1, res);
    }

    return VM_RESULT_OK;
}


static vm_ins_result vm_sub(vm_struct *vm) {
    vmopvalue_t value1;
    vmopvalue_t value2;
    vmopvalue_t res;

    vm_instruction *ip = vm_get_current_instruction(vm);

    if (vm_get_operand(vm, ip->op1Type, ip->op1, &value1) == VM_RESULT_OK &&
        vm_get_operand(vm, ip->op2Type, ip->op2, &value2) == VM_RESULT_OK) {

        res = value1 - value2;

        if (res == 0) {
            vm_set_flags(vm, ZF_UP);
        }
        else {
            vm_set_flags(vm, ZF_DOWN);
        }

        vm_set_operand(vm, ip->op1Type, ip->op1, res);
    }

    return VM_RESULT_OK;
}


static vm_ins_result vm_xor(vm_struct *vm) {
    vmopvalue_t value1;
    vmopvalue_t value2;

    vm_instruction *ip = vm_get_current_instruction(vm);

    if (vm_get_operand(vm, ip->op1Type, ip->op1, &value1) == VM_RESULT_OK &&
        vm_get_operand(vm, ip->op2Type, ip->op2, &value2) == VM_RESULT_OK) {

        value1 ^= value2;
        vm_set_operand(vm, ip->op1Type, ip->op1, value1);
    }

    return VM_RESULT_OK;
}


static vm_ins_result vm_and (vm_struct *vm) {
    vmopvalue_t value1;
    vmopvalue_t value2;

    vm_instruction *ip = vm_get_current_instruction(vm);

    if (vm_get_operand(vm, ip->op1Type, ip->op1, &value1) == VM_RESULT_OK &&
        vm_get_operand(vm, ip->op2Type, ip->op2, &value2) == VM_RESULT_OK) {

        value1 &= value2;
        vm_set_operand(vm, ip->op1Type, ip->op1, value1);
    }

    return VM_RESULT_OK;
}


static vm_ins_result vm_or (vm_struct *vm) {
    vmopvalue_t value1;
    vmopvalue_t value2;

    vm_instruction *ip = vm_get_current_instruction(vm);

    if (vm_get_operand(vm, ip->op1Type, ip->op1, &value1) == VM_RESULT_OK &&
        vm_get_operand(vm, ip->op2Type, ip->op2, &value2) == VM_RESULT_OK) {

        value1 |= value2;
        vm_set_operand(vm, ip->op1Type, ip->op1, value1);
    }

    return VM_RESULT_OK;
}


static vm_ins_result vm_break (vm_struct *vm) {
    return VM_RESULT_BREAK;
}


static vm_ins_result vm_push (vm_struct *vm) {
    vmopvalue_t value = 0;

    vm_instruction *ip = vm_get_current_instruction(vm);

    if (vm_get_operand(vm, ip->op1Type, ip->op1, &value) == VM_RESULT_OK) {
        if (vm_put_memory(vm, vm->regs[VM_REG_SP] - 4, 4, &value)) {
            vm->regs[VM_REG_SP] -= 4;
        }
    }

    return VM_RESULT_OK;
}


static vm_ins_result vm_pop (vm_struct *vm) {
    vmopvalue_t value = 0;

    vm_instruction *ip = vm_get_current_instruction(vm);

    if (vm_get_memory(vm, vm->regs[VM_REG_SP], 4, &value)) {
        vm_set_operand(vm, ip->op1Type, ip->op1, value);
        vm->regs[VM_REG_SP] += 4;
    }

    return VM_RESULT_OK;
}


static vm_ins_result vm_mul(vm_struct *vm) {
	vmopvalue_t value1;
    vmopvalue_t value2;

    vm_instruction *ip = vm_get_current_instruction(vm);

    if (vm_get_operand(vm, ip->op1Type, ip->op1, &value1) == VM_RESULT_OK &&
        vm_get_operand(vm, ip->op2Type, ip->op2, &value2) == VM_RESULT_OK) {

        value1 *= value2;
        vm_set_operand(vm, ip->op1Type, ip->op1, value1);
    }

    return VM_RESULT_OK;
}


static vm_ins_result vm_div(vm_struct *vm) {
	vmopvalue_t value1;
	vmopvalue_t value2;

	vm_instruction *ip = vm_get_current_instruction(vm);

    if (vm_get_operand(vm, ip->op1Type, ip->op1, &value1) == VM_RESULT_OK &&
        vm_get_operand(vm, ip->op2Type, ip->op2, &value2) == VM_RESULT_OK) {

        value1 /= value2;
        vm_set_operand(vm, ip->op1Type, ip->op1, value1);
    }

	return VM_RESULT_OK;
}


static vm_ins_result vm_cmp(vm_struct *vm) {
	vmopvalue_t value1;
	vmopvalue_t value2;
	vmopvalue_t res;

	vm_instruction *ip = vm_get_current_instruction(vm);

    if (vm_get_operand(vm, ip->op1Type, ip->op1, &value1) == VM_RESULT_OK &&
        vm_get_operand(vm, ip->op2Type, ip->op2, &value2) == VM_RESULT_OK) {

        res = value1 - value2;

        //ставим флаги
        //a = b
        if (res == 0) {
            vm_set_flags(vm, ZF_UP);
        }
        //a != b
        else if (res != 0) {
            vm_set_flags(vm, ZF_DOWN);
        }

        //a < b
        if (value1 < value2) {
            vm_set_flags(vm, CF_UP);
        }
        //a > b
        else if (value1 > value2) {
            vm_set_flags(vm, CF_DOWN);
        }
    }

	return VM_RESULT_OK;
}


static vm_ins_result vm_shr(vm_struct *vm) {
	vmopvalue_t value1;
	vmopvalue_t value2;

	vm_instruction *ip = vm_get_current_instruction(vm);

	if (vm_get_operand(vm, ip->op1Type, ip->op1, &value1) == VM_RESULT_OK &&
		vm_get_operand(vm, ip->op2Type, ip->op2, &value2) == VM_RESULT_OK) {
		
		value1 = value1 >> value2;
		vm_set_operand(vm, ip->op1Type, ip->op1, value1);
	}

	return VM_RESULT_OK;
}


static vm_ins_result vm_shl(vm_struct *vm) {
	vmopvalue_t value1;
	vmopvalue_t value2;

	vm_instruction *ip = vm_get_current_instruction(vm);

    if (vm_get_operand(vm, ip->op1Type, ip->op1, &value1) == VM_RESULT_OK &&
        vm_get_operand(vm, ip->op2Type, ip->op2, &value2) == VM_RESULT_OK) {

        value1 = value1 << value2;
        vm_set_operand(vm, ip->op1Type, ip->op1, value1);
    }

	return VM_RESULT_OK;
}


static vm_ins_result vm_lea(vm_struct *vm) {
	vmopvalue_t value1;
	vmopvalue_t value2;

	vm_instruction *ip = vm_get_current_instruction(vm);

    if (vm_get_operand(vm, ip->op1Type, ip->op1, &value1) == VM_RESULT_OK &&
        vm_get_operand(vm, ip->op2Type, ip->op2, &value2) == VM_RESULT_OK) {

        vm_set_operand(vm, ip->op1Type, ip->op1, value1);
    }

	return VM_RESULT_OK;
}


static vm_ins_result vm_inc(vm_struct *vm) {
	vmopvalue_t value;

	vm_instruction *ip = vm_get_current_instruction(vm);

    if (vm_get_operand(vm, ip->op1Type, ip->op1, &value) == VM_RESULT_OK) {

        value++;
        vm_set_operand(vm, ip->op1Type, ip->op1, value);
    }

	return VM_RESULT_OK;
}


static vm_ins_result vm_dec(vm_struct *vm) {
	vmopvalue_t value;

	vm_instruction *ip = vm_get_current_instruction(vm);

    if (vm_get_operand(vm, ip->op1Type, ip->op1, &value) == VM_RESULT_OK) {

        value--;
        vm_set_operand(vm, ip->op1Type, ip->op1, value);
    }

	return VM_RESULT_OK;
}


static vm_ins_result vm_call(vm_struct *vm) {
	vmopvalue_t value = 0;
	vmopvalue_t oldIp = 0;
	vm_ins_result res;

	vm_instruction *curInst = vm_get_current_instruction(vm);

	res = vm_get_operand(vm, curInst->op1Type, curInst->op1, &value);

    if (res == VM_RESULT_OK) {
        oldIp = vm_get_ip(vm) + VM_INSTRUCTION_SIZE;
        if (vm_put_memory(vm, vm->regs[VM_REG_SP] - 4, 4, &oldIp)) {
            vm->regs[VM_REG_SP] -= 4;
            res = vm_set_ip(vm, value);
            if (res == VM_RESULT_OK) {
                res = VM_RESULT_CHANGE_IP;
            }
        }
        else {
            vm->state = VM_STATE_GENERAL_ERROR;
            res = VM_RESULT_GENERAL_ERROR;
        }
    }

	return res;
}


static vm_ins_result vm_ret(vm_struct *vm) {
	vmopvalue_t value = 0;
	vm_ins_result res;

    if (vm_get_memory(vm, vm->regs[VM_REG_SP], 4, &value)) {
        vm->regs[VM_REG_SP] += 4;
        res = vm_set_ip(vm, value);
        if (res == VM_RESULT_OK) {
            res = VM_RESULT_CHANGE_IP;
        }
    }
    else {
        vm->state = VM_STATE_GENERAL_ERROR;
        res = VM_RESULT_GENERAL_ERROR;
    }

	return res;
}


static vm_ins_result vm_jmp(vm_struct *vm) {
    vmopvalue_t value = 0;
    vm_ins_result res;

    vm_instruction *ip = vm_get_current_instruction(vm);

    //value - адрес перехода
    res = vm_get_operand(vm, ip->op1Type, ip->op1, &value);

    if (res == VM_RESULT_OK) {
        res = vm_set_ip(vm, value);

        if (res == VM_RESULT_OK) {
            res = VM_RESULT_CHANGE_IP;
        }
    }
    else {
        vm->state = VM_STATE_GENERAL_ERROR;
        res = VM_RESULT_GENERAL_ERROR;
    }

    return res;
}


static vm_ins_result vm_je(vm_struct *vm) {
    vmopvalue_t value = 0;
    vm_ins_result res;

    vm_instruction *ip = vm_get_current_instruction(vm);

    //value - адрес перехода
    res = vm_get_operand(vm, ip->op1Type, ip->op1, &value);

    if (res == VM_RESULT_OK) {
        if (vm_check_flags(vm, ZF) == TRUE) {
            res = vm_set_ip(vm, value);
        }
        else {
            return VM_RESULT_OK;
        }

        if (res == VM_RESULT_OK) {
            res = VM_RESULT_CHANGE_IP;
        }
    }
    else {
        vm->state = VM_STATE_GENERAL_ERROR;
        res = VM_RESULT_GENERAL_ERROR;
    }

    return res;
}


static vm_ins_result vm_jne(vm_struct *vm) {
    vmopvalue_t value = 0;
    vm_ins_result res;

    vm_instruction *ip = vm_get_current_instruction(vm);

    //value - адрес перехода
    res = vm_get_operand(vm, ip->op1Type, ip->op1, &value);

    if (res == VM_RESULT_OK) {
        if (vm_check_flags(vm, ZF) == FALSE) {
            res = vm_set_ip(vm, value);
        }
        else {
            return VM_RESULT_OK;
        }

        if (res == VM_RESULT_OK) {
            res = VM_RESULT_CHANGE_IP;
        }
    }
    else {
        vm->state = VM_STATE_GENERAL_ERROR;
        res = VM_RESULT_GENERAL_ERROR;
    }

    return res;
}


static vm_ins_result vm_ja(vm_struct *vm) {
    vmopvalue_t value = 0;
    vm_ins_result res;

    vm_instruction *ip = vm_get_current_instruction(vm);

    //value - адрес перехода
    res = vm_get_operand(vm, ip->op1Type, ip->op1, &value);

    if (res == VM_RESULT_OK) {
        //CF=0 and ZF = 0
        if ((vm_check_flags(vm, CF) == FALSE) && (vm_check_flags(vm, ZF) == FALSE)) {
            res = vm_set_ip(vm, value);
        }
        else {
            return VM_RESULT_OK;
        }

        if (res == VM_RESULT_OK) {
            res = VM_RESULT_CHANGE_IP;
        }
    }
    else {
        vm->state = VM_STATE_GENERAL_ERROR;
        res = VM_RESULT_GENERAL_ERROR;
    }

    return res;
}


static vm_ins_result vm_jae(vm_struct *vm) {
    vmopvalue_t value = 0;
    vm_ins_result res;

    vm_instruction *ip = vm_get_current_instruction(vm);

    //value - адрес перехода
    res = vm_get_operand(vm, ip->op1Type, ip->op1, &value);

    if (res == VM_RESULT_OK) {
        if (vm_check_flags(vm, CF) == FALSE) {
            res = vm_set_ip(vm, value);
        }
        else {
            return VM_RESULT_OK;
        }

        if (res == VM_RESULT_OK) {
            res = VM_RESULT_CHANGE_IP;
        }
    }
    else {
        vm->state = VM_STATE_GENERAL_ERROR;
        res = VM_RESULT_GENERAL_ERROR;
    }

    return res;
}


static vm_ins_result vm_jb(vm_struct *vm) {
    vmopvalue_t value = 0;
    vm_ins_result res;

    vm_instruction *ip = vm_get_current_instruction(vm);

    //value - адрес перехода
    res = vm_get_operand(vm, ip->op1Type, ip->op1, &value);

    if (res == VM_RESULT_OK) {
        if (vm_check_flags(vm, CF) == TRUE) {
            res = vm_set_ip(vm, value);
        }
        else {
            return VM_RESULT_OK;
        }

        if (res == VM_RESULT_OK) {
            res = VM_RESULT_CHANGE_IP;
        }
    }
    else {
        vm->state = VM_STATE_GENERAL_ERROR;
        res = VM_RESULT_GENERAL_ERROR;
    }

    return res;
}


static vm_ins_result vm_jbe(vm_struct *vm) {
    vmopvalue_t value = 0;
    vm_ins_result res;

    vm_instruction *ip = vm_get_current_instruction(vm);

    //value - адрес перехода
    res = vm_get_operand(vm, ip->op1Type, ip->op1, &value);

    if (res == VM_RESULT_OK) {
        //CF=1 or ZF = 1
        if ((vm_check_flags(vm, CF) == TRUE) || (vm_check_flags(vm, ZF) == TRUE)) {
            res = vm_set_ip(vm, value);
        }
        else {
            return VM_RESULT_OK;
        }

        if (res == VM_RESULT_OK) {
            res = VM_RESULT_CHANGE_IP;
        }
    }
    else {
        vm->state = VM_STATE_GENERAL_ERROR;
        res = VM_RESULT_GENERAL_ERROR;
    }

    return res;
}


//--------------------
static unsigned int vm_disas_operand(vm_operand_type opType, vmopvalue_t opValue, char *buf) {
    unsigned int offset = 0;

    switch (opType) {
    case VM_OPTYPE_REG_BYTE:
        offset += sprintf(buf + offset, "rb%d", opValue);
        break;
    case VM_OPTYPE_REG_WORD:
        offset += sprintf(buf + offset, "rw%d", opValue);
        break;
    case VM_OPTYPE_REG_DWORD:
        offset += sprintf(buf + offset, "r%d", opValue);
        break;

    case VM_OPTYPE_CONST:
        offset += sprintf(buf, "0x%08x", opValue);
        break;

    case VM_OPTYPE_MEM_BYTE:
        offset += sprintf(buf, "byte ptr");
        offset += sprintf(buf + offset, "[%08x]", opValue);
        break;
    case VM_OPTYPE_MEM_WORD:
        offset += sprintf(buf, "word ptr");
        offset += sprintf(buf + offset, "[%08x]", opValue);
        break;
    case VM_OPTYPE_MEM_DWORD:
        offset += sprintf(buf + offset, "[%08x]", opValue);
        break;

    case VM_OPTYPE_REG_MEM_BYTE:
        offset += sprintf(buf, "byte ptr ");
        offset += sprintf(buf + offset, "[r%d]", opValue);
        break;
    case VM_OPTYPE_REG_MEM_WORD:
        offset += sprintf(buf, "word ptr ");
        offset += sprintf(buf + offset, "[r%d]", opValue);
        break;
    case VM_OPTYPE_REG_MEM_DWORD:
        offset += sprintf(buf + offset, "[r%d]", opValue);
        break;
    }

    return offset;
}

 
static void vm_disas_current_ins(vm_struct *vm) {
    char buf[100];

    vm_instruction *ip = vm_get_current_instruction(vm);

    vm_get_disas_ins(vm, ip, buf);
    printf("%s", buf);

    return;
}

 
unsigned int vm_get_disas_ins (vm_struct *vm, vm_instruction *ins, char *buf) {
	unsigned int offset = 0;
	unsigned int i;

	VM_INSTRUCTION_SIZE = vm_get_instruction_size(ins);

    if (ins->opcode >= VM_COUNT_INST) {
        return sprintf(buf, "invalid instruction\n");
    }

    offset += sprintf (buf + offset, "%08x: ", (unsigned int)ins - (unsigned int)vm->memory);
    
    for (i = 0; i < VM_INSTRUCTION_SIZE; ++i) {
        offset += sprintf(buf + offset, "%02x  ", *((unsigned char*)ins + i));
    }
    offset += sprintf (buf + offset, "%s ", vm_mnem[ins->opcode]);

	if (VM_INSTRUCTION_SIZE != 1) {
        if (ins->op1Type != VM_OPTYPE_NONE) {
            offset += vm_disas_operand(ins->op1Type, ins->op1, buf + offset);
        }

		if (ins->op2Type != VM_OPTYPE_NONE) {
			offset += sprintf (buf + offset, ", ");
			offset += vm_disas_operand (ins->op2Type, ins->op2, buf + offset);
		}
	}

    return offset;
}

//--------------------

static void vm_print_context(vm_struct *vm) {
    unsigned int i, j;

    vm_instruction *ip = vm_get_current_instruction(vm);

    printf("\n");
    for (i = 0; i < VM_REG_COUNT / 4; ++i) {
        for (j = 0; j < 4; ++j) {
            printf("r%d %08x\t", 4 * i + j, vm->regs[4 * i + j]);
        }
        printf("\n");
    }

    printf("%08p: ", (unsigned int)ip);
    vm_disas_current_ins(vm);
    printf("\n");

    return;
}

//--------------------
vm_ins_result vm_run_current_instruction(vm_struct *vm) {
    vm_ins_result res;
    vm_instruction *ip = vm_get_current_instruction(vm);

    VM_INSTRUCTION_SIZE = vm_get_instruction_size(ip);

    if (vm->state != VM_STATE_OK && vm->state != VM_STATE_WAIT) {
        return VM_RESULT_UNKNOW_ERROR;
    }

    if ((unsigned int)ip >= (unsigned int)vm->memory + VM_MEM_SIZE ||
        (unsigned int)ip < (unsigned int)vm->memory) {
        vm->state = VM_STATE_GENERAL_ERROR;
        return VM_RESULT_GENERAL_ERROR;
    }

    if (ip->opcode >= VM_COUNT_INST) {
        return VM_RESULT_UNKNOW_INSTRUCTION;
    }

    res = vm_handlers[ip->opcode](vm);
    if (res != VM_RESULT_OK &&
        res != VM_RESULT_BREAK &&
        res != VM_RESULT_CHANGE_IP) {
        return FALSE;
    }

    if (res != VM_RESULT_CHANGE_IP) {
        vm_set_ip(vm, vm_get_ip(vm) + VM_INSTRUCTION_SIZE);
    }
    else {
        res = VM_RESULT_OK;
    }

    return res;
}


vm_state vm_run(vm_struct *vm) {
    vm_state state;

    while (vm->state == VM_STATE_OK &&
        vm_run_current_instruction(vm) == VM_RESULT_OK);

    state = vm->state;

    return state;
}

//--------------------

static void vm_init(vm_struct *vm) {
    memset(vm->memory, 0, VM_MEM_SIZE);
    memset(vm->regs, 0, VM_REG_COUNT * sizeof(vmopvalue_t));

    vm->fdin = _fileno(stdin);
    vm->fdout = _fileno(stdout);
    vm->regs[VM_REG_IP] = VM_CODE_START;
    vm->regs[VM_REG_SP] = VM_STACK_START;
    vm->regs[VM_REG_FLAGS] = 0;
    vm->state = VM_STATE_OK;

    return;
}


vm_struct * create_vm(HWND hwnd) {
    vm_struct *vm = (vm_struct*)malloc(sizeof(vm_struct));
    if (!vm) {
        return NULL;
    }

    vm->vmvideo = create_vmvideo(VM_VIDEOMEM_WIDTH, VM_VIDEOMEM_HEIGHT);
    if (!vm->vmvideo) {
        free(vm);
        return NULL;
    }

    vm->hwnd = hwnd;

    vm_init(vm);

    return vm;
}


void destroy_vm(vm_struct *vm) {
    free(vm);
    return;
}

//--------------------

BOOL vm_load_code(vm_struct *vm, unsigned char *code, unsigned int code_size) {
    if (code_size < VM_MEM_SIZE - VM_CODE_START) {
        vm_init(vm);
        memcpy(vm->memory + VM_CODE_START, code, code_size);
        return TRUE;
    }

    return FALSE;
}

//--------------------
BOOL vm_interactive(vm_struct *vm) {
    char command[100];

    while (TRUE) {
        printf("# ");
        command[vm_read(vm->fdin, command, 100)] = 0;

        if (command[0] == 'q') {
            return FALSE;
        }

        if (command[0] == 'r') {
            unsigned int number;
            sscanf(command + 1, "%d", &number);
            if (number < VM_REG_COUNT) {
                printf("\n> %08x\n", vm->regs[number]);
            }
            else {
                printf("> invalid reg number\n");
            }
            continue;
        }
        break;
    }

    return TRUE;
}


void vm_trace(vm_struct *vm) {
    while (TRUE) {
        vm_print_context(vm);

        if (!vm_interactive(vm)) {
            break;
        }

        if (!vm_run_current_instruction(vm)) {
            vm_print_error(vm);
            break;
        }
    }

    return;
}


void vm_print_error(vm_struct *vm) {
    if (vm->state != VM_STATE_OK) {
        printf("%s\n", vm_state_message[vm->state]);
    }

    return;
}


vmopvalue_t vm_get_reg_full(vm_struct *vm, unsigned int reg_number) {
    vmopvalue_t value = 0;

    if (reg_number < VM_REG_COUNT) {
        value = vm->regs[reg_number];
    }

    return value;
}
