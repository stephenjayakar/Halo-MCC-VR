#pragma once
#include "physical_contact_logic.h"
#include <cfloat>

// Each sphere encloses one complete AABB slab of one authored convex child.
// This covers faces and interior, not just selected vertices or triangles.
// Coordinates and radii stay in weapon-local units; the caller applies scale.
struct PhysicalContactVolumeSphere
{
    PhysicalContactVec3 center{};
    float radius = 0;
    uint16_t child = 0;
};

struct PhysicalContactVolumeCover
{
    static constexpr size_t kMaximumSpheres = 64;
    std::array<PhysicalContactVolumeSphere, kMaximumSpheres> spheres{};
    uint16_t count = 0;
};

inline bool PhysicalContactBuildVolumeCover(const PhysicalContactCompoundShape& shape,
    float maximumSlabLength, PhysicalContactVolumeCover& output)
{
    output.count = 0;
    if (!PhysicalContactCompoundValid(shape) || !std::isfinite(maximumSlabLength) ||
        maximumSlabLength < 1.0e-5f || maximumSlabLength > 10.0f)
        return false;
    PhysicalContactVolumeCover candidate{};
    for (uint16_t childIndex = 0; childIndex < shape.childCount; ++childIndex)
    {
        const auto& child = shape.children[childIndex];
        double low[3]{DBL_MAX, DBL_MAX, DBL_MAX};
        double high[3]{-DBL_MAX, -DBL_MAX, -DBL_MAX};
        for (uint16_t i = 0; i < child.vertexCount; ++i)
        {
            const auto p = child.vertices[i];
            const double v[3]{p.x, p.y, p.z};
            for (int axis = 0; axis < 3; ++axis)
            {
                low[axis] = std::min(low[axis], v[axis]);
                high[axis] = std::max(high[axis], v[axis]);
            }
        }
        const double lengths[3]{high[0]-low[0], high[1]-low[1], high[2]-low[2]};
        int axis = lengths[1] > lengths[0] ? 1 : 0;
        if (lengths[2] > lengths[axis]) axis = 2;
        const double requested = std::ceil(lengths[axis] / maximumSlabLength);
        if (!std::isfinite(requested) || requested > PhysicalContactVolumeCover::kMaximumSpheres)
            return false;
        const auto slabs = static_cast<uint16_t>(std::max(1.0, requested));
        if (candidate.count + slabs > candidate.kMaximumSpheres)
            return false;
        const double step = lengths[axis] / slabs;
        for (uint16_t slab = 0; slab < slabs; ++slab)
        {
            float c[3]{};
            double half[3]{};
            for (int coordinate = 0; coordinate < 3; ++coordinate)
            {
                const double a = coordinate == axis ? low[coordinate] + slab*step : low[coordinate];
                const double b = coordinate == axis ? low[coordinate] + (slab+1)*step : high[coordinate];
                c[coordinate] = static_cast<float>((a+b)*.5);
                // Include the rounding of the stored center, not only the
                // ideal mathematical half-extents of this slab.
                half[coordinate] = std::max(std::abs(c[coordinate]-a), std::abs(b-c[coordinate]));
            }
            if (!PhysicalContactFinite({c[0],c[1],c[2]})) return false;
            // Expanding the enclosing ball by the authored Minkowski radius
            // covers the entire padded slab. Round the stored radius outward.
            const float radius = std::nextafter(static_cast<float>(
                std::sqrt(half[0]*half[0]+half[1]*half[1]+half[2]*half[2])
                    + child.radius + 1.0e-6), INFINITY);
            if (!std::isfinite(radius) || radius <= 0 || radius > 100.0f) return false;
            candidate.spheres[candidate.count++] = {{c[0],c[1],c[2]}, radius, childIndex};
        }
    }
    if (!candidate.count) return false;
    output = candidate;
    return true;
}

// Rotating a local sphere center along the shortest rigid arc differs from
// its chord by at most leverArm * (1-cos(angle/2)). A fixed query skin can
// contain that error; bound the angular substep before issuing a native cast.
inline float PhysicalContactVolumeArcError(float leverArm, float angleRadians)
{
    if (!std::isfinite(leverArm) || leverArm < 0 || !std::isfinite(angleRadians) ||
        angleRadians < 0 || angleRadians > 3.141593f)
        return INFINITY;
    // 2*sin(a/4)^2 avoids cancellation for small angles. Round outward so a
    // positive arc never becomes zero because float cosine rounded to one.
    if (leverArm == 0 || angleRadians == 0) return 0;
    const double sine = std::sin(static_cast<double>(angleRadians) * .25);
    return std::nextafter(static_cast<float>(2.0 * leverArm * sine * sine), INFINITY);
}

inline uint32_t PhysicalContactVolumeAngularSteps(float leverArm, float angleRadians,
    float skin, uint32_t maximumSteps = 16)
{
    if (!std::isfinite(skin) || skin <= 0 || !maximumSteps || maximumSteps > 64 ||
        !std::isfinite(PhysicalContactVolumeArcError(leverArm, angleRadians))) return 0;
    for (uint32_t steps = 1; steps <= maximumSteps; ++steps)
        if (PhysicalContactVolumeArcError(leverArm, angleRadians / steps) <= skin)
            return steps;
    return 0; // Never silently exceed the fixed angular coverage budget.
}
