#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <utility>

struct PhysicalContactVec3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

inline PhysicalContactVec3 operator+(
    PhysicalContactVec3 a, PhysicalContactVec3 b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

inline PhysicalContactVec3 operator-(
    PhysicalContactVec3 a, PhysicalContactVec3 b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

inline PhysicalContactVec3 operator*(PhysicalContactVec3 v, float s)
{
    return {v.x * s, v.y * s, v.z * s};
}

inline float PhysicalContactDot(PhysicalContactVec3 a, PhysicalContactVec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline bool PhysicalContactFinite(PhysicalContactVec3 v)
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

inline float PhysicalContactLengthSquared(PhysicalContactVec3 v)
{
    return PhysicalContactDot(v, v);
}

inline float PhysicalContactLength(PhysicalContactVec3 v)
{
    return std::sqrt(PhysicalContactLengthSquared(v));
}

inline PhysicalContactVec3 PhysicalContactNormalize(
    PhysicalContactVec3 v, PhysicalContactVec3 fallback = {1.0f, 0.0f, 0.0f})
{
    const float length = PhysicalContactLength(v);
    return std::isfinite(length) && length > 1.0e-5f
        ? v * (1.0f / length) : fallback;
}

inline PhysicalContactVec3 PhysicalContactCross(
    PhysicalContactVec3 a, PhysicalContactVec3 b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x};
}

struct PhysicalContactPointVelocity
{
    bool valid = false;
    float capsuleFraction = 0.0f;
    PhysicalContactVec3 weaponMetersPerSecond{};
    PhysicalContactVec3 targetMetersPerSecond{};
    PhysicalContactVec3 relativeMetersPerSecond{};
};

// Compute velocity where contact actually occurred rather than classifying a
// rotational strike from controller-origin speed. The same capsule fraction is
// evaluated at the previous and current visible weapon poses. Target velocity
// includes the rigid body's angular contribution, v + omega x (point-center).
inline PhysicalContactPointVelocity PhysicalContactVelocityAtPoint(
    PhysicalContactVec3 previousGrip, PhysicalContactVec3 previousTip,
    PhysicalContactVec3 currentGrip, PhysicalContactVec3 currentTip,
    PhysicalContactVec3 contactPoint, PhysicalContactVec3 targetLinearVelocity,
    PhysicalContactVec3 targetAngularVelocity,
    PhysicalContactVec3 targetCenter, float elapsedSeconds,
    float worldUnitsPerMeter)
{
    PhysicalContactPointVelocity result{};
    if (!PhysicalContactFinite(previousGrip) ||
        !PhysicalContactFinite(previousTip) ||
        !PhysicalContactFinite(currentGrip) ||
        !PhysicalContactFinite(currentTip) ||
        !PhysicalContactFinite(contactPoint) ||
        !PhysicalContactFinite(targetLinearVelocity) ||
        !PhysicalContactFinite(targetAngularVelocity) ||
        !PhysicalContactFinite(targetCenter) ||
        !std::isfinite(elapsedSeconds) || elapsedSeconds <= 0.0f ||
        elapsedSeconds > 0.1f || !std::isfinite(worldUnitsPerMeter) ||
        worldUnitsPerMeter <= 0.0f)
        return result;

    const PhysicalContactVec3 currentSpine = currentTip - currentGrip;
    const float spineLengthSquared = PhysicalContactLengthSquared(currentSpine);
    const float u = spineLengthSquared > 1.0e-8f
        ? std::clamp(
              PhysicalContactDot(contactPoint - currentGrip, currentSpine) /
                  spineLengthSquared,
              0.0f, 1.0f)
        : 0.0f;
    const PhysicalContactVec3 weaponPoint = currentGrip + currentSpine * u;
    const PhysicalContactVec3 previousWeaponPoint =
        previousGrip + (previousTip - previousGrip) * u;
    const float metersPerWorldUnit = 1.0f / worldUnitsPerMeter;
    result.weaponMetersPerSecond =
        (weaponPoint - previousWeaponPoint) *
        (metersPerWorldUnit / elapsedSeconds);
    const PhysicalContactVec3 angularPointVelocity = PhysicalContactCross(
        targetAngularVelocity, contactPoint - targetCenter);
    result.targetMetersPerSecond =
        (targetLinearVelocity + angularPointVelocity) * metersPerWorldUnit;
    result.relativeMetersPerSecond =
        result.weaponMetersPerSecond - result.targetMetersPerSecond;
    result.capsuleFraction = u;
    result.valid = PhysicalContactFinite(result.weaponMetersPerSecond) &&
        PhysicalContactFinite(result.targetMetersPerSecond) &&
        PhysicalContactFinite(result.relativeMetersPerSecond);
    return result;
}

struct PhysicalContactHit
{
    bool hit = false;
    float fraction = 1.0f;
    PhysicalContactVec3 point{};
    PhysicalContactVec3 normal{1.0f, 0.0f, 0.0f};
};

inline PhysicalContactHit PhysicalContactSweepPoint(
    PhysicalContactVec3 from, PhysicalContactVec3 to,
    PhysicalContactVec3 sphereCenter, float expandedRadius)
{
    PhysicalContactHit result{};
    if (!PhysicalContactFinite(from) || !PhysicalContactFinite(to) ||
        !PhysicalContactFinite(sphereCenter) ||
        !std::isfinite(expandedRadius) || expandedRadius <= 0.0f)
        return result;
    const PhysicalContactVec3 delta = to - from;
    const PhysicalContactVec3 offset = from - sphereCenter;
    const float radiusSquared = expandedRadius * expandedRadius;
    const float a = PhysicalContactLengthSquared(delta);
    if (PhysicalContactLengthSquared(offset) <= radiusSquared)
    {
        result.hit = true;
        result.fraction = 0.0f;
        result.point = from;
        result.normal = PhysicalContactNormalize(offset);
        return result;
    }
    if (a <= 1.0e-10f)
        return result;
    const float b = PhysicalContactDot(offset, delta);
    const float c = PhysicalContactLengthSquared(offset) - radiusSquared;
    const float discriminant = b * b - a * c;
    if (discriminant < 0.0f || !std::isfinite(discriminant))
        return result;
    const float fraction = (-b - std::sqrt(discriminant)) / a;
    if (fraction < 0.0f || fraction > 1.0f)
        return result;
    result.hit = true;
    result.fraction = fraction;
    result.point = from + delta * fraction;
    result.normal = PhysicalContactNormalize(result.point - sphereCenter);
    return result;
}

// Conservative, allocation-free sweep for the approved bounds-derived
// capsule fallback. The two endpoints and midpoint cover translation and the
// dominant rotational arc; the previous/current capsule spines cover grazing
// contacts at either visible pose.
inline PhysicalContactHit PhysicalContactSweepCapsule(
    PhysicalContactVec3 previousGrip, PhysicalContactVec3 previousTip,
    PhysicalContactVec3 currentGrip, PhysicalContactVec3 currentTip,
    float capsuleRadius, PhysicalContactVec3 targetCenter, float targetRadius)
{
    PhysicalContactHit best{};
    const float expanded = capsuleRadius + targetRadius;
    const PhysicalContactVec3 previousMid = (previousGrip + previousTip) * 0.5f;
    const PhysicalContactVec3 currentMid = (currentGrip + currentTip) * 0.5f;
    const std::array<std::pair<PhysicalContactVec3, PhysicalContactVec3>, 5>
        sweeps{{
            {previousGrip, currentGrip},
            {previousMid, currentMid},
            {previousTip, currentTip},
            {previousGrip, previousTip},
            {currentGrip, currentTip},
        }};
    for (const auto& sweep : sweeps)
    {
        const PhysicalContactHit candidate = PhysicalContactSweepPoint(
            sweep.first, sweep.second, targetCenter, expanded);
        if (candidate.hit && (!best.hit || candidate.fraction < best.fraction))
            best = candidate;
    }
    return best;
}

enum class PhysicalContactAction : uint8_t
{
    None = 0,
    ImpulseOnly,
    ImpulseAndMelee,
};

inline PhysicalContactAction PhysicalContactClassify(
    float speedMetersPerSecond, float meleeThresholdMetersPerSecond)
{
    if (!std::isfinite(speedMetersPerSecond) ||
        !std::isfinite(meleeThresholdMetersPerSecond) ||
        speedMetersPerSecond < 0.05f)
        return PhysicalContactAction::None;
    return speedMetersPerSecond >= meleeThresholdMetersPerSecond
        ? PhysicalContactAction::ImpulseAndMelee
        : PhysicalContactAction::ImpulseOnly;
}

inline bool PhysicalContactMeleeReady(
    PhysicalContactAction action, bool nativeBindingsAvailable, bool armed,
    uint64_t nowMs, uint64_t lastMeleeMs)
{
    return action == PhysicalContactAction::ImpulseAndMelee &&
        nativeBindingsAvailable && armed && nowMs >= lastMeleeMs &&
        nowMs - lastMeleeMs >= 250;
}

inline bool PhysicalContactMovableKind(uint8_t kind)
{
    // biped, vehicle, weapon, equipment, garbage, crate, creature, giant.
    return kind <= 4 || kind == 11 || kind == 12 || kind == 13;
}

inline float PhysicalContactImpulseDeltaMetersPerSecond(float speed)
{
    return std::clamp(speed * 0.5f, 0.0f, 1.5f);
}

struct PhysicalContactPushResponse
{
    bool apply = false;
    float approachMetersPerSecond = 0.0f;
    float desiredMetersPerSecond = 0.0f;
    PhysicalContactVec3 worldVelocity{};
};

// A contact constraint, not a hit impulse. Match only a small, capped portion
// of the weapon's velocity into the surface and preserve every tangential
// component Halo already owns. Re-evaluating this from native target velocity
// on each overlapping sample lets a resting prop follow a gentle hand while
// preventing velocity from accumulating once it is already moving with it.
inline PhysicalContactPushResponse PhysicalContactStablePush(
    PhysicalContactVec3 targetWorldVelocity,
    PhysicalContactVec3 relativeMetersPerSecond,
    PhysicalContactVec3 contactNormal, float worldUnitsPerMeter)
{
    PhysicalContactPushResponse result{};
    if (!PhysicalContactFinite(targetWorldVelocity) ||
        !PhysicalContactFinite(relativeMetersPerSecond) ||
        !PhysicalContactFinite(contactNormal) ||
        !std::isfinite(worldUnitsPerMeter) || worldUnitsPerMeter <= 0.0f)
        return result;

    const PhysicalContactVec3 relativeDirection = PhysicalContactNormalize(
        relativeMetersPerSecond, {1.0f, 0.0f, 0.0f});
    PhysicalContactVec3 outward = PhysicalContactNormalize(
        contactNormal, relativeDirection * -1.0f);
    // Native and fallback normals should face the incoming weapon. Correct a
    // reversed provider normal locally instead of ever pushing a target back
    // toward the hand.
    if (PhysicalContactDot(relativeMetersPerSecond, outward) > 0.0f)
        outward = outward * -1.0f;
    const PhysicalContactVec3 pushDirection = outward * -1.0f;
    const float approach = PhysicalContactDot(
        relativeMetersPerSecond, pushDirection);
    if (!std::isfinite(approach) || approach < 0.05f)
        return result;

    constexpr float kFollowFraction = 0.25f;
    constexpr float kMaximumPushMetersPerSecond = 0.30f;
    constexpr float kMaximumCorrectionPerSample = 0.08f;
    const float desired = std::min(
        approach * kFollowFraction, kMaximumPushMetersPerSecond);
    const float current = PhysicalContactDot(
        targetWorldVelocity, pushDirection) / worldUnitsPerMeter;
    const float correction = std::clamp(
        desired - current, 0.0f, kMaximumCorrectionPerSample);
    if (!std::isfinite(current) || correction <= 1.0e-5f)
        return result;

    result.worldVelocity = targetWorldVelocity +
        pushDirection * (correction * worldUnitsPerMeter);
    result.approachMetersPerSecond = approach;
    result.desiredMetersPerSecond = desired;
    result.apply = PhysicalContactFinite(result.worldVelocity);
    return result;
}

// Retail Halo 3 game-options enums: mode 1 campaign, mode 2 multiplayer.
// Halo 3 MCC's solo Forge host reports simulation 5 (distributed server), not
// simulation 1 (local). Admit the authoritative Forge host and reject every
// client and synchronous role. The feature remains opt-in and the launcher is
// the anti-cheat-disabled mod launcher.
inline bool PhysicalContactGameModeAllowed(
    uint8_t gameMode, uint8_t gameSimulation, bool cooperative)
{
    return (gameMode == 1 && !cooperative) ||
        (gameMode == 2 && gameSimulation == 5);
}

struct PhysicalContactTargetState
{
    int32_t handle = -1;
    bool overlapping = false;
    bool previouslyOverlapping = false;
    bool meleeArmed = true;
    uint64_t belowHalfSinceMs = 0;
};

class PhysicalContactDebounce
{
public:
    static constexpr size_t kCapacity = 32;

    void BeginSample()
    {
        for (auto& slot : slots_)
        {
            slot.previouslyOverlapping = slot.overlapping;
            slot.overlapping = false;
        }
    }

    PhysicalContactTargetState* Touch(int32_t handle, bool* firstContact = nullptr)
    {
        PhysicalContactTargetState* empty = nullptr;
        for (auto& slot : slots_)
        {
            if (slot.handle == handle)
            {
                if (firstContact)
                    *firstContact = !slot.previouslyOverlapping;
                slot.overlapping = true;
                return &slot;
            }
            if (!empty && slot.handle == -1)
                empty = &slot;
        }
        if (!empty)
            empty = &slots_[replacement_++ % slots_.size()];
        *empty = {};
        empty->handle = handle;
        empty->overlapping = true;
        if (firstContact)
            *firstContact = true;
        return empty;
    }

    void EndSample()
    {
        for (auto& slot : slots_)
            if (slot.handle != -1 && !slot.overlapping)
                slot = {};
    }

    void Reset()
    {
        slots_ = {};
        replacement_ = 0;
    }

private:
    std::array<PhysicalContactTargetState, kCapacity> slots_{};
    size_t replacement_ = 0;
};
