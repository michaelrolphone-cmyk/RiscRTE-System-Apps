#pragma once
#include "RiscResidentShellV1.h"
/* Reason-specific, backward-compatible POLL reply contract. Only the pair
 * POLICY_REQUEST | INPUT_CONTINUITY promises that the following POLICY will
 * neither borrow focus nor enter sleep. The native owner must latch that
 * promise through time/preference changes and consume it once. Existing
 * clients ignore this advisory bit and still detach fully. Absent the pair,
 * a new client must release all input custody before calling POLICY.
 * CONFIGURATION_CHANGED retains its original meaning on other replies. */
#define PORTABLE_RESIDENT_REPLY_INPUT_CONTINUITY RISC_RESIDENT_REPLY_CONFIGURATION_CHANGED
