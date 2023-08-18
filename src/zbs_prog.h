#ifndef ZBS_PROG_H
#define ZBS_PROG_H

#include <stddef.h>

#define USB_SHELL_MAX_CMD_LINE_SIZE     0x100
#define USB_SHELL_MAC_CMD_ARGS          0x10

extern void cdc_shell_write(const void *buf, size_t count);

void zbs_prog_init();
void zbs_prog_process_input(const void *buf, size_t count);

#endif /* ZBS_PROG_H */
