#pragma once
#include <stddef.h>
/* Application-owned, deployment-linked menu. This is not a runtime ABI,
 * installed-package discovery, or permission policy. Boot grants stay separate. */
#define PAPER_SPACE_MAX_ITEMS 8u
#define PAPER_SPACE_LABEL_BYTES 49u
#define PAPER_SPACE_PATH_BYTES 193u
typedef struct {
    char label[PAPER_SPACE_LABEL_BYTES];
    char path[PAPER_SPACE_PATH_BYTES];
} paper_space_item;
extern const paper_space_item paper_space_items[];
extern const unsigned paper_space_item_count;
