#pragma once

#include "env.h"

void core_def_procs(MalEnv *env);

MalDatum *verify_proc_arg_type(const Proc *proc, const Arr *args, size_t arg_idx, 
        MalType expect_type);
