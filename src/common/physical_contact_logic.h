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

// Rotate an OpenXR local-space vector into Halo's on-foot world axes. This is
// the same proper rotation used for controller displacement. Keeping it here
// makes contact velocity independent from the visible weapon's idle, recoil,
// and authored animation.
inline bool PhysicalContactTrackingVectorToGame(
    PhysicalContactVec3 tracking, float headYawReference,
    float gameYawReference, PhysicalContactVec3& game)
{
    game = {};
    if (!PhysicalContactFinite(tracking) ||
        !std::isfinite(headYawReference) ||
        !std::isfinite(gameYawReference))
        return false;
    const float sh = std::sin(headYawReference);
    const float ch = std::cos(headYawReference);
    const float roomForward = tracking.x * sh - tracking.z * ch;
    const float roomRight = tracking.x * ch + tracking.z * sh;
    const float cg = std::cos(gameYawReference);
    const float sg = std::sin(gameYawReference);
    game = {
        cg * roomForward + sg * roomRight,
        sg * roomForward - cg * roomRight,
        tracking.y};
    return PhysicalContactFinite(game);
}

// Use tracked hand motion at the exact authored weapon contact point. The
// visible transform still supplies collision geometry and the world-space
// lever arm. It never supplies velocity, so Halo animation cannot create a
// push or qualify as a melee strike.
inline PhysicalContactPointVelocity PhysicalContactTrackedPointVelocity(
    PhysicalContactVec3 weaponLinearMetersPerSecond,
    PhysicalContactVec3 weaponAngularRadiansPerSecond,
    PhysicalContactVec3 weaponPivotWorld,
    PhysicalContactVec3 weaponContactWorld,
    PhysicalContactVec3 targetLinearWorldUnitsPerSecond,
    PhysicalContactVec3 targetAngularRadiansPerSecond,
    PhysicalContactVec3 targetCenterWorld, float worldUnitsPerMeter)
{
    PhysicalContactPointVelocity result{};
    if (!PhysicalContactFinite(weaponLinearMetersPerSecond) ||
        !PhysicalContactFinite(weaponAngularRadiansPerSecond) ||
        !PhysicalContactFinite(weaponPivotWorld) ||
        !PhysicalContactFinite(weaponContactWorld) ||
        !PhysicalContactFinite(targetLinearWorldUnitsPerSecond) ||
        !PhysicalContactFinite(targetAngularRadiansPerSecond) ||
        !PhysicalContactFinite(targetCenterWorld) ||
        !std::isfinite(worldUnitsPerMeter) || worldUnitsPerMeter <= 0.0f)
        return result;

    const float metersPerWorldUnit = 1.0f / worldUnitsPerMeter;
    const PhysicalContactVec3 weaponLeverMeters =
        (weaponContactWorld - weaponPivotWorld) * metersPerWorldUnit;
    result.weaponMetersPerSecond = weaponLinearMetersPerSecond +
        PhysicalContactCross(
            weaponAngularRadiansPerSecond, weaponLeverMeters);
    result.targetMetersPerSecond =
        (targetLinearWorldUnitsPerSecond +
         PhysicalContactCross(
             targetAngularRadiansPerSecond,
             weaponContactWorld - targetCenterWorld)) *
        metersPerWorldUnit;
    result.relativeMetersPerSecond =
        result.weaponMetersPerSecond - result.targetMetersPerSecond;
    result.valid = PhysicalContactFinite(result.weaponMetersPerSecond) &&
        PhysicalContactFinite(result.targetMetersPerSecond) &&
        PhysicalContactFinite(result.relativeMetersPerSecond);
    return result;
}

struct PhysicalContactTransform
{
    PhysicalContactVec3 position{};
    PhysicalContactVec3 forward{1.0f, 0.0f, 0.0f};
    PhysicalContactVec3 left{0.0f, 1.0f, 0.0f};
    PhysicalContactVec3 up{0.0f, 0.0f, 1.0f};
    float scale = 1.0f;
};

inline bool PhysicalContactTransformFinite(const PhysicalContactTransform& t)
{
    return PhysicalContactFinite(t.position) &&
        PhysicalContactFinite(t.forward) && PhysicalContactFinite(t.left) &&
        PhysicalContactFinite(t.up) && std::isfinite(t.scale) &&
        t.scale > 1.0e-4f && t.scale < 1000.0f;
}

inline PhysicalContactVec3 PhysicalContactTransformVector(
    const PhysicalContactTransform& t, PhysicalContactVec3 local)
{
    return (t.forward * local.x + t.left * local.y + t.up * local.z) *
        t.scale;
}

inline PhysicalContactVec3 PhysicalContactTransformPoint(
    const PhysicalContactTransform& t, PhysicalContactVec3 local)
{
    return t.position + PhysicalContactTransformVector(t, local);
}

inline PhysicalContactVec3 PhysicalContactInverseTransformVector(
    const PhysicalContactTransform& t, PhysicalContactVec3 world)
{
    const float inverseScale = 1.0f / t.scale;
    return {
        PhysicalContactDot(world, t.forward) * inverseScale,
        PhysicalContactDot(world, t.left) * inverseScale,
        PhysicalContactDot(world, t.up) * inverseScale};
}

inline PhysicalContactVec3 PhysicalContactInverseTransformPoint(
    const PhysicalContactTransform& t, PhysicalContactVec3 world)
{
    return PhysicalContactInverseTransformVector(t, world - t.position);
}

