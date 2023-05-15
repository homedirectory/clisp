#pragma once

#include "env.h"

void core_def_procs(MalEnv *env);

// returns LispDatum*, but void* is declared so that returned value can be cast to
// a concrete type implicitly without compiler warnings
void *verify_proc_arg_type(const Proc *proc, const Arr *args, size_t arg_idx, 
        LispType expect_type);
