#ifndef OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_SERVICE_H
#define OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_SERVICE_H

#include "force_feedback/script_executor.h"
#include "force_feedback/script_store.h"

void force_feedback_script_service_run(ForceFeedbackScriptRuntime *runtime,
                                       const ForceFeedbackScriptStore *store,
                                       ForceFeedbackScriptClock *clock);

#endif