inline PhysicalContactTransform PhysicalContactInterpolateTransform(
    const PhysicalContactTransform& from, const PhysicalContactTransform& to,
    float fraction)
{
    const float t = std::clamp(fraction, 0.0f, 1.0f);
    PhysicalContactTransform result{};
    result.position = from.position + (to.position - from.position) * t;
    result.scale = from.scale + (to.scale - from.scale) * t;
    result.forward = PhysicalContactNormalize(
        from.forward + (to.forward - from.forward) * t, to.forward);
    const PhysicalContactVec3 upHint = PhysicalContactNormalize(
        from.up + (to.up - from.up) * t, to.up);
    result.left = PhysicalContactNormalize(
        PhysicalContactCross(upHint, result.forward), to.left);
    result.up = PhysicalContactNormalize(
        PhysicalContactCross(result.forward, result.left), to.up);
    return result;
}

struct PhysicalContactConvexShape
{
    static constexpr size_t kMaximumVertices = 256;
    std::array<PhysicalContactVec3, kMaximumVertices> vertices{};
    uint16_t vertexCount = 0;
    float radius = 0.0f;
};

struct PhysicalContactCompoundShape
{
    static constexpr size_t kMaximumChildren = 8;
    std::array<PhysicalContactConvexShape, kMaximumChildren> children{};
    uint16_t childCount = 0;
};

inline bool PhysicalContactConvexValid(const PhysicalContactConvexShape& shape)
{
    if (!shape.vertexCount ||
        shape.vertexCount > PhysicalContactConvexShape::kMaximumVertices ||
        !std::isfinite(shape.radius) || shape.radius < 0.0f ||
        shape.radius > 10.0f)
        return false;
    for (uint16_t i = 0; i < shape.vertexCount; ++i)
        if (!PhysicalContactFinite(shape.vertices[i]))
            return false;
    return true;
}

inline bool PhysicalContactCompoundValid(
    const PhysicalContactCompoundShape& shape)
{
    if (!shape.childCount ||
        shape.childCount > PhysicalContactCompoundShape::kMaximumChildren)
        return false;
    for (uint16_t i = 0; i < shape.childCount; ++i)
        if (!PhysicalContactConvexValid(shape.children[i]))
            return false;
    return true;
}

// Native Halo collision queries resolve animated bipeds and other complex
// targets. Publish every authored vertex plus one centre per disjoint child as
// fixed material points. Their motion fractions remain directly comparable.
inline size_t PhysicalContactCompoundSamplePoints(
    const PhysicalContactCompoundShape& shape,
    PhysicalContactVec3* samples, size_t capacity)
{
    if (!PhysicalContactCompoundValid(shape) || !samples)
        return 0;
    size_t required = shape.childCount;
    for (uint16_t childIndex = 0;
         childIndex < shape.childCount; ++childIndex)
        required += shape.children[childIndex].vertexCount;
    if (capacity < required)
        return 0;

    size_t count = 0;
    for (uint16_t childIndex = 0;
         childIndex < shape.childCount; ++childIndex)
    {
        const PhysicalContactConvexShape& child =
            shape.children[childIndex];
        PhysicalContactVec3 centre{};
        for (uint16_t vertexIndex = 0;
             vertexIndex < child.vertexCount; ++vertexIndex)
        {
            samples[count++] = child.vertices[vertexIndex];
            centre = centre + child.vertices[vertexIndex];
        }
        samples[count++] = centre *
            (1.0f / static_cast<float>(child.vertexCount));
    }
    return count;
}

inline float PhysicalContactConvexBoundRadius(
    const PhysicalContactConvexShape& shape)
{
    float radiusSquared = 0.0f;
    for (uint16_t i = 0; i < shape.vertexCount; ++i)
        radiusSquared = std::max(
            radiusSquared,
            PhysicalContactLengthSquared(shape.vertices[i]));
    return std::sqrt(radiusSquared) + shape.radius;
}

inline float PhysicalContactCompoundBoundRadius(
    const PhysicalContactCompoundShape& shape)
{
    float radius = 0.0f;
    for (uint16_t i = 0; i < shape.childCount; ++i)
        radius = std::max(
            radius, PhysicalContactConvexBoundRadius(shape.children[i]));
    return radius;
}

inline PhysicalContactVec3 PhysicalContactConvexSupport(
    const PhysicalContactConvexShape& shape,
    const PhysicalContactTransform& transform,
    PhysicalContactVec3 worldDirection)
{
    const PhysicalContactVec3 localDirection =
        PhysicalContactInverseTransformVector(transform, worldDirection);
    uint16_t best = 0;
    float bestProjection = PhysicalContactDot(
        shape.vertices[0], localDirection);
    for (uint16_t i = 1; i < shape.vertexCount; ++i)
    {
        const float projection = PhysicalContactDot(
            shape.vertices[i], localDirection);
        if (projection > bestProjection)
        {
            bestProjection = projection;
            best = i;
        }
    }
    PhysicalContactVec3 result = PhysicalContactTransformPoint(
        transform, shape.vertices[best]);
    const float scaledRadius = shape.radius * transform.scale;
    if (scaledRadius > 0.0f)
        result = result + PhysicalContactNormalize(worldDirection) *
            scaledRadius;
    return result;
}

struct PhysicalContactGjkSimplex
{
    std::array<PhysicalContactVec3, 4> points{};
    int count = 0;
};

inline PhysicalContactVec3 PhysicalContactPerpendicular(
    PhysicalContactVec3 value)
{
    PhysicalContactVec3 axis = std::fabs(value.x) < std::fabs(value.y)
        ? PhysicalContactVec3{1.0f, 0.0f, 0.0f}
        : PhysicalContactVec3{0.0f, 1.0f, 0.0f};
    PhysicalContactVec3 perpendicular = PhysicalContactCross(value, axis);
    if (PhysicalContactLengthSquared(perpendicular) <= 1.0e-10f)
        perpendicular = PhysicalContactCross(
            value, {0.0f, 0.0f, 1.0f});
    return PhysicalContactNormalize(perpendicular);
}

