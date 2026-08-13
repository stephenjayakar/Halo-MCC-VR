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

// Halo's world basis uses yaw in XY and pitch on Z. This conversion remains
// covered for the controller-to-body steering calculation. It must not publish
// the direct projectile sample: that sample has one owner, the final visible
// weapon basis below.
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

// The final visible right-hand pose already carries Halo's forward/left/up
// basis. Publishing column zero from that render-owned pose keeps firing
// independent of whether MCC happened to poll its XInput right-stick path in
// the same frame.
inline bool Halo3DirectWeaponAimFromVisibleBasis(
    const float* basis, float (&outDirection)[3]) noexcept
{
    if (!basis)
        return false;
    const float lengthSquared =
        basis[0] * basis[0] + basis[1] * basis[1] + basis[2] * basis[2];
    if (!std::isfinite(basis[0]) || !std::isfinite(basis[1]) ||
        !std::isfinite(basis[2]) || !std::isfinite(lengthSquared) ||
        lengthSquared < 1.0e-6f)
    {
        return false;
    }
    const float inverseLength = 1.0f / std::sqrt(lengthSquared);
    if (!std::isfinite(inverseLength))
        return false;
    for (int axis = 0; axis < 3; ++axis)
        outDirection[axis] = basis[axis] * inverseLength;
    return true;
}

// Halo's projectile-targeting helper is downstream of
// unit_adjust_projectile_ray and may replace that helper's direction. Restore
// the already validated visible-barrel ray at this last targeting boundary;
// the caller applies authored weapon spread afterward.
inline bool Halo3DirectWeaponAimRestoreAfterTargeting(
    bool localVrShot, const float* visibleDirection,
    float* targetedDirection) noexcept
{
    if (!localVrShot || !visibleDirection || !targetedDirection)
        return false;
    const float lengthSquared =
        visibleDirection[0] * visibleDirection[0] +
        visibleDirection[1] * visibleDirection[1] +
        visibleDirection[2] * visibleDirection[2];
    if (!std::isfinite(visibleDirection[0]) ||
        !std::isfinite(visibleDirection[1]) ||
        !std::isfinite(visibleDirection[2]) ||
        !std::isfinite(lengthSquared) ||
        lengthSquared < 0.9025f || lengthSquared > 1.1025f)
        return false;
    const float inverseLength = 1.0f / std::sqrt(lengthSquared);
    if (!std::isfinite(inverseLength))
        return false;
    for (int axis = 0; axis < 3; ++axis)
        targetedDirection[axis] = visibleDirection[axis] * inverseLength;
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
    // `offsetAim` controls whether Halo's helper copies the unit's integrated
    // torso aim into this weapon-barrel call. It is not an identity or
    // lifecycle gate. The uniquely verified caller is already the projectile
    // creation path, so every local on-foot shot must use the visible barrel
    // direction, including calls where Halo elected to keep its incoming
    // stock direction.
    (void)offsetAim;
    if (!bindingActive || !vrActive || !onFoot ||
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
