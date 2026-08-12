#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
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

inline bool PhysicalContactSegmentIntersectsExpandedAabb(
    PhysicalContactVec3 start, PhysicalContactVec3 end,
    PhysicalContactVec3 minimum, PhysicalContactVec3 maximum,
    float expansion)
{
    if (!PhysicalContactFinite(start) || !PhysicalContactFinite(end) ||
        !PhysicalContactFinite(minimum) || !PhysicalContactFinite(maximum) ||
        !std::isfinite(expansion) || expansion < 0.0f)
        return false;
    const float origins[3] = {start.x, start.y, start.z};
    const PhysicalContactVec3 delta = end - start;
    const float directions[3] = {delta.x, delta.y, delta.z};
    const float lower[3] = {
        minimum.x - expansion, minimum.y - expansion,
        minimum.z - expansion};
    const float upper[3] = {
        maximum.x + expansion, maximum.y + expansion,
        maximum.z + expansion};
    float first = 0.0f;
    float last = 1.0f;
    for (unsigned axis = 0; axis < 3u; ++axis)
    {
        if (lower[axis] > upper[axis])
            return false;
        if (std::fabs(directions[axis]) <= 1.0e-8f)
        {
            if (origins[axis] < lower[axis] || origins[axis] > upper[axis])
                return false;
            continue;
        }
        float nearFraction =
            (lower[axis] - origins[axis]) / directions[axis];
        float farFraction =
            (upper[axis] - origins[axis]) / directions[axis];
        if (nearFraction > farFraction)
            std::swap(nearFraction, farFraction);
        first = std::max(first, nearFraction);
        last = std::min(last, farFraction);
        if (first > last)
            return false;
    }
    return true;
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
    // The official Mongoose collision tag has ten simultaneous BSP children:
    // bumper, hull, four root fender pieces, and four animated wheels. Sixteen
    // keeps that complete visible shape bounded while the held-weapon census
    // still limits the other side of a sweep to four children.
    static constexpr size_t kMaximumChildren = 16;
    std::array<PhysicalContactConvexShape, kMaximumChildren> children{};
    uint16_t childCount = 0;
};

struct PhysicalContactTriangle
{
    std::array<PhysicalContactVec3, 3> vertices{};
    PhysicalContactVec3 centre{};
    PhysicalContactVec3 halfExtents{};
    float boundRadius = 0.0f;
};

struct PhysicalContactTriangleGroup
{
    PhysicalContactVec3 centre{};
    PhysicalContactVec3 halfExtents{};
    float boundRadius = 0.0f;
    uint16_t firstTriangle = 0;
    uint16_t triangleCount = 0;
};

struct PhysicalContactTriangleMesh
{
    // The official held-weapon census tops out at 222 triangles. The complete
    // default Mongoose collision model has 502 triangles in ten node-bound
    // BSPs. These fixed limits cover both without allocation in the hot path.
    static constexpr size_t kMaximumTriangles = 768;
    static constexpr size_t kMaximumGroups = 32;
    std::array<PhysicalContactTriangle, kMaximumTriangles> triangles{};
    std::array<PhysicalContactTriangleGroup, kMaximumGroups> groups{};
    uint16_t triangleCount = 0;
    uint16_t groupCount = 0;
};

// The first-person renderer can submit the held weapon and several smaller
// attachment models through the same interpolated bone bank. Contact may use
// only the render-model tag stored in the active primary-weapon runtime slot.
// A size/wrist test alone is insufficient: a one-node attachment can satisfy
// both and overwrite the weapon pose later in the same render transaction.
inline constexpr bool PhysicalContactVisibleWeaponSubmissionAccepted(
    uint16_t expectedRenderTag, uint16_t submittedRenderTag,
    int32_t renderNodeCount, int32_t mappedRoot, int32_t sourceBoneCount,
    uint64_t wristDescendants, bool finiteRoot)
{
    return expectedRenderTag != 0xFFFFu &&
        submittedRenderTag == expectedRenderTag &&
        renderNodeCount > 0 && renderNodeCount <= 16 &&
        mappedRoot >= 0 && mappedRoot < sourceBoneCount && mappedRoot < 64 &&
        (wristDescendants & (uint64_t{1} << mappedRoot)) != 0 && finiteRoot;
}

// A render submission may consume only an approval produced for this exact
// weapon palette identity. The worker can lag the renderer by a few proposals,
// but an approval from the future, another weapon, or an old gameplay sample
// is never valid.
inline constexpr bool PhysicalContactApprovedPaletteUsable(
    uint16_t expectedRenderTag, uint16_t approvedRenderTag,
    int32_t expectedWeaponHandle, int32_t approvedWeaponHandle,
    uint32_t expectedNodeCount, uint32_t approvedNodeCount,
    uint64_t currentProposalSerial, uint64_t approvedProposalSerial,
    uint64_t approvedSampleMs, uint64_t nowMs,
    uint64_t maximumAgeMs = 100)
{
    return expectedRenderTag != 0xFFFFu &&
        approvedRenderTag == expectedRenderTag &&
        expectedWeaponHandle != -1 &&
        approvedWeaponHandle == expectedWeaponHandle &&
        expectedNodeCount > 0 && expectedNodeCount <= 16 &&
        approvedNodeCount == expectedNodeCount &&
        currentProposalSerial != 0 && approvedProposalSerial != 0 &&
        approvedProposalSerial <= currentProposalSerial &&
        approvedSampleMs != 0 && nowMs >= approvedSampleMs &&
        nowMs - approvedSampleMs <= maximumAgeMs;
}

inline int32_t PhysicalContactCollisionPermutationIndex(
    int32_t permutationCount, bool allowFirstOfMany)
{
    if (permutationCount == 1)
        return 0;
    return allowFirstOfMany && permutationCount > 1 ? 0 : -1;
}

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

inline bool PhysicalContactTriangleValid(
    const PhysicalContactTriangle& triangle)
{
    if (!std::isfinite(triangle.boundRadius) ||
        triangle.boundRadius < 0.0f || triangle.boundRadius > 10.0f ||
        !PhysicalContactFinite(triangle.centre) ||
        !PhysicalContactFinite(triangle.halfExtents) ||
        triangle.halfExtents.x < 0.0f || triangle.halfExtents.y < 0.0f ||
        triangle.halfExtents.z < 0.0f)
        return false;
    for (const PhysicalContactVec3 vertex : triangle.vertices)
        if (!PhysicalContactFinite(vertex))
            return false;
    return true;
}

inline PhysicalContactConvexShape PhysicalContactConvexFromTriangle(
    const PhysicalContactTriangle& triangle)
{
    PhysicalContactConvexShape shape{};
    if (!PhysicalContactTriangleValid(triangle))
        return shape;
    shape.vertexCount = 3;
    shape.vertices[0] = triangle.vertices[0];
    shape.vertices[1] = triangle.vertices[1];
    shape.vertices[2] = triangle.vertices[2];
    return shape;
}

inline bool PhysicalContactTriangleMeshValid(
    const PhysicalContactTriangleMesh& mesh)
{
    if (!mesh.triangleCount ||
        mesh.triangleCount > PhysicalContactTriangleMesh::kMaximumTriangles ||
        !mesh.groupCount ||
        mesh.groupCount > PhysicalContactTriangleMesh::kMaximumGroups)
        return false;
    uint32_t expectedFirst = 0;
    for (uint16_t groupIndex = 0; groupIndex < mesh.groupCount; ++groupIndex)
    {
        const PhysicalContactTriangleGroup& group = mesh.groups[groupIndex];
        if (!PhysicalContactFinite(group.centre) ||
            !PhysicalContactFinite(group.halfExtents) ||
            group.halfExtents.x < 0.0f ||
            group.halfExtents.y < 0.0f ||
            group.halfExtents.z < 0.0f ||
            !std::isfinite(group.boundRadius) || group.boundRadius < 0.0f ||
            group.boundRadius > 10.0f || !group.triangleCount ||
            group.firstTriangle != expectedFirst ||
            static_cast<uint32_t>(group.firstTriangle) +
                    group.triangleCount >
                mesh.triangleCount)
            return false;
        expectedFirst += group.triangleCount;
    }
    if (expectedFirst != mesh.triangleCount)
        return false;
    for (uint16_t triangle = 0; triangle < mesh.triangleCount; ++triangle)
        if (!PhysicalContactTriangleValid(mesh.triangles[triangle]))
            return false;
    return true;
}