inline bool PhysicalContactGjkLine(
    PhysicalContactGjkSimplex& simplex, PhysicalContactVec3& direction)
{
    const PhysicalContactVec3 a = simplex.points[1];
    const PhysicalContactVec3 b = simplex.points[0];
    const PhysicalContactVec3 ab = b - a;
    const PhysicalContactVec3 ao = a * -1.0f;
    if (PhysicalContactDot(ab, ao) > 0.0f)
    {
        direction = PhysicalContactCross(
            PhysicalContactCross(ab, ao), ab);
        if (PhysicalContactLengthSquared(direction) <= 1.0e-12f)
            direction = PhysicalContactPerpendicular(ab);
    }
    else
    {
        simplex.points[0] = a;
        simplex.count = 1;
        direction = ao;
    }
    return false;
}

inline bool PhysicalContactGjkTriangle(
    PhysicalContactGjkSimplex& simplex, PhysicalContactVec3& direction)
{
    const PhysicalContactVec3 a = simplex.points[2];
    const PhysicalContactVec3 b = simplex.points[1];
    const PhysicalContactVec3 c = simplex.points[0];
    const PhysicalContactVec3 ab = b - a;
    const PhysicalContactVec3 ac = c - a;
    const PhysicalContactVec3 ao = a * -1.0f;
    const PhysicalContactVec3 abc = PhysicalContactCross(ab, ac);

    if (PhysicalContactDot(PhysicalContactCross(abc, ac), ao) > 0.0f)
    {
        if (PhysicalContactDot(ac, ao) > 0.0f)
        {
            simplex.points[0] = c;
            simplex.points[1] = a;
            simplex.count = 2;
            direction = PhysicalContactCross(
                PhysicalContactCross(ac, ao), ac);
        }
        else
        {
            simplex.points[0] = b;
            simplex.points[1] = a;
            simplex.count = 2;
            return PhysicalContactGjkLine(simplex, direction);
        }
        return false;
    }
    if (PhysicalContactDot(PhysicalContactCross(ab, abc), ao) > 0.0f)
    {
        simplex.points[0] = b;
        simplex.points[1] = a;
        simplex.count = 2;
        return PhysicalContactGjkLine(simplex, direction);
    }
    if (PhysicalContactDot(abc, ao) > 0.0f)
        direction = abc;
    else
    {
        simplex.points[0] = b;
        simplex.points[1] = c;
        direction = abc * -1.0f;
    }
    return false;
}

inline bool PhysicalContactGjkTetrahedron(
    PhysicalContactGjkSimplex& simplex, PhysicalContactVec3& direction)
{
    const PhysicalContactVec3 a = simplex.points[3];
    const PhysicalContactVec3 b = simplex.points[2];
    const PhysicalContactVec3 c = simplex.points[1];
    const PhysicalContactVec3 d = simplex.points[0];
    const PhysicalContactVec3 ao = a * -1.0f;
    const auto outsideFace = [&](PhysicalContactVec3 p,
                                 PhysicalContactVec3 q,
                                 PhysicalContactVec3 opposite,
                                 PhysicalContactVec3& normal) {
        normal = PhysicalContactCross(p - a, q - a);
        if (PhysicalContactDot(normal, opposite - a) > 0.0f)
            normal = normal * -1.0f;
        return PhysicalContactDot(normal, ao) > 0.0f;
    };
    PhysicalContactVec3 normal{};
    if (outsideFace(b, c, d, normal))
    {
        simplex.points[0] = c;
        simplex.points[1] = b;
        simplex.points[2] = a;
        simplex.count = 3;
        direction = normal;
        return false;
    }
    if (outsideFace(c, d, b, normal))
    {
        simplex.points[0] = d;
        simplex.points[1] = c;
        simplex.points[2] = a;
        simplex.count = 3;
        direction = normal;
        return false;
    }
    if (outsideFace(d, b, c, normal))
    {
        simplex.points[0] = b;
        simplex.points[1] = d;
        simplex.points[2] = a;
        simplex.count = 3;
        direction = normal;
        return false;
    }
    return true;
}

inline bool PhysicalContactGjkContainsOrigin(
    PhysicalContactGjkSimplex& simplex, PhysicalContactVec3& direction)
{
    if (simplex.count == 2)
        return PhysicalContactGjkLine(simplex, direction);
    if (simplex.count == 3)
        return PhysicalContactGjkTriangle(simplex, direction);
    return simplex.count == 4 &&
        PhysicalContactGjkTetrahedron(simplex, direction);
}

inline bool PhysicalContactConvexIntersect(
    const PhysicalContactConvexShape& a,
    const PhysicalContactTransform& transformA,
    const PhysicalContactConvexShape& b,
    const PhysicalContactTransform& transformB,
    PhysicalContactVec3* separatingDirection = nullptr)
{
    PhysicalContactVec3 direction = transformB.position - transformA.position;
    if (PhysicalContactLengthSquared(direction) <= 1.0e-10f)
        direction = {1.0f, 0.0f, 0.0f};
    PhysicalContactGjkSimplex simplex{};
    const auto support = [&](PhysicalContactVec3 d) {
        return PhysicalContactConvexSupport(a, transformA, d) -
            PhysicalContactConvexSupport(b, transformB, d * -1.0f);
    };
    simplex.points[0] = support(direction);
    simplex.count = 1;
    direction = simplex.points[0] * -1.0f;
    for (int iteration = 0; iteration < 32; ++iteration)
    {
        if (!PhysicalContactFinite(direction))
            break;
        if (PhysicalContactLengthSquared(direction) <= 1.0e-12f)
            return true;
        const PhysicalContactVec3 point = support(direction);
        if (!PhysicalContactFinite(point) ||
            PhysicalContactDot(point, direction) < 0.0f)
        {
            if (separatingDirection)
                *separatingDirection = direction;
            return false;
        }
        simplex.points[simplex.count++] = point;
        if (PhysicalContactGjkContainsOrigin(simplex, direction))
            return true;
    }
    if (separatingDirection)
        *separatingDirection = direction;
    return false;
}

