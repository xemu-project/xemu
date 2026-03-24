/*
 * xemu CCI disc (UI / block glue)
 */
#ifndef QAPI_SYSTEM_H
#define QAPI_SYSTEM_H

typedef struct Error Error;

void xemu_cci_blockdev_change_dvd_medium(const char *path, Error **errp);

#endif /* QAPI_SYSTEM_H */