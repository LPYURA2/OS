/* Two separately packaged CPL3 acceptance clients. Executable bytes exist
 * only on the explicitly provisioned data disk. No call takes a peer PID. */
#include <styleos/deep_drop_abi.h>
.intel_syntax noprefix
.text
.globl dd_start
.equ DATA, 0x12000000
.equ EVENT, DATA+128
.equ TEXT, DATA+1024
/* r12 = current transfer id, r14d = private selection/document length. */
dd_start:
    xor r12d,r12d
    xor r14d,r14d
    mov dword ptr [DATA+8],160
    mov dword ptr [DATA+12],120
#ifdef DD_SOURCE
    lea rsi,[rip+selection]
    mov edi,TEXT
    mov ecx,OFFSET selection_length
    mov r14d,ecx
    rep movsb
    mov eax,0x2579a1
#else
    mov eax,0x976332
#endif
    call paint
poll:
    mov eax,0x102
    xor ebx,ebx
    mov ecx,EVENT
    mov edx,64
    int 0x80
    test eax,eax
    jnz poll
    cmp dword ptr [EVENT+12],DD_EVENT_KIND
    je drop_event
    cmp dword ptr [EVENT+12],1
    jne poll
    mov eax,[EVENT+16]
    cmp eax,0x10                         /* Q: normal exit */
    je quit
    cmp eax,0x2d                         /* X: deliberate client #UD */
    je fault
    cmp eax,0x2f                         /* V: render actual private bytes */
    je show
    cmp eax,1                            /* Escape: cancel */
    je cancel
#ifdef DD_SOURCE
    cmp eax,0x32                         /* M: full one-KiB selection */
    je full_selection
    cmp eax,0x22                         /* G: try an unsolicited offer */
    je unsolicited
    mov dl,'a'
    cmp eax,0x1e
    je append
    mov dl,'b'
    cmp eax,0x30
    jne poll
append:
    cmp r14d,DD_TEXT_LIMIT
    jae poll
    mov [r14+TEXT],dl
    inc r14d
    jmp poll
full_selection:
    xor r14d,r14d
    mov dl,'A'
full_byte:
    mov [r14+TEXT],dl
    inc r14d
    inc dl
    cmp dl,'Z'+1
    jne full_next
    mov dl,'A'
full_next:
    cmp r14d,DD_TEXT_LIMIT
    jb full_byte
    jmp poll
unsolicited:
    mov ebx,1
    mov eax,DD_CALL_OFFER
    mov ecx,TEXT
    mov edx,r14d
    int 0x80
    jmp poll
drop_event:
    cmp dword ptr [EVENT+16],DD_REQUEST_TEXT
    jne poll
    mov rbx,[EVENT+32]
    mov eax,DD_CALL_OFFER
    mov ecx,TEXT
    mov edx,r14d
    int 0x80
    test eax,eax
    jnz poll
    mov r12,rdx
    /* The source cannot accept on behalf of the target. */
    mov eax,DD_CALL_ACCEPT
    call control
    cmp eax,2
    jne fault
    jmp poll
selection:
    .ascii "StyleOS / "
    .byte 0xd0,0xa7,0xd0,0xb0,0xd0,0xb9,0x20
.set selection_length,.-selection
#else
    cmp eax,0x1c                         /* Enter: insert once */
    je accept
    cmp eax,0x13                         /* R: retry old delivery */
    jne poll
replay:
    mov eax,DD_CALL_READ
    mov rbx,r12
    mov ecx,TEXT
    mov edx,DD_TEXT_LIMIT
    int 0x80
    jmp poll
drop_event:
    cmp dword ptr [EVENT+16],DD_TEXT_OFFER
    jne poll
    mov r12,[EVENT+32]
    jmp poll
accept:
    mov eax,DD_CALL_ACCEPT
    call control
    test eax,eax
    jnz poll
    /* A crossing of the private page and a short destination must leave
     * the approved transfer available for a correct retry. */
    mov eax,DD_CALL_READ
    mov ecx,DATA+4094
    mov edx,8
    int 0x80
    cmp eax,1
    jne fault
    mov eax,DD_CALL_READ
    lea ecx,[r14+TEXT]
    mov edx,1
    int 0x80
    cmp eax,5
    jne fault
    mov eax,DD_CALL_READ
    lea ecx,[r14+TEXT]
    mov edx,DD_TEXT_LIMIT
    sub edx,r14d
    jz cancel
    int 0x80
    test eax,eax
    jnz cancel
    add r14d,edx                         /* append to this process's document */
    mov eax,DD_CALL_FINISH
    call control
    test eax,eax
    jnz fault
    call show_text
    jmp replay
#endif
show:
    call show_text
    jmp poll
cancel:
    mov eax,DD_CALL_CANCEL
    call control
    jmp poll
control:
    mov rbx,r12
    xor ecx,ecx
    xor edx,edx
    int 0x80
    ret
show_text:
    mov eax,2166136261
    mov esi,TEXT
    mov ecx,r14d
    test ecx,ecx
    jz paint_hash
hash_byte:
    movzx edx,byte ptr [rsi]
    xor eax,edx
    imul eax,eax,16777619
    inc rsi
    dec ecx
    jnz hash_byte
paint_hash:
    and eax,0xffffff
paint:
    mov [DATA+16],eax
    mov eax,0x103
    xor ebx,ebx
    mov ecx,DATA
    mov edx,1
    int 0x80
    test eax,eax
    jnz fault
    ret
quit:
    mov eax,2
    int 0x80
fault:
    ud2
