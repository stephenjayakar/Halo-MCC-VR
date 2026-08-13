#pragma once

#include <cmath>
#include <cstdint>

inline constexpr uint64_t kHalo3DirectWeaponAimMaxAgeMs = 100;

struct Halo3DirectWeaponAimSample
{
    uint32_t generation = 0;
    uint64_t sampleMs = 0;
    float origin[3]{};
    float direction[3]{};
};

struct Halo3DirectWeaponAimComparison
{
    float originDeltaMeters = 0.0f;
    float directionDeltaDegrees = 0.0f;
    bool valid = false;
};

enum class Halo3DirectWeaponAimAnchor : uint8_t
{
    Stock,
    VisibleRoot,
    AuthoredMarker,
};

inline Halo3DirectWeaponAimAnchor Halo3DirectWeaponAimAnchorForPalette(
    bool visiblePaletteValid, bool authoredMarkerValid) noexcept
{
    if (!visiblePaletteValid)
        return Halo3DirectWeaponAimAnchor::Stock;
    return authoredMarkerValid
        ? Halo3DirectWeaponAimAnchor::AuthoredMarker
        : Halo3DirectWeaponAimAnchor::VisibleRoot;
}

inline Halo3DirectWeaponAimComparison Halo3CompareDirectWeaponAim(
    const float* stockOrigin, const float* stockDirection,
    const float* visibleOrigin, const float* visibleDirection,
    float worldUnitsPerMeter) noexcept
{
    Halo3DirectWeaponAimComparison result{};
    if (!stockOrigin || !stockDirection || !visibleOrigin ||
        !visibleDirection || !std::isfinite(worldUnitsPerMeter) ||
        worldUnitsPerMeter <= 0.0f)
        return result;
    float originDeltaSquared = 0.0f;
    float stockLengthSquared = 0.0f;
    float visibleLengthSquared = 0.0f;
    float directionDot = 0.0f;
    for (int axis = 0; axis < 3; ++axis)
    {
        if (!std::isfinite(stockOrigin[axis]) ||
            !std::isfinite(stockDirection[axis]) ||
            !std::isfinite(visibleOrigin[axis]) ||
            !std::isfinite(visibleDirection[axis]))
            return result;
        const float delta = visibleOrigin[axis] - stockOrigin[axis];
        originDeltaSquared += delta * delta;
        stockLengthSquared += stockDirection[axis] * stockDirection[axis];
        visibleLengthSquared +=
            visibleDirection[axis] * visibleDirection[axis];
        directionDot += stockDirection[axis] * visibleDirection[axis];
    }
    if (!std::isfinite(originDeltaSquared) ||
        !std::isfinite(stockLengthSquared) ||
        !std::isfinite(visibleLengthSquared) ||
        stockLengthSquared < 1.0e-8f || visibleLengthSquared < 1.0e-8f)
        return result;
    const float denominator = std::sqrt(
        stockLengthSquared * visibleLengthSquared);
    if (!std::isfinite(denominator) || denominator <= 0.0f)
        return result;
    const float cosine = std::fmax(
        -1.0f, std::fmin(1.0f, directionDot / denominator));
    result.originDeltaMeters =
        std::sqrt(originDeltaSquared) / worldUnitsPerMeter;
    result.directionDeltaDegrees = std::acos(cosine) * 57.2957795f;
    result.valid = std::isfinite(result.originDeltaMeters) &&
        std::isfinite(result.directionDeltaDegrees);
    return result;
}

inline float Halo3DirectWeaponAimOriginDistanceMeters(
    const float* firstOrigin, const float* secondOrigin,
    float worldUnitsPerMeter) noexcept
{
    if (!firstOrigin || !secondOrigin ||
        !std::isfinite(worldUnitsPerMeter) || worldUnitsPerMeter <= 0.0f)
        return -1.0f;
    float distanceSquared = 0.0f;
    for (int axis = 0; axis < 3; ++axis)
    {
        if (!std::isfinite(firstOrigin[axis]) ||
            !std::isfinite(secondOrigin[axis]))
            return -1.0f;
        const float delta = secondOrigin[axis] - firstOrigin[axis];
        distanceSquared += delta * delta;
    }
    if (!std::isfinite(distanceSquared))
        return -1.0f;
    const float result = std::sqrt(distanceSquared) / worldUnitsPerMeter;
    return std::isfinite(result) ? result : -1.0f;
}

inline bool Halo3DirectWeaponAimOriginForShot(
    const Halo3DirectWeaponAimSample& sample, float (&outOrigin)[3]) noexcept
{
    for (int axis = 0; axis < 3; ++axis)
    {
        if (!std::isfinite(sample.origin[axis]))
            return false;
        outOrigin[axis] = sample.origin[axis];
    }
    return true;
}

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

