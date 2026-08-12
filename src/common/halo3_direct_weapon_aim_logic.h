#pragma once

#include <cmath>
#include <cstdint>

inline constexpr uint64_t kHalo3DirectWeaponAimMaxAgeMs = 100;

struct Halo3DirectWeaponAimSample
{
    uint32_t generation = 0;
    uint64_t sampleMs = 0;
    float direction[3]{};
};

// Halo's world basis uses yaw in XY and pitch on Z. This is the same basis
// used by the visible Halo 3 weapon placement and by Game_ComputeAimStick.
inline bool Halo3DirectWeaponAimFromYawPitch(
    float yaw, float pitch, float (&outDirection)[3]) noexcept
{
    if (!std::isfinite(yaw) || !std::isfinite(pitch))
        return false;
    const float cosPitch = std::cos(pitch);
    const float direction[3] = {
        cosPitch * std::cos(yaw),
        cosPitch * std::sin(yaw),
        std::sin(pitch),
    };
    for (int axis = 0; axis < 3; ++axis)
    {
        if (!std::isfinite(direction[axis]))
            return false;
        outDirection[axis] = direction[axis];
    }
    return true;
}

// The detour calls Halo first, then may replace only the resulting direction.
// Every lifecycle or identity doubt keeps that original direction unchanged.
inline bool Halo3DirectWeaponAimDirectionForShot(
    bool bindingActive, bool vrActive, bool offsetAim, bool onFoot,
    int32_t firingUnitHandle, int32_t localUnitHandle,
    uint32_t activeGeneration, uint64_t nowMs,
    const Halo3DirectWeaponAimSample& sample,
    float (&outDirection)[3]) noexcept
{
    if (!bindingActive || !vrActive || !offsetAim || !onFoot ||
        firingUnitHandle == -1 || firingUnitHandle != localUnitHandle ||
        activeGeneration == 0 || sample.generation != activeGeneration ||
        sample.sampleMs == 0 || nowMs < sample.sampleMs ||
        nowMs - sample.sampleMs > kHalo3DirectWeaponAimMaxAgeMs)
    {
        return false;
    }

    const float lengthSquared =
        sample.direction[0] * sample.direction[0] +
        sample.direction[1] * sample.direction[1] +
        sample.direction[2] * sample.direction[2];
    if (!std::isfinite(sample.direction[0]) ||
        !std::isfinite(sample.direction[1]) ||
        !std::isfinite(sample.direction[2]) ||
        !std::isfinite(lengthSquared) ||
        lengthSquared < 0.9025f || lengthSquared > 1.1025f)
    {
        return false;
    }

    const float inverseLength = 1.0f / std::sqrt(lengthSquared);
    if (!std::isfinite(inverseLength))
        return false;
    for (int axis = 0; axis < 3; ++axis)
        outDirection[axis] = sample.direction[axis] * inverseLength;
    return true;
}
