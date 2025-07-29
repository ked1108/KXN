#ifndef PLATFORM_IO_H
#define PLATFORM_IO_H

#include <stdint.h>
#include <stdbool.h>

// Forward declaration for VM structure
typedef struct vm_t vm_t;

// Platform I/O operation IDs (renamed from SYS_*)
#define IO_EXIT        0x00  // Exit program
#define IO_PRINT_CHAR  0x01  // Print char (pop 1: ASCII)
#define IO_READ_CHAR   0x02  // Read char from stdin, push ASCII
#define IO_DRAW_PIXEL  0x10  // Draw pixel (pop x, y, color)
#define IO_DRAW_LINE   0x11  // Draw line (pop x1,y1,x2,y2,color)
#define IO_FILL_RECT   0x12  // Fill rect (pop x,y,w,h,color)
#define IO_REFRESH     0x13  // Refresh display buffer
#define IO_POLL_KEY    0x20  // Poll keyboard push 1 if key available
#define IO_GET_KEY     0x21  // Get key push ASCII code
#define IO_POLL_MOUSE  0x22  // Poll mouse push 1 if mouse event
#define IO_GET_MOUSE_X 0x23  // Get mouse X push lo, hi
#define IO_GET_MOUSE_Y 0x24  // Get mouse Y push lo, hi
#define IO_GET_MOUSE_B 0x25  // Get mouse buttons push 8-bit flags

// Error codes for platform I/O operations
typedef enum {
    PLATFORM_IO_OK = 0,
    PLATFORM_IO_ERROR_INIT_FAILED,
    PLATFORM_IO_ERROR_INVALID_OPERATION,
    PLATFORM_IO_ERROR_DEVICE_NOT_READY,
    PLATFORM_IO_ERROR_OUT_OF_BOUNDS,
    PLATFORM_IO_ERROR_UNKNOWN
} platform_io_error_t;

// Platform I/O context structure (opaque to VM core)
typedef struct platform_io_context_t platform_io_context_t;

/**
 * Initialize the platform I/O subsystem
 * Returns: platform_io_context_t* on success, NULL on failure
 */
platform_io_context_t* platform_io_init(void);

/**
 * Cleanup the platform I/O subsystem
 * @param ctx: Platform I/O context to cleanup
 */
void platform_io_cleanup(platform_io_context_t* ctx);

/**
 * Handle a platform I/O operation
 * @param vm: VM instance
 * @param ctx: Platform I/O context
 * @param io_id: I/O operation ID
 * @return: Error code
 */
platform_io_error_t handle_platform_io(vm_t* vm, platform_io_context_t* ctx, uint8_t io_id);

/**
 * Process platform events (keyboard, mouse, window events)
 * Should be called regularly to handle input and system events
 * @param vm: VM instance
 * @param ctx: Platform I/O context
 * @return: true if VM should continue running, false if should exit
 */
bool platform_io_process_events(vm_t* vm, platform_io_context_t* ctx);

/**
 * Check if platform is waiting for input
 * @param ctx: Platform I/O context
 * @return: true if waiting for input, false otherwise
 */
bool platform_io_is_waiting_for_input(platform_io_context_t* ctx);

#endif // PLATFORM_IO_H