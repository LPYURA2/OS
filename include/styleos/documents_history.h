#ifndef STYLEOS_DOCUMENTS_HISTORY_H
#define STYLEOS_DOCUMENTS_HISTORY_H
#include "styleos/documents_model.h"
/* Intentionally small, bounded full-state history: comments and page settings
 * move together with text. Preview: four undo and four redo steps per window.
 * Avoids silently losing comment anchors or restoring them to other text. */
#define SDH_STEPS 4u
typedef struct {
 sdm_document undo[SDH_STEPS];
 sdm_document redo[SDH_STEPS];
 sdm_document pending;
 unsigned undo_count,redo_count;
 int pending_valid;
} sdh_history;
void sdh_clear(sdh_history *history);
void sdh_begin(sdh_history *history,const sdm_document *document);
void sdh_commit(sdh_history *history);
int sdh_undo(sdh_history *history,sdm_document *document);
int sdh_redo(sdh_history *history,sdm_document *document);
#endif
