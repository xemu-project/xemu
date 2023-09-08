#ifndef DBUS_MODULE_H
#define DBUS_MODULE_H

struct QemuDBusDisplayOps {
    bool (*add_client)(int csock, Error **errp);
};

extern int using_dbus_display;
extern struct QemuDBusDisplayOps qemu_dbus_display;

#endif /* DBUS_MODULE_H */
