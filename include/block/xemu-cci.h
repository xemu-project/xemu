/*
 * xemu CCI multi-part disc (UI / block glue)
 */
#ifndef BLOCK_XEMU_CCI_H
#define BLOCK_XEMU_CCI_H

typedef struct Error Error;

void xemu_cci_blockdev_change_dvd_medium(const char **paths, int n, Error **errp);

#endif /* BLOCK_XEMU_CCI_H */
