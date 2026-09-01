#ifndef OPENTEC_BASE_PLATFORM_BOARD_IDENTITY_H
#define OPENTEC_BASE_PLATFORM_BOARD_IDENTITY_H

#include "board/identity.h"

/**
 * @brief Reads and decodes the board identity straps.
 *
 * Samples the board-variant input pins and returns the decoded identity and option bits.
 *
 * @return Decoded board identity.
 */
BoardIdentity platform_board_identity_read(void);

#endif
