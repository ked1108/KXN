#ifndef VM_H
#define VM_H

#include <stdint.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#define VM_MEMORY_SIZE 65536
#define VM_STACK_TOP 0xFFFF
#define VM_DISPLAY_WIDTH 320
#define VM_DISPLAY_HEIGHT 240

// Opcodes - General
#define OP_NOP    0x00  // Do nothing
#define OP_HALT   0x01  // Stop execution

// Opcodes - Stack Operations
#define OP_PUSH   0x02  // Push 8-bit immediate value
#define OP_POP    0x03  // Pop top value
#define OP_DUP    0x04  // Duplicate top value
#define OP_SWAP   0x05  // Swap top 2 values

// Opcodes - Arithmetic
#define OP_ADD    0x06  // Pop 2, push a + b
#define OP_SUB    0x07  // Pop 2, push a - b
#define OP_MUL    0x08  // Pop 2, push a * b
#define OP_DIV    0x09  // Pop 2, push a / b
#define OP_MOD    0x0A  // Pop 2, push a % b
#define OP_NEG    0x0B  // Pop 1, push -a

// Opcodes - Logic & Comparison
#define OP_AND    0x0C  // Pop 2, push a & b
#define OP_OR     0x0D  // Pop 2, push a | b
#define OP_XOR    0x0E  // Pop 2, push a ^ b
#define OP_NOT    0x0F  // Pop 1, push ~a
#define OP_SHL    0x10  // Pop 2, push a << b
#define OP_SHR    0x11  // Pop 2, push a >> b
#define OP_EQ     0x12  // Pop 2, push 1 if equal else 0
#define OP_NEQ    0x13  // Pop 2, push 1 if not equal
#define OP_GT     0x14  // Pop 2, push 1 if a > b
#define OP_LT     0x15  // Pop 2, push 1 if a < b
#define OP_GTE    0x16  // Pop 2, push 1 if a >= b
#define OP_LTE    0x17  // Pop 2, push 1 if a <= b

// Opcodes - Memory
#define OP_LOAD     0x18  // Push value at addr
#define OP_STORE    0x19  // Pop value store to addr
#define OP_LOAD_IND 0x1A  // Pop addr push value at addr
#define OP_STORE_IND 0x1B // Pop addr, pop val \u2192 store val to addr

// Opcodes - Control Flow
#define OP_JMP    0x1C  // Jump unconditionally
#define OP_JZ     0x1D  // Pop if zero, jump
#define OP_JNZ    0x1E  // Pop if not zero, jump
#define OP_CALL   0x1F  // Call subroutine
#define OP_RET    0x20  // Return from subroutine

// Opcodes - System Call
#define OP_SYS    0x21  // Perform syscall with ID imm8

// System Call IDs
#define SYS_EXIT        0x00  // Exit program
#define SYS_PRINT_CHAR  0x01  // Print char (pop 1: ASCII)
#define SYS_READ_CHAR   0x02  // Read char from stdin, push ASCII
#define SYS_DRAW_PIXEL  0x10  // Draw pixel (pop x, y, color)
#define SYS_DRAW_LINE   0x11  // Draw line (pop x1,y1,x2,y2,color)
#define SYS_FILL_RECT   0x12  // Fill rect (pop x,y,w,h,color)
#define SYS_REFRESH     0x13  // Refresh display buffer
#define SYS_POLL_KEY    0x20  // Poll keyboard push 1 if key available
#define SYS_GET_KEY     0x21  // Get key push ASCII code
#define SYS_POLL_MOUSE  0x22  // Poll mouse push 1 if mouse event
#define SYS_GET_MOUSE_X 0x23  // Get mouse X push lo, hi
#define SYS_GET_MOUSE_Y 0x24  // Get mouse Y push lo, hi
#define SYS_GET_MOUSE_B 0x25  // Get mouse buttons push 8-bit flags

typedef enum {
    VM_OK = 0,
    VM_ERROR_STACK_OVERFLOW,
    VM_ERROR_STACK_UNDERFLOW,
    VM_ERROR_INVALID_OPCODE,
    VM_ERROR_DIVISION_BY_ZERO,
    VM_ERROR_INVALID_ADDRESS,
    VM_ERROR_HALT,
    VM_ERROR_SDL_INIT
} vm_error_t;

typedef struct {
    uint8_t memory[VM_MEMORY_SIZE];  
    uint16_t pc;                    
    uint16_t sp;                   
    uint16_t bp;                  
    bool running;                
    vm_error_t error;           
    
    // SDL graphics
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    uint32_t* pixels;
    
    uint8_t last_key;
    bool key_available;
    int mouse_x, mouse_y;
    uint8_t mouse_buttons;
    bool mouse_event;
		bool waiting_for_input;
} vm_t;

vm_error_t init_vm(vm_t* vm);
void cleanup_vm(vm_t* vm);
vm_error_t load_program(vm_t* vm, const char* filename);
vm_error_t run_vm(vm_t* vm);
void vm_push(vm_t* vm, uint8_t value);
uint8_t vm_pop(vm_t* vm);
uint16_t vm_read16(vm_t* vm, uint16_t addr);
void vm_write16(vm_t* vm, uint16_t addr, uint16_t value);

#endif // VM_H
