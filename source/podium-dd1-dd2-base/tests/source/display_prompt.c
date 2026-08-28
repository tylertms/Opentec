#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "display/framebuffer.h"
#include "display/prompt.h"

static void test_render_and_clear(void) {
    uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE] = {0};
    uint16_t lit = 0;

    display_prompt_render(framebuffer, true);
    for (uint16_t index = 0; index < DISPLAY_FRAMEBUFFER_SIZE; index++) {
        if (framebuffer[index] != 0) {
            lit++;
        }
    }
    assert(lit > 100);

    display_prompt_render(framebuffer, false);
    for (uint16_t index = 0; index < DISPLAY_FRAMEBUFFER_SIZE; index++) {
        assert(framebuffer[index] == 0);
    }
}

static void test_acknowledge_on_release(void) {
    DisplayPrompt prompt = {0};

    assert(!display_prompt_update(&prompt, true, false));
    assert(!display_prompt_update(&prompt, true, true));
    assert(prompt.input_seen);
    assert(!display_prompt_update(&prompt, true, true));
    assert(display_prompt_update(&prompt, true, false));
    assert(!prompt.input_seen);
    assert(!display_prompt_update(&prompt, true, false));
}

static void test_hide_clears_input(void) {
    DisplayPrompt prompt = {0};

    assert(!display_prompt_update(&prompt, true, true));
    assert(!display_prompt_update(&prompt, false, false));
    assert(!prompt.input_seen);
    assert(!display_prompt_update(&prompt, true, false));
}

int main(void) {
    test_render_and_clear();
    test_acknowledge_on_release();
    test_hide_clears_input();
    return 0;
}
