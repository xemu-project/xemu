/*
 * QEMU I/O channels utility APIs
 *
 * Copyright (c) 2016 Red Hat, Inc.
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

#ifndef QIO_CHANNEL_UTIL_H
#define QIO_CHANNEL_UTIL_H

#include "io/channel.h"

/*
 * This module provides helper functions that are useful when dealing
 * with QIOChannel objects
 */


/**
 * qio_channel_new_fd:
 * @fd: the file descriptor
 * @errp: pointer to a NULL-initialized error object
 *
 * Create a channel for performing I/O on the file
 * descriptor @fd. The particular subclass of QIOChannel
 * that is returned will depend on what underlying object
 * the file descriptor is associated with. It may be either
 * a QIOChannelSocket or a QIOChannelFile instance. Upon
 * success, the returned QIOChannel instance will own
 * the @fd file descriptor, and take responsibility for
 * closing it when no longer required. On failure, the
 * caller is responsible for closing @fd.
 *
 * Returns: the channel object, or NULL on error
 */
QIOChannel *qio_channel_new_fd(int fd,
                               Error **errp);

/**
 * qio_channel_util_set_aio_fd_handler:
 * @read_fd: the file descriptor for the read handler
 * @read_ctx: the AioContext for the read handler
 * @io_read: the read handler
 * @write_fd: the file descriptor for the write handler
 * @write_ctx: the AioContext for the write handler
 * @io_write: the write handler
 * @opaque: the opaque argument to the read and write handler
 *
 * Set the read and write handlers when @read_ctx and @write_ctx are non-NULL,
 * respectively. To leave a handler in its current state, pass a NULL
 * AioContext. To clear a handler, pass a non-NULL AioContext and a NULL
 * handler.
 */
void qio_channel_util_set_aio_fd_handler(int read_fd,
                                         AioContext *read_ctx,
                                         IOHandler *io_read,
                                         int write_fd,
                                         AioContext *write_ctx,
                                         IOHandler *io_write,
                                         void *opaque);

#endif /* QIO_CHANNEL_UTIL_H */
