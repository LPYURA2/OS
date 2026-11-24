#include <styleos/monotonic_855.h>
/* The frequency bound makes remainder * NS_PER_SECOND fit in uint64_t.
 * Convert from the original epoch, not rounded increments: no drift from
 * repeated sampling and no 128-bit division runtime dependency. */
_Static_assert((S855_MAX_HZ-1)<=UINT64_MAX/S855_NS_PER_SECOND,"fraction safe");
static bool convert(uint64_t hz,uint64_t delta,uint64_t *ns)
{
    if (!hz || hz>S855_MAX_HZ) return false;
    uint64_t sec=delta/hz, rem=delta%hz;
    uint64_t fraction=(rem*S855_NS_PER_SECOND)/hz;
    if (sec>UINT64_MAX/S855_NS_PER_SECOND) return false;
    uint64_t whole=sec*S855_NS_PER_SECOND;
    if (fraction>UINT64_MAX-whole) return false;
    *ns=whole+fraction;return true;
}
bool monotonic_init_855(struct monotonic_855 *s,uint64_t hz,uint64_t counter)
{
    if (!s || s->ready || s->faulted || s->hz || s->origin || s->last_counter || s->ns || !hz || hz>S855_MAX_HZ) return false;
    *s=(struct monotonic_855){.hz=hz,.origin=counter,.last_counter=counter,.ready=true};
    return true;
}
bool monotonic_sample_855(struct monotonic_855 *s,uint64_t counter)
{
    if (!s || !s->ready || s->faulted) return false;
    uint64_t previous,next;
    if (s->last_counter<s->origin || counter<s->last_counter ||
        !convert(s->hz,s->last_counter-s->origin,&previous) || previous!=s->ns ||
        !convert(s->hz,counter-s->origin,&next) || next<previous) {
        s->faulted=true;return false;
    }
    s->last_counter=counter;s->ns=next;return true;
}
