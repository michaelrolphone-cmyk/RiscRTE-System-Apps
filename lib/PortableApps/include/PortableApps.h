#pragma once
#include "T5AppApi.h"
/* Deployment-owned bounded catalog, linked into the application. No discovery,
 * filesystem access or installation claims. Entries are admitted by boot
 * policy. */
/* Historical deployments retain the 17-entry limit. Only the SDR Springboard
 * opts into 18; fail closed rather than silently growing other app catalogs. */
#ifndef PORTABLE_CATALOG_LIMIT
#define PORTABLE_CATALOG_LIMIT 17
#endif
#if PORTABLE_CATALOG_LIMIT < 17 || PORTABLE_CATALOG_LIMIT > 18
#error "PORTABLE_CATALOG_LIMIT must be 17 or 18"
#endif
extern const t5_app_manifest_t portable_catalog[];
extern const unsigned portable_catalog_count;
