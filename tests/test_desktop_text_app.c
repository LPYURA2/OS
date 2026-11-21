#include <styleos/desktop_text_app.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
static struct desk_editor_draft editor, notes;
int main(void)
{
    desk_text_app_replace(&editor,0,0,'A',DESK_SESSION_TEXT_BYTES);
    assert(editor.length==1 && editor.dirty && editor.content[0]=='A');
    desk_text_app_replace(&notes,0,0,0x416,4096);
    assert(notes.length==2 && notes.caret==2 && editor.content[0]=='A');
    desk_text_app_replace(&editor,1,1,'B',DESK_SESSION_TEXT_BYTES);
    assert(editor.length==2 && editor.content[1]=='B' && notes.length==2);
    desk_text_app_replace(&editor,0,2,'C',DESK_SESSION_TEXT_BYTES);
    assert(editor.length==1 && editor.content[0]=='C' && editor.caret==1);
    unsigned char before=editor.content[0];
    desk_text_app_replace(&editor,99,99,'X',DESK_SESSION_TEXT_BYTES);
    assert(editor.length==1 && (unsigned char)editor.content[0]==before);
    notes.length=4096; notes.caret=4096;
    for(size_t i=0;i<4096;i++)notes.content[i]='x';
    desk_text_app_replace(&notes,4096,4096,'X',4096);
    assert(notes.length==4096 && notes.caret==4096 &&
           strcmp(notes.notice,"Лимит заметки 4096 байт")==0);
    puts("TEXT APP CORE HOST PASS: independent drafts, UTF8, limits, invalid range");
    return 0;
}