struct PhysicalContactConvexHit
{
    bool hit = false;
    bool normalReliable = false;
    float fraction = 1.0f;
    PhysicalContactVec3 point{};
    PhysicalContactVec3 weaponPoint{};
    PhysicalContactVec3 targetPoint{};
    PhysicalContactVec3 normal{1.0f, 0.0f, 0.0f};
};

inline PhysicalContactConvexHit PhysicalContactSweepConvex(
    const PhysicalContactConvexShape& weapon,
    const PhysicalContactTransform& previousWeaponTransform,
    const PhysicalContactTransform& currentWeaponTransform,
    const PhysicalContactConvexShape& target,
    const PhysicalContactTransform& targetTransform)
{
    PhysicalContactConvexHit result{};
    if (!PhysicalContactConvexValid(weapon) ||
        !PhysicalContactConvexValid(target) ||
        !PhysicalContactTransformFinite(previousWeaponTransform) ||
        !PhysicalContactTransformFinite(currentWeaponTransform) ||
        !PhysicalContactTransformFinite(targetTransform))
        return result;

    PhysicalContactVec3 separation{};
    if (PhysicalContactConvexIntersect(
            weapon, previousWeaponTransform, target, targetTransform,
            &separation))
    {
        result.hit = true;
        result.fraction = 0.0f;
    }
    else
    {
        const float translation = PhysicalContactLength(
            currentWeaponTransform.position -
            previousWeaponTransform.position);
        const float axisChange = std::max({
            PhysicalContactLength(currentWeaponTransform.forward -
                                  previousWeaponTransform.forward),
            PhysicalContactLength(currentWeaponTransform.left -
                                  previousWeaponTransform.left),
            PhysicalContactLength(currentWeaponTransform.up -
                                  previousWeaponTransform.up)});
        const float sweptDistance = translation + axisChange *
            PhysicalContactConvexBoundRadius(weapon) *
                std::max(previousWeaponTransform.scale,
                         currentWeaponTransform.scale);
        const int steps = std::clamp(
            static_cast<int>(std::ceil(sweptDistance / 0.01f)), 1, 32);
        float low = 0.0f;
        float high = 1.0f;
        bool found = false;
        for (int step = 1; step <= steps; ++step)
        {
            const float fraction = static_cast<float>(step) /
                static_cast<float>(steps);
            const PhysicalContactTransform transform =
                PhysicalContactInterpolateTransform(
                    previousWeaponTransform, currentWeaponTransform,
                    fraction);
            PhysicalContactVec3 candidateSeparation{};
            if (PhysicalContactConvexIntersect(
                    weapon, transform, target, targetTransform,
                    &candidateSeparation))
            {
                high = fraction;
                low = static_cast<float>(step - 1) /
                    static_cast<float>(steps);
                found = true;
                break;
            }
            separation = candidateSeparation;
        }
        if (!found)
            return result;
        for (int iteration = 0; iteration < 9; ++iteration)
        {
            const float middle = (low + high) * 0.5f;
            const PhysicalContactTransform transform =
                PhysicalContactInterpolateTransform(
                    previousWeaponTransform, currentWeaponTransform, middle);
            PhysicalContactVec3 candidateSeparation{};
            if (PhysicalContactConvexIntersect(
                    weapon, transform, target, targetTransform,
                    &candidateSeparation))
                high = middle;
            else
            {
                low = middle;
                separation = candidateSeparation;
            }
        }
        result.hit = true;
        result.fraction = high;
        result.normalReliable =
            PhysicalContactLengthSquared(separation) > 1.0e-12f;
    }

    const PhysicalContactTransform impact =
        PhysicalContactInterpolateTransform(
            previousWeaponTransform, currentWeaponTransform,
            result.fraction);
    const PhysicalContactVec3 fallbackNormal = PhysicalContactNormalize(
        impact.position - targetTransform.position,
        currentWeaponTransform.position - previousWeaponTransform.position);
    result.normal = PhysicalContactNormalize(
        separation * -1.0f, fallbackNormal);
    result.weaponPoint = PhysicalContactConvexSupport(
        weapon, impact, result.normal * -1.0f);
    result.targetPoint = PhysicalContactConvexSupport(
        target, targetTransform, result.normal);
    result.point = (result.weaponPoint + result.targetPoint) * 0.5f;
    return result;
}

struct PhysicalContactCompoundHit : PhysicalContactConvexHit
{
    uint16_t weaponChild = 0;
    uint16_t targetChild = 0;
};

inline PhysicalContactCompoundHit PhysicalContactSweepCompound(
    const PhysicalContactCompoundShape& weapon,
    const PhysicalContactTransform& previousWeaponTransform,
    const PhysicalContactTransform& currentWeaponTransform,
    const PhysicalContactCompoundShape& target,
    const PhysicalContactTransform& targetTransform)
{
    PhysicalContactCompoundHit closest{};
    if (!PhysicalContactCompoundValid(weapon) ||
        !PhysicalContactCompoundValid(target))
        return closest;
    for (uint16_t weaponChild = 0;
         weaponChild < weapon.childCount; ++weaponChild)
    {
        for (uint16_t targetChild = 0;
             targetChild < target.childCount; ++targetChild)
        {
            const PhysicalContactConvexHit candidate =
                PhysicalContactSweepConvex(
                    weapon.children[weaponChild], previousWeaponTransform,
                    currentWeaponTransform, target.children[targetChild],
                    targetTransform);
            if (!candidate.hit ||
                (closest.hit &&
                 candidate.fraction >= closest.fraction - 1.0e-6f))
                continue;
            static_cast<PhysicalContactConvexHit&>(closest) = candidate;
            closest.weaponChild = weaponChild;
            closest.targetChild = targetChild;
        }
    }
    return closest;
}

