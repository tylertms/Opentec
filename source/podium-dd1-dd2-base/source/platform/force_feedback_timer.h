#ifndef OPENTEC_BASE_PLATFORM_FORCE_FEEDBACK_TIMER_H
#define OPENTEC_BASE_PLATFORM_FORCE_FEEDBACK_TIMER_H

/**
 * @brief Callback invoked by the force-feedback timer.
 *
 * Receives the context supplied during timer initialization.
 *
 * @param[in,out] context Caller-owned state passed to the callback.
 */
typedef void (*PlatformForceFeedbackTickHandler)(void *context);

/**
 * @brief Initializes and starts the force-feedback runtime timer.
 *
 * Starts the periodic Timer 2 cadence and invokes handler on each timer interrupt. The application
 * starts this timer only after selecting an operating mode that requires force-feedback scripting.
 *
 * @param[in] handler Callback to invoke for each timer tick.
 * @param[in,out] context State passed unchanged to handler.
 */
void platform_force_feedback_timer_init(PlatformForceFeedbackTickHandler handler, void *context);

#endif
