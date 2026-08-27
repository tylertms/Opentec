#include <xc.h>

#include "board/identity.h"
#include "platform/board_identity.h"
#include "platform/clock.h"
#include "platform/pin_mux.h"
#include "platform/time.h"

#pragma config GWRP = OFF
#pragma config GSS = OFF
#pragma config GSSK = OFF
#pragma config FNOSC = FRC
#pragma config IESO = OFF
#pragma config POSCMD = HS
#pragma config OSCIOFNC = OFF
#pragma config IOL1WAY = OFF
#pragma config FCKSM = CSECMD
#pragma config WDTPOST = PS32768
#pragma config WDTPRE = PR128
#pragma config PLLKEN = ON
#pragma config WINDIS = OFF
#pragma config FWDTEN = OFF
#pragma config FPWRT = PWR16
#pragma config BOREN = ON
#pragma config ALTI2C1 = OFF
#pragma config ALTI2C2 = OFF
#pragma config ICS = PGD2
#pragma config RSTPRI = PF
#pragma config JTAGEN = OFF

static BoardIdentity board_identity;

int main(void) {
    platform_clock_init();
    board_identity = platform_board_identity_read();
    platform_pin_mux_init();
    platform_time_init();
    for (;;) {
    }
}
