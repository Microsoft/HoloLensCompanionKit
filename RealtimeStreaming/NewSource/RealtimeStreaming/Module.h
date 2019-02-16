// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include "Plugin.Module.g.h"

namespace winrt::RealtimeStreaming::Plugin::implementation
{
    struct Module : ModuleT<Module>
    {
        Module() = default;

        void Shutdown();
    };
}