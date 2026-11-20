#ifndef STYLEOS_DOCUMENTS_MODEL_H
#define STYLEOS_DOCUMENTS_MODEL_H

#include <stddef.h>
#include <stdint.h>

/* Recovery implementation: bounded standalone data model, NOT installed in StyleOS 744. */
#define SDM_TEXT_CAPACITY 65536u
#define SDM_MAX_COMMENTS 32u
#define SDM_COMMENT_CAPACITY 256u

typedef enum {
    SDM_OK = 0,
    SDM_ARGUMENT = -1,
    SDM_RANGE = -2,
    SDM_CAPACITY = -3,
    SDM_ENCODING = -4,
    SDM_NOT_FOUND = -5
} sdm_status;

typedef enum { SDM_A4, SDM_LETTER } sdm_paper;
typedef enum { SDM_PORTRAIT, SDM_LANDSCAPE } sdm_orientation;

typedef struct {
    sdm_paper paper;
    sdm_orientation orientation;
    uint16_t top_twips;
    uint16_t bottom_twips;
    uint16_t left_twips;
    uint16_t right_twips;
} sdm_page;

typedef struct {
    uint32_t id;
    size_t from;
    size_t to;
    char body[SDM_COMMENT_CAPACITY];
    int resolved;
    int active;
} sdm_comment;

typedef struct {
    char content[SDM_TEXT_CAPACITY];
    size_t bytes;
    size_t cursor;
    size_t anchor;
    sdm_page page;
    sdm_comment comments[SDM_MAX_COMMENTS];
    uint32_t next_id;
    int changed;
} sdm_document;

/* All offsets are UTF-8 byte offsets at code-point boundaries. */
void sdm_init(sdm_document *doc);
int sdm_set_text(sdm_document *doc, const char *text, size_t length);
int sdm_select(sdm_document *doc, size_t anchor, size_t cursor);
int sdm_insert(sdm_document *doc, const char *text, size_t length);
int sdm_remove(sdm_document *doc);
int sdm_set_page(sdm_document *doc, sdm_page page);
int sdm_add_comment(sdm_document *doc, const char *text, size_t length,
                    uint32_t *id_out);
int sdm_resolve_comment(sdm_document *doc, uint32_t id, int resolved);
int sdm_selected_text(const sdm_document *doc, char *result, size_t capacity,
                      size_t *length_out);
int sdm_valid_utf8(const char *text, size_t len);

#endif