inline PhysicalContactPointVelocity PhysicalContactRigidPointVelocity(
    const PhysicalContactTransform& previousWeaponTransform,
    const PhysicalContactTransform& currentWeaponTransform,
    PhysicalContactVec3 currentWeaponPoint,
    PhysicalContactVec3 targetLinearVelocity,
    PhysicalContactVec3 targetAngularVelocity,
    PhysicalContactVec3 targetCenter, float elapsedSeconds,
    float worldUnitsPerMeter)
{
    PhysicalContactPointVelocity result{};
    if (!PhysicalContactTransformFinite(previousWeaponTransform) ||
        !PhysicalContactTransformFinite(currentWeaponTransform) ||
        !PhysicalContactFinite(currentWeaponPoint) ||
        !PhysicalContactFinite(targetLinearVelocity) ||
        !PhysicalContactFinite(targetAngularVelocity) ||
        !PhysicalContactFinite(targetCenter) ||
        !std::isfinite(elapsedSeconds) || elapsedSeconds <= 0.0f ||
        elapsedSeconds > 0.1f || !std::isfinite(worldUnitsPerMeter) ||
        worldUnitsPerMeter <= 0.0f)
        return result;
    const PhysicalContactVec3 localPoint =
        PhysicalContactInverseTransformPoint(
            currentWeaponTransform, currentWeaponPoint);
    const PhysicalContactVec3 previousWeaponPoint =
        PhysicalContactTransformPoint(previousWeaponTransform, localPoint);
    const float metersPerWorldUnit = 1.0f / worldUnitsPerMeter;
    result.weaponMetersPerSecond =
        (currentWeaponPoint - previousWeaponPoint) *
        (metersPerWorldUnit / elapsedSeconds);
    const PhysicalContactVec3 angularPointVelocity = PhysicalContactCross(
        targetAngularVelocity, currentWeaponPoint - targetCenter);
    result.targetMetersPerSecond =
        (targetLinearVelocity + angularPointVelocity) * metersPerWorldUnit;
    result.relativeMetersPerSecond =
        result.weaponMetersPerSecond - result.targetMetersPerSecond;
    result.valid = PhysicalContactFinite(result.weaponMetersPerSecond) &&
        PhysicalContactFinite(result.targetMetersPerSecond) &&
        PhysicalContactFinite(result.relativeMetersPerSecond);
    return result;
}

struct PhysicalContactWallConstraint
{
    bool constrained = false;
    float setbackWorldUnits = 0.0f;
    PhysicalContactVec3 offset{};
};

struct PhysicalContactWallPlane
{
    PhysicalContactVec3 weaponPoint{};
    PhysicalContactVec3 surfacePoint{};
    PhysicalContactVec3 freeSideNormal{1.0f, 0.0f, 0.0f};
    float clearanceWorldUnits = 0.0f;
};

// Project one rigid translation against every exact static surface plane.
// All normals face the camera-side free space. Four bounded passes resolve
// corners without moving separate weapon vertices by different amounts.
inline PhysicalContactWallConstraint PhysicalContactSolveWallPlanes(
    const PhysicalContactWallPlane* planes, size_t planeCount,
    float maximumOffsetWorldUnits)
{
    PhysicalContactWallConstraint result{};
    if (!planes || !planeCount || planeCount > 256 ||
        !std::isfinite(maximumOffsetWorldUnits) ||
        maximumOffsetWorldUnits <= 0.0f)
        return result;

    PhysicalContactVec3 offset{};
    bool constrained = false;
    for (int pass = 0; pass < 4; ++pass)
    {
        bool changed = false;
        for (size_t i = 0; i < planeCount; ++i)
        {
            const PhysicalContactWallPlane& plane = planes[i];
            if (!PhysicalContactFinite(plane.weaponPoint) ||
                !PhysicalContactFinite(plane.surfacePoint) ||
                !PhysicalContactFinite(plane.freeSideNormal) ||
                !std::isfinite(plane.clearanceWorldUnits) ||
                plane.clearanceWorldUnits < 0.0f)
                return {};
            const PhysicalContactVec3 normal = PhysicalContactNormalize(
                plane.freeSideNormal, {});
            if (PhysicalContactLengthSquared(normal) <= 1.0e-12f)
                return {};
            const float signedClearance = PhysicalContactDot(
                plane.weaponPoint + offset - plane.surfacePoint, normal);
            const float deficit =
                plane.clearanceWorldUnits - signedClearance;
            if (!std::isfinite(deficit))
                return {};
            if (deficit > 1.0e-5f)
            {
                offset = offset + normal * deficit;
                changed = true;
                constrained = true;
            }
        }
        if (!changed)
            break;
    }
    const float length = PhysicalContactLength(offset);
    if (!constrained || !std::isfinite(length) || length <= 1.0e-5f)
        return result;
    if (length > maximumOffsetWorldUnits)
        offset = offset * (maximumOffsetWorldUnits / length);
    result.constrained = PhysicalContactFinite(offset);
    result.offset = offset;
    result.setbackWorldUnits = PhysicalContactLength(offset);
    return result;
}

