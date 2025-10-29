/*
 * QEMU Object Model - QObject wrappers
 *
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * Author: Paolo Bonzini <pbonzini@redhat.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */

#ifndef QEMU_QOM_QOBJECT_H
#define QEMU_QOM_QOBJECT_H

/*
 * object_property_get_qobject:
 * @obj: the object
 * @name: the name of the property
 * @errp: returns an error if this function fails
 *
 * Returns: the value of the property, converted to QObject, or NULL if
 * an error occurs.
 */
struct QObject *object_property_get_qobject(Object *obj, const char *name,
                                            struct Error **errp);

/**
 * object_property_set_qobject:
 * @obj: the object
 * @name: the name of the property
 * @value: The value that will be written to the property.
 * @errp: returns an error if this function fails
 *
 * Writes a property to a object.
 *
 * Returns: %true on success, %false on failure.
 */
bool object_property_set_qobject(Object *obj,
                                 const char *name, struct QObject *value,
                                 struct Error **errp);

#endif
