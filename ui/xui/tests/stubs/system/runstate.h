#pragma once

enum RunState {
    RUN_STATE_PAUSED = 0,
};

bool runstate_is_running(void);
void vm_start(void);
int vm_stop(RunState state);
