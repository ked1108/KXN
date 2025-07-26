#include "vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Helper function to push a value onto the stack
void vm_push(vm_t* vm, uint8_t value) {
	if (vm->sp == 0) {
		vm->error = VM_ERROR_STACK_OVERFLOW;
		return;
	}
	vm->memory[vm->sp--] = value;
}

// Helper function to pop a value from the stack
uint8_t vm_pop(vm_t* vm) {
	if (vm->sp >= VM_STACK_TOP) {
		vm->error = VM_ERROR_STACK_UNDERFLOW;
		return 0;
	}
	return vm->memory[++vm->sp];
}

// Helper function to read 16-bit value (little endian)
uint16_t vm_read16(vm_t* vm, uint16_t addr) {
	if (addr >= VM_MEMORY_SIZE - 1) {
		vm->error = VM_ERROR_INVALID_ADDRESS;
		return 0;
	}
	return vm->memory[addr] | (vm->memory[addr + 1] << 8);
}

// Helper function to write 16-bit value (little endian)
void vm_write16(vm_t* vm, uint16_t addr, uint16_t value) {
	if (addr >= VM_MEMORY_SIZE - 1) {
		vm->error = VM_ERROR_INVALID_ADDRESS;
		return;
	}
	vm->memory[addr] = value & 0xFF;
	vm->memory[addr + 1] = (value >> 8) & 0xFF;
}

// Initialize the virtual machine
vm_error_t init_vm(vm_t* vm) {
	// Clear memory
	memset(vm->memory, 0, VM_MEMORY_SIZE);

	// Initialize registers
	vm->pc = 0;
	vm->sp = VM_STACK_TOP;
	vm->bp = VM_STACK_TOP;
	vm->running = true;
	vm->error = VM_OK;

	// Initialize input state
	vm->last_key = 0;
	vm->key_available = false;
	vm->mouse_x = 0;
	vm->mouse_y = 0;
	vm->mouse_buttons = 0;
	vm->mouse_event = false;
	vm->waiting_for_input = false;

	// Initialize SDL
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
		return VM_ERROR_SDL_INIT;
	}

	// Create window
	vm->window = SDL_CreateWindow("8-bit VM", 
															 SDL_WINDOWPOS_CENTERED, 
															 SDL_WINDOWPOS_CENTERED,
															 VM_DISPLAY_WIDTH * 2, 
															 VM_DISPLAY_HEIGHT * 2,
															 SDL_WINDOW_SHOWN);
	if (!vm->window) {
		SDL_Quit();
		return VM_ERROR_SDL_INIT;
	}

	// Create renderer
	vm->renderer = SDL_CreateRenderer(vm->window, -1, SDL_RENDERER_ACCELERATED);
	if (!vm->renderer) {
		SDL_DestroyWindow(vm->window);
		SDL_Quit();
		return VM_ERROR_SDL_INIT;
	}

	// Create texture for pixel buffer
	vm->texture = SDL_CreateTexture(vm->renderer,
																 SDL_PIXELFORMAT_ARGB8888,
																 SDL_TEXTUREACCESS_STREAMING,
																 VM_DISPLAY_WIDTH,
																 VM_DISPLAY_HEIGHT);
	if (!vm->texture) {
		SDL_DestroyRenderer(vm->renderer);
		SDL_DestroyWindow(vm->window);
		SDL_Quit();
		return VM_ERROR_SDL_INIT;
	}

	// Allocate pixel buffer
	vm->pixels = malloc(VM_DISPLAY_WIDTH * VM_DISPLAY_HEIGHT * sizeof(uint32_t));
	if (!vm->pixels) {
		SDL_DestroyTexture(vm->texture);
		SDL_DestroyRenderer(vm->renderer);
		SDL_DestroyWindow(vm->window);
		SDL_Quit();
		return VM_ERROR_SDL_INIT;
	}

	// Clear pixel buffer to black
	memset(vm->pixels, 0, VM_DISPLAY_WIDTH * VM_DISPLAY_HEIGHT * sizeof(uint32_t));

	return VM_OK;
}

