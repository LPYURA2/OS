#ifndef STYLEOS_DOCUMENTS_IMPORT_H
#define STYLEOS_DOCUMENTS_IMPORT_H
#include <stddef.h>
#include <stdint.h>
#include "styleos/documents_model.h"
/* Restricted, dependency-free DOCX importer for the uncompressed ZIP flavor
 * emitted by sdm_docx_export. External compressed DOCX is UNSUPPORTED.
 * Never call with output overlapping input; output is left initialized or
 * partially populated on failure. Caller stages before committing. */
int sdm_docx_import_stored(const uint8_t *zip, size_t bytes, sdm_document *out);
#endif
