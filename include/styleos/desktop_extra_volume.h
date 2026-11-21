#ifndef STYLEOS_DESKTOP_EXTRA_VOLUME_H
#define STYLEOS_DESKTOP_EXTRA_VOLUME_H
#include <styleos/stylefs.h>
/* A separately owned second volume, not an Explorer window object. */
void desk_extra_volume_mount(void);
const struct sfs_volume *desk_extra_volume_readonly(void);
#endif
