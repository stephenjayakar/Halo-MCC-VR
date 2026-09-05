// Offline prototype; not included by the DLL. Authored geometry must already
// be verified against the selected retail render mesh before runtime use.
#pragma once
#include "../src/common/physical_contact_logic.h"
#include <cfloat>

inline bool H3PrototypeBladeInRootFrame(
    const PhysicalContactTriangleMesh& authored,
    const PhysicalContactTransform& inverseBind,
    const PhysicalContactTransform& animatedNode,
    const PhysicalContactTransform& root,
    PhysicalContactTriangleMesh& result)
{
    if (&authored == &result) return false;
    result.triangleCount = result.groupCount = 0;
    if (!PhysicalContactTriangleMeshValid(authored) ||
        !PhysicalContactTransformFinite(inverseBind) ||
        !PhysicalContactTransformFinite(animatedNode) ||
        !PhysicalContactTransformFinite(root)) return false;
    const auto expand = [](PhysicalContactVec3 p, PhysicalContactVec3& lo,
                           PhysicalContactVec3& hi) {
        lo = {std::min(lo.x,p.x), std::min(lo.y,p.y), std::min(lo.z,p.z)};
        hi = {std::max(hi.x,p.x), std::max(hi.y,p.y), std::max(hi.z,p.z)};
    };
    for (uint16_t i = 0; i < authored.triangleCount; ++i)
    {
        auto& out = result.triangles[i];
        out = {};
        PhysicalContactVec3 lo{FLT_MAX,FLT_MAX,FLT_MAX}, hi{-FLT_MAX,-FLT_MAX,-FLT_MAX};
        for (unsigned j = 0; j < 3; ++j)
        {
            const auto local = PhysicalContactTransformPoint(inverseBind, authored.triangles[i].vertices[j]);
            const auto world = PhysicalContactTransformPoint(animatedNode, local);
            out.vertices[j] = PhysicalContactInverseTransformPoint(root, world);
            if (!PhysicalContactFinite(out.vertices[j])) return false;
            expand(out.vertices[j], lo, hi);
        }
        out.centre = (lo + hi) * .5f;
        out.halfExtents = (hi - lo) * .5f;
        for (auto p : out.vertices)
            out.boundRadius = std::max(out.boundRadius, PhysicalContactLength(p-out.centre));
    }
    for (uint16_t i = 0; i < authored.groupCount; ++i)
    {
        auto& out = result.groups[i];
        out = {};
        out.firstTriangle = authored.groups[i].firstTriangle;
        out.triangleCount = authored.groups[i].triangleCount;
        PhysicalContactVec3 lo{FLT_MAX,FLT_MAX,FLT_MAX}, hi{-FLT_MAX,-FLT_MAX,-FLT_MAX};
        for (unsigned j = out.firstTriangle; j < out.firstTriangle + out.triangleCount; ++j)
            for (auto p : result.triangles[j].vertices) expand(p, lo, hi);
        out.centre = (lo + hi) * .5f;
        out.halfExtents = (hi - lo) * .5f;
        out.boundRadius = PhysicalContactLength(out.halfExtents);
    }
    result.triangleCount = authored.triangleCount;
    result.groupCount = authored.groupCount;
    if (PhysicalContactTriangleMeshValid(result)) return true;
    result.triangleCount = result.groupCount = 0;
    return false;
}