// Cleanup the virtual machine
void cleanup_vm(vm_t* vm) {
	if (vm->pixels) {
		free(vm->pixels);
	}
	if (vm->texture) {
		SDL_DestroyTexture(vm->texture);
	}
	if (vm->renderer) {
		SDL_DestroyRenderer(vm->renderer);
	}
	if (vm->window) {
		SDL_DestroyWindow(vm->window);
	}
	SDL_Quit();
}

// Load program from file
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

// Draw a line using Bresenham's algorithm
void draw_line(vm_t* vm, int x0, int y0, int x1, int y1, uint32_t color) {
	int dx = abs(x1 - x0);
	int dy = abs(y1 - y0);
	int sx = (x0 < x1) ? 1 : -1;
	int sy = (y0 < y1) ? 1 : -1;
	int err = dx - dy;

	while (true) {
		if (x0 >= 0 && x0 < VM_DISPLAY_WIDTH && y0 >= 0 && y0 < VM_DISPLAY_HEIGHT) {
			vm->pixels[y0 * VM_DISPLAY_WIDTH + x0] = color;
		}

		if (x0 == x1 && y0 == y1) break;

		int e2 = 2 * err;
		if (e2 > -dy) {
			err -= dy;
			x0 += sx;
		}
		if (e2 < dx) {
			err += dx;
			y0 += sy;
		}
	}
}

// Handle system calls
vm_error_t handle_syscall(vm_t* vm, uint8_t syscall_id) {
	switch (syscall_id) {
		case SYS_EXIT:
			vm->running = false;
			return VM_ERROR_HALT;

		case SYS_PRINT_CHAR: {
			uint8_t ch = vm_pop(vm);
			printf("%c", ch);
			fflush(stdout);
			break;
		}

		case SYS_READ_CHAR: {
			if (!vm->waiting_for_input) {
				vm->waiting_for_input = true;
				vm->pc--;
			} else if (vm->key_available) {
				vm_push(vm, vm->last_key);
				vm->waiting_for_input = false;
				vm->key_available = false;
			} 
			break;
		}

		case SYS_DRAW_PIXEL: {
			uint8_t color = vm_pop(vm);
			uint8_t y = vm_pop(vm);
			uint8_t x = vm_pop(vm);

			if (x < VM_DISPLAY_WIDTH && y < VM_DISPLAY_HEIGHT) {
				uint32_t pixel_color = 0xFF000000 | (color << 16) | (color << 8) | color;
				vm->pixels[y * VM_DISPLAY_WIDTH + x] = pixel_color;
			}
			break;
		}

		case SYS_DRAW_LINE: {
			uint8_t color = vm_pop(vm);
			uint8_t y2 = vm_pop(vm);
			uint8_t x2 = vm_pop(vm);
			uint8_t y1 = vm_pop(vm);
			uint8_t x1 = vm_pop(vm);

			uint32_t pixel_color = 0xFF000000 | (color << 16) | (color << 8) | color;
			draw_line(vm, x1, y1, x2, y2, pixel_color);
			break;
		}

		case SYS_FILL_RECT: {
			uint8_t color = vm_pop(vm);
			uint8_t h = vm_pop(vm);
			uint8_t w = vm_pop(vm);
			uint8_t y = vm_pop(vm);
			uint8_t x = vm_pop(vm);

			uint32_t pixel_color = 0xFF000000 | (color << 16) | (color << 8) | color;

			for (int py = y; py < y + h && py < VM_DISPLAY_HEIGHT; py++) {
				for (int px = x; px < x + w && px < VM_DISPLAY_WIDTH; px++) {
					if (px >= 0 && py >= 0) {
						vm->pixels[py * VM_DISPLAY_WIDTH + px] = pixel_color;
					}
				}
			}
			break;
		}

		case SYS_REFRESH:
			SDL_UpdateTexture(vm->texture, NULL, vm->pixels, VM_DISPLAY_WIDTH * sizeof(uint32_t));
			SDL_RenderClear(vm->renderer);
			SDL_RenderCopy(vm->renderer, vm->texture, NULL, NULL);
			SDL_RenderPresent(vm->renderer);
			break;

		case SYS_POLL_KEY:
			vm_push(vm, vm->key_available ? 1 : 0);
			break;

		case SYS_GET_KEY:
			vm_push(vm, vm->last_key);
			vm->key_available = false;
			break;

		case SYS_POLL_MOUSE:
			vm_push(vm, vm->mouse_event ? 1 : 0);
			break;

		case SYS_GET_MOUSE_X:
			vm_push(vm, vm->mouse_x & 0xFF);
			vm_push(vm, (vm->mouse_x >> 8) & 0xFF);
			break;

		case SYS_GET_MOUSE_Y:
			vm_push(vm, vm->mouse_y & 0xFF);
			vm_push(vm, (vm->mouse_y >> 8) & 0xFF);
			break;

		case SYS_GET_MOUSE_B:
			vm_push(vm, vm->mouse_buttons);
			vm->mouse_event = false;
			break;

		default:
			return VM_ERROR_INVALID_OPCODE;
	}

	return VM_OK;
}

