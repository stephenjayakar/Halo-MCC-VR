#pragma once
#include "halo3_sword_geometry_data.h"
#include <cfloat>

// Geometry only. The caller must prove the live sword identity, blade selection
// and inverse/live-node mapping. No tag ids or visibility assumptions live here.
inline bool Halo3SwordTransformUsable(const PhysicalContactTransform& t)
{
    return PhysicalContactTransformFinite(t) &&
        std::fabs(PhysicalContactDot(t.forward,t.forward)-1.f) < .002f &&
        std::fabs(PhysicalContactDot(t.left,t.left)-1.f) < .002f &&
        std::fabs(PhysicalContactDot(t.up,t.up)-1.f) < .002f &&
        std::fabs(PhysicalContactDot(t.forward,t.left)) < .002f &&
        std::fabs(PhysicalContactDot(t.forward,t.up)) < .002f &&
        std::fabs(PhysicalContactDot(t.left,t.up)) < .002f &&
        PhysicalContactDot(PhysicalContactCross(t.forward,t.left),t.up) > .998f;
}

// Append two conservative prong hulls and all 244 exact authored triangles.
// The gap is never enclosed by one hull. Existing handle entries remain intact.
// Failed appends leave their original valid counts unchanged; scratch slots
// beyond those counts are deliberately unpublished.
inline bool Halo3AppendSwordBladeGeometry(
    const PhysicalContactTransform& inverseBind,
    const PhysicalContactTransform& animatedBlade,
    const PhysicalContactTransform& root,
    PhysicalContactCompoundShape& compound,
    PhysicalContactTriangleMesh* mesh = nullptr)
{
    if (!Halo3SwordTransformUsable(inverseBind) ||
        !Halo3SwordTransformUsable(animatedBlade) || !Halo3SwordTransformUsable(root) ||
        compound.childCount > 2 ||
        (compound.childCount && !PhysicalContactCompoundValid(compound)) ||
        (mesh && (mesh->triangleCount > mesh->kMaximumTriangles-244 ||
                  mesh->groupCount > mesh->kMaximumGroups-2 ||
                  ((mesh->triangleCount || mesh->groupCount) &&
                   !PhysicalContactTriangleMeshValid(*mesh))))) return false;
    PhysicalContactVec3 points[126]{};
    for (size_t i = 0; i < 126; ++i)
    {
        points[i] = PhysicalContactInverseTransformPoint(root,
            PhysicalContactTransformPoint(animatedBlade,
                PhysicalContactTransformPoint(inverseBind,kHalo3SwordBladePoints[i])));
        if (!PhysicalContactFinite(points[i])) return false;
    }
    const auto expand = [](PhysicalContactVec3 p, PhysicalContactVec3& lo, PhysicalContactVec3& hi) {
        lo = {std::min(lo.x,p.x),std::min(lo.y,p.y),std::min(lo.z,p.z)};
        hi = {std::max(hi.x,p.x),std::max(hi.y,p.y),std::max(hi.z,p.z)};
    };
    for (unsigned side = 0; side < 2; ++side)
    {
        auto& child = compound.children[compound.childCount+side];
        child = {};
        child.vertexCount = 63;
        PhysicalContactVec3 lo{FLT_MAX,FLT_MAX,FLT_MAX}, hi{-FLT_MAX,-FLT_MAX,-FLT_MAX};
        for (unsigned v = 0; v < 63; ++v)
        {
            child.vertices[v] = points[side*63+v];
            expand(child.vertices[v],lo,hi);
        }
        if (!PhysicalContactConvexValid(child)) return false;
        if (!mesh) continue;
        auto& group = mesh->groups[mesh->groupCount+side];
        group = {};
        group.firstTriangle = static_cast<uint16_t>(mesh->triangleCount+side*122);
        group.triangleCount = 122;
        group.centre = (lo+hi)*.5f;
        group.halfExtents = (hi-lo)*.5f;
        group.boundRadius = PhysicalContactLength(group.halfExtents);
        for (unsigned f = 0; f < 122; ++f)
        {
            auto& triangle = mesh->triangles[group.firstTriangle+f];
            triangle = {};
            PhysicalContactVec3 tlo{FLT_MAX,FLT_MAX,FLT_MAX}, thi{-FLT_MAX,-FLT_MAX,-FLT_MAX};
            for (unsigned v = 0; v < 3; ++v)
            {
                triangle.vertices[v] = points[kHalo3SwordBladeFaces[side*122+f][v]];
                expand(triangle.vertices[v],tlo,thi);
            }
            triangle.centre = (tlo+thi)*.5f;
            triangle.halfExtents = (thi-tlo)*.5f;
            for (auto p : triangle.vertices)
                triangle.boundRadius = std::max(triangle.boundRadius,
                    PhysicalContactLength(p-triangle.centre));
            if (!PhysicalContactTriangleValid(triangle)) return false;
        }
    }
    compound.childCount += 2;
    if (mesh) { mesh->triangleCount += 244; mesh->groupCount += 2; }
    return true;
}
