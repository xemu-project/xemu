/*
 * Interface for configuring and controlling the state of tracing events.
 *
 * Copyright (C) 2011-2016 Lluís Vilanova <vilanova@ac.upc.edu>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#ifndef TRACE__CONTROL_H
#define TRACE__CONTROL_H

#include "event-internal.h"

typedef struct TraceEventIter {
    /* iter state */
    size_t event;
    size_t group;
    /* filter conditions */
    size_t group_id;
    const char *pattern;
} TraceEventIter;


/**
 * trace_event_iter_init_all:
 * @iter: the event iterator struct
 *
 * Initialize the event iterator struct @iter,
 * for all events.
 */
void trace_event_iter_init_all(TraceEventIter *iter);

/**
 * trace_event_iter_init_pattern:
 * @iter: the event iterator struct
 * @pattern: pattern to filter events on name
 *
 * Initialize the event iterator struct @iter,
 * using @pattern to filter out events
 * with non-matching names.
 */
void trace_event_iter_init_pattern(TraceEventIter *iter, const char *pattern);

/**
 * trace_event_iter_init_group:
 * @iter: the event iterator struct
 * @group_id: group_id to filter events by group.
 *
 * Initialize the event iterator struct @iter,
 * using @group_id to filter for events in the group.
 */
void trace_event_iter_init_group(TraceEventIter *iter, size_t group_id);

/**
 * trace_event_iter_next:
 * @iter: the event iterator struct
 *
 * Get the next event, if any. When this returns NULL,
 * the iterator should no longer be used.
 *
 * Returns: the next event, or NULL if no more events exist
 */
TraceEvent *trace_event_iter_next(TraceEventIter *iter);


/**
 * trace_event_name:
 * @id: Event name.
 *
 * Search an event by its name.
 *
 * Returns: pointer to #TraceEvent or NULL if not found.
 */
TraceEvent *trace_event_name(const char *name);

/**
 * trace_event_is_pattern:
 *
 * Whether the given string is an event name pattern.
 */
static bool trace_event_is_pattern(const char *str);


/**
 * trace_event_get_id:
 *
 * Get the identifier of an event.
 */
static uint32_t trace_event_get_id(TraceEvent *ev);

/**
 * trace_event_get_name:
 *
 * Get the name of an event.
 */
static const char * trace_event_get_name(TraceEvent *ev);

/**
 * trace_event_get_state:
 * @id: Event identifier name.
 *
 * Get the tracing state of an event, both static and the QEMU dynamic state.
 *
 * If the event has the disabled property, the check will have no performance
 * impact.
 */
#define trace_event_get_state(id)                       \
    ((id ##_ENABLED) && trace_event_get_state_dynamic_by_id(id))

/**
 * trace_event_get_state_backends:
 * @id: Event identifier name.
 *
 * Get the tracing state of an event, both static and dynamic state from all
 * compiled-in backends.
 *
 * If the event has the disabled property, the check will have no performance
 * impact.
 *
 * Returns: true if at least one backend has the event enabled and the event
 * does not have the disabled property.
 */
#define trace_event_get_state_backends(id)              \
    ((id ##_ENABLED) && id ##_BACKEND_DSTATE())

/**
 * trace_event_get_state_static:
 * @id: Event identifier.
 *
 * Get the static tracing state of an event.
 *
 * Use the define 'TRACE_${EVENT_NAME}_ENABLED' for compile-time checks (it will
 * be set to 1 or 0 according to the presence of the disabled property).
 */
static bool trace_event_get_state_static(TraceEvent *ev);

/**
 * trace_event_get_state_dynamic:
 *
 * Get the dynamic tracing state of an event.
 *
 * If the event has the 'vcpu' property, gets the OR'ed state of all vCPUs.
 */
static bool trace_event_get_state_dynamic(TraceEvent *ev);

/**
 * trace_event_set_state_dynamic:
 *
 * Set the dynamic tracing state of an event.
 *
 * If the event has the 'vcpu' property, sets the state on all vCPUs.
 *
 * Pre-condition: trace_event_get_state_static(ev) == true
 */
void trace_event_set_state_dynamic(TraceEvent *ev, bool state);

/**
 * trace_init_backends:
 *
 * Initialize the tracing backend.
 *
 * Returns: Whether the backends could be successfully initialized.
 */
bool trace_init_backends(void);

/**
 * trace_init_file:
 *
 * Record the name of the output file for the tracing backend.
 * Exits if no selected backend does not support specifying the
 * output file, and a file was specified with "-trace file=...".
 */
void trace_init_file(void);

/**
 * trace_list_events:
 * @f: Where to send output.
 *
 * List all available events.
 */
void trace_list_events(FILE *f);

/**
 * trace_enable_events:
 * @line_buf: A string with a glob pattern of events to be enabled or,
 *            if the string starts with '-', disabled.
 *
 * Enable or disable matching events.
 */
void trace_enable_events(const char *line_buf);

/**
 * Definition of QEMU options describing trace subsystem configuration
 */
extern QemuOptsList qemu_trace_opts;

/**
 * trace_opt_parse:
 * @optstr: A string argument of --trace command line argument
 *
 * Initialize tracing subsystem.
 */
void trace_opt_parse(const char *optstr);

/**
 * trace_get_vcpu_event_count:
 *
 * Return the number of known vcpu-specific events
 */
uint32_t trace_get_vcpu_event_count(void);


#include "control-internal.h"

#endif /* TRACE__CONTROL_H */
