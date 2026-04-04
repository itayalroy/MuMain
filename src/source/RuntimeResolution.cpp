#include "stdafx.h"

#include "RuntimeResolution.h"

#include <cmath>

#include "GameConfig/GameConfig.h"
#include "Winmain.h"

extern BOOL g_bUseWindowMode;
extern BOOL g_bUseBorderlessMode;
extern CErrorReport g_ErrorReport;

namespace
{
    constexpr DWORD kWindowedWindowStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_BORDER | WS_CLIPCHILDREN;
    constexpr DWORD kBorderlessWindowStyle = WS_POPUP | WS_CLIPCHILDREN;
    constexpr DWORD kBorderlessWindowExStyle = WS_EX_APPWINDOW;
    constexpr DWORD kWindowedRuntimeStyleMask = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_BORDER | WS_POPUP | WS_CLIPCHILDREN;
    constexpr DWORD kWindowedRuntimeExStyleMask = WS_EX_APPWINDOW;
    constexpr unsigned int kMinimumSelectableResolutionWidth = 1024u;
    constexpr unsigned int kMinimumSelectableResolutionHeight = 720u;
    constexpr double kSupportedAspectRatioTolerance = 0.03;

    enum class ResolutionSelectionFailure
    {
        None,
        UnsupportedResolution,
        WindowedPlacementUnavailable,
        FullscreenModeUnavailable,
    };

    RuntimeResolutionPendingChange g_PendingResolutionChange = {};

    DWORD GetRequestedBitsPerPixel()
    {
        DEVMODEW mode = {};
        mode.dmSize = sizeof(mode);

        DWORD fallbackBitsPerPixel = m_nColorDepth == 0 ? 16u : 32u;
        for (DWORD index = 0; EnumDisplaySettingsW(nullptr, index, &mode) != FALSE; ++index)
        {
            if (mode.dmBitsPerPel == 16 && m_nColorDepth == 0)
            {
                return 16u;
            }

            if (mode.dmBitsPerPel == 24 && m_nColorDepth == 1)
            {
                return 24u;
            }

            if (mode.dmBitsPerPel == 32 && m_nColorDepth == 1)
            {
                return 32u;
            }

            fallbackBitsPerPixel = mode.dmBitsPerPel;
            mode.dmSize = sizeof(mode);
        }

        return fallbackBitsPerPixel;
    }

    bool TryGetFullscreenDisplayMode(unsigned int width, unsigned int height, DEVMODEW& displayMode)
    {
        const DWORD bitsPerPixel = GetRequestedBitsPerPixel();
        DEVMODEW candidate = {};
        candidate.dmSize = sizeof(candidate);

        for (DWORD index = 0; EnumDisplaySettingsW(nullptr, index, &candidate) != FALSE; ++index)
        {
            if (candidate.dmPelsWidth == width
                && candidate.dmPelsHeight == height
                && candidate.dmBitsPerPel == bitsPerPixel)
            {
                displayMode = candidate;
                return true;
            }

            candidate.dmSize = sizeof(candidate);
        }

        return false;
    }

    bool DoesWindowedPlacementFitDesktop(const RuntimeWindowPlacement& placement)
    {
        const int desktopWidth = ::GetSystemMetrics(SM_CXSCREEN);
        const int desktopHeight = ::GetSystemMetrics(SM_CYSCREEN);

        if (desktopWidth <= 0 || desktopHeight <= 0)
        {
            return false;
        }

        const bool hasWindowFrame = placement.WindowWidth != static_cast<int>(placement.ClientWidth)
            || placement.WindowHeight != static_cast<int>(placement.ClientHeight);
        if (!hasWindowFrame)
        {
            return placement.WindowWidth <= desktopWidth && placement.WindowHeight <= desktopHeight;
        }

        return placement.ClientWidth <= static_cast<unsigned int>(desktopWidth)
            && placement.ClientHeight <= static_cast<unsigned int>(desktopHeight)
            && (placement.ClientWidth < static_cast<unsigned int>(desktopWidth)
                || placement.ClientHeight < static_cast<unsigned int>(desktopHeight));
    }

    bool IsSupportedAspectRatio(unsigned int width, unsigned int height)
    {
        if (height == 0)
        {
            return false;
        }

        const double aspectRatio = static_cast<double>(width) / static_cast<double>(height);
        static const double SupportedAspectRatios[] =
        {
            4.0 / 3.0,
            5.0 / 4.0,
            16.0 / 10.0,
            16.0 / 9.0,
        };

        for (double supportedAspectRatio : SupportedAspectRatios)
        {
            if (std::fabs(aspectRatio - supportedAspectRatio) <= kSupportedAspectRatioTolerance)
            {
                return true;
            }
        }

        return false;
    }

    bool IsResolutionSelectableByPolicy(unsigned int width, unsigned int height)
    {
        return width >= kMinimumSelectableResolutionWidth
            && height >= kMinimumSelectableResolutionHeight
            && IsSupportedAspectRatio(width, height);
    }

