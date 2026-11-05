#pragma once
#include <stdint.h>
#include "AlarmServiceV1.h"
#define RISC_ALARM_FOREGROUND_CAPABILITY "ui.alarm-foreground"
enum { RISC_ALARM_FOREGROUND_OK=0,RISC_ALARM_FOREGROUND_AGAIN=1,
 RISC_ALARM_FOREGROUND_BUSY=-3,RISC_ALARM_FOREGROUND_INVALID=-2,
 RISC_ALARM_FOREGROUND_RETAINED=-9 };
#define RISC_ALARM_FOREGROUND_FOCUS_AVAILABLE 1u
#define RISC_ALARM_FOREGROUND_NEEDS_FOCUS 1u
#define RISC_ALARM_FOREGROUND_HAS_FOCUS 2u
#define RISC_ALARM_FOREGROUND_CUE 4u
#define RISC_ALARM_FOREGROUND_LOADING 8u
typedef struct {uint32_t struct_size,flags;alarm_status_v1 alarm;} risc_alarm_foreground_status_v1;
/* Ordinary foreground-only provider. No calls from provider poll/interrupts.
 * advance performs bounded synchronous scheduler work. Caller must first pause
 * background output and establish any required native PHY/storage service
 * lease. Never retain that lease across yield or a focus handoff.
 * FOCUS_AVAILABLE additionally promises: conflicting storage work has drained
 * and the caller's scene/text checkpoint completed full focus pause. A checked
 * synchronous radio service lease may preserve an existing RF connection;
 * this is not a Runtime checkpoint or app exit. While HAS_FOCUS,
 * only this service may service input/presentation. Resume the original owner
 * with REDRAW|FOCUS_CHANGED only when both focus flags clear.
 * CUE/LOADING consume normal app intents but need not open a modal. Errors in
 * alarm are copied retryable states; RETAINED forbids further provider I/O.
 * No app pointer, document, draft or callback is retained by this contract. */
typedef struct {
 uint32_t api_version,struct_size;void *context;
 int32_t (*advance)(void *,uint32_t,risc_alarm_foreground_status_v1 *);
} risc_alarm_foreground_v1;
