// SPDX-FileCopyrightText: 2026 Dolphin Contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOLPHINAPPEARANCE_H
#define DOLPHINAPPEARANCE_H

#include "dolphin_export.h"

namespace DolphinAppearance
{
// The running mode is fixed at startup. Changing the saved preference requires a restart.
DOLPHIN_EXPORT void initialize();
DOLPHIN_EXPORT bool isEnabled();
}

#endif