// Halo 3 decorator instances use a fixed 16-byte vertex record. XYZ are
// unsigned 16-bit values expanded by a draw-block minimum and step. The four
// orientation bytes hold rotation * sqrt(scale): signed zero is byte 127 and
// the full quaternion range is sqrt(2), matching the authored 2.0 scale cap.
// Keeping this decoder pure lets the renderer bridge copy fixed records out of
// Halo-owned memory before the contact thread uses them.
inline bool PhysicalContactDecodeH3DecoratorPlacement(
    const uint8_t* record, PhysicalContactVec3 blockMinimum,
    PhysicalContactVec3 blockStep, PhysicalContactTransform& output,
    uint16_t* partIndex = nullptr)
{
    output = {};
    if (!record || !PhysicalContactFinite(blockMinimum) ||
        !PhysicalContactFinite(blockStep) || blockStep.x <= 0.0f ||
        blockStep.y <= 0.0f || blockStep.z <= 0.0f)
        return false;
    const auto word = [&](size_t offset) {
        return static_cast<uint16_t>(
            static_cast<uint16_t>(record[offset]) |
            static_cast<uint16_t>(record[offset + 1]) << 8u);
    };
    output.position = {
        blockMinimum.x + static_cast<float>(word(0)) * blockStep.x,
        blockMinimum.y + static_cast<float>(word(2)) * blockStep.y,
        blockMinimum.z + static_cast<float>(word(4)) * blockStep.z};
    if (partIndex)
        *partIndex = word(6);

    constexpr float kSqrtTwoOverSignedByteMaximum =
        1.4142135623730950488f / 127.0f;
    const float x = (static_cast<float>(record[8]) - 127.0f) *
        kSqrtTwoOverSignedByteMaximum;
    const float y = (static_cast<float>(record[9]) - 127.0f) *
        kSqrtTwoOverSignedByteMaximum;
    const float z = (static_cast<float>(record[10]) - 127.0f) *
        kSqrtTwoOverSignedByteMaximum;
    const float w = (static_cast<float>(record[11]) - 127.0f) *
        kSqrtTwoOverSignedByteMaximum;
    const float scale = x * x + y * y + z * z + w * w;
    if (!std::isfinite(scale) || scale < 0.01f || scale > 2.10f ||
        !PhysicalContactFinite(output.position))
        return false;
    const float inverseRootScale = 1.0f / std::sqrt(scale);
    const float qx = x * inverseRootScale;
    const float qy = y * inverseRootScale;
    const float qz = z * inverseRootScale;
    const float qw = w * inverseRootScale;
    output.forward = {
        1.0f - 2.0f * (qy * qy + qz * qz),
        2.0f * (qx * qy + qz * qw),
        2.0f * (qx * qz - qy * qw)};
    output.left = {
        2.0f * (qx * qy - qz * qw),
        1.0f - 2.0f * (qx * qx + qz * qz),
        2.0f * (qy * qz + qx * qw)};
    output.up = {
        2.0f * (qx * qz + qy * qw),
        2.0f * (qy * qz - qx * qw),
        1.0f - 2.0f * (qx * qx + qy * qy)};
    output.scale = scale;
    return PhysicalContactTransformFinite(output);
}

inline bool PhysicalContactDecodeH3DecoratorTriangleStrip(
    const uint8_t* bytes, size_t byteCount, uint32_t startVertex,
    uint32_t vertexCount, PhysicalContactVec3 positionMinimum,
    PhysicalContactVec3 positionSize, PhysicalContactTriangleMesh& output)
{
    output = {};
    constexpr size_t kStride = 20u;
    if (!bytes || vertexCount < 3u ||
        vertexCount > PhysicalContactTriangleMesh::kMaximumTriangles + 2u ||
        !PhysicalContactFinite(positionMinimum) ||
        !PhysicalContactFinite(positionSize) || positionSize.x <= 0.0f ||
        positionSize.y <= 0.0f || positionSize.z <= 0.0f ||
        positionSize.x > 10.0f || positionSize.y > 10.0f ||
        positionSize.z > 10.0f || startVertex > SIZE_MAX / kStride ||
        vertexCount > (SIZE_MAX / kStride) - startVertex ||
        (static_cast<size_t>(startVertex) + vertexCount) * kStride >
            byteCount)
        return false;

    std::array<PhysicalContactVec3,
               PhysicalContactTriangleMesh::kMaximumTriangles + 2u> vertices{};
    PhysicalContactVec3 groupMinimum{INFINITY, INFINITY, INFINITY};
    PhysicalContactVec3 groupMaximum{-INFINITY, -INFINITY, -INFINITY};
    for (uint32_t index = 0; index < vertexCount; ++index)
    {
        const uint8_t* vertex = bytes +
            (static_cast<size_t>(startVertex) + index) * kStride;
        const auto word = [&](size_t offset) {
            return static_cast<uint16_t>(
                static_cast<uint16_t>(vertex[offset]) |
                static_cast<uint16_t>(vertex[offset + 1]) << 8u);
        };
        constexpr float kInverseUnsignedShortMaximum = 1.0f / 65535.0f;
        const PhysicalContactVec3 point{
            positionMinimum.x + static_cast<float>(word(0)) *
                kInverseUnsignedShortMaximum * positionSize.x,
            positionMinimum.y + static_cast<float>(word(2)) *
                kInverseUnsignedShortMaximum * positionSize.y,
            positionMinimum.z + static_cast<float>(word(4)) *
                kInverseUnsignedShortMaximum * positionSize.z};
        if (!PhysicalContactFinite(point))
            return false;
        vertices[index] = point;
        groupMinimum.x = std::min(groupMinimum.x, point.x);
        groupMinimum.y = std::min(groupMinimum.y, point.y);
        groupMinimum.z = std::min(groupMinimum.z, point.z);
        groupMaximum.x = std::max(groupMaximum.x, point.x);
        groupMaximum.y = std::max(groupMaximum.y, point.y);
        groupMaximum.z = std::max(groupMaximum.z, point.z);
    }

    for (uint32_t index = 2; index < vertexCount; ++index)
    {
        uint32_t a = index - 2u;
        uint32_t b = index - 1u;
        if ((index & 1u) != 0u)
            std::swap(a, b);
        const PhysicalContactVec3 edgeA = vertices[b] - vertices[a];
        const PhysicalContactVec3 edgeB = vertices[index] - vertices[a];
        if (PhysicalContactLengthSquared(
                PhysicalContactCross(edgeA, edgeB)) <= 1.0e-16f)
            continue;
        if (output.triangleCount >=
            PhysicalContactTriangleMesh::kMaximumTriangles)
            return false;
        PhysicalContactTriangle& triangle =
            output.triangles[output.triangleCount++];
        triangle.vertices = {vertices[a], vertices[b], vertices[index]};
        PhysicalContactVec3 minimum{INFINITY, INFINITY, INFINITY};
        PhysicalContactVec3 maximum{-INFINITY, -INFINITY, -INFINITY};
        for (const PhysicalContactVec3 point : triangle.vertices)
        {
            minimum.x = std::min(minimum.x, point.x);
            minimum.y = std::min(minimum.y, point.y);
            minimum.z = std::min(minimum.z, point.z);
            maximum.x = std::max(maximum.x, point.x);
            maximum.y = std::max(maximum.y, point.y);
            maximum.z = std::max(maximum.z, point.z);
        }
        triangle.centre = (minimum + maximum) * 0.5f;
        triangle.halfExtents = (maximum - minimum) * 0.5f;
        for (const PhysicalContactVec3 point : triangle.vertices)
            triangle.boundRadius = std::max(
                triangle.boundRadius,
                PhysicalContactLength(point - triangle.centre));
    }
    if (!output.triangleCount)
        return false;
    output.groupCount = 1;
    PhysicalContactTriangleGroup& group = output.groups[0];
    group.firstTriangle = 0;
    group.triangleCount = output.triangleCount;
    group.centre = (groupMinimum + groupMaximum) * 0.5f;
    group.halfExtents = (groupMaximum - groupMinimum) * 0.5f;
    for (uint32_t index = 0; index < vertexCount; ++index)
        group.boundRadius = std::max(
            group.boundRadius,
            PhysicalContactLength(vertices[index] - group.centre));
    return PhysicalContactTriangleMeshValid(output);
}

inline bool PhysicalContactH3DecoratorMeshIsSolid(
    const PhysicalContactTriangleMesh& mesh, float minimumAxisRatio = 0.08f)
{
    if (!PhysicalContactTriangleMeshValid(mesh) ||
        !std::isfinite(minimumAxisRatio) || minimumAxisRatio < 0.0f ||
        minimumAxisRatio > 1.0f)
        return false;
    const PhysicalContactVec3 size = mesh.groups[0].halfExtents * 2.0f;
    const float minimum = std::min({size.x, size.y, size.z});
    const float maximum = std::max({size.x, size.y, size.z});
    return std::isfinite(minimum) && std::isfinite(maximum) &&
        maximum > 1.0e-5f && minimum / maximum >= minimumAxisRatio;
}

