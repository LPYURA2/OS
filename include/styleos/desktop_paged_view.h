#ifndef STYLEOS_DESKTOP_PAGED_VIEW_H
#define STYLEOS_DESKTOP_PAGED_VIEW_H
#include <styleos/stylefs_paged_object64.h>
#include <styleos/desktop_v0.h>
/* Trusted desktop-only read-only cached view; no disk grants or user ABI.
 * Bounded first page, explicit truncation, no filesystem I/O in renderer. */
enum { DP41_ROWS=16 };
struct dp41_view {struct po64_row rows[DP41_ROWS];unsigned count,selected;bool more,valid;uint64_t parent,first;};
const struct dp41_view *s41_root_view(void);
/* Trusted shell dispatch: arrows select, Enter opens folder, Backspace
 * goes up; PageUp/PageDown fetch bounded pages. No disk I/O in draw. */
bool s42_navigate(unsigned key);
bool s42_select(unsigned row,bool open);
bool desk_fs_paged_click(const struct dp41_view *,struct desk_box,int x,int y);
void desk_fs_paged_root_draw(const struct dp41_view *,struct desk_box);
/* Trusted read-only text handoff. The receiver's buffer is provisional
 * until CRC, exact byte count and text validation have all succeeded. */
bool s43_text_supported(const struct po64_row *);
bool s43_read_text(const struct po64_row *,char *,size_t,size_t *);
bool s43_open_editor(const struct po64_row *);
#endif
