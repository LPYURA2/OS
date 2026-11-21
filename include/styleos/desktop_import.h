#ifndef STYLEOS_DESKTOP_IMPORT_H
#define STYLEOS_DESKTOP_IMPORT_H

#include <stdbool.h>
#include <stdint.h>

struct desk_fs;

bool desk_import_available(void);
bool desk_import_active(void);
void desk_import_begin(struct desk_fs *);
void desk_import_cancel(struct desk_fs *);
bool desk_import_poll(struct desk_fs *);

/* COM2 only; COM1 remains the diagnostic stream. */
bool desk_com2_init(void);
int desk_com2_read(void);
bool desk_com2_write(uint8_t);
bool desk_pointer_idle(uint64_t earlier, uint64_t now, unsigned seconds);

#endif
