#pragma once
#include <stdbool.h>
bool portable_update_services_safe(void);
bool portable_update_suspend(void);
void portable_update_resume(void);
bool portable_update_close(void);
