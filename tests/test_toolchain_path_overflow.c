// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

#include "eos/toolchain.h"
#include <assert.h>
#include <string.h>

int main(void) {
    EosToolchain toolchain;
    char target[EOS_MAX_PATH];
    memset(target, 'a', sizeof(target) - 1);
    target[sizeof(target) - 1] = '\0';

    assert(eos_toolchain_load_by_target(&toolchain, target, ".") ==
           EOS_ERR_OVERFLOW);
    return 0;
}
