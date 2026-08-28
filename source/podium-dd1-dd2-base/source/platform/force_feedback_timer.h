#ifndef OPENTEC_BASE_PLATFORM_FORCE_FEEDBACK_TIMER_H
#define OPENTEC_BASE_PLATFORM_FORCE_FEEDBACK_TIMER_H

typedef void (*PlatformForceFeedbackTickHandler)(void *context);

void platform_force_feedback_timer_init(PlatformForceFeedbackTickHandler handler, void *context);

#endif
