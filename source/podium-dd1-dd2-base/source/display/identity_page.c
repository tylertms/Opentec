#include "display/identity_page.h"

#include "board/identity.h"
#include "display/framebuffer.h"
#include "display/text.h"

enum {
    IDENTITY_COLOR = 15,
    IDENTITY_WORDMARK_Y = 10,
    IDENTITY_MODEL_Y = 50,
};

/**
 * @brief Renders the base identity page.
 *
 * Shows the Fanatec wordmark and the hardware-selected Podium Wheel Base DD1 or DD2 PS4 model
 * text used by the root local-display controller.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 * @param[in] variant Hardware-selected base variant.
 */
void display_identity_page_render(DisplayFramebuffer framebuffer, BoardVariant variant) {
    const char *model =
        variant == BOARD_VARIANT_DD1 ? "Podium Wheel Base DD1 PS4" : "Podium Wheel Base DD2 PS4";
    display_framebuffer_clear(framebuffer);
    display_text_draw_centered(framebuffer, "FANATEC", IDENTITY_WORDMARK_Y, 3, IDENTITY_COLOR);
    display_text_draw_centered(framebuffer, model, IDENTITY_MODEL_Y, 1, IDENTITY_COLOR);
}
