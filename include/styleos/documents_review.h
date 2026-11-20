#ifndef STYLEOS_DOCUMENTS_REVIEW_H
#define STYLEOS_DOCUMENTS_REVIEW_H

#include <stddef.h>
#include <stdint.h>

/* Independent, user-mode text review engine. No dependency on the shell. */
#define SREV_MAX_BYTES 8192u
#define SREV_MAX_PIECES 192u
#define SREV_MAX_ISSUES 128u

typedef enum {
    SREV_OK = 0, SREV_INVALID = -1, SREV_RANGE = -2,
    SREV_LIMIT = -3, SREV_UTF8 = -4, SREV_NOT_FOUND = -5,
    SREV_NO_MEMORY = -6
} srev_status;

typedef enum { SREV_NORMAL = 0, SREV_INSERTED = 1, SREV_DELETED = 2 } srev_mark;

typedef struct {
    char *bytes; size_t len; uint32_t change_id; srev_mark mark;
} srev_piece;

typedef struct {
    srev_piece pieces[SREV_MAX_PIECES];
    size_t count;
    uint32_t next_id;
} srev_document;

void srev_destroy(srev_document *doc);
srev_status srev_init(srev_document *doc, const char *utf8);
size_t srev_visible_size(const srev_document *doc);
size_t srev_original_size(const srev_document *doc);
srev_status srev_insert(srev_document *doc, size_t visible_byte_offset,
                        const char *utf8, uint32_t *change_id);
srev_status srev_delete(srev_document *doc, size_t visible_byte_offset,
                        size_t utf8_bytes, uint32_t *change_id);
srev_status srev_resolve(srev_document *doc, uint32_t change_id, int accept);
/* Resolve every pending change, preserving chronological accepted/rejected semantics. */
srev_status srev_resolve_all(srev_document *doc, int accept);
/* One span per marked piece; several pieces may share the same change_id.
   Deleted text has zero visible length and is anchored at visible_byte_offset.
   Inserted text has zero original length and is anchored at original_byte_offset. */
typedef struct {
    uint32_t change_id;
    srev_mark mark;
    size_t visible_byte_offset;
    size_t original_byte_offset;
    size_t bytes;
} srev_change_span;
srev_status srev_collect_changes(const srev_document *doc,
                                 srev_change_span *spans, size_t capacity,
                                 size_t *count);
srev_status srev_render(const srev_document *doc, int original,
                        char *output, size_t output_capacity);
/* Private interchange, NOT the StyleOS SOD6 or DOCX format. */
srev_status srev_pack(const srev_document *doc, unsigned char *out,
                      size_t cap, size_t *written);
srev_status srev_unpack(srev_document *doc, const unsigned char *in,
                        size_t length);

typedef enum {
    SREV_GRAMMAR_DOUBLE_SPACE = 1,
    SREV_GRAMMAR_SPACE_BEFORE_PUNCTUATION = 2,
    SREV_GRAMMAR_NO_SPACE_AFTER_COMMA = 3,
    SREV_GRAMMAR_DUPLICATE_WORD = 4
} srev_grammar_rule;
typedef enum { SREV_RU = 1, SREV_EN = 2, SREV_BOTH = 3 } srev_language;
typedef struct {
    srev_grammar_rule rule;
    size_t start_byte;
    size_t end_byte;
} srev_grammar_issue;
/* Advisory hints only: never automatically edit a document. */
srev_status srev_grammar_scan(const char *utf8, srev_language language,
                              srev_grammar_issue *issues, size_t issue_cap,
                              size_t *issue_count);
#endif
