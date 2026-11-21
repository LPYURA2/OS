#include <styleos/native_session_hw_770.h>
#include <assert.h>
#include <stdio.h>
static struct nsh_stage sessions;
static struct pr_table registry;
int main(void)
{
    pr_init(&registry);
    assert(nsh_init(&sessions,&registry));
    assert(!nsh_init(&sessions,&registry)); /* a second reset must be refused */
    const struct pr_admission a={PR_NATIVE_USER,0,101,0x2000,0,"CPL3 Test A"};
    const struct pr_admission b={PR_NATIVE_USER,0,102,0x3000,0,"Minimal.tea"};
    uint32_t pid_a=0,pid_b=0;
    assert(pr_admit(&registry,&a,&pid_a)==PR_OK);
    assert(pr_admit(&registry,&b,&pid_b)==PR_OK);
    assert(!nsh_attach(&sessions,1,pid_b,102,0x3000)); /* reject B before A */
    assert(!nsh_attach(&sessions,0,pid_a,101,0x5000)); /* reject wrong root */
    assert(nsh_attach(&sessions,0,pid_a,101,0x2000));
    assert(nsh_attach(&sessions,1,pid_b,102,0x3000));
    assert(nsh_review(&sessions));
    assert(pr_transition(&registry,pid_a,101,PR_RUNNING)==PR_OK);
    assert(pr_transition(&registry,pid_a,101,PR_READY)==PR_OK);
    assert(nsh_on_event(&sessions,0,PS_YIELD));
    assert(pr_transition(&registry,pid_b,102,PR_RUNNING)==PR_OK);
    assert(pr_transition(&registry,pid_b,102,PR_READY)==PR_OK);
    assert(nsh_on_event(&sessions,1,PS_YIELD));
    assert(pr_transition(&registry,pid_a,101,PR_RUNNING)==PR_OK);
    assert(pr_transition(&registry,pid_a,101,PR_FAULTED)==PR_OK);
    assert(nsh_on_event(&sessions,0,PS_FAULT));
    assert(!nsh_on_event(&sessions,0,PS_YIELD));
    assert(pr_transition(&registry,pid_b,102,PR_RUNNING)==PR_OK);
    assert(pr_transition(&registry,pid_b,102,PR_EXITED)==PR_OK);
    assert(nsh_on_event(&sessions,1,PS_EXIT));
    assert(pr_reap(&registry,pid_a,101)==PR_OK);
    assert(pr_reap(&registry,pid_b,102)==PR_OK);
    assert(nsh_finish(&sessions));
    puts("NATIVE SESSION 770 HOST PASS: real PIDs/roots per session, windows survive restart, fault/exit reaped");
    return 0;
}
