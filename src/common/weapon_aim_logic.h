#pragma once

#include <algorithm>
#include <cmath>

struct VrWeaponAimRay
{
    bool valid = false;
    float direction[3]{};
    float rangeMeters = 0.0f;
};

// Select the controller-owned weapon ray used by Halo's normal aim-steering
// path. Halo 3 used to converge the engine camera (the player's head) onto a
// point on the hand ray. That made the firing angle change when the head moved
// relative to an otherwise motionless weapon. In VR, the visible weapon owns
// the firing angle, so Halo 3 instead receives its normalized forward vector
// directly. The legacy convergence path remains available for titles that have
// not yet established that invariant.
inline VrWeaponAimRay ComputeVrWeaponAimRay(
    bool weaponOwnsDirection,
    const float weaponOrigin[3],
    const float weaponForward[3],
    const float engineCameraOrigin[3],
    float crosshairDistanceMeters) noexcept
{
    VrWeaponAimRay result{};
    if (!weaponOrigin || !weaponForward || !engineCameraOrigin)
        return result;

    for (int i = 0; i < 3; ++i)
    {
        if (!std::isfinite(weaponOrigin[i]) ||
            !std::isfinite(weaponForward[i]) ||
            !std::isfinite(engineCameraOrigin[i]))
        {
            return result;
        }
    }
    if (!std::isfinite(crosshairDistanceMeters))
        return result;

    const float distance =
        std::clamp(crosshairDistanceMeters, 2.0f, 50.0f);
    if (weaponOwnsDirection)
    {
        result.direction[0] = weaponForward[0];
        result.direction[1] = weaponForward[1];
        result.direction[2] = weaponForward[2];
        result.rangeMeters = distance;
    }
    else
    {
        for (int i = 0; i < 3; ++i)
        {
            result.direction[i] =
                weaponOrigin[i] + weaponForward[i] * distance -
                engineCameraOrigin[i];
        }
        result.rangeMeters = std::sqrt(
            result.direction[0] * result.direction[0] +
            result.direction[1] * result.direction[1] +
            result.direction[2] * result.direction[2]);
    }

    const float length = std::sqrt(
        result.direction[0] * result.direction[0] +
        result.direction[1] * result.direction[1] +
        result.direction[2] * result.direction[2]);
    if (!std::isfinite(length) || length < 1.0e-3f ||
        !std::isfinite(result.rangeMeters) || result.rangeMeters < 1.0e-3f)
    {
        return VrWeaponAimRay{};
    }
    for (float& component : result.direction)
        component /= length;
    result.valid = true;
    return result;
}
