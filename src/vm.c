#include "vm.h"
#include "platform_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Push a value onto the VM stack
 */
void vm_push(vm_t* vm, uint8_t value) {
    if (vm->sp == 0) {
        vm->error = VM_ERROR_STACK_OVERFLOW;
        return;
    }
    vm->memory[vm->sp--] = value;
}

/**
 * Pop a value from the VM stack
 */
uint8_t vm_pop(vm_t* vm) {
    if (vm->sp >= VM_STACK_TOP) {
        vm->error = VM_ERROR_STACK_UNDERFLOW;
        return 0;
    }
    return vm->memory[++vm->sp];
}

/**
 * Read a 16-bit value from VM memory (little-endian)
 */
uint16_t vm_read16(vm_t* vm, uint16_t addr) {
    if (addr >= VM_MEMORY_SIZE - 1) {
        vm->error = VM_ERROR_INVALID_ADDRESS;
        return 0;
    }
    return vm->memory[addr] | (vm->memory[addr + 1] << 8);
}

/**
 * Write a 16-bit value to VM memory (little-endian)
 */
void vm_write16(vm_t* vm, uint16_t addr, uint16_t value) {
    if (addr >= VM_MEMORY_SIZE - 1) {
        vm->error = VM_ERROR_INVALID_ADDRESS;
        return;
    }
    vm->memory[addr] = value & 0xFF;
    vm->memory[addr + 1] = (value >> 8) & 0xFF;
}

/**
 * Initialize the VM core (platform-agnostic)
 */
vm_error_t init_vm(vm_t* vm) {
    memset(vm->memory, 0, VM_MEMORY_SIZE);
    
    vm->pc = 0;
    vm->sp = VM_STACK_TOP;
    vm->bp = VM_STACK_TOP;
    vm->running = true;
    vm->error = VM_OK;
    
    return VM_OK;
}

/**
 * Cleanup VM core resources
 */
void cleanup_vm(vm_t* vm) {
    // VM core has no resources to cleanup
    // Platform-specific cleanup is handled by platform_io_cleanup()
}

/**
 * Load a program into VM memory
 */
vm_error_t load_program(vm_t* vm, const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        return VM_ERROR_INVALID_ADDRESS;
    }
    
    size_t bytes_read = fread(vm->memory, 1, VM_MEMORY_SIZE, file);
    fclose(file);
    
    if (bytes_read == 0) {
        return VM_ERROR_INVALID_ADDRESS;
    }
    
    return VM_OK;
}

/**
 * Main VM execution loop
 * @param vm: VM instance
 * @param platform_ctx: Platform I/O context (opaque to VM core)
 */
