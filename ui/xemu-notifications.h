/*
 * xemu Notification Management
 *
 * Helper functions for other subsystems to queue a notification for the user,
 * which can be displayed by the HUD.
 *
 * Copyright (C) 2020-2021 Matt Borgerson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef XEMU_NOTIFICATION
#define XEMU_NOTIFICATION

#ifdef __cplusplus
extern "C" {
#endif

// Simple API to show a message on the screen when some event happens
void xemu_queue_notification(const char *msg);
void xemu_queue_error_message(const char *msg);

#ifdef __cplusplus
}
#endif

#endif
