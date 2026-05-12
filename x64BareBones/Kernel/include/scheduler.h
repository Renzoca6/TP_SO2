#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include "process.h"

void     init_scheduler(void);
uint64_t schedule(uint64_t current_rsp);
void     block_current_process(void);
void     unblock_process(uint64_t pid);
void     yield_process(void);

#endif