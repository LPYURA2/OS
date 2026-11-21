#include <styleos/desktop_text_app.h>
void desk_text_app_replace(struct desk_editor_draft *draft,
                           size_t lo, size_t hi, uint32_t cp, size_t text_limit)
{
    if (!draft || hi < lo || hi > draft->length || lo > draft->length ||
        draft->length > DESK_SESSION_TEXT_BYTES) return;
    if (text_limit > DESK_SESSION_TEXT_BYTES) text_limit = DESK_SESSION_TEXT_BYTES;
    char bytes[2];
    size_t n = 0;
    if (cp && cp < 0x80) bytes[n++] = (char)cp;
    else if (cp >= 0x80 && cp < 0x800) {
        bytes[n++] = (char)(0xc0 | (cp >> 6));
        bytes[n++] = (char)(0x80 | (cp & 0x3f));
    }
    if (hi == lo && !n) return;
    if (draft->length - (hi - lo) + n > text_limit) {
        draft->notice = text_limit == DESK_SESSION_TEXT_BYTES ?
            "Лимит текста 64 КиБ" : "Лимит заметки 4096 байт";
        return;
    }
    if (lo + n > hi)
        for (size_t p = draft->length; p > hi; --p)
            draft->content[p - 1 - hi + lo + n] = draft->content[p - 1];
    else
        for (size_t p = hi; p < draft->length; ++p)
            draft->content[p - hi + lo + n] = draft->content[p];
    for (size_t i = 0; i < n; ++i) draft->content[lo + i] = bytes[i];
    draft->length = draft->length - (hi - lo) + n;
    draft->caret = lo + n;
    draft->dirty = true;
    draft->notice = "Есть несохранённые изменения";
}
