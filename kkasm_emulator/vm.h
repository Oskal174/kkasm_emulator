#ifndef _VM_H_
#define _VM_H_

#define MAX_INT		‭4294967295‬

#define VM_REG_COUNT    32

// Register with keyboard key index
#define VM_REG_KEY		28

#define VM_REG_FLAGS	29

#define VM_REG_SP       30

#define VM_REG_IP       31

#define VM_MEM_SIZE     (4096*1024)

#define VM_CODE_START   0x5000

#define VM_STACK_START  0x4000

// Video memory
#define VM_VIDEOMEM_START   0x6000

#define VM_VIDEOMEM_WIDTH   0x200

#define VM_VIDEOMEM_HEIGHT  0x100

#define VM_VIDEOMEM_SIZE    (VM_VIDEOMEM_WIDTH * VM_VIDEOMEM_HEIGHT)

// Commands for work with FLAGS register
#define CF			0
#define ZF			1

#define CF_UP		0
#define CF_DOWN		1
#define ZF_UP		2
#define ZF_DOWN		3

static unsigned int VM_INSTRUCTION_SIZE;

typedef struct _vm_struct vm_struct;
typedef struct _vm_instruction vm_instruction;

// тип значения регистров и операндов
typedef unsigned int vmopvalue_t;

// возможные состояния виртуальной машины
typedef enum _vm_state {
    VM_STATE_OK = 0,
    VM_STATE_GENERAL_ERROR,
    VM_STATE_UNKNOW_ERROR,
    VM_STATE_HALT,
	VM_STATE_WAIT,
} vm_state;


typedef enum _vm_ins_result {
    VM_RESULT_OK = 0,
    VM_RESULT_UNKNOW_INSTRUCTION,
    VM_RESULT_GENERAL_ERROR,
    VM_RESULT_INVALID_OPERAND,
    VM_RESULT_IO_ERROR,
    VM_RESULT_UNKNOW_ERROR,
    VM_RESULT_BREAK,
    VM_RESULT_CHANGE_IP
} vm_ins_result;


typedef int BOOL;
#ifndef TRUE
#define TRUE    1
#endif
#ifndef FALSE
#define FALSE   0
#endif

//----------------------------------------
unsigned int vm_getCurrentInstSize(vm_instruction *ins);
void vm_setCurrentKey(vm_struct *vm, int keyCode);

vm_instruction * vm_get_current_instruction(vm_struct *vm);
vmopvalue_t vm_get_ip(vm_struct *vm);
vm_ins_result vm_set_ip(vm_struct *vm, vmopvalue_t value);
extern vm_ins_result vm_run_current_instruction(vm_struct *vm);
extern vm_state vm_run(vm_struct *vm);
extern vm_struct * create_vm(HWND hwnd);
void destroy_vm(vm_struct *vm);
BOOL vm_load_code(vm_struct *vm, unsigned char *code, unsigned int code_size);
void vm_trace(vm_struct *vm);
vmopvalue_t vm_get_reg_full(vm_struct *vm, unsigned int reg_number);
unsigned int vm_get_disas_ins(vm_struct *vm, vm_instruction *ins, char *buf);
void vm_print_error(vm_struct *vm);

//----------------------------------------

#endif  // _VM_H_

