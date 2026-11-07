#pragma once
#include <stdbool.h>
#include "cJSON.h"
/* Exact Python canonical JSON for Unicode strings and safe integers only.
 * Caller owns returned allocation. Floating-point values are rejected. */
char *bm_canonical(const cJSON *value);
bool bm_json_integers_only(const cJSON *value);
bool bm_sign_request(cJSON *envelope,const char *private_pem,const char *provider);

char *bm_sign_value(const cJSON *value,const char *private_pem);
