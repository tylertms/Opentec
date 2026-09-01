#include "display/identity_page.h"

#include "board/identity.h"
#include "display/framebuffer.h"
#include "display/identity_logo.h"
#include "display/text.h"

/**
 * @brief Defines identity-page colors and fixed layout coordinates.
 *
 * The coordinates place the logo and selected model name in the local display framebuffer.
 */
enum {
    IDENTITY_COLOR = 15,               /**< Foreground grayscale value for identity-page content. */
    IDENTITY_LOGO_X = 0,               /**< Logo left coordinate. */
    IDENTITY_LOGO_Y = 19,              /**< Logo top coordinate. */
    IDENTITY_MODEL_X = 70,             /**< Standard model-name left coordinate. */
    IDENTITY_PLAYSTATION_MODEL_X = 63, /**< PS4 model-name left coordinate. */
    IDENTITY_MODEL_Y = 50,             /**< Model-name top coordinate. */
};

/**
 * @brief Renders the base identity page.
 *
 * Shows the Opentec logo bitmap and the hardware-selected Podium Wheel Base DD1 or DD2 model text,
 * including the PS4 suffix when the option strap is set.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 * @param[in] identity Hardware-selected base variant and option straps.
 */
void display_identity_page_render(DisplayFramebuffer framebuffer, BoardIdentity identity) {
    const char *model;
    uint16_t model_x;
    if (identity.hardware_option != 0) {
        model = identity.variant == BOARD_VARIANT_DD1 ? "Podium Wheel Base DD1 PS4"
                                                      : "Podium Wheel Base DD2 PS4";
        model_x = IDENTITY_PLAYSTATION_MODEL_X;
    } else {
        model = identity.variant == BOARD_VARIANT_DD1 ? "Podium Wheel Base DD1"
                                                      : "Podium Wheel Base DD2";
        model_x = IDENTITY_MODEL_X;
    }
    display_framebuffer_clear(framebuffer);
    display_identity_logo_draw(framebuffer, IDENTITY_LOGO_X, IDENTITY_LOGO_Y);
    display_text_draw(framebuffer, model, model_x, IDENTITY_MODEL_Y, 1, IDENTITY_COLOR);
}