inline PhysicalContactVec3 PhysicalContactCompoundWorldCentroid(
    const PhysicalContactCompoundShape& shape,
    const PhysicalContactTransform& transform)
{
    if (!PhysicalContactCompoundValid(shape) ||
        !PhysicalContactTransformFinite(transform))
        return transform.position;
    PhysicalContactVec3 sum{};
    uint32_t count = 0;
    for (uint16_t child = 0; child < shape.childCount; ++child)
        for (uint16_t vertex = 0;
             vertex < shape.children[child].vertexCount; ++vertex)
        {
            sum = sum + PhysicalContactTransformPoint(
                transform, shape.children[child].vertices[vertex]);
            ++count;
        }
    return count ? sum * (1.0f / static_cast<float>(count))
                 : transform.position;
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

inline float PhysicalContactTriangleMeshBoundRadius(
    const PhysicalContactTriangleMesh& mesh)
{
    float radius = 0.0f;
    for (uint16_t group = 0; group < mesh.groupCount; ++group)
        radius = std::max(
            radius, PhysicalContactLength(mesh.groups[group].centre) +
                        mesh.groups[group].boundRadius);
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

inline PhysicalContactVec3 PhysicalContactTriangleSupport(
    const PhysicalContactTriangle& triangle,
    const PhysicalContactTransform& transform,
    PhysicalContactVec3 worldDirection, float surfaceRadius)
{
    const PhysicalContactVec3 localDirection =
        PhysicalContactInverseTransformVector(transform, worldDirection);
    uint16_t best = 0;
    float bestProjection = PhysicalContactDot(
        triangle.vertices[0], localDirection);
    for (uint16_t vertex = 1; vertex < 3; ++vertex)
    {
        const float projection = PhysicalContactDot(
            triangle.vertices[vertex], localDirection);
        if (projection > bestProjection)
        {
            bestProjection = projection;
            best = vertex;
        }
    }
    PhysicalContactVec3 result = PhysicalContactTransformPoint(
        transform, triangle.vertices[best]);
    if (surfaceRadius > 0.0f)
        result = result + PhysicalContactNormalize(worldDirection) *
            surfaceRadius;
    return result;
}

inline PhysicalContactVec3 PhysicalContactTriangleMeshSupport(
    const PhysicalContactTriangleMesh& mesh,
    const PhysicalContactTransform& transform,
    PhysicalContactVec3 worldDirection, float surfaceRadius)
{
    PhysicalContactVec3 result = transform.position;
    float bestProjection = -FLT_MAX;
    for (uint16_t triangle = 0; triangle < mesh.triangleCount; ++triangle)
    {
        const PhysicalContactVec3 candidate = PhysicalContactTriangleSupport(
            mesh.triangles[triangle], transform, worldDirection,
            surfaceRadius);
        const float projection = PhysicalContactDot(candidate, worldDirection);
        if (projection > bestProjection)
        {
            bestProjection = projection;
            result = candidate;
        }
    }
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

inline bool PhysicalContactBoundingSpheresOverlap(
    PhysicalContactVec3 centreA, float radiusA,
    PhysicalContactVec3 centreB, float radiusB)
{
    if (!PhysicalContactFinite(centreA) || !PhysicalContactFinite(centreB) ||
        !std::isfinite(radiusA) || !std::isfinite(radiusB) ||
        radiusA < 0.0f || radiusB < 0.0f)
        return false;
    const float radius = radiusA + radiusB;
    return PhysicalContactLengthSquared(centreA - centreB) <= radius * radius;
}

inline PhysicalContactVec3 PhysicalContactWorldAabbHalfExtents(
    const PhysicalContactTransform& transform,
    PhysicalContactVec3 localHalfExtents)
{
    const float scale = transform.scale;
    return {
        scale * (std::fabs(transform.forward.x) * localHalfExtents.x +
                 std::fabs(transform.left.x) * localHalfExtents.y +
                 std::fabs(transform.up.x) * localHalfExtents.z),
        scale * (std::fabs(transform.forward.y) * localHalfExtents.x +
                 std::fabs(transform.left.y) * localHalfExtents.y +
                 std::fabs(transform.up.y) * localHalfExtents.z),
        scale * (std::fabs(transform.forward.z) * localHalfExtents.x +
                 std::fabs(transform.left.z) * localHalfExtents.y +
                 std::fabs(transform.up.z) * localHalfExtents.z)};
}

inline bool PhysicalContactAabbsOverlap(
    PhysicalContactVec3 centreA, PhysicalContactVec3 halfExtentsA,
    PhysicalContactVec3 centreB, PhysicalContactVec3 halfExtentsB,
    float expansionA)
{
    if (!PhysicalContactFinite(centreA) ||
        !PhysicalContactFinite(halfExtentsA) ||
        !PhysicalContactFinite(centreB) ||
        !PhysicalContactFinite(halfExtentsB) ||
        !std::isfinite(expansionA) || expansionA < 0.0f)
        return false;
    return std::fabs(centreA.x - centreB.x) <=
               halfExtentsA.x + halfExtentsB.x + expansionA &&
        std::fabs(centreA.y - centreB.y) <=
               halfExtentsA.y + halfExtentsB.y + expansionA &&
        std::fabs(centreA.z - centreB.z) <=
               halfExtentsA.z + halfExtentsB.z + expansionA;
}

template <typename Support>
inline bool PhysicalContactGjkIntersectSupport(
    PhysicalContactVec3 direction, Support support,
    PhysicalContactVec3* separatingDirection)
{
    if (PhysicalContactLengthSquared(direction) <= 1.0e-10f)
        direction = {1.0f, 0.0f, 0.0f};
    PhysicalContactGjkSimplex simplex{};
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

inline bool PhysicalContactTriangleTriangleIntersect(
    const PhysicalContactTriangle& a,
    const PhysicalContactTransform& transformA,
    const PhysicalContactTriangle& b,
    const PhysicalContactTransform& transformB,
    float surfaceRadius, PhysicalContactVec3* separatingDirection = nullptr)
{
    const PhysicalContactVec3 centreA = PhysicalContactTransformPoint(
        transformA, a.centre);
    const PhysicalContactVec3 centreB = PhysicalContactTransformPoint(
        transformB, b.centre);
    const PhysicalContactVec3 halfExtentsA =
        PhysicalContactWorldAabbHalfExtents(transformA, a.halfExtents);
    const PhysicalContactVec3 halfExtentsB =
        PhysicalContactWorldAabbHalfExtents(transformB, b.halfExtents);
    if (!PhysicalContactAabbsOverlap(
            centreA, halfExtentsA, centreB, halfExtentsB, surfaceRadius))
    {
        if (separatingDirection)
            *separatingDirection = centreB - centreA;
        return false;
    }
    const float radiusA = a.boundRadius * transformA.scale + surfaceRadius;
    const float radiusB = b.boundRadius * transformB.scale;
    if (!PhysicalContactBoundingSpheresOverlap(
            centreA, radiusA, centreB, radiusB))
    {
        if (separatingDirection)
            *separatingDirection = centreB - centreA;
        return false;
    }
    const auto support = [&](PhysicalContactVec3 direction) {
        return PhysicalContactTriangleSupport(
                   a, transformA, direction, surfaceRadius) -
            PhysicalContactTriangleSupport(
                   b, transformB, direction * -1.0f, 0.0f);
    };
    return PhysicalContactGjkIntersectSupport(
        centreB - centreA, support, separatingDirection);
}

inline bool PhysicalContactTriangleConvexIntersectPrepared(
    const PhysicalContactTriangle& triangle,
    const PhysicalContactTransform& triangleTransform,
    const PhysicalContactConvexShape& convex,
    const PhysicalContactTransform& convexTransform,
    PhysicalContactVec3 convexCentre, PhysicalContactVec3 convexHalfExtents,
    float convexRadius,
    float surfaceRadius, PhysicalContactVec3* separatingDirection = nullptr)
{
    const PhysicalContactVec3 triangleCentre = PhysicalContactTransformPoint(
        triangleTransform, triangle.centre);
    const PhysicalContactVec3 triangleHalfExtents =
        PhysicalContactWorldAabbHalfExtents(
            triangleTransform, triangle.halfExtents);
    if (!PhysicalContactAabbsOverlap(
            triangleCentre, triangleHalfExtents,
            convexCentre, convexHalfExtents, surfaceRadius))
    {
        if (separatingDirection)
            *separatingDirection = convexCentre - triangleCentre;
        return false;
    }
    const float triangleRadius =
        triangle.boundRadius * triangleTransform.scale + surfaceRadius;
    if (!PhysicalContactBoundingSpheresOverlap(
            triangleCentre, triangleRadius, convexCentre, convexRadius))
    {
        if (separatingDirection)
            *separatingDirection = convexCentre - triangleCentre;
        return false;
    }
    const auto support = [&](PhysicalContactVec3 direction) {
        return PhysicalContactTriangleSupport(
                   triangle, triangleTransform, direction, surfaceRadius) -
            PhysicalContactConvexSupport(
                   convex, convexTransform, direction * -1.0f);
    };
    return PhysicalContactGjkIntersectSupport(
        convexCentre - triangleCentre, support, separatingDirection);
}

inline bool PhysicalContactTriangleConvexIntersect(
    const PhysicalContactTriangle& triangle,
    const PhysicalContactTransform& triangleTransform,
    const PhysicalContactConvexShape& convex,
    const PhysicalContactTransform& convexTransform,
    float surfaceRadius, PhysicalContactVec3* separatingDirection = nullptr)
{
    PhysicalContactVec3 minimum{
        FLT_MAX, FLT_MAX, FLT_MAX};
    PhysicalContactVec3 maximum{
        -FLT_MAX, -FLT_MAX, -FLT_MAX};
    for (uint16_t vertex = 0; vertex < convex.vertexCount; ++vertex)
    {
        minimum.x = std::min(minimum.x, convex.vertices[vertex].x);
        minimum.y = std::min(minimum.y, convex.vertices[vertex].y);
        minimum.z = std::min(minimum.z, convex.vertices[vertex].z);
        maximum.x = std::max(maximum.x, convex.vertices[vertex].x);
        maximum.y = std::max(maximum.y, convex.vertices[vertex].y);
        maximum.z = std::max(maximum.z, convex.vertices[vertex].z);
    }
    const PhysicalContactVec3 convexLocalCentre =
        (minimum + maximum) * 0.5f;
    const PhysicalContactVec3 convexCentre =
        PhysicalContactTransformPoint(convexTransform, convexLocalCentre);
    const PhysicalContactVec3 convexHalfExtents =
        PhysicalContactWorldAabbHalfExtents(
            convexTransform, (maximum - minimum) * 0.5f);
    const float convexRadius =
        PhysicalContactConvexBoundRadius(convex) * convexTransform.scale;
    return PhysicalContactTriangleConvexIntersectPrepared(
        triangle, triangleTransform, convex, convexTransform,
        convexCentre, convexHalfExtents, convexRadius,
        surfaceRadius, separatingDirection);
}

struct PhysicalContactTrianglePair
{
    bool hit = false;
    uint16_t weaponTriangle = 0;
    uint16_t targetIndex = 0;
    PhysicalContactVec3 separation{};
};

inline PhysicalContactTrianglePair PhysicalContactTriangleMeshesIntersect(
    const PhysicalContactTriangleMesh& weapon,
    const PhysicalContactTransform& weaponTransform,
    const PhysicalContactTriangleMesh& target,
    const PhysicalContactTransform& targetTransform,
    float surfaceRadius)
{
    PhysicalContactTrianglePair result{};
    float nearestGap = FLT_MAX;
    for (uint16_t weaponGroup = 0;
         weaponGroup < weapon.groupCount; ++weaponGroup)
    {
        const PhysicalContactTriangleGroup& groupA =
            weapon.groups[weaponGroup];
        const PhysicalContactVec3 groupCentreA =
            PhysicalContactTransformPoint(weaponTransform, groupA.centre);
        const PhysicalContactVec3 groupHalfExtentsA =
            PhysicalContactWorldAabbHalfExtents(
                weaponTransform, groupA.halfExtents);
        const float groupRadiusA =
            groupA.boundRadius * weaponTransform.scale + surfaceRadius;
        for (uint16_t targetGroup = 0;
             targetGroup < target.groupCount; ++targetGroup)
        {
            const PhysicalContactTriangleGroup& groupB =
                target.groups[targetGroup];
            const PhysicalContactVec3 groupCentreB =
                PhysicalContactTransformPoint(targetTransform, groupB.centre);
            const PhysicalContactVec3 groupHalfExtentsB =
                PhysicalContactWorldAabbHalfExtents(
                    targetTransform, groupB.halfExtents);
            const float groupRadiusB =
                groupB.boundRadius * targetTransform.scale;
            if (!PhysicalContactAabbsOverlap(
                    groupCentreA, groupHalfExtentsA,
                    groupCentreB, groupHalfExtentsB, surfaceRadius) ||
                !PhysicalContactBoundingSpheresOverlap(
                    groupCentreA, groupRadiusA,
                    groupCentreB, groupRadiusB))
            {
                const float gap = PhysicalContactLengthSquared(
                    groupCentreB - groupCentreA);
                if (gap < nearestGap)
                {
                    nearestGap = gap;
                    result.separation = groupCentreB - groupCentreA;
                }
                continue;
            }
            const uint16_t weaponEnd = static_cast<uint16_t>(
                groupA.firstTriangle + groupA.triangleCount);
            const uint16_t targetEnd = static_cast<uint16_t>(
                groupB.firstTriangle + groupB.triangleCount);
            for (uint16_t weaponTriangle = groupA.firstTriangle;
                 weaponTriangle < weaponEnd; ++weaponTriangle)
            {
                const PhysicalContactTriangle& triangleA =
                    weapon.triangles[weaponTriangle];
                for (uint16_t targetTriangle = groupB.firstTriangle;
                     targetTriangle < targetEnd; ++targetTriangle)
                {
                    const PhysicalContactTriangle& triangleB =
                        target.triangles[targetTriangle];
                    PhysicalContactVec3 separation{};
                    if (PhysicalContactTriangleTriangleIntersect(
                            triangleA, weaponTransform,
                            triangleB, targetTransform,
                            surfaceRadius, &separation))
                    {
                        result.hit = true;
                        result.weaponTriangle = weaponTriangle;
                        result.targetIndex = targetTriangle;
                        return result;
                    }
                    const float gap = PhysicalContactLengthSquared(separation);
                    if (PhysicalContactFinite(separation) && gap < nearestGap)
                    {
                        nearestGap = gap;
                        result.separation = separation;
                        result.weaponTriangle = weaponTriangle;
                        result.targetIndex = targetTriangle;
                    }
                }
            }
        }
    }
    return result;
}

inline PhysicalContactTrianglePair PhysicalContactTriangleMeshCompoundIntersect(
    const PhysicalContactTriangleMesh& weapon,
    const PhysicalContactTransform& weaponTransform,
    const PhysicalContactCompoundShape& target,
    const PhysicalContactTransform& targetTransform,
    float surfaceRadius)
{
    PhysicalContactTrianglePair result{};
    float nearestGap = FLT_MAX;
    for (uint16_t targetChild = 0;
         targetChild < target.childCount; ++targetChild)
    {
        const PhysicalContactConvexShape& convex =
            target.children[targetChild];
        PhysicalContactVec3 minimum{FLT_MAX, FLT_MAX, FLT_MAX};
        PhysicalContactVec3 maximum{-FLT_MAX, -FLT_MAX, -FLT_MAX};
        for (uint16_t vertex = 0; vertex < convex.vertexCount; ++vertex)
        {
            minimum.x = std::min(minimum.x, convex.vertices[vertex].x);
            minimum.y = std::min(minimum.y, convex.vertices[vertex].y);
            minimum.z = std::min(minimum.z, convex.vertices[vertex].z);
            maximum.x = std::max(maximum.x, convex.vertices[vertex].x);
            maximum.y = std::max(maximum.y, convex.vertices[vertex].y);
            maximum.z = std::max(maximum.z, convex.vertices[vertex].z);
        }
        const PhysicalContactVec3 convexLocalCentre =
            (minimum + maximum) * 0.5f;
        const PhysicalContactVec3 convexCentre =
            PhysicalContactTransformPoint(
                targetTransform, convexLocalCentre);
        const PhysicalContactVec3 convexHalfExtents =
            PhysicalContactWorldAabbHalfExtents(
                targetTransform, (maximum - minimum) * 0.5f);
        const float convexRadius =
            PhysicalContactConvexBoundRadius(convex) * targetTransform.scale;
        for (uint16_t weaponGroup = 0;
             weaponGroup < weapon.groupCount; ++weaponGroup)
        {
            const PhysicalContactTriangleGroup& group =
                weapon.groups[weaponGroup];
            const PhysicalContactVec3 groupCentre =
                PhysicalContactTransformPoint(
                    weaponTransform, group.centre);
            const PhysicalContactVec3 groupHalfExtents =
                PhysicalContactWorldAabbHalfExtents(
                    weaponTransform, group.halfExtents);
            const float groupRadius =
                group.boundRadius * weaponTransform.scale + surfaceRadius;
            if (!PhysicalContactAabbsOverlap(
                    groupCentre, groupHalfExtents,
                    convexCentre, convexHalfExtents, surfaceRadius) ||
                !PhysicalContactBoundingSpheresOverlap(
                    groupCentre, groupRadius,
                    convexCentre, convexRadius))
            {
                const PhysicalContactVec3 separation =
                    convexCentre - groupCentre;
                const float gap = PhysicalContactLengthSquared(separation);
                if (gap < nearestGap)
                {
                    nearestGap = gap;
                    result.separation = separation;
                }
                continue;
            }
            const uint16_t weaponEnd = static_cast<uint16_t>(
                group.firstTriangle + group.triangleCount);
            for (uint16_t weaponTriangle = group.firstTriangle;
                 weaponTriangle < weaponEnd; ++weaponTriangle)
            {
                PhysicalContactVec3 separation{};
                if (PhysicalContactTriangleConvexIntersectPrepared(
                        weapon.triangles[weaponTriangle], weaponTransform,
                        convex, targetTransform, convexCentre,
                        convexHalfExtents, convexRadius,
                        surfaceRadius, &separation))
                {
                    result.hit = true;
                    result.weaponTriangle = weaponTriangle;
                    result.targetIndex = targetChild;
                    return result;
                }
                const float gap = PhysicalContactLengthSquared(separation);
                if (PhysicalContactFinite(separation) && gap < nearestGap)
                {
                    nearestGap = gap;
                    result.separation = separation;
                    result.weaponTriangle = weaponTriangle;
                    result.targetIndex = targetChild;
                }
            }
        }
    }
    return result;
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

inline bool PhysicalContactCompoundsIntersect(
    const PhysicalContactCompoundShape& a,
    const PhysicalContactTransform& transformA,
    const PhysicalContactCompoundShape& b,
    const PhysicalContactTransform& transformB)
{
    if (!PhysicalContactCompoundValid(a) ||
        !PhysicalContactTransformFinite(transformA) ||
        !PhysicalContactCompoundValid(b) ||
        !PhysicalContactTransformFinite(transformB))
        return false;
    for (uint16_t childA = 0; childA < a.childCount; ++childA)
        for (uint16_t childB = 0; childB < b.childCount; ++childB)
            if (PhysicalContactConvexIntersect(
                    a.children[childA], transformA,
                    b.children[childB], transformB))
                return true;
    return false;
}

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

struct PhysicalContactTriangleMeshHit : PhysicalContactConvexHit
{
    uint16_t weaponTriangle = 0;
    uint16_t targetIndex = 0;
};

template <typename IntersectAt, typename IntersectPairAt,
          typename TargetSupport>
inline PhysicalContactTriangleMeshHit PhysicalContactSweepTriangleMeshInternal(
    const PhysicalContactTriangleMesh& weapon,
    const PhysicalContactTransform& previousWeaponTransform,
    const PhysicalContactTransform& currentWeaponTransform,
    const PhysicalContactTransform& targetTransform,
    float stepWorldUnits, float surfaceRadius,
    IntersectAt intersectAt, IntersectPairAt intersectPairAt,
    TargetSupport targetSupport)
{
    PhysicalContactTriangleMeshHit result{};
    if (!PhysicalContactTriangleMeshValid(weapon) ||
        !PhysicalContactTransformFinite(previousWeaponTransform) ||
        !PhysicalContactTransformFinite(currentWeaponTransform) ||
        !PhysicalContactTransformFinite(targetTransform) ||
        !std::isfinite(stepWorldUnits) || stepWorldUnits < 1.0e-5f ||
        stepWorldUnits > 0.05f || !std::isfinite(surfaceRadius) ||
        surfaceRadius < 0.0f || surfaceRadius > 0.05f)
        return result;

    PhysicalContactTrianglePair pair = intersectAt(previousWeaponTransform);
    PhysicalContactVec3 separation = pair.separation;
    if (pair.hit)
    {
        result.hit = true;
        result.fraction = 0.0f;
        result.weaponTriangle = pair.weaponTriangle;
        result.targetIndex = pair.targetIndex;
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
            PhysicalContactTriangleMeshBoundRadius(weapon) *
                std::max(previousWeaponTransform.scale,
                         currentWeaponTransform.scale);
        const int steps = std::clamp(
            static_cast<int>(std::ceil(sweptDistance / stepWorldUnits)),
            1, 64);
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
            const PhysicalContactTrianglePair candidate =
                intersectAt(transform);
            if (candidate.hit)
            {
                high = fraction;
                low = static_cast<float>(step - 1) /
                    static_cast<float>(steps);
                pair = candidate;
                found = true;
                break;
            }
            separation = candidate.separation;
        }
        if (!found)
            return result;
        for (int iteration = 0; iteration < 9; ++iteration)
        {
            const float middle = (low + high) * 0.5f;
            const PhysicalContactTransform transform =
                PhysicalContactInterpolateTransform(
                    previousWeaponTransform, currentWeaponTransform,
                    middle);
            const PhysicalContactTrianglePair candidate =
                intersectPairAt(
                    transform, pair.weaponTriangle, pair.targetIndex);
            if (candidate.hit)
            {
                high = middle;
                pair = candidate;
            }
            else
            {
                low = middle;
                separation = candidate.separation;
            }
        }
        result.hit = true;
        result.fraction = high;
        result.weaponTriangle = pair.weaponTriangle;
        result.targetIndex = pair.targetIndex;
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
    result.weaponPoint = PhysicalContactTriangleSupport(
        weapon.triangles[result.weaponTriangle], impact,
        result.normal * -1.0f, 0.0f);
    result.targetPoint = targetSupport(result.targetIndex, result.normal);
    result.point = (result.weaponPoint + result.targetPoint) * 0.5f;
    return result;
}

inline PhysicalContactTriangleMeshHit PhysicalContactSweepTriangleMeshes(
    const PhysicalContactTriangleMesh& weapon,
    const PhysicalContactTransform& previousWeaponTransform,
    const PhysicalContactTransform& currentWeaponTransform,
    const PhysicalContactTriangleMesh& target,
    const PhysicalContactTransform& targetTransform,
    float stepWorldUnits, float surfaceRadius)
{
    if (!PhysicalContactTriangleMeshValid(target))
        return {};
    const auto intersectAt = [&](const PhysicalContactTransform& transform) {
        return PhysicalContactTriangleMeshesIntersect(
            weapon, transform, target, targetTransform, surfaceRadius);
    };
    const auto targetSupport = [&](uint16_t triangle,
                                   PhysicalContactVec3 direction) {
        return PhysicalContactTriangleSupport(
            target.triangles[triangle], targetTransform, direction, 0.0f);
    };
    const auto intersectPairAt = [&] (
        const PhysicalContactTransform& transform,
        uint16_t weaponTriangle, uint16_t targetTriangle) {
        PhysicalContactTrianglePair pair{};
        pair.weaponTriangle = weaponTriangle;
        pair.targetIndex = targetTriangle;
        pair.hit = PhysicalContactTriangleTriangleIntersect(
            weapon.triangles[weaponTriangle], transform,
            target.triangles[targetTriangle], targetTransform,
            surfaceRadius, &pair.separation);
        return pair;
    };
    return PhysicalContactSweepTriangleMeshInternal(
        weapon, previousWeaponTransform, currentWeaponTransform,
        targetTransform, stepWorldUnits, surfaceRadius,
        intersectAt, intersectPairAt, targetSupport);
}

inline PhysicalContactTriangleMeshHit PhysicalContactSweepTriangleMeshCompound(
    const PhysicalContactTriangleMesh& weapon,
    const PhysicalContactTransform& previousWeaponTransform,
    const PhysicalContactTransform& currentWeaponTransform,
    const PhysicalContactCompoundShape& target,
    const PhysicalContactTransform& targetTransform,
    float stepWorldUnits, float surfaceRadius)
{
    if (!PhysicalContactCompoundValid(target))
        return {};
    const auto intersectAt = [&](const PhysicalContactTransform& transform) {
        return PhysicalContactTriangleMeshCompoundIntersect(
            weapon, transform, target, targetTransform, surfaceRadius);
    };
    const auto targetSupport = [&](uint16_t child,
                                   PhysicalContactVec3 direction) {
        return PhysicalContactConvexSupport(
            target.children[child], targetTransform, direction);
    };
    const auto intersectPairAt = [&] (
        const PhysicalContactTransform& transform,
        uint16_t weaponTriangle, uint16_t targetChild) {
        PhysicalContactTrianglePair pair{};
        pair.weaponTriangle = weaponTriangle;
        pair.targetIndex = targetChild;
        pair.hit = PhysicalContactTriangleConvexIntersect(
            weapon.triangles[weaponTriangle], transform,
            target.children[targetChild], targetTransform,
            surfaceRadius, &pair.separation);
        return pair;
    };
    return PhysicalContactSweepTriangleMeshInternal(
        weapon, previousWeaponTransform, currentWeaponTransform,
        targetTransform, stepWorldUnits, surfaceRadius,
        intersectAt, intersectPairAt, targetSupport);
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

inline size_t PhysicalContactWallTriangleCentroidBudget(
    size_t authoredConvexVertices, size_t authoredTriangles,
    size_t fixedCapacity)
{
    if (authoredConvexVertices >= fixedCapacity)
        return 0;
    return std::min(
        authoredTriangles, fixedCapacity - authoredConvexVertices);
}

inline size_t PhysicalContactWallTriangleSampleIndex(
    size_t sampleIndex, size_t sampleCount, size_t triangleCount,
    size_t phase)
{
    if (!sampleCount || sampleIndex >= sampleCount || !triangleCount ||
        sampleCount > triangleCount)
        return triangleCount;
    return ((sampleIndex * triangleCount) / sampleCount +
            phase % triangleCount) % triangleCount;
}

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

inline bool PhysicalContactPublishedOffsetUsable(
    PhysicalContactVec3 offset, uint64_t sampleMs, uint64_t nowMs,
    float worldUnitsPerMeter, float maximumMeters = 1.0f)
{
    if (!PhysicalContactFinite(offset) || !sampleMs || nowMs < sampleMs ||
        nowMs - sampleMs > 100 || !std::isfinite(worldUnitsPerMeter) ||
        worldUnitsPerMeter <= 0.0f || !std::isfinite(maximumMeters) ||
        maximumMeters <= 0.0f)
        return false;
    const float maximumWorldUnits = maximumMeters * worldUnitsPerMeter;
    return PhysicalContactLengthSquared(offset) <=
        maximumWorldUnits * maximumWorldUnits;
}

enum class PhysicalContactDynamicBodyObservation : uint8_t
{
    Separated,
    Uncertain,
    Blocked,
};

// A missing collision is proof of separation only when the previously
// constrained object was either removed or its exact geometry was resolved and
// found clear. A live object with temporarily unavailable geometry remains
// uncertain. This distinction prevents a bad query sample from draining the
// visible weapon correction while the weapon is still inside the object.
inline PhysicalContactDynamicBodyObservation
PhysicalContactDynamicBodyObservationForTarget(
    int32_t constrainedTargetHandle, bool targetFound,
    bool targetGeometryResolved, bool targetHit)
{
    if (constrainedTargetHandle == -1 || !targetFound ||
        (targetGeometryResolved && !targetHit))
        return PhysicalContactDynamicBodyObservation::Separated;
    return PhysicalContactDynamicBodyObservation::Uncertain;
}

inline bool PhysicalContactDynamicImpulseNormalEligible(
    bool sweptNormalReliable, bool cachedReliableNormal)
{
    return sweptNormalReliable || cachedReliableNormal;
}

inline PhysicalContactVec3 PhysicalContactUpdateDynamicBodyOffset(
    PhysicalContactVec3 currentOffset, PhysicalContactVec3 requestedOffset,
    PhysicalContactDynamicBodyObservation observation, float elapsedSeconds,
    float worldUnitsPerMeter)
{
    if (!PhysicalContactFinite(currentOffset) ||
        !PhysicalContactFinite(requestedOffset) ||
        !std::isfinite(elapsedSeconds) || elapsedSeconds < 0.0f ||
        elapsedSeconds > 0.1f || !std::isfinite(worldUnitsPerMeter) ||
        worldUnitsPerMeter <= 0.0f)
        return {};

    if (observation == PhysicalContactDynamicBodyObservation::Blocked)
        return requestedOffset;
    if (observation == PhysicalContactDynamicBodyObservation::Uncertain)
        return currentOffset;
    if (observation != PhysicalContactDynamicBodyObservation::Separated)
        return {};
    return PhysicalContactUpdateWallOffset(
        currentOffset, {}, false, elapsedSeconds, worldUnitsPerMeter);
}

// Keep the rendered kinematic weapon on the target-facing side of an exact
// dynamic-body hit. Only normal travel is rejected, so the controller may still
// slide along a surface to scoop or carry it. The current pose is the
// unconstrained controller intent; the previous pose is the last rendered,
// constrained pose. Measured end-pose penetration also covers rotation about
// the grip, where position travel alone is insufficient.
inline PhysicalContactWallConstraint PhysicalContactDynamicBodyOffset(
    const PhysicalContactTransform& previousWeaponTransform,
    const PhysicalContactTransform& intendedWeaponTransform,
    float hitFraction, PhysicalContactVec3 targetToWeaponNormal,
    float penetrationMeters, float clearanceMeters,
    float worldUnitsPerMeter)
{
    PhysicalContactWallConstraint result{};
    if (!PhysicalContactTransformFinite(previousWeaponTransform) ||
        !PhysicalContactTransformFinite(intendedWeaponTransform) ||
        !std::isfinite(hitFraction) || hitFraction < 0.0f ||
        hitFraction > 1.0f || !PhysicalContactFinite(targetToWeaponNormal) ||
        !std::isfinite(penetrationMeters) || penetrationMeters < 0.0f ||
        penetrationMeters > 10.0f || !std::isfinite(clearanceMeters) ||
        clearanceMeters < 0.0f || clearanceMeters > 0.05f ||
        !std::isfinite(worldUnitsPerMeter) || worldUnitsPerMeter <= 0.0f)
        return result;

    const PhysicalContactVec3 outward = PhysicalContactNormalize(
        targetToWeaponNormal, {});
    if (PhysicalContactLengthSquared(outward) <= 1.0e-12f)
        return result;
    const PhysicalContactTransform impact = PhysicalContactInterpolateTransform(
        previousWeaponTransform, intendedWeaponTransform, hitFraction);
    const PhysicalContactVec3 postImpact =
        intendedWeaponTransform.position - impact.position;
    const float inwardTravelWorldUnits = std::max(
        0.0f, PhysicalContactDot(postImpact, outward * -1.0f));
    const float measuredPenetrationWorldUnits =
        penetrationMeters * worldUnitsPerMeter;
    float setback = std::max(
        inwardTravelWorldUnits, measuredPenetrationWorldUnits) +
        clearanceMeters * worldUnitsPerMeter;
    const float maximum = worldUnitsPerMeter;
    setback = std::min(setback, maximum);
    if (!std::isfinite(setback) || setback <= 1.0e-5f)
        return result;

    result.offset = outward * setback;
    result.setbackWorldUnits = setback;
    result.constrained = PhysicalContactFinite(result.offset);
    return result;
}

// Validate a proposed visual setback against the complete caller-supplied
// geometry predicate. This covers both a swept surface hit whose final pose is
// outside and an end pose already contained in a closed body. The fixed search
// count keeps the callback bounded; no correction is returned unless its final
// pose is directly proven clear.
template <typename IntersectsAt>
inline PhysicalContactWallConstraint PhysicalContactVerifiedSeparationOffset(
    const PhysicalContactTransform& intendedWeaponTransform,
    PhysicalContactVec3 outwardDirection,
    float proposedOffsetWorldUnits,
    float clearanceWorldUnits,
    float maximumOffsetWorldUnits,
    IntersectsAt intersectsAt)
{
    PhysicalContactWallConstraint result{};
    if (!PhysicalContactTransformFinite(intendedWeaponTransform) ||
        !PhysicalContactFinite(outwardDirection) ||
        !std::isfinite(proposedOffsetWorldUnits) ||
        proposedOffsetWorldUnits < 0.0f ||
        !std::isfinite(clearanceWorldUnits) || clearanceWorldUnits < 0.0f ||
        !std::isfinite(maximumOffsetWorldUnits) ||
        maximumOffsetWorldUnits <= 0.0f ||
        clearanceWorldUnits > maximumOffsetWorldUnits)
        return result;
    const PhysicalContactVec3 outward = PhysicalContactNormalize(
        outwardDirection, {});
    if (PhysicalContactLengthSquared(outward) <= 1.0e-12f)
        return result;

    const auto overlapsAtDistance = [&](float distance) {
        PhysicalContactTransform candidate = intendedWeaponTransform;
        candidate.position = candidate.position + outward * distance;
        return intersectsAt(candidate);
    };
    const bool intendedOverlaps = overlapsAtDistance(0.0f);
    if (!intendedOverlaps && proposedOffsetWorldUnits <= 1.0e-5f)
        return result;

    float low = intendedOverlaps ? 0.0f : std::clamp(
        proposedOffsetWorldUnits, 1.0e-5f, maximumOffsetWorldUnits);
    float high = std::clamp(
        std::max(proposedOffsetWorldUnits, clearanceWorldUnits),
        1.0e-5f, maximumOffsetWorldUnits);
    bool highIsClear = !overlapsAtDistance(high);
    if (highIsClear && intendedOverlaps)
    {
        low = 0.0f;
    }
    else if (!highIsClear)
    {
        low = high;
        for (int expansion = 0; expansion < 7 && !highIsClear; ++expansion)
        {
            const float expanded = std::min(
                maximumOffsetWorldUnits,
                std::max(high * 2.0f, high + clearanceWorldUnits));
            if (expanded <= high + 1.0e-6f)
                break;
            high = expanded;
            highIsClear = !overlapsAtDistance(high);
            if (!highIsClear)
                low = high;
        }
    }
    if (!highIsClear)
        return result;

    // Only an overlapping low endpoint supports a monotonic boundary search.
    // A tunnelling sweep can end clear on the far side; its already-clear
    // proposed setback must be preserved rather than bisected through space.
    if (overlapsAtDistance(low))
    {
        for (int iteration = 0; iteration < 10; ++iteration)
        {
            const float middle = (low + high) * 0.5f;
            if (overlapsAtDistance(middle))
                low = middle;
            else
                high = middle;
        }
    }
    const float padded = std::min(
        maximumOffsetWorldUnits, high + clearanceWorldUnits);
    if (padded > high && !overlapsAtDistance(padded))
        high = padded;
    if (overlapsAtDistance(high))
        return result;
    result.offset = outward * high;
    result.setbackWorldUnits = high;
    result.constrained = PhysicalContactFinite(result.offset);
    return result;
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

// A real hand swing can comfortably cross the configured 1.50 m/s melee
// threshold, but the preserved 120 Hz headset trace also contains isolated
// 25.03 m/s controller samples. Keep those implausible tracking spikes in the
// bounded physics path without turning them into native damage events.
inline constexpr float kPhysicalContactMaximumMeleeSpeedMetersPerSecond = 8.0f;

inline bool PhysicalContactMeleeSpeedPlausible(float impactSpeedMetersPerSecond)
{
    return std::isfinite(impactSpeedMetersPerSecond) &&
        impactSpeedMetersPerSecond >= 0.0f &&
        impactSpeedMetersPerSecond <=
            kPhysicalContactMaximumMeleeSpeedMetersPerSecond;
}

// Melee is an impact, not general hand motion. Only the first exact contact
// may damage, and only velocity closing into the target-facing normal counts.
// Tangential sliding, pulling away, and sustained pressure remain physics-only.
inline float PhysicalContactMeleeImpactSpeed(
    bool firstContact, PhysicalContactVec3 relativeVelocityMetersPerSecond,
    PhysicalContactVec3 targetToWeaponNormal)
{
    if (!firstContact ||
        !PhysicalContactFinite(relativeVelocityMetersPerSecond) ||
        !PhysicalContactFinite(targetToWeaponNormal))
        return 0.0f;
    const PhysicalContactVec3 outward = PhysicalContactNormalize(
        targetToWeaponNormal, {});
    if (PhysicalContactLengthSquared(outward) <= 1.0e-12f)
        return 0.0f;
    const float closing = -PhysicalContactDot(
        relativeVelocityMetersPerSecond, outward);
    return std::isfinite(closing) ? std::max(0.0f, closing) : 0.0f;
}

inline bool PhysicalContactEnemyMeleeKind(uint8_t kind)
{
    // H3 object kinds: biped, creature, giant. Vehicles deliberately retain
    // the stricter surface-normal impact rule so a shove cannot become melee.
    return kind == 0 || kind == 12 || kind == 13;
}

inline bool PhysicalContactTargetMeleeSpeedEligible(
    bool firstContact, uint8_t targetKind, bool meleeArmed)
{
    // Props and vehicles retain the strict first-contact surface test. An
    // enemy may admit a deliberate swing later in the same overlap because an
    // animated limb can first touch between controller samples, or while the
    // global melee cooldown is still active. The per-target armed latch still
    // limits the continuous contact to one native melee event.
    return firstContact ||
        (meleeArmed && PhysicalContactEnemyMeleeKind(targetKind));
}

inline PhysicalContactVec3 PhysicalContactEnemyMeleeFallbackNormal(
    PhysicalContactVec3 weaponVelocityMetersPerSecond,
    PhysicalContactVec3 movementDirection)
{
    if (!PhysicalContactFinite(weaponVelocityMetersPerSecond) ||
        !PhysicalContactFinite(movementDirection))
        return {};
    const PhysicalContactVec3 movementFallback = PhysicalContactNormalize(
        movementDirection * -1.0f, {});
    return PhysicalContactNormalize(
        weaponVelocityMetersPerSecond * -1.0f, movementFallback);
}

inline float PhysicalContactTargetMeleeImpactSpeed(
    bool firstContact, bool enemyWeaponSpeedEligible, uint8_t targetKind,
    PhysicalContactVec3 relativeVelocityMetersPerSecond,
    PhysicalContactVec3 weaponVelocityMetersPerSecond,
    PhysicalContactVec3 targetToWeaponNormal)
{
    const float normalImpact = PhysicalContactMeleeImpactSpeed(
        firstContact, relativeVelocityMetersPerSecond,
        targetToWeaponNormal);
    if (!enemyWeaponSpeedEligible ||
        !PhysicalContactEnemyMeleeKind(targetKind) ||
        !PhysicalContactFinite(weaponVelocityMetersPerSecond))
        return normalImpact;

    // Animated limb normals turn sharply across elbows, shoulders and heads.
    // For enemies only, a deliberate armed weapon-point swing may melee even
    // when that exact limb normal makes the hit look tangential.
    // This enemy-only tangential allowance uses weapon speed, not relative
    // target motion. First contact still retains the ordinary inward-surface
    // rule used by every target kind.
    const float weaponSpeed = PhysicalContactLength(
        weaponVelocityMetersPerSecond);
    return std::isfinite(weaponSpeed)
        ? std::max(normalImpact, weaponSpeed) : normalImpact;
}

inline PhysicalContactAction PhysicalContactClassify(
    float relativeSpeedMetersPerSecond, float meleeImpactSpeedMetersPerSecond,
    float meleeThresholdMetersPerSecond)
{
    if (!std::isfinite(relativeSpeedMetersPerSecond) ||
        !std::isfinite(meleeImpactSpeedMetersPerSecond) ||
        !std::isfinite(meleeThresholdMetersPerSecond) ||
        relativeSpeedMetersPerSecond < 0.05f)
        return PhysicalContactAction::None;
    return meleeImpactSpeedMetersPerSecond >= meleeThresholdMetersPerSecond &&
        PhysicalContactMeleeSpeedPlausible(meleeImpactSpeedMetersPerSecond)
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

inline bool PhysicalContactUseExactBodyPointImpulse(
    uint32_t targetShapeSource, int32_t rigidBodyIndex)
{
    // Shape source 3 is the H3EK-authored animated multi-body path. The
    // physics-model reader admits at most 32 bodies, so keep the command in
    // that same fixed domain.
    return targetShapeSource == 3 && rigidBodyIndex >= 0 &&
        rigidBodyIndex < 32;
}

inline bool PhysicalContactLeftGrabKind(uint8_t kind)
{
    // H3 object kinds proven by the contact runs: weapon, equipment/grenade,
    // garbage, and crate-class. Bipeds, vehicles, scenery, projectiles, and
    // giants are deliberately excluded.
    return kind == 2 || kind == 3 || kind == 4 || kind == 10;
}

inline bool PhysicalContactGripHeld(float gripValue, bool previouslyHeld)
{
    if (!std::isfinite(gripValue))
        return false;
    return previouslyHeld ? gripValue > 0.45f : gripValue >= 0.65f;
}

inline PhysicalContactVec3 PhysicalContactLeftGrabFollowVelocity(
    PhysicalContactVec3 currentCentreWorld,
    PhysicalContactVec3 desiredCentreWorld,
    PhysicalContactVec3 palmVelocityMetersPerSecond,
    float worldUnitsPerMeter)
{
    if (!PhysicalContactFinite(currentCentreWorld) ||
        !PhysicalContactFinite(desiredCentreWorld) ||
        !PhysicalContactFinite(palmVelocityMetersPerSecond) ||
        !std::isfinite(worldUnitsPerMeter) || worldUnitsPerMeter < 0.05f ||
        worldUnitsPerMeter > 2.0f)
        return {};
    PhysicalContactVec3 correctionMetersPerSecond =
        (desiredCentreWorld - currentCentreWorld) *
        (12.0f / worldUnitsPerMeter);
    const float correctionSpeed = PhysicalContactLength(
        correctionMetersPerSecond);
    if (correctionSpeed > 2.5f)
        correctionMetersPerSecond = correctionMetersPerSecond *
            (2.5f / correctionSpeed);
    PhysicalContactVec3 velocityMetersPerSecond =
        palmVelocityMetersPerSecond + correctionMetersPerSecond;
    const float speed = PhysicalContactLength(velocityMetersPerSecond);
    if (speed > 8.0f)
        velocityMetersPerSecond = velocityMetersPerSecond * (8.0f / speed);
    const PhysicalContactVec3 worldVelocity =
        velocityMetersPerSecond * worldUnitsPerMeter;
    return PhysicalContactFinite(worldVelocity) ? worldVelocity
                                                 : PhysicalContactVec3{};
}

struct PhysicalContactDebugTargetRank
{
    bool valid = false;
    int priority = -1;
    float score = 0.0f;
};

// The automated Forge rig needs a settled light prop. The nearest weapon can
// be a falling hammer or launcher, whose own velocity makes a slow-hand test
// measure a large relative impact. The Construct probe also contains lighter
// map-authored weapons that report dynamic Havok motion but remain constrained
// to their Forge spawn. Prefer the repeatedly proven near-2 kg loose weapon,
// then another near-stationary light weapon, then any weapon, then another
// movable root. An existing anchor always wins.
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
    const bool provenLooseWeapon = stableLightWeapon &&
        massKilograms >= 1.8f && massKilograms <= 2.2f;
    result.priority = anchored
        ? 4
        : (provenLooseWeapon ? 3
           : (stableLightWeapon ? 2 : (kind == 2 ? 1 : 0)));
    result.score = anchored
        ? 0.0f
        : speedMetersPerSecond + std::fabs(massKilograms - 2.0f) * 0.02f +
          distanceSquaredWorldUnits * 0.001f;
    result.valid = std::isfinite(result.score);
    return result;
}

struct PhysicalContactDebugBoundsPlacement
{
    bool valid = false;
    PhysicalContactVec3 translation{};
};

// Debug-only placement for objects whose authored contact shape is resolved
// from node-bound bodies instead of one root compound. The bounds do not decide
// contact. They only move the synthetic weapon across the near side so the
// production exact-shape sweep remains the sole hit authority.
inline PhysicalContactDebugBoundsPlacement
PhysicalContactDebugBoundsSweepPlacement(
    PhysicalContactVec3 weaponCentroidWorld,
    PhysicalContactVec3 targetCenterWorld,
    PhysicalContactVec3 forwardWorld,
    float weaponRadiusWorld, float targetRadiusWorld,
    float worldUnitsPerMeter, float maximumSpeedMetersPerSecond,
    float angularRateRadiansPerSecond, float phaseRadians)
{
    PhysicalContactDebugBoundsPlacement result{};
    if (!PhysicalContactFinite(weaponCentroidWorld) ||
        !PhysicalContactFinite(targetCenterWorld) ||
        !PhysicalContactFinite(forwardWorld) ||
        !std::isfinite(weaponRadiusWorld) || weaponRadiusWorld <= 0.0f ||
        !std::isfinite(targetRadiusWorld) || targetRadiusWorld <= 0.0f ||
        !std::isfinite(worldUnitsPerMeter) || worldUnitsPerMeter <= 0.0f ||
        !std::isfinite(maximumSpeedMetersPerSecond) ||
        maximumSpeedMetersPerSecond <= 0.0f ||
        !std::isfinite(angularRateRadiansPerSecond) ||
        angularRateRadiansPerSecond <= 0.0f ||
        !std::isfinite(phaseRadians))
        return result;
    const PhysicalContactVec3 forward = PhysicalContactNormalize(
        forwardWorld, {});
    if (PhysicalContactLengthSquared(forward) <= 1.0e-12f)
        return result;
    const float amplitudeWorld = worldUnitsPerMeter *
        maximumSpeedMetersPerSecond / angularRateRadiansPerSecond;
    const float displacementWorld = amplitudeWorld * std::sin(phaseRadians) -
        worldUnitsPerMeter * 0.02f;
    const PhysicalContactVec3 targetNear =
        targetCenterWorld - forward * targetRadiusWorld;
    const PhysicalContactVec3 weaponFront =
        weaponCentroidWorld + forward * weaponRadiusWorld;
    result.translation =
        targetNear + forward * displacementWorld - weaponFront;
    result.valid = PhysicalContactFinite(result.translation);
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
    // Keep moving sideways while the weapon separates downward. This leaves
    // the target with measurable lateral velocity instead of carrying it to
    // rest before release.
    const float carryPhase = (elapsedMilliseconds - 2500.0f) / 2500.0f;
    const float releasePhase = (elapsedMilliseconds - 3500.0f) / 500.0f;
    return {
        0.35f * smoothStep(liftPhase),
        0.35f * smoothStep(carryPhase),
        0.40f * smoothStep(releasePhase),
        0.35f * smoothStepVelocity(liftPhase, 1.5f),
        0.35f * smoothStepVelocity(carryPhase, 2.5f),
        0.40f * smoothStepVelocity(releasePhase, 0.5f)};
}

inline bool PhysicalContactDebugScoopPassed(
    float peakLiftMeters, float peakCarryMeters,
    float peakReleaseSpeedMetersPerSecond)
{
    return std::isfinite(peakLiftMeters) && peakLiftMeters >= 0.02f &&
        std::isfinite(peakCarryMeters) && peakCarryMeters >= 0.05f &&
        std::isfinite(peakReleaseSpeedMetersPerSecond) &&
        peakReleaseSpeedMetersPerSecond >= 0.05f;
}

inline bool PhysicalContactDebugTargetMovedEnough(
    PhysicalContactVec3 initialWorldPosition,
    PhysicalContactVec3 currentWorldPosition, float worldUnitsPerMeter,
    float requiredMeters = 0.05f)
{
    if (!PhysicalContactFinite(initialWorldPosition) ||
        !PhysicalContactFinite(currentWorldPosition) ||
        !std::isfinite(worldUnitsPerMeter) || worldUnitsPerMeter <= 0.0f ||
        !std::isfinite(requiredMeters) || requiredMeters <= 0.0f)
        return false;
    const float movedMeters = PhysicalContactLength(
        currentWorldPosition - initialWorldPosition) / worldUnitsPerMeter;
    return std::isfinite(movedMeters) && movedMeters >= requiredMeters;
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

inline bool PhysicalContactLeftGrabCandidate(
    bool exactPalmOverlap, uint8_t kind, uint8_t motionType,
    float massKilograms)
{
    return exactPalmOverlap && PhysicalContactLeftGrabKind(kind) &&
        PhysicalContactMotionTypeIsDynamic(motionType) &&
        std::isfinite(massKilograms) && massKilograms >= 0.01f &&
        massKilograms <= 25.0f;
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
// bounded normal load. An upward-facing contact also supports the target's
// native mass against standard Halo gravity while the weapon is not separating.
// Coulomb-limited tangential force then lets the weapon carry or scoop a light
// body without giving a heavy body the same response.
inline PhysicalContactConstraintImpulse PhysicalContactSustainedImpulse(
    float weaponMassKilograms, float targetMassKilograms,
    PhysicalContactVec3 relativeMetersPerSecond,
    PhysicalContactVec3 contactNormal, float penetrationMeters,
    float elapsedSeconds, float worldUnitsPerMeter,
    PhysicalContactVec3 worldUp = {0.0f, 0.0f, 1.0f})
{
    PhysicalContactConstraintImpulse result{};
    if (!std::isfinite(weaponMassKilograms) ||
        !std::isfinite(targetMassKilograms) ||
        weaponMassKilograms <= 0.001f || targetMassKilograms <= 0.001f ||
        weaponMassKilograms > 1000000.0f ||
        targetMassKilograms > 1000000.0f ||
        !PhysicalContactFinite(relativeMetersPerSecond) ||
        !PhysicalContactFinite(contactNormal) ||
        !PhysicalContactFinite(worldUp) ||
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
    const PhysicalContactVec3 up = PhysicalContactNormalize(worldUp, {});
    if (PhysicalContactLengthSquared(up) <= 1.0e-12f)
        return result;
    const float signedApproach = PhysicalContactDot(
        relativeMetersPerSecond, inward);
    const float approach = std::max(signedApproach, 0.0f);
    const float reducedMass =
        weaponMassKilograms * targetMassKilograms /
        (weaponMassKilograms + targetMassKilograms);

    // A floor-loaded body can lose its previous correction inside Halo's next
    // object update. Solve the measured relative velocity in one contact
    // sample; authored reduced mass, Coulomb friction, and the per-sample cap
    // still bound the response. The old 20% solve moved the body while held
    // but left almost no velocity at separation.
    constexpr float kVelocityFollowFraction = 1.00f;
    constexpr float kPenetrationCorrectionFraction = 0.15f;
    constexpr float kMaximumPenetrationSpeed = 0.25f;
    constexpr float kMaximumTargetDeltaPerSample = 0.08f;
    constexpr float kStandardGravityMetersPerSecondSquared = 9.81f;
    constexpr float kFriction = 0.80f;
    const float penetrationSpeed = std::min(
        penetrationMeters * kPenetrationCorrectionFraction / elapsedSeconds,
        kMaximumPenetrationSpeed);
    const float normalCorrectionSpeed = std::max(
        approach * kVelocityFollowFraction, penetrationSpeed);
    // The native floor solver removes a small upward velocity correction on
    // the next tick. Counter only the component of gravity that loads this
    // exact contact. Stop the support as soon as the weapon separates, so a
    // released prop keeps Halo's own ballistic motion.
    const float upwardLoad = std::max(
        0.0f, PhysicalContactDot(inward, up));
    const float gravitySupportDelta = signedApproach >= -0.01f
        ? kStandardGravityMetersPerSecondSquared * elapsedSeconds *
              upwardLoad
        : 0.0f;
    float normalImpulse = reducedMass * normalCorrectionSpeed +
        targetMassKilograms * gravitySupportDelta;
    normalImpulse = std::min(
        normalImpulse,
        targetMassKilograms *
            (kMaximumTargetDeltaPerSample + gravitySupportDelta));

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
        targetMassKilograms *
        (kMaximumTargetDeltaPerSample + gravitySupportDelta);
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

// Halo's object_set_velocities entry consumes an absolute world-space linear
// velocity. Convert the bounded physical impulse into only the target's
// velocity change before adding it to the native velocity readback. This keeps
// authored mass in the response: a light prop follows the weapon, while a
// vehicle or other very heavy body receives a proportionally tiny nudge.
inline PhysicalContactVec3 PhysicalContactTargetDeltaVelocity(
    const PhysicalContactConstraintImpulse& response,
    float targetMassKilograms)
{
    if (!response.apply || !PhysicalContactFinite(response.worldImpulse) ||
        !std::isfinite(targetMassKilograms) ||
        targetMassKilograms <= 0.001f ||
        targetMassKilograms > 1000000.0f)
        return {};
    const PhysicalContactVec3 delta =
        response.worldImpulse * (1.0f / targetMassKilograms);
    return PhysicalContactFinite(delta) ? delta : PhysicalContactVec3{};
}

// After separation Halo can clear the velocity that a floor-loaded body had
// while following the weapon. Restore only the difference between the tracked
// weapon contact point and the post-update target point. Authored target mass
// converts that velocity difference to impulse. The rejected vehicle trace
// used 114 kg m/s, so this one-shot handoff has a fixed 1 kg m/s ceiling.
inline PhysicalContactVec3 PhysicalContactReleaseImpulse(
    float targetMassKilograms,
    PhysicalContactVec3 desiredMetersPerSecond,
    PhysicalContactVec3 currentMetersPerSecond,
    float worldUnitsPerMeter)
{
    if (!std::isfinite(targetMassKilograms) ||
        targetMassKilograms <= 0.001f ||
        targetMassKilograms > 1000000.0f ||
        !PhysicalContactFinite(desiredMetersPerSecond) ||
        !PhysicalContactFinite(currentMetersPerSecond) ||
        !std::isfinite(worldUnitsPerMeter) || worldUnitsPerMeter <= 0.0f)
        return {};
    PhysicalContactVec3 impulse =
        (desiredMetersPerSecond - currentMetersPerSecond) *
        targetMassKilograms;
    const float length = PhysicalContactLength(impulse);
    if (!std::isfinite(length) || length <= 1.0e-8f)
        return {};
    constexpr float kMaximumReleaseImpulseKilogramMetersPerSecond = 1.0f;
    if (length > kMaximumReleaseImpulseKilogramMetersPerSecond)
    {
        impulse = impulse *
            (kMaximumReleaseImpulseKilogramMetersPerSecond / length);
    }
    const PhysicalContactVec3 result = impulse * worldUnitsPerMeter;
    return PhysicalContactFinite(result) ? result : PhysicalContactVec3{};
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
    uint64_t lastOverlapMs = 0;
};

class PhysicalContactDebounce
{
public:
    static constexpr size_t kCapacity = 32;
    static constexpr uint64_t kSeparationRearmMs = 100;

    void BeginSample()
    {
        for (auto& slot : slots_)
        {
            slot.previouslyOverlapping = slot.overlapping;
            slot.overlapping = false;
        }
    }

    PhysicalContactTargetState* Touch(
        int32_t handle, uint64_t nowMs, bool* firstContact = nullptr)
    {
        PhysicalContactTargetState* empty = nullptr;
        for (auto& slot : slots_)
        {
            if (slot.handle == handle)
            {
                const bool separated = !slot.lastOverlapMs ||
                    nowMs < slot.lastOverlapMs ||
                    nowMs - slot.lastOverlapMs >= kSeparationRearmMs;
                if (separated)
                {
                    slot.meleeArmed = true;
                    slot.contactNormalValid = false;
                    slot.belowHalfSinceMs = 0;
                }
                if (firstContact)
                    *firstContact = separated;
                slot.overlapping = true;
                slot.lastOverlapMs = nowMs;
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
        empty->lastOverlapMs = nowMs;
        if (firstContact)
            *firstContact = true;
        return empty;
    }

    bool PreserveUncertainContact(int32_t handle, uint64_t nowMs)
    {
        for (auto& slot : slots_)
        {
            if (slot.handle != handle)
                continue;
            slot.overlapping = true;
            slot.lastOverlapMs = nowMs;
            return true;
        }
        return false;
    }

    void EndSample(uint64_t nowMs)
    {
        for (auto& slot : slots_)
            if (slot.handle != -1 && !slot.overlapping &&
                (!slot.lastOverlapMs || nowMs < slot.lastOverlapMs ||
                 nowMs - slot.lastOverlapMs >= kSeparationRearmMs))
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

struct PhysicalContactReleaseCommand
{
    int32_t handle = -1;
    PhysicalContactVec3 point{};
    PhysicalContactVec3 desiredMetersPerSecond{};
    bool apply = false;
};

// Keep the newest tracked slow-contact handoff. Consume it once after exact
// shape separation. A different target and stale or invalid state clear the
// latch without transferring motion.
class PhysicalContactReleaseLatch
{
public:
    void Arm(int32_t handle, PhysicalContactVec3 point,
             PhysicalContactVec3 desiredMetersPerSecond, uint64_t sampleMs)
    {
        if (handle == -1 || !sampleMs || !PhysicalContactFinite(point) ||
            !PhysicalContactFinite(desiredMetersPerSecond) ||
            PhysicalContactLengthSquared(desiredMetersPerSecond) <= 0.0025f)
        {
            Reset();
            return;
        }
        handle_ = handle;
        point_ = point;
        desiredMetersPerSecond_ = desiredMetersPerSecond;
        sampleMs_ = sampleMs;
    }

    PhysicalContactReleaseCommand TakeIfSeparated(
        int32_t currentContactHandle, uint64_t nowMs)
    {
        PhysicalContactReleaseCommand result{};
        if (handle_ == -1)
            return result;
        if (currentContactHandle == handle_)
            return result;
        if (currentContactHandle != -1 || !nowMs || nowMs < sampleMs_ ||
            nowMs - sampleMs_ > 100)
        {
            Reset();
            return result;
        }
        result.handle = handle_;
        result.point = point_;
        result.desiredMetersPerSecond = desiredMetersPerSecond_;
        result.apply = true;
        Reset();
        return result;
    }

    void Reset()
    {
        handle_ = -1;
        point_ = {};
        desiredMetersPerSecond_ = {};
        sampleMs_ = 0;
    }

private:
    int32_t handle_ = -1;
    PhysicalContactVec3 point_{};
    PhysicalContactVec3 desiredMetersPerSecond_{};
    uint64_t sampleMs_ = 0;
};
