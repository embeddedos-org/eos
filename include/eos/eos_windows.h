// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/* The Windows SDK can emit C5105 under MSVC's conforming preprocessor. Keep
 * that SDK-specific suppression at one boundary rather than repeating it at
 * every Windows include site. */
#ifndef EOS_WINDOWS_H
#define EOS_WINDOWS_H

#if defined(_WIN32)
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 5105)
#endif
#include <windows.h>
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
#endif

#endif /* EOS_WINDOWS_H */
