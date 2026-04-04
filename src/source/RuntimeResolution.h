#pragma once

#include <windows.h>

struct RuntimeResolutionApplyResult
{
    unsigned int Width = 0;
    unsigned int Height = 0;
};

struct RuntimeWindowPlacement
{
    DWORD Style = 0;
    DWORD ExStyle = 0;
    unsigned int ClientWidth = 0;
    unsigned int ClientHeight = 0;
    int WindowX = 0;
    int WindowY = 0;
    int WindowWidth = 0;
    int WindowHeight = 0;
};

struct RuntimeResolutionPendingChange
{
    bool HasPendingChange = false;
    unsigned int RequestedWidth = 0;
    unsigned int RequestedHeight = 0;
    bool RequestedBorderlessMode = false;
};

namespace RuntimeResolution
{
    DWORD GetWindowedWindowStyle(bool borderless);
    DWORD GetWindowedWindowExStyle(bool borderless);
    bool TryBuildWindowedPlacement(unsigned int width, unsigned int height, bool borderless, RuntimeWindowPlacement& placement);
    bool CanSelectForCurrentMode(unsigned int width, unsigned int height);
    bool QueueResolutionChange(unsigned int width, unsigned int height);
    bool QueueBorderlessModeChange(bool borderless);
    RuntimeResolutionPendingChange GetPendingChange();
    bool ApplyQueuedChange(RuntimeResolutionApplyResult& result);
}
