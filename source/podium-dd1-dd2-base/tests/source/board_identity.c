#include <assert.h>

#include "board/identity.h"

int main(void) {
    BoardIdentity dd2 = board_identity_decode(0);
    assert(dd2.variant == BOARD_VARIANT_DD2);
    assert(dd2.hardware_option == 0);
    assert(dd2.mode_bits == 0);

    BoardIdentity dd1 = board_identity_decode(0x1f);
    assert(dd1.variant == BOARD_VARIANT_DD1);
    assert(dd1.hardware_option == 1);
    assert(dd1.mode_bits == 0x1f);

    BoardIdentity masked = board_identity_decode(0xe2);
    assert(masked.variant == BOARD_VARIANT_DD2);
    assert(masked.hardware_option == 1);
    assert(masked.mode_bits == 2);
    return 0;
}
