#ifndef OPENTEC_BASE_DISPLAY_ICON_BITMAPS_H
#define OPENTEC_BASE_DISPLAY_ICON_BITMAPS_H

#include <stdint.h>

#include "display/bitmap.h"
/**
 * @brief Queues one official seven-segment icon group for row-service rendering.
 *
 * @param[in,out] queue Bitmap queue owned by the framebuffer composition.
 * @param[in] x Glyph origin.
 * @param[in] y Glyph origin.
 * @param[in] segments Seven-segment mask and decimal-point bit.
 */
void display_icon_group_draw_queued(DisplayBitmapQueue *queue, uint16_t x, uint16_t y,
                                    uint8_t segments);

#endif