    ResolutionSelectionFailure GetResolutionSelectionFailure(unsigned int width, unsigned int height, bool borderless)
    {
        if (width == 0 || height == 0 || !IsResolutionSelectableByPolicy(width, height))
        {
            return ResolutionSelectionFailure::UnsupportedResolution;
        }

        if (g_bUseWindowMode == TRUE)
        {
            RuntimeWindowPlacement placement = {};
            return RuntimeResolution::TryBuildWindowedPlacement(width, height, borderless, placement)
                && DoesWindowedPlacementFitDesktop(placement)
                ? ResolutionSelectionFailure::None
                : ResolutionSelectionFailure::WindowedPlacementUnavailable;
        }

        DEVMODEW displayMode = {};
        return TryGetFullscreenDisplayMode(width, height, displayMode)
            ? ResolutionSelectionFailure::None
            : ResolutionSelectionFailure::FullscreenModeUnavailable;
    }
}

DWORD RuntimeResolution::GetWindowedWindowStyle(bool borderless)
{
    return borderless ? kBorderlessWindowStyle : kWindowedWindowStyle;
}

DWORD RuntimeResolution::GetWindowedWindowExStyle(bool borderless)
{
    return borderless ? kBorderlessWindowExStyle : 0u;
}

bool RuntimeResolution::TryBuildWindowedPlacement(unsigned int width, unsigned int height, bool borderless, RuntimeWindowPlacement& placement)
{
    placement = {};

    const int desktopWidth = ::GetSystemMetrics(SM_CXSCREEN);
    const int desktopHeight = ::GetSystemMetrics(SM_CYSCREEN);
    if (desktopWidth <= 0 || desktopHeight <= 0)
    {
        return false;
    }

    placement.Style = GetWindowedWindowStyle(borderless);
    placement.ExStyle = GetWindowedWindowExStyle(borderless);
    placement.ClientWidth = width;
    placement.ClientHeight = height;
    placement.WindowWidth = static_cast<int>(width);
    placement.WindowHeight = static_cast<int>(height);

    if (!borderless)
    {
        RECT rc = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
        if (::AdjustWindowRectEx(&rc, placement.Style, FALSE, placement.ExStyle) == FALSE)
        {
            return false;
        }

        placement.WindowWidth = rc.right - rc.left;
        placement.WindowHeight = rc.bottom - rc.top;
    }

    placement.WindowX = (desktopWidth - placement.WindowWidth) / 2;
    if (placement.WindowX < 0)
    {
        placement.WindowX = 0;
    }

    placement.WindowY = (desktopHeight - placement.WindowHeight) / 2;
    if (placement.WindowY < 0)
    {
        placement.WindowY = 0;
    }

    return true;
}

bool RuntimeResolution::CanSelectForCurrentMode(unsigned int width, unsigned int height)
{
    return GetResolutionSelectionFailure(width, height, g_bUseBorderlessMode == TRUE) == ResolutionSelectionFailure::None;
}

bool RuntimeResolution::QueueResolutionChange(unsigned int width, unsigned int height)
{
    static const wchar_t* const FailureMessages[] =
    {
        nullptr,
        L"> Failed to apply resolution %ux%u: unsupported resolution.\r\n",
        L"> Failed to apply windowed resolution %ux%u: window does not fit on the current desktop.\r\n",
        L"> Failed to find fullscreen mode %ux%u.\r\n",
    };

    const auto failure = GetResolutionSelectionFailure(width, height, g_bUseBorderlessMode == TRUE);
    if (failure != ResolutionSelectionFailure::None)
    {
        g_ErrorReport.Write(FailureMessages[static_cast<int>(failure)], width, height);
        return false;
    }

    g_PendingResolutionChange.HasPendingChange = true;
    g_PendingResolutionChange.RequestedWidth = width;
    g_PendingResolutionChange.RequestedHeight = height;
    g_PendingResolutionChange.RequestedBorderlessMode = g_bUseBorderlessMode == TRUE;
    return true;
}

bool RuntimeResolution::QueueBorderlessModeChange(bool borderless)
{
    const bool previousBorderless = g_bUseBorderlessMode == TRUE;
    if (previousBorderless == borderless)
    {
        return true;
    }

    if (g_bUseWindowMode != TRUE)
    {
        g_bUseBorderlessMode = borderless ? TRUE : FALSE;
        GameConfig::GetInstance().SetBorderlessMode(borderless);
        GameConfig::GetInstance().Save();
        return true;
    }

    const unsigned int configuredWidth = g_PendingResolutionChange.HasPendingChange
        ? g_PendingResolutionChange.RequestedWidth
        : static_cast<unsigned int>(GameConfig::GetInstance().GetWindowWidth());
    const unsigned int configuredHeight = g_PendingResolutionChange.HasPendingChange
        ? g_PendingResolutionChange.RequestedHeight
        : static_cast<unsigned int>(GameConfig::GetInstance().GetWindowHeight());

    g_bUseBorderlessMode = borderless ? TRUE : FALSE;
    if (!QueueResolutionChange(configuredWidth, configuredHeight))
    {
        g_bUseBorderlessMode = previousBorderless ? TRUE : FALSE;
        return false;
    }

    return true;
}