// Convert a native camera-to-weapon structure hit into one rigid translation
// for the whole visible weapon. The clearance is measured back from the hit
// along the same camera ray, so a blocked tip is pulled toward the player's
// view instead of clipped or shortened independently from the grip.
inline PhysicalContactWallConstraint PhysicalContactWallOffsetForRay(
    PhysicalContactVec3 camera, PhysicalContactVec3 weaponPoint,
    float hitFraction, float clearanceWorldUnits)
{
    PhysicalContactWallConstraint result{};
    if (!PhysicalContactFinite(camera) ||
        !PhysicalContactFinite(weaponPoint) ||
        !std::isfinite(hitFraction) || hitFraction < 0.0f ||
        hitFraction > 1.0f || !std::isfinite(clearanceWorldUnits) ||
        clearanceWorldUnits < 0.0f)
        return result;

    const PhysicalContactVec3 ray = weaponPoint - camera;
    const float length = PhysicalContactLength(ray);
    if (!std::isfinite(length) || length <= 1.0e-5f)
        return result;
    const float allowedDistance = std::clamp(
        length * hitFraction - clearanceWorldUnits, 0.0f, length);
    const float setback = length - allowedDistance;
    if (!std::isfinite(setback) || setback <= 1.0e-5f)
        return result;

    result.offset = ray * (-setback / length);
    result.setbackWorldUnits = setback;
    result.constrained = PhysicalContactFinite(result.offset);
    return result;
}