// Process SDL events
void process_events(vm_t* vm) {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_QUIT:
				vm->running = false;
				break;

			case SDL_KEYDOWN:
				vm->last_key = event.key.keysym.sym & 0xFF;
				vm->key_available = true;
				break;

			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
			case SDL_MOUSEMOTION:
				vm->mouse_x = event.motion.x / 2; // Scale down by 2
				vm->mouse_y = event.motion.y / 2;
				vm->mouse_buttons = SDL_GetMouseState(NULL, NULL);
				vm->mouse_event = true;
				break;
		}
	}
}

// Main execution loop
vm_error_t run_vm(vm_t* vm) {
	while (vm->running && vm->error == VM_OK) {
		process_events(vm);

		if (vm->waiting_for_input && !vm->key_available) {
			SDL_Delay(1);
			continue;
    }

		// Fetch instruction
		if (vm->pc >= VM_MEMORY_SIZE) {
			vm->error = VM_ERROR_INVALID_ADDRESS;
			break;
		}

		uint8_t opcode = vm->memory[vm->pc++];

		// Decode and execute
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
				// Push return address (little endian)
				vm_push(vm, vm->pc & 0xFF);
				vm_push(vm, (vm->pc >> 8) & 0xFF);
				vm->pc = addr;
				break;
			}

			case OP_RET: {
				// Pop return address (little endian)
				uint16_t addr = vm_pop(vm) << 8;
				addr |= vm_pop(vm);
				vm->pc = addr;
				break;
			}

			case OP_SYS: {
				uint8_t syscall_id = vm->memory[vm->pc++];
				vm_error_t sys_error = handle_syscall(vm, syscall_id);
				if (sys_error != VM_OK && sys_error != VM_ERROR_HALT) {
					vm->error = sys_error;
				}
				break;
			}

			default:
				vm->error = VM_ERROR_INVALID_OPCODE;
				break;
		}

		if (vm->error != VM_OK && vm->error != VM_ERROR_HALT) {
			break;
		}

	}

	SDL_Delay(1);
	return vm->error;
}

// Example main function
int main(int argc, char* argv[]) {
	if (argc != 2) {
		printf("Usage: %s <program_file>\n", argv[0]);
		return 1;
	}

	vm_t vm;
	vm_error_t error;

	// Initialize VM
	error = init_vm(&vm);
	if (error != VM_OK) {
		printf("Failed to initialize VM: %d\n", error);
		return 1;
	}

	// Load program
	error = load_program(&vm, argv[1]);
	if (error != VM_OK) {
		printf("Failed to load program: %d\n", error);
		cleanup_vm(&vm);
		return 1;
	}

	// Run VM
	printf("Running VM...\n");
	error = run_vm(&vm);

	if (error == VM_ERROR_HALT) {
		printf("VM halted normally\n");
	} else if (error != VM_OK) {
		printf("VM error: %d\n", error);
	}

	// Cleanup
	cleanup_vm(&vm);
	return 0;
}