RuntimeResolutionPendingChange RuntimeResolution::GetPendingChange()
{
    return g_PendingResolutionChange;
}

bool RuntimeResolution::ApplyQueuedChange(RuntimeResolutionApplyResult& result)
{
    result = {};
    if (!g_PendingResolutionChange.HasPendingChange || g_hWnd == nullptr)
    {
        return false;
    }

    const RuntimeResolutionPendingChange pendingChange = g_PendingResolutionChange;
    g_PendingResolutionChange = {};

    g_ErrorReport.Write(L"> Applying queued resolution %ux%u.\r\n",
        pendingChange.RequestedWidth,
        pendingChange.RequestedHeight);

    if (g_bUseWindowMode == TRUE)
    {
        RuntimeWindowPlacement placement = {};
        if (!TryBuildWindowedPlacement(
            pendingChange.RequestedWidth,
            pendingChange.RequestedHeight,
            pendingChange.RequestedBorderlessMode,
            placement)
            || !DoesWindowedPlacementFitDesktop(placement))
        {
            g_bUseBorderlessMode = GameConfig::GetInstance().GetBorderlessMode() ? TRUE : FALSE;
            g_ErrorReport.Write(L"> Failed to apply windowed resolution %ux%u: window does not fit.\r\n",
                pendingChange.RequestedWidth, pendingChange.RequestedHeight);
            return false;
        }

        const DWORD desiredStyle = placement.Style;
        const DWORD desiredExStyle = placement.ExStyle;
        const DWORD currentStyle = static_cast<DWORD>(::GetWindowLongPtrW(g_hWnd, GWL_STYLE));
        const DWORD currentExStyle = static_cast<DWORD>(::GetWindowLongPtrW(g_hWnd, GWL_EXSTYLE));
        const DWORD nextStyle = (currentStyle & ~kWindowedRuntimeStyleMask) | desiredStyle;
        const DWORD nextExStyle = (currentExStyle & ~kWindowedRuntimeExStyleMask) | desiredExStyle;
        const bool styleChanged = currentStyle != nextStyle || currentExStyle != nextExStyle;

        if (styleChanged)
        {
            ::SetWindowLongPtrW(g_hWnd, GWL_STYLE, static_cast<LONG_PTR>(nextStyle));
            ::SetWindowLongPtrW(g_hWnd, GWL_EXSTYLE, static_cast<LONG_PTR>(nextExStyle));
        }

        if (::SetWindowPos(
            g_hWnd,
            nullptr,
            placement.WindowX,
            placement.WindowY,
            placement.WindowWidth,
            placement.WindowHeight,
            SWP_NOZORDER | SWP_NOOWNERZORDER | (styleChanged ? SWP_FRAMECHANGED : 0u)) == FALSE)
        {
            g_bUseBorderlessMode = GameConfig::GetInstance().GetBorderlessMode() ? TRUE : FALSE;
            g_ErrorReport.Write(L"> Failed to resize window for %ux%u.\r\n",
                pendingChange.RequestedWidth, pendingChange.RequestedHeight);
            return false;
        }

        result.Width = placement.ClientWidth;
        result.Height = placement.ClientHeight;
    }
    else
    {
        DEVMODEW displayMode = {};
        if (!TryGetFullscreenDisplayMode(pendingChange.RequestedWidth, pendingChange.RequestedHeight, displayMode))
        {
            g_ErrorReport.Write(L"> Failed to find fullscreen mode %ux%u.\r\n",
                pendingChange.RequestedWidth, pendingChange.RequestedHeight);
            return false;
        }

        if (::ChangeDisplaySettingsW(&displayMode, 0) != DISP_CHANGE_SUCCESSFUL)
        {
            g_ErrorReport.Write(L"> Failed to switch to fullscreen mode %ux%u.\r\n",
                pendingChange.RequestedWidth, pendingChange.RequestedHeight);
            return false;
        }

        ::SetWindowPos(
            g_hWnd,
            HWND_TOPMOST,
            0,
            0,
            static_cast<int>(pendingChange.RequestedWidth),
            static_cast<int>(pendingChange.RequestedHeight),
            SWP_SHOWWINDOW | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);

        result.Width = pendingChange.RequestedWidth;
        result.Height = pendingChange.RequestedHeight;
    }

    GameConfig::GetInstance().SetWindowSize(
        static_cast<int>(pendingChange.RequestedWidth),
        static_cast<int>(pendingChange.RequestedHeight));
    GameConfig::GetInstance().SetBorderlessMode(pendingChange.RequestedBorderlessMode);
    GameConfig::GetInstance().Save();
    return true;
}
