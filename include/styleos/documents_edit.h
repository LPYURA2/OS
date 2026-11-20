#ifndef STYLEOS_DOCUMENTS_EDIT_H
#define STYLEOS_DOCUMENTS_EDIT_H
#include <stddef.h>
#include "styleos/documents_model.h"
/* Case-sensitive UTF-8 exact search; found remains unchanged on no match. */
int sde_find(const sdm_document *doc,const char *needle,size_t len,
             size_t start,int wrap,size_t *found);
/* Atomic all-occurrences replacement. Needles <=256 bytes, replacement <=1024.
 * Caller must provide distinct 75 KiB staged document. No partial mutation. */
int sde_replace_all(sdm_document *doc,const char *needle,size_t needle_len,
        const char *replacement,size_t replacement_len,sdm_document *staged,
        size_t *replaced);
#endif
