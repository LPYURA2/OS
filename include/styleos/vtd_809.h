#ifndef STYLEOS_VTD_809_H
#define STYLEOS_VTD_809_H
#include <stdint.h>
#include <stdbool.h>
/* Read-only VT-d register observation. A discovered engine is NOT isolation. */
struct vtd809_regs {
 uint32_t version, gcmd, gsts, fsts;
 uint64_t cap, ecap, root_table;
};
struct vtd809_state {
 uint8_t major, minor;
 bool translation_status, translation_command, root_set_status;
};
bool vtd809_decode(const struct vtd809_regs *in,struct vtd809_state *out);
#endif
