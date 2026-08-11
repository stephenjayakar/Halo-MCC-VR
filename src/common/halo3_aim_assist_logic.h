#pragma once

#include <cstdint>

inline constexpr int16_t kHalo3UnscopedMagnificationLevel = -1;
inline constexpr int16_t kHalo3FirstScopedMagnificationLevel = 0;

// Halo 3 passes -1 while unscoped and zero for a weapon's first authored zoom
// level. Only a weapon that actually authors at least one zoom level can have
// a scoped auto-aim policy. Already-scoped calls and all stock/non-VR calls
// remain untouched.
inline int16_t Halo3VrAimAssistMagnificationLevel(
    bool vrActive, int16_t requestedLevel,
    int16_t authoredMagnificationLevels) noexcept
{
    if (vrActive &&
        requestedLevel == kHalo3UnscopedMagnificationLevel &&
        authoredMagnificationLevels > 0)
    {
        return kHalo3FirstScopedMagnificationLevel;
    }
    return requestedLevel;
}