// Blocking engages in one update so the weapon cannot visibly tunnel. Release
// moves toward the unconstrained controller pose at a bounded physical speed,
// avoiding a one-frame pop once the muzzle clears an edge.
inline PhysicalContactVec3 PhysicalContactUpdateWallOffset(
    PhysicalContactVec3 currentOffset, PhysicalContactVec3 requestedOffset,
    bool structureBlocked, float elapsedSeconds, float worldUnitsPerMeter)
{
    if (!PhysicalContactFinite(currentOffset) ||
        !PhysicalContactFinite(requestedOffset) ||
        !std::isfinite(elapsedSeconds) || elapsedSeconds < 0.0f ||
        elapsedSeconds > 0.1f || !std::isfinite(worldUnitsPerMeter) ||
        worldUnitsPerMeter <= 0.0f)
        return {};
    if (structureBlocked)
        return requestedOffset;

    const float currentLength = PhysicalContactLength(currentOffset);
    if (!std::isfinite(currentLength) || currentLength <= 1.0e-5f)
        return {};
    constexpr float kReleaseMetersPerSecond = 1.50f;
    const float release = std::min(
        currentLength,
        kReleaseMetersPerSecond * worldUnitsPerMeter * elapsedSeconds);
    return currentOffset * ((currentLength - release) / currentLength);
}

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
    float relativeSpeedMetersPerSecond, float weaponSpeedMetersPerSecond,
    float meleeThresholdMetersPerSecond)
{
    if (!std::isfinite(relativeSpeedMetersPerSecond) ||
        !std::isfinite(weaponSpeedMetersPerSecond) ||
        !std::isfinite(meleeThresholdMetersPerSecond) ||
        relativeSpeedMetersPerSecond < 0.05f)
        return PhysicalContactAction::None;
    return weaponSpeedMetersPerSecond >= meleeThresholdMetersPerSecond
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

struct PhysicalContactDebugTargetRank
{
    bool valid = false;
    int priority = -1;
    float score = 0.0f;
};

// The automated Forge rig needs a settled light prop. The nearest weapon can
// be a falling hammer or launcher, whose own velocity makes a slow-hand test
// measure a large relative impact. Prefer a near-stationary weapon around 2 kg,
// then any weapon, then another movable root. An existing anchor always wins.
inline PhysicalContactDebugTargetRank PhysicalContactRankDebugTarget(
    uint8_t kind, float massKilograms, float speedMetersPerSecond,
    float distanceSquaredWorldUnits, bool anchored)
{
    PhysicalContactDebugTargetRank result{};
    if (!std::isfinite(massKilograms) || massKilograms <= 0.001f ||
        !std::isfinite(speedMetersPerSecond) || speedMetersPerSecond < 0.0f ||
        !std::isfinite(distanceSquaredWorldUnits) ||
        distanceSquaredWorldUnits < 0.25f)
        return result;
    const bool stableLightWeapon = kind == 2 && massKilograms <= 5.0f &&
        speedMetersPerSecond <= 0.25f;
    result.priority = anchored
        ? 3 : (stableLightWeapon ? 2 : (kind == 2 ? 1 : 0));
    result.score = anchored
        ? 0.0f
        : speedMetersPerSecond + std::fabs(massKilograms - 2.0f) * 0.02f +
          distanceSquaredWorldUnits * 0.001f;
    result.valid = std::isfinite(result.score);
    return result;
}

struct PhysicalContactDebugScoopPose
{
    float liftMeters = 0.0f;
    float carryMeters = 0.0f;
    float releaseMeters = 0.0f;
    float liftMetersPerSecond = 0.0f;
    float carryMetersPerSecond = 0.0f;
    float releaseMetersPerSecond = 0.0f;
};

inline PhysicalContactDebugScoopPose PhysicalContactDebugScoopTrajectory(
    float elapsedMilliseconds)
{
    if (!std::isfinite(elapsedMilliseconds) || elapsedMilliseconds < 0.0f)
        return {};
    const auto smoothStep = [](float value) -> float
    {
        const float t = std::clamp(value, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    };
    const auto smoothStepVelocity = [](float value, float durationSeconds)
        -> float
    {
        if (value <= 0.0f || value >= 1.0f)
            return 0.0f;
        return 6.0f * value * (1.0f - value) / durationSeconds;
    };
    const float liftPhase = (elapsedMilliseconds - 1000.0f) / 1500.0f;
    const float carryPhase = (elapsedMilliseconds - 2500.0f) / 1500.0f;
    const float releasePhase = (elapsedMilliseconds - 4000.0f) / 750.0f;
    return {
        0.35f * smoothStep(liftPhase),
        0.35f * smoothStep(carryPhase),
        0.30f * smoothStep(releasePhase),
        0.35f * smoothStepVelocity(liftPhase, 1.5f),
        0.35f * smoothStepVelocity(carryPhase, 1.5f),
        0.30f * smoothStepVelocity(releasePhase, 0.75f)};
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

struct PhysicalContactMassImpulse
{
    bool apply = false;
    float approachMetersPerSecond = 0.0f;
    float impulseKilogramMetersPerSecond = 0.0f;
    PhysicalContactVec3 worldImpulse{};
};

// Havok stores inverse mass in hkpMotion. H3EK's hkpRigidBody::getMass leaf
// reads that field and returns 1.0 / inverseMass. Keep this conversion in one
// tested helper so a heavy body cannot be mistaken for a light body.
inline float PhysicalContactMassFromInverseMass(float inverseMass)
{
    if (!std::isfinite(inverseMass) || inverseMass <= 1.0e-6f)
        return 0.0f;
    const float mass = 1.0f / inverseMass;
    return std::isfinite(mass) && mass > 0.001f && mass <= 1000000.0f
        ? mass : 0.0f;
}

// Official H3EK's hkpMotion constructors and getter prove the byte values.
// Keyframed and fixed bodies have authored mass data but do not respond to a
// native impulse, so they must not enter the movable-body response.
inline bool PhysicalContactMotionTypeIsDynamic(uint8_t motionType)
{
    return (motionType >= 1 && motionType <= 5) || motionType == 8;
}

// Native object collision also reports scenery, machines, and fixed physics
// objects. They are exact visual blockers, but a dynamic rigid body must stay
// in the impulse path. If a valid root object has no resolvable movable body,
// fail closed for wall rendering and leave it untouched by physics contact.
inline bool PhysicalContactObjectBlocksAsStatic(
    bool validRootObject, bool excludedObject, bool bodyResolved,
    uint8_t motionType)
{
    if (!validRootObject || excludedObject)
        return false;
    return !bodyResolved || !PhysicalContactMotionTypeIsDynamic(motionType);
}

inline bool PhysicalContactObjectReceivesImpulse(
    bool validRootObject, bool excludedObject, bool bodyResolved,
    uint8_t motionType)
{
    return validRootObject && !excludedObject && bodyResolved &&
        PhysicalContactMotionTypeIsDynamic(motionType);
}

// A bounded inelastic collision response. Native Halo masses decide how much
// momentum the held weapon transfers. The engine's point-impulse function then
// uses the target's authored mass and inertia to produce linear and angular
// motion at the exact contact point.
inline PhysicalContactMassImpulse PhysicalContactMassAwareImpulse(
    float weaponMassKilograms, float targetMassKilograms,
    PhysicalContactVec3 relativeMetersPerSecond,
    PhysicalContactVec3 contactNormal, float worldUnitsPerMeter)
{
    PhysicalContactMassImpulse result{};
    if (!std::isfinite(weaponMassKilograms) ||
        !std::isfinite(targetMassKilograms) ||
        weaponMassKilograms <= 0.001f || targetMassKilograms <= 0.001f ||
        weaponMassKilograms > 1000000.0f ||
        targetMassKilograms > 1000000.0f ||
        !PhysicalContactFinite(relativeMetersPerSecond) ||
        !PhysicalContactFinite(contactNormal) ||
        !std::isfinite(worldUnitsPerMeter) || worldUnitsPerMeter <= 0.0f)
        return result;

    const PhysicalContactVec3 relativeDirection = PhysicalContactNormalize(
        relativeMetersPerSecond, {1.0f, 0.0f, 0.0f});
    PhysicalContactVec3 outward = PhysicalContactNormalize(
        contactNormal, relativeDirection * -1.0f);
    if (PhysicalContactDot(relativeMetersPerSecond, outward) > 0.0f)
        outward = outward * -1.0f;
    const PhysicalContactVec3 pushDirection = outward * -1.0f;
    const float approach = PhysicalContactDot(
        relativeMetersPerSecond, pushDirection);
    if (!std::isfinite(approach) || approach < 0.05f)
        return result;

    constexpr float kMaximumApproachMetersPerSecond = 8.0f;
    constexpr float kMaximumTargetDeltaMetersPerSecond = 2.5f;
    const float reducedMass =
        weaponMassKilograms * targetMassKilograms /
        (weaponMassKilograms + targetMassKilograms);
    float impulse = reducedMass * std::min(
        approach, kMaximumApproachMetersPerSecond);
    impulse = std::min(
        impulse,
        targetMassKilograms * kMaximumTargetDeltaMetersPerSecond);
    if (!std::isfinite(impulse) || impulse <= 1.0e-5f)
        return result;

    result.approachMetersPerSecond = approach;
    result.impulseKilogramMetersPerSecond = impulse;
    result.worldImpulse =
        pushDirection * (impulse * worldUnitsPerMeter);
    result.apply = PhysicalContactFinite(result.worldImpulse);
    return result;
}

struct PhysicalContactConstraintImpulse
{
    bool apply = false;
    float approachMetersPerSecond = 0.0f;
    float penetrationMeters = 0.0f;
    float normalImpulseKilogramMetersPerSecond = 0.0f;
    float tangentImpulseKilogramMetersPerSecond = 0.0f;
    PhysicalContactVec3 worldImpulse{};
};

// A sustained point-contact constraint for a kinematic VR weapon. Each sample
// corrects only a small part of the relative velocity. Penetration supplies a
// bounded normal load. Coulomb-limited tangential force then lets the weapon
// carry or scoop a light body without giving a heavy body the same response.
inline PhysicalContactConstraintImpulse PhysicalContactSustainedImpulse(
    float weaponMassKilograms, float targetMassKilograms,
    PhysicalContactVec3 relativeMetersPerSecond,
    PhysicalContactVec3 contactNormal, float penetrationMeters,
    float elapsedSeconds, float worldUnitsPerMeter)
{
    PhysicalContactConstraintImpulse result{};
    if (!std::isfinite(weaponMassKilograms) ||
        !std::isfinite(targetMassKilograms) ||
        weaponMassKilograms <= 0.001f || targetMassKilograms <= 0.001f ||
        weaponMassKilograms > 1000000.0f ||
        targetMassKilograms > 1000000.0f ||
        !PhysicalContactFinite(relativeMetersPerSecond) ||
        !PhysicalContactFinite(contactNormal) ||
        !std::isfinite(penetrationMeters) || penetrationMeters < 0.0f ||
        penetrationMeters > 10.0f || !std::isfinite(elapsedSeconds) ||
        elapsedSeconds <= 0.0f || elapsedSeconds > 0.1f ||
        !std::isfinite(worldUnitsPerMeter) || worldUnitsPerMeter <= 0.0f)
        return result;

    // The contact normal points from the target surface toward the weapon.
    // Keep that orientation stable while the bodies separate. Flipping it to
    // oppose every relative velocity would pull or kick a released object.
    const PhysicalContactVec3 outward = PhysicalContactNormalize(
        contactNormal, {});
    if (PhysicalContactLengthSquared(outward) <= 1.0e-12f)
        return result;
    const PhysicalContactVec3 inward = outward * -1.0f;
    const float signedApproach = PhysicalContactDot(
        relativeMetersPerSecond, inward);
    const float approach = std::max(signedApproach, 0.0f);
    const float reducedMass =
        weaponMassKilograms * targetMassKilograms /
        (weaponMassKilograms + targetMassKilograms);

    constexpr float kVelocityFollowFraction = 0.20f;
    constexpr float kPenetrationCorrectionFraction = 0.15f;
    constexpr float kMaximumPenetrationSpeed = 0.25f;
    constexpr float kMaximumTargetDeltaPerSample = 0.08f;
    constexpr float kFriction = 0.80f;
    const float penetrationSpeed = std::min(
        penetrationMeters * kPenetrationCorrectionFraction / elapsedSeconds,
        kMaximumPenetrationSpeed);
    const float normalCorrectionSpeed = std::max(
        approach * kVelocityFollowFraction, penetrationSpeed);
    float normalImpulse = reducedMass * normalCorrectionSpeed;
    normalImpulse = std::min(
        normalImpulse,
        targetMassKilograms * kMaximumTargetDeltaPerSample);

    const PhysicalContactVec3 tangentVelocity =
        relativeMetersPerSecond -
        outward * PhysicalContactDot(relativeMetersPerSecond, outward);
    const float tangentSpeed = PhysicalContactLength(tangentVelocity);
    float tangentImpulse = reducedMass * tangentSpeed *
        kVelocityFollowFraction;
    tangentImpulse = std::min(tangentImpulse, normalImpulse * kFriction);
    const PhysicalContactVec3 tangentDirection = tangentSpeed > 1.0e-5f
        ? tangentVelocity * (1.0f / tangentSpeed)
        : PhysicalContactVec3{};
    PhysicalContactVec3 impulse =
        inward * normalImpulse + tangentDirection * tangentImpulse;
    const float maximumImpulse =
        targetMassKilograms * kMaximumTargetDeltaPerSample;
    const float impulseLength = PhysicalContactLength(impulse);
    if (impulseLength > maximumImpulse)
    {
        const float scale = maximumImpulse / impulseLength;
        impulse = impulse * scale;
        normalImpulse *= scale;
        tangentImpulse *= scale;
    }
    if (!PhysicalContactFinite(impulse) ||
        PhysicalContactLengthSquared(impulse) <= 1.0e-10f)
        return result;

    result.approachMetersPerSecond = approach;
    result.penetrationMeters = penetrationMeters;
    result.normalImpulseKilogramMetersPerSecond = normalImpulse;
    result.tangentImpulseKilogramMetersPerSecond = tangentImpulse;
    result.worldImpulse = impulse * worldUnitsPerMeter;
    result.apply = PhysicalContactFinite(result.worldImpulse);
    return result;
}

// Convert the exact mass-aware point impulse into portable controller feedback.
// The square-root response keeps a light prop readable without letting a heavy
// vehicle saturate the controller. Native melee gets a clear minimum pulse.
inline float PhysicalContactHapticAmplitude(
    float normalImpulseKilogramMetersPerSecond,
    float tangentImpulseKilogramMetersPerSecond, bool melee)
{
    if (!std::isfinite(normalImpulseKilogramMetersPerSecond) ||
        !std::isfinite(tangentImpulseKilogramMetersPerSecond) ||
        normalImpulseKilogramMetersPerSecond < 0.0f ||
        tangentImpulseKilogramMetersPerSecond < 0.0f)
        return 0.0f;
    const float magnitude = std::sqrt(
        normalImpulseKilogramMetersPerSecond *
            normalImpulseKilogramMetersPerSecond +
        tangentImpulseKilogramMetersPerSecond *
            tangentImpulseKilogramMetersPerSecond);
    float amplitude = magnitude > 1.0e-6f
        ? std::clamp(0.08f + 0.55f * std::sqrt(magnitude), 0.0f, 0.60f)
        : 0.0f;
    if (melee)
        amplitude = std::max(amplitude, 0.75f);
    return amplitude;
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
    bool contactNormalValid = false;
    PhysicalContactVec3 contactNormal{1.0f, 0.0f, 0.0f};
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
