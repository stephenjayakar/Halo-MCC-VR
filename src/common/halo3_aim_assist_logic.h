#pragma once

#include <cstddef>
#include <cstdint>

inline constexpr int16_t kHalo3UnscopedMagnificationLevel = -1;
inline constexpr int16_t kHalo3FirstScopedMagnificationLevel = 0;
inline constexpr size_t kHalo3AimAssistTagInstanceLoadOffset = 0x94;
inline constexpr size_t kHalo3AimAssistDefinitionIndexLoadOffset = 0x9B;
inline constexpr size_t kHalo3AimAssistTagDataBaseLoadOffset = 0xAC;

// These are instruction-boundary checks inside the already unique query
// builder. They protect the RIP-relative global decoding and the weapon
// definition-index read without shipping an absolute address.
inline bool Halo3AimAssistQueryLayoutMatches(
    const uint8_t* query, size_t availableBytes) noexcept
{
    if (!query ||
        availableBytes < kHalo3AimAssistTagDataBaseLoadOffset + 7)
        return false;
    return
        query[kHalo3AimAssistTagInstanceLoadOffset + 0] == 0x48 &&
        query[kHalo3AimAssistTagInstanceLoadOffset + 1] == 0x8B &&
        query[kHalo3AimAssistTagInstanceLoadOffset + 2] == 0x05 &&
        query[kHalo3AimAssistDefinitionIndexLoadOffset + 0] == 0x0F &&
        query[kHalo3AimAssistDefinitionIndexLoadOffset + 1] == 0xB7 &&
        query[kHalo3AimAssistDefinitionIndexLoadOffset + 2] == 0x11 &&
        query[kHalo3AimAssistTagDataBaseLoadOffset + 0] == 0x48 &&
        query[kHalo3AimAssistTagDataBaseLoadOffset + 1] == 0x8B &&
        query[kHalo3AimAssistTagDataBaseLoadOffset + 2] == 0x05;
}

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
