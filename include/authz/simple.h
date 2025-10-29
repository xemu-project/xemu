/*
 * QEMU simple authorization driver
 *
 * Copyright (c) 2018 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef QAUTHZ_SIMPLE_H
#define QAUTHZ_SIMPLE_H

#include "authz/base.h"
#include "qom/object.h"

#define TYPE_QAUTHZ_SIMPLE "authz-simple"

OBJECT_DECLARE_SIMPLE_TYPE(QAuthZSimple,
                           QAUTHZ_SIMPLE)



/**
 * QAuthZSimple:
 *
 * This authorization driver provides a simple mechanism
 * for granting access based on an exact matched username.
 *
 * To create an instance of this class via QMP:
 *
 *  {
 *    "execute": "object-add",
 *    "arguments": {
 *      "qom-type": "authz-simple",
 *      "id": "authz0",
 *      "props": {
 *        "identity": "fred"
 *      }
 *    }
 *  }
 *
 * Or via the command line
 *
 *   -object authz-simple,id=authz0,identity=fred
 *
 */
struct QAuthZSimple {
    QAuthZ parent_obj;

    char *identity;
};




QAuthZSimple *qauthz_simple_new(const char *id,
                                const char *identity,
                                Error **errp);


#endif /* QAUTHZ_SIMPLE_H */
