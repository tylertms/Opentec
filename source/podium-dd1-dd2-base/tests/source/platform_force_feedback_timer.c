#include <assert.h>
#include <stdint.h>
#include <xc.h>

#include "platform/force_feedback_timer.h"

static void test_initializes_timer_registers(void) {
    T2CON = 0;
    TMR2 = 0x1234;
    PR2 = 0;
    IPC1bits.T2IP = 0;
    IFS0bits.T2IF = 1;
    IEC0bits.T2IE = 0;

    platform_force_feedback_timer_init(0, 0);

    assert(PR2 == 0x16a8);
    assert(IPC1bits.T2IP == 4);
    assert(IFS0bits.T2IF == 0);
    assert(IEC0bits.T2IE == 1);
}

int main(void) {
    test_initializes_timer_registers();
    return 0;
}
