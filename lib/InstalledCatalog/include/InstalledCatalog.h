#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define INSTALLED_CATALOG_MAX 64u
#define INSTALLED_CATALOG_CACHE_MAX 49152u
enum { INSTALLED_APP=1, INSTALLED_DRIVER=2 };
enum { INSTALLED_INTERNAL=1, INSTALLED_SD=2 };
typedef struct {
  char id[96], version[64], path[193], name[96], icon[24];
  uint32_t installation_revision;
  uint8_t kind, medium;
  bool admitted; /* session authority: NEVER restored from the UI cache */
} installed_record;
typedef struct {
  installed_record current[INSTALLED_CATALOG_MAX], pending[INSTALLED_CATALOG_MAX];
  uint32_t generation, count, pending_count;
  uint8_t scanning_medium;
  bool scanning, failed, sd_ready;
} installed_catalog;
typedef struct { uint32_t generation; char id[96]; } installed_selection;
/* Caller owns allocation (PSRAM on target) and serialized access. No method
 * does filesystem I/O, calls a provider, hashes an ELF or executes a package. */
void installed_catalog_init(installed_catalog*);
bool installed_catalog_begin_all(installed_catalog*);
bool installed_catalog_begin(installed_catalog*,uint8_t medium);
bool installed_catalog_offer(installed_catalog*,const installed_record*);
/* Publish only after complete enumeration, successful admission and checked
 * handle cleanup. false abandons the entire candidate, preserving the cache. */
bool installed_catalog_finish(installed_catalog*,bool complete);
void installed_catalog_media(installed_catalog*,bool ready);
bool installed_catalog_select(const installed_catalog*,uint32_t,installed_selection*);
/* A queued touch must be rejected after any catalog generation changes. */
bool installed_catalog_resolve(const installed_catalog*,const installed_selection*,installed_record*);
/* Metadata-only versioned codec, bound to the exact owner policy/cohort key.
 * Persistence adapter must atomically replace and verify the complete file.
 * Decode is transactional. Cached rows remain unadmitted until native rehydration. */
size_t installed_catalog_encode(const installed_catalog*,const char* policy_key,void*,size_t);
bool installed_catalog_decode(installed_catalog*,const char* policy_key,const void*,size_t);
#ifdef __cplusplus
}
#endif
