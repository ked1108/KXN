/**
 * SDL2 Platform I/O Implementation for KXN VM
 * This file contains all SDL2-specific code for graphics, input, and system operations.
 */

#include "platform_io.h"
#include "vm.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * SDL2-specific platform I/O context
 */
typedef struct platform_io_context_t {
    // SDL objects
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    uint32_t* pixels;
    
    // Input state
    uint8_t last_key;
    bool key_available;
    int mouse_x, mouse_y;
    uint8_t mouse_buttons;
    bool mouse_event;
    bool waiting_for_input;
} platform_io_context_t;

/**
 * Initialize SDL2 platform I/O subsystem
 */
platform_io_context_t* platform_io_init(void) {
    platform_io_context_t* ctx = malloc(sizeof(platform_io_context_t));
    if (!ctx) {
        return NULL;
    }
    
    // Initialize context state
    memset(ctx, 0, sizeof(platform_io_context_t));
    
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        free(ctx);
        return NULL;
    }
    
    // Create window
    ctx->window = SDL_CreateWindow("KXN VM",
                                   SDL_WINDOWPOS_CENTERED,
                                   SDL_WINDOWPOS_CENTERED,
                                   VM_DISPLAY_WIDTH * 2,
                                   VM_DISPLAY_HEIGHT * 2,
                                   SDL_WINDOW_SHOWN);
    if (!ctx->window) {
        printf("SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        free(ctx);
        return NULL;
    }
    
    // Create renderer
    ctx->renderer = SDL_CreateRenderer(ctx->window, -1, SDL_RENDERER_ACCELERATED);
    if (!ctx->renderer) {
        printf("SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(ctx->window);
        SDL_Quit();
        free(ctx);
        return NULL;
    }
    
    // Create texture for pixel buffer
    ctx->texture = SDL_CreateTexture(ctx->renderer,
                                     SDL_PIXELFORMAT_ARGB8888,
                                     SDL_TEXTUREACCESS_STREAMING,
                                     VM_DISPLAY_WIDTH,
                                     VM_DISPLAY_HEIGHT);
    if (!ctx->texture) {
        printf("SDL_CreateTexture failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(ctx->renderer);
        SDL_DestroyWindow(ctx->window);
        SDL_Quit();
        free(ctx);
        return NULL;
    }
    
    // Allocate pixel buffer
    ctx->pixels = malloc(VM_DISPLAY_WIDTH * VM_DISPLAY_HEIGHT * sizeof(uint32_t));
    if (!ctx->pixels) {
        printf("Failed to allocate pixel buffer\n");
        SDL_DestroyTexture(ctx->texture);
        SDL_DestroyRenderer(ctx->renderer);
        SDL_DestroyWindow(ctx->window);
        SDL_Quit();
        free(ctx);
        return NULL;
    }
    
    // Clear pixel buffer
    memset(ctx->pixels, 0, VM_DISPLAY_WIDTH * VM_DISPLAY_HEIGHT * sizeof(uint32_t));
    
    printf("SDL2 platform initialized successfully\n");
    return ctx;
}

/**
 * Cleanup SDL2 platform I/O subsystem
 */
void platform_io_cleanup(platform_io_context_t* ctx) {
    if (!ctx) return;
    
    if (ctx->pixels) {
        free(ctx->pixels);
    }
    if (ctx->texture) {
        SDL_DestroyTexture(ctx->texture);
    }
    if (ctx->renderer) {
        SDL_DestroyRenderer(ctx->renderer);
    }
    if (ctx->window) {
        SDL_DestroyWindow(ctx->window);
    }
    
    SDL_Quit();
    free(ctx);
    printf("SDL2 platform cleaned up\n");
}

/**
 * Bresenham line drawing algorithm
 */
static void draw_line(platform_io_context_t* ctx, int x0, int y0, int x1, int y1, uint32_t color) {
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;
    
    while (true) {
        // Draw pixel if within bounds
        if (x0 >= 0 && x0 < VM_DISPLAY_WIDTH && y0 >= 0 && y0 < VM_DISPLAY_HEIGHT) {
            ctx->pixels[y0 * VM_DISPLAY_WIDTH + x0] = color;
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

/**
 * Handle platform I/O operations
 */
platform_io_error_t handle_platform_io(vm_t* vm, platform_io_context_t* ctx, uint8_t io_id) {
    switch (io_id) {
        case IO_EXIT:
            vm->running = false;
            return PLATFORM_IO_OK;
            
        case IO_PRINT_CHAR: {
            uint8_t ch = vm_pop(vm);
            printf("%c", ch);
            fflush(stdout);
            return PLATFORM_IO_OK;
        }
        
        case IO_READ_CHAR: {
            if (!ctx->waiting_for_input) {
                ctx->waiting_for_input = true;
                vm->pc--; // Retry this instruction until input is available
            } else if (ctx->key_available) {
                vm_push(vm, ctx->last_key);
                ctx->waiting_for_input = false;
                ctx->key_available = false;
            }
            return PLATFORM_IO_OK;
        }
        
        case IO_DRAW_PIXEL: {
            uint8_t color = vm_pop(vm);
            uint8_t y = vm_pop(vm);
            uint8_t x = vm_pop(vm);
            
            if (x < VM_DISPLAY_WIDTH && y < VM_DISPLAY_HEIGHT) {
                // Convert 8-bit grayscale to ARGB
                uint32_t pixel_color = 0xFF000000 | (color << 16) | (color << 8) | color;
                ctx->pixels[y * VM_DISPLAY_WIDTH + x] = pixel_color;
            }
            return PLATFORM_IO_OK;
        }
        
        case IO_DRAW_LINE: {
            uint8_t color = vm_pop(vm);
            uint8_t y2 = vm_pop(vm);
            uint8_t x2 = vm_pop(vm);
            uint8_t y1 = vm_pop(vm);
            uint8_t x1 = vm_pop(vm);
            
            // Convert 8-bit grayscale to ARGB
            uint32_t pixel_color = 0xFF000000 | (color << 16) | (color << 8) | color;
            draw_line(ctx, x1, y1, x2, y2, pixel_color);
            return PLATFORM_IO_OK;
        }
        
        case IO_FILL_RECT: {
            uint8_t color = vm_pop(vm);
            uint8_t h = vm_pop(vm);
            uint8_t w = vm_pop(vm);
            uint8_t y = vm_pop(vm);
            uint8_t x = vm_pop(vm);
            
            // Convert 8-bit grayscale to ARGB
            uint32_t pixel_color = 0xFF000000 | (color << 16) | (color << 8) | color;
            
            // Fill rectangle with bounds checking
            for (int py = y; py < y + h && py < VM_DISPLAY_HEIGHT; py++) {
                for (int px = x; px < x + w && px < VM_DISPLAY_WIDTH; px++) {
                    if (px >= 0 && py >= 0) {
                        ctx->pixels[py * VM_DISPLAY_WIDTH + px] = pixel_color;
                    }
                }
            }
            return PLATFORM_IO_OK;
        }
        
        case IO_REFRESH:
            // Update texture with pixel data and present to screen
            SDL_UpdateTexture(ctx->texture, NULL, ctx->pixels, VM_DISPLAY_WIDTH * sizeof(uint32_t));
            SDL_RenderClear(ctx->renderer);
            SDL_RenderCopy(ctx->renderer, ctx->texture, NULL, NULL);
            SDL_RenderPresent(ctx->renderer);
            return PLATFORM_IO_OK;
            
        case IO_POLL_KEY:
            // Push 1 if key is available, 0 otherwise
            vm_push(vm, ctx->key_available ? 1 : 0);
            return PLATFORM_IO_OK;
            
        case IO_GET_KEY:
            // Get the last pressed key and mark as consumed
            vm_push(vm, ctx->last_key);
            ctx->key_available = false;
            return PLATFORM_IO_OK;
            
        case IO_POLL_MOUSE:
            // Push 1 if mouse event occurred, 0 otherwise
            vm_push(vm, ctx->mouse_event ? 1 : 0);
            return PLATFORM_IO_OK;
            
        case IO_GET_MOUSE_X:
            // Push mouse X coordinate (little-endian 16-bit)
            vm_push(vm, ctx->mouse_x & 0xFF);
            vm_push(vm, (ctx->mouse_x >> 8) & 0xFF);
            return PLATFORM_IO_OK;
            
        case IO_GET_MOUSE_Y:
            // Push mouse Y coordinate (little-endian 16-bit)
            vm_push(vm, ctx->mouse_y & 0xFF);
            vm_push(vm, (ctx->mouse_y >> 8) & 0xFF);
            return PLATFORM_IO_OK;
            
        case IO_GET_MOUSE_B:
            // Get mouse button state and clear event flag
            vm_push(vm, ctx->mouse_buttons);
            ctx->mouse_event = false;
            return PLATFORM_IO_OK;
            
        default:
            printf("Unknown I/O operation: 0x%02X\n", io_id);
            return PLATFORM_IO_ERROR_INVALID_OPERATION;
    }
}

/**
 * Process SDL events (keyboard, mouse, window)
 */
bool platform_io_process_events(vm_t* vm, platform_io_context_t* ctx) {
    SDL_Event event;
    
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                // User closed the window
                return false;
                
            case SDL_KEYDOWN:
                // Store the pressed key
                ctx->last_key = event.key.keysym.sym & 0xFF;
                ctx->key_available = true;
                break;
                
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
            case SDL_MOUSEMOTION:
                // Update mouse state (scale coordinates from window to display size)
                ctx->mouse_x = event.motion.x / 2;
                ctx->mouse_y = event.motion.y / 2;
                ctx->mouse_buttons = SDL_GetMouseState(NULL, NULL);
                ctx->mouse_event = true;
                break;
        }
    }
    
    // Add a small delay to prevent excessive CPU usage
    SDL_Delay(1);
    return true;
}

/**
 * Check if platform is waiting for input
 */
bool platform_io_is_waiting_for_input(platform_io_context_t* ctx) {
    return ctx->waiting_for_input && !ctx->key_available;
}