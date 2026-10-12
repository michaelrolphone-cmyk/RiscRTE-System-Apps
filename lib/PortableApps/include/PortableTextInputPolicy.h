#pragma once
#include "PortableTextInputCheckpointClient.h"
/* Optional linked-adapter policy bridge. prepare returns 1 only for a settled
 * policy boundary, 0 when there is no work, and a negative retained status.
 * continuity is a one-dispatch promise that apply cannot enter native sleep.
 * The caller pauses the shared host before apply and resumes it afterwards. */
int portable_text_adapter_policy_prepare(uint32_t checkpoint_flags,bool *continuity);
int portable_text_adapter_policy_apply(bool continuity,uint32_t *resume_flags);

/* Capture-only progression is safe through submitted/mutable host frames. */
int portable_text_adapter_policy_capture(void);
