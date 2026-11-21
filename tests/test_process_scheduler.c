#include <styleos/process_scheduler.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    static struct pr_table registry;
    struct ps_dispatcher s;
    pr_init(&registry);
    assert(ps_init(&s, &registry, 0) == PS_ARGUMENT);
    assert(ps_init(&s, &registry, 0x1000) == PS_OK);
    uint32_t a = 0, b = 0;
    assert(ps_register(&s, 0, 11, 0x2000, 0, "User A", &a) == PS_OK);
    assert(ps_register(&s, 0, 12, 0x3000, 0, "Duplicate", &b) == PS_STATE);
    assert(ps_register(&s, 1, 11, 0x3000, 0, "Alias", &b) == PS_EVIDENCE);
    assert(ps_register(&s, 1, 12, 0x2000, 0, "Root alias", &b) == PS_EVIDENCE);
    assert(ps_register(&s, 1, 12, 0x3000, 0, "User B", &b) == PS_OK);
    assert(a && b && a != b && pr_snapshot(&registry, 0, 0, false) == 2);
    assert(pr_find(&registry, a)->resident_bytes == 0);
    unsigned slot = 99; uint32_t pid = 99;
    assert(ps_next(&s, &slot, &pid) == PS_OK && slot == 0 && pid == a);
    assert(pr_find(&registry, a)->state == PR_RUNNING);
    assert(ps_next(&s, &slot, &pid) == PS_STATE);
    uint32_t serial = registry.change_serial;
    assert(ps_accept(&s, 0, 11, 0x2000, 0x9999, PS_YIELD) == PS_EVIDENCE);
    assert(ps_accept(&s, 0, 11, 0x3000, 0x1000, PS_YIELD) == PS_EVIDENCE);
    assert(ps_accept(&s, 0, 12, 0x2000, 0x1000, PS_YIELD) == PS_EVIDENCE);
    assert(registry.change_serial == serial && s.current == 0);
    assert(ps_accept(&s, 0, 11, 0x2000, 0x1000, PS_YIELD) == PS_OK);
    assert(pr_find(&registry, a)->state == PR_READY);
    assert(ps_next(&s, &slot, &pid) == PS_OK && slot == 1 && pid == b);
    assert(ps_accept(&s, 1, 12, 0x3000, 0x1000, PS_YIELD) == PS_OK);
    assert(ps_next(&s, &slot, &pid) == PS_OK && slot == 0 && pid == a);
    assert(ps_accept(&s, 0, 11, 0x2000, 0x1000, PS_FAULT) == PS_OK);
    assert(pr_find(&registry, a)->state == PR_FAULTED);
    assert(pr_snapshot(&registry, 0, 0, false) == 1);
    assert(ps_next(&s, &slot, &pid) == PS_OK && slot == 1 && pid == b);
    assert(ps_accept(&s, 1, 12, 0x3000, 0x1000, PS_EXIT) == PS_OK);
    assert(pr_snapshot(&registry, 0, 0, false) == 0);
    slot = 77; pid = 88;
    assert(ps_next(&s, &slot, &pid) == PS_EMPTY && slot == 77 && pid == 88);
    assert(s.dispatches == 4);
    assert(ps_reap(&s, 0, 12) == PS_EVIDENCE);
    assert(ps_reap(&s, 0, 11) == PS_OK);
    assert(ps_reap(&s, 1, 12) == PS_OK);
    assert(pr_find(&registry, a) == 0 && pr_find(&registry, b) == 0);
    assert(ps_reap(&s, 1, 12) == PS_STATE);
    uint32_t c = 0;
    assert(ps_register(&s, 0, 21, 0x5000, 0, "New", &c) == PS_OK && c > b);
    puts("PROCESS SCHEDULER HOST PASS: registry, round-robin, guarded traps, reaping");
    return 0;
}