vm_error_t run_vm(vm_t* vm, void* platform_ctx) {
    platform_io_context_t* io_ctx = (platform_io_context_t*)platform_ctx;
    
    while (vm->running && vm->error == VM_OK) {
        // Process platform events
        if (!platform_io_process_events(vm, io_ctx)) {
            vm->running = false;
            break;
        }
        
        // If waiting for input, skip instruction execution
        if (platform_io_is_waiting_for_input(io_ctx)) {
            continue;
        }
        
        // Check program counter bounds
        if (vm->pc >= VM_MEMORY_SIZE) {
            vm->error = VM_ERROR_INVALID_ADDRESS;
            break;
        }
        
        // Fetch and execute instruction
        uint8_t opcode = vm->memory[vm->pc++];
        
        switch (opcode) {
            case OP_NOP:
                // Do nothing
                break;
                
            case OP_HALT:
                vm->running = false;
                vm->error = VM_ERROR_HALT;
                break;
                
            case OP_PUSH: {
                uint8_t value = vm->memory[vm->pc++];
                vm_push(vm, value);
                break;
            }
            
            case OP_POP:
                vm_pop(vm);
                break;
                
            case OP_DUP: {
                uint8_t value = vm_pop(vm);
                vm_push(vm, value);
                vm_push(vm, value);
                break;
            }
            
            case OP_SWAP: {
                uint8_t a = vm_pop(vm);
                uint8_t b = vm_pop(vm);
                vm_push(vm, a);
                vm_push(vm, b);
                break;
            }
            
            // Arithmetic operations
            case OP_ADD: {
                uint8_t b = vm_pop(vm);
                uint8_t a = vm_pop(vm);
                vm_push(vm, a + b);
                break;
            }
            
            case OP_SUB: {
                uint8_t b = vm_pop(vm);
                uint8_t a = vm_pop(vm);
                vm_push(vm, a - b);
                break;
            }
            
            case OP_MUL: {
                uint8_t b = vm_pop(vm);
                uint8_t a = vm_pop(vm);
                vm_push(vm, a * b);
                break;
            }
            
            case OP_DIV: {
                uint8_t b = vm_pop(vm);
                uint8_t a = vm_pop(vm);
                if (b == 0) {
                    vm->error = VM_ERROR_DIVISION_BY_ZERO;
                    break;
                }
                vm_push(vm, a / b);
                break;
            }
            
            case OP_MOD: {
                uint8_t b = vm_pop(vm);
                uint8_t a = vm_pop(vm);
                if (b == 0) {
                    vm->error = VM_ERROR_DIVISION_BY_ZERO;
                    break;
                }
                vm_push(vm, a % b);
                break;
            }
            
            case OP_NEG: {
                uint8_t a = vm_pop(vm);
                vm_push(vm, -a);
                break;
            }
            
            // Logic operations
            case OP_AND: {
                uint8_t b = vm_pop(vm);
                uint8_t a = vm_pop(vm);
                vm_push(vm, a & b);
                break;
            }
            
            case OP_OR: {
                uint8_t b = vm_pop(vm);
                uint8_t a = vm_pop(vm);
                vm_push(vm, a | b);
                break;
            }
            
            case OP_XOR: {
                uint8_t b = vm_pop(vm);
                uint8_t a = vm_pop(vm);
                vm_push(vm, a ^ b);
                break;
            }
            
            case OP_NOT: {
                uint8_t a = vm_pop(vm);
                vm_push(vm, ~a);
                break;
            }
            
            case OP_SHL: {
                uint8_t b = vm_pop(vm);
                uint8_t a = vm_pop(vm);
                vm_push(vm, a << b);
                break;
            }
            
            case OP_SHR: {
                uint8_t b = vm_pop(vm);
                uint8_t a = vm_pop(vm);
                vm_push(vm, a >> b);
                break;
            }
            
            // Comparison operations
            case OP_EQ: {
                uint8_t b = vm_pop(vm);
                uint8_t a = vm_pop(vm);
                vm_push(vm, a == b ? 1 : 0);
                break;
            }
            
            case OP_NEQ: {
                uint8_t b = vm_pop(vm);
                uint8_t a = vm_pop(vm);
                vm_push(vm, a != b ? 1 : 0);
                break;
            }
            
            case OP_GT: {
                uint8_t b = vm_pop(vm);
                uint8_t a = vm_pop(vm);
                vm_push(vm, a > b ? 1 : 0);
                break;
            }
            
            case OP_LT: {
                uint8_t b = vm_pop(vm);
                uint8_t a = vm_pop(vm);
                vm_push(vm, a < b ? 1 : 0);
                break;
            }
            
            case OP_GTE: {
                uint8_t b = vm_pop(vm);
                uint8_t a = vm_pop(vm);
                vm_push(vm, a >= b ? 1 : 0);
                break;
            }
            
            case OP_LTE: {
                uint8_t b = vm_pop(vm);
                uint8_t a = vm_pop(vm);
                vm_push(vm, a <= b ? 1 : 0);
                break;
            }
            
            // Memory operations
            case OP_LOAD: {
                uint16_t addr = vm_read16(vm, vm->pc);
                vm->pc += 2;
                if (addr < VM_MEMORY_SIZE) {
                    vm_push(vm, vm->memory[addr]);
                } else {
                    vm->error = VM_ERROR_INVALID_ADDRESS;
                }
                break;
            }
            
            case OP_STORE: {
                uint16_t addr = vm_read16(vm, vm->pc);
                vm->pc += 2;
                uint8_t value = vm_pop(vm);
                if (addr < VM_MEMORY_SIZE) {
                    vm->memory[addr] = value;
                } else {
                    vm->error = VM_ERROR_INVALID_ADDRESS;
                }
                break;
            }
            
            case OP_LOAD_IND: {
                uint16_t addr = vm_pop(vm) | (vm_pop(vm) << 8);
                if (addr < VM_MEMORY_SIZE) {
                    vm_push(vm, vm->memory[addr]);
                } else {
                    vm->error = VM_ERROR_INVALID_ADDRESS;
                }
                break;
            }
            
            case OP_STORE_IND: {
                uint16_t addr = vm_pop(vm) | (vm_pop(vm) << 8);
                uint8_t value = vm_pop(vm);
                if (addr < VM_MEMORY_SIZE) {
                    vm->memory[addr] = value;
                } else {
                    vm->error = VM_ERROR_INVALID_ADDRESS;
                }
                break;
            }
            
            // Control flow operations
            case OP_JMP: {
                uint16_t addr = vm_read16(vm, vm->pc);
                vm->pc = addr;
                break;
            }
            
            case OP_JZ: {
                uint16_t addr = vm_read16(vm, vm->pc);
                vm->pc += 2;
                uint8_t value = vm_pop(vm);
                if (value == 0) {
                    vm->pc = addr;
                }
                break;
            }
            
            case OP_JNZ: {
                uint16_t addr = vm_read16(vm, vm->pc);
                vm->pc += 2;
                uint8_t value = vm_pop(vm);
                if (value != 0) {
                    vm->pc = addr;
                }
                break;
            }
            
            case OP_CALL: {
                uint16_t addr = vm_read16(vm, vm->pc);
                vm->pc += 2;
                vm_push(vm, vm->pc & 0xFF);
                vm_push(vm, (vm->pc >> 8) & 0xFF);
                vm->pc = addr;
                break;
            }
            
            case OP_RET: {
                uint16_t addr = vm_pop(vm) << 8;
                addr |= vm_pop(vm);
                vm->pc = addr;
                break;
            }
            
            // Platform I/O operation (formerly OP_SYS)
            case OP_IO: {
                uint8_t io_id = vm->memory[vm->pc++];
                platform_io_error_t io_error = handle_platform_io(vm, io_ctx, io_id);
                
                // Convert platform I/O errors to VM errors
                if (io_error != PLATFORM_IO_OK) {
                    if (io_id == IO_EXIT) {
                        vm->error = VM_ERROR_HALT;
                    } else {
                        vm->error = VM_ERROR_PLATFORM_IO;
                    }
                }
                break;
            }
            
            default:
                vm->error = VM_ERROR_INVALID_OPCODE;
                break;
        }
        
        // Break on error (except normal halt)
        if (vm->error != VM_OK && vm->error != VM_ERROR_HALT) {
            break;
        }
    }
    
    return vm->error;
}

/**
 * Main entry point
 */
int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: %s <program_file>\n", argv[0]);
        return 1;
    }
    
    // Initialize VM core
    vm_t vm;
    vm_error_t error = init_vm(&vm);
    if (error != VM_OK) {
        printf("Failed to initialize VM: %d\n", error);
        return 1;
    }
    
    // Initialize platform I/O
    platform_io_context_t* io_ctx = platform_io_init();
    if (!io_ctx) {
        printf("Failed to initialize platform I/O\n");
        cleanup_vm(&vm);
        return 1;
    }
    
    // Load and run program
    error = load_program(&vm, argv[1]);
    if (error != VM_OK) {
        printf("Failed to load program: %d\n", error);
        platform_io_cleanup(io_ctx);
        cleanup_vm(&vm);
        return 1;
    }
    
    printf("Running VM...\n");
    error = run_vm(&vm, io_ctx);
    
    // Report execution result
    if (error == VM_ERROR_HALT) {
        printf("VM halted normally\n");
    } else if (error != VM_OK) {
        printf("VM error: %d\n", error);
    }
    
    // Cleanup
    platform_io_cleanup(io_ctx);
    cleanup_vm(&vm);
    return 0;
}
