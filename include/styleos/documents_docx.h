#ifndef STYLEOS_DOCUMENTS_DOCX_H
#define STYLEOS_DOCUMENTS_DOCX_H
#include <stddef.h>
#include <stdint.h>
#include "styleos/documents_model.h"

/* Recovery-only library. Generates a standard DOCX ZIP_STORED archive using
 * caller-provided output storage. No filesystem or StyleOS UI binding yet.
 * Text and page properties are preserved; complex objects are not supported. */
#define SDM_DOCX_MAX_OUTPUT 458752u
int sdm_docx_export(const sdm_document *doc, uint8_t *out, size_t capacity, size_t *written);
#endif
