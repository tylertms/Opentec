#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

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

static void test_render_bite_point(void) {
    uint8_t first[DISPLAY_FRAMEBUFFER_SIZE] = {0};
    uint8_t second[DISPLAY_FRAMEBUFFER_SIZE] = {0};
    uint8_t third[DISPLAY_FRAMEBUFFER_SIZE] = {0};

    display_prompt_render_bite_point(first, true, 0);
    display_prompt_render_bite_point(second, true, 73);
    display_prompt_render_bite_point(third, true, 100);
    assert(memcmp(first, second, sizeof(first)) != 0);
    assert(memcmp(second, third, sizeof(second)) != 0);

    display_prompt_render_bite_point(third, false, 100);
    for (uint16_t index = 0; index < DISPLAY_FRAMEBUFFER_SIZE; index++) {
        assert(third[index] == 0);
    }
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
    test_render_bite_point();
    test_acknowledge_on_release();
    test_hide_clears_input();
    return 0;
}
