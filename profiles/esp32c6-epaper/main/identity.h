#pragma once
#include <stdbool.h>
typedef enum { IDENTITY_INVALID, IDENTITY_ENROLL, IDENTITY_JOURNAL, IDENTITY_RESTORE } identity_action;
bool identity_valid(const char *id);
identity_action identity_resolve(const char *journal_id, const char *provisioned_id);
