#ifndef OPENTEC_BASE_DISPLAY_CONTROLLER_H
#define OPENTEC_BASE_DISPLAY_CONTROLLER_H

#include <stdint.h>

/**
 * @brief Selects whether a display-bus byte is a command or data.
 *
 * The controller callback receives this mode before each byte so the platform can drive the
 * display's command/data select line correctly.
 */
typedef enum {
    DISPLAY_BUS_COMMAND, /**< Byte configures the display controller. */
    DISPLAY_BUS_DATA,    /**< Byte is display-controller data. */
} DisplayBusMode;

/**
 * @brief Writes one byte to the display bus.
 *
 * Implementations select command or data mode from the supplied mode and may use context for
 * platform-specific bus state.
 *
 * @param[in,out] context Opaque platform-specific bus state.
 * @param[in] mode Command or data mode for the byte.
 * @param[in] value Byte to write.
 */
typedef void (*DisplayBusWrite)(void *context, DisplayBusMode mode, uint8_t value);

/**
 * @brief Initializes the display controller.
 *
 * Sends the fixed controller configuration required before framebuffer transfers begin.
 *
 * @param[in] write Callback that writes one command or data byte.
 * @param[in,out] context Opaque state passed to write for every byte.
 */
void display_controller_initialize(DisplayBusWrite write, void *context);

/**
 * @brief Starts a complete framebuffer transfer.
 *
 * Selects the panel's full column and row window and switches the controller to display-RAM write
 * mode.
 *
 * @param[in] write Callback that writes one command or data byte.
 * @param[in,out] context Opaque state passed to write for every byte.
 */
void display_controller_begin_frame(DisplayBusWrite write, void *context);

#endif