// Convert an authored render-model marker into the same world space as the
// final visible node palette. Halo stores each node as a uniformly scaled
// forward/left/up basis plus translation; marker rotation is an x/y/z/w
// quaternion. The marker's +X axis is the authored firing direction.
inline bool Halo3DirectWeaponAimFromVisibleMarker(
    float nodeScale, const float* nodeBasis, const float* nodePosition,
    const float* markerTranslation, const float* markerQuaternion,
    float (&outOrigin)[3], float (&outDirection)[3]) noexcept
{
    if (!nodeBasis || !nodePosition || !markerTranslation ||
        !markerQuaternion || !std::isfinite(nodeScale) ||
        nodeScale <= 0.001f || nodeScale > 100.0f)
        return false;
    for (int value = 0; value < 9; ++value)
        if (!std::isfinite(nodeBasis[value]))
            return false;
    for (int axis = 0; axis < 3; ++axis)
        if (!std::isfinite(nodePosition[axis]) ||
            !std::isfinite(markerTranslation[axis]))
            return false;
    float quaternionLengthSquared = 0.0f;
    for (int value = 0; value < 4; ++value)
    {
        if (!std::isfinite(markerQuaternion[value]))
            return false;
        quaternionLengthSquared +=
            markerQuaternion[value] * markerQuaternion[value];
    }
    if (!std::isfinite(quaternionLengthSquared) ||
        quaternionLengthSquared < 0.9025f ||
        quaternionLengthSquared > 1.1025f)
        return false;
    const float inverseQuaternionLength =
        1.0f / std::sqrt(quaternionLengthSquared);
    const float x = markerQuaternion[0] * inverseQuaternionLength;
    const float y = markerQuaternion[1] * inverseQuaternionLength;
    const float z = markerQuaternion[2] * inverseQuaternionLength;
    const float w = markerQuaternion[3] * inverseQuaternionLength;
    const float markerForward[3] = {
        1.0f - 2.0f * (y * y + z * z),
        2.0f * (x * y + w * z),
        2.0f * (x * z - w * y),
    };
    for (int row = 0; row < 3; ++row)
    {
        outOrigin[row] = nodePosition[row] + nodeScale * (
            nodeBasis[row] * markerTranslation[0] +
            nodeBasis[3 + row] * markerTranslation[1] +
            nodeBasis[6 + row] * markerTranslation[2]);
        outDirection[row] =
            nodeBasis[row] * markerForward[0] +
            nodeBasis[3 + row] * markerForward[1] +
            nodeBasis[6 + row] * markerForward[2];
    }
    const float directionLengthSquared =
        outDirection[0] * outDirection[0] +
        outDirection[1] * outDirection[1] +
        outDirection[2] * outDirection[2];
    if (!std::isfinite(outOrigin[0]) || !std::isfinite(outOrigin[1]) ||
        !std::isfinite(outOrigin[2]) ||
        !std::isfinite(directionLengthSquared) ||
        directionLengthSquared < 1.0e-6f)
        return false;
    const float inverseDirectionLength =
        1.0f / std::sqrt(directionLengthSquared);
    if (!std::isfinite(inverseDirectionLength))
        return false;
    for (int axis = 0; axis < 3; ++axis)
        outDirection[axis] *= inverseDirectionLength;
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

enum class Halo3DirectWeaponAimTargetingDisposition : uint8_t
{
    Rejected,
    VisibleDirectionRestored,
    AuthoredCorrectionPreserved,
};

inline Halo3DirectWeaponAimTargetingDisposition
Halo3DirectWeaponAimApplyAuthoredTargeting(
    bool localVrShot, bool nativeTargetingResult,
    float maximumAuthoredCorrectionRadians,
    const float* visibleDirection, float* targetedDirection) noexcept
{
    if (!localVrShot || !visibleDirection || !targetedDirection)
        return Halo3DirectWeaponAimTargetingDisposition::Rejected;
    float visibleLengthSquared = 0.0f;
    float targetedLengthSquared = 0.0f;
    float dot = 0.0f;
    for (int axis = 0; axis < 3; ++axis)
    {
        if (!std::isfinite(visibleDirection[axis]) ||
            !std::isfinite(targetedDirection[axis]))
            return Halo3DirectWeaponAimTargetingDisposition::Rejected;
        visibleLengthSquared += visibleDirection[axis] * visibleDirection[axis];
        targetedLengthSquared +=
            targetedDirection[axis] * targetedDirection[axis];
        dot += visibleDirection[axis] * targetedDirection[axis];
    }
    if (!std::isfinite(visibleLengthSquared) ||
        !std::isfinite(targetedLengthSquared) ||
        visibleLengthSquared < 0.9025f || visibleLengthSquared > 1.1025f ||
        targetedLengthSquared < 1.0e-8f)
        return Halo3DirectWeaponAimTargetingDisposition::Rejected;
    const float targetedInverseLength =
        1.0f / std::sqrt(targetedLengthSquared);
    const float denominator =
        std::sqrt(visibleLengthSquared * targetedLengthSquared);
    if (!std::isfinite(targetedInverseLength) ||
        !std::isfinite(denominator) || denominator <= 0.0f)
        return Halo3DirectWeaponAimTargetingDisposition::Rejected;
    const float cosine = std::fmax(
        -1.0f, std::fmin(1.0f, dot / denominator));
    const float correctionRadians = std::acos(cosine);
    constexpr float kAngleToleranceRadians = 0.5f / 57.2957795f;
    const bool authoredCorrection = nativeTargetingResult &&
        std::isfinite(maximumAuthoredCorrectionRadians) &&
        maximumAuthoredCorrectionRadians > 0.0f &&
        maximumAuthoredCorrectionRadians <= 1.5707964f &&
        std::isfinite(correctionRadians) &&
        correctionRadians <=
            maximumAuthoredCorrectionRadians + kAngleToleranceRadians;
    if (authoredCorrection)
    {
        for (int axis = 0; axis < 3; ++axis)
            targetedDirection[axis] *= targetedInverseLength;
        return Halo3DirectWeaponAimTargetingDisposition::
            AuthoredCorrectionPreserved;
    }
    if (!Halo3DirectWeaponAimRestoreAfterTargeting(
            true, visibleDirection, targetedDirection))
        return Halo3DirectWeaponAimTargetingDisposition::Rejected;
    return Halo3DirectWeaponAimTargetingDisposition::
        VisibleDirectionRestored;
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
