// Offline native contact-kernel benchmark using official authored blade data.
// Never linked into the mod and never accesses a game process.
#include <cfloat>
#include <chrono>
#include <fstream>
#include <iostream>
#include <vector>
#include "../src/common/physical_contact_logic.h"
#include "h3-blade-mesh-prototype.h"
#include "../src/common/halo3_sword_contact_logic.h"

int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    std::ifstream input(argv[1]);
    static PhysicalContactTriangleMesh mesh{};
    unsigned count = 0;
    if (!(input >> count) || !count || count > mesh.kMaximumTriangles) return 3;
    PhysicalContactVec3 minimum{FLT_MAX, FLT_MAX, FLT_MAX};
    PhysicalContactVec3 maximum{-FLT_MAX, -FLT_MAX, -FLT_MAX};
    const auto expand = [](PhysicalContactVec3 p, PhysicalContactVec3& lo,
                           PhysicalContactVec3& hi) {
        lo = {std::min(lo.x,p.x), std::min(lo.y,p.y), std::min(lo.z,p.z)};
        hi = {std::max(hi.x,p.x), std::max(hi.y,p.y), std::max(hi.z,p.z)};
    };
    for (unsigned i = 0; i < count; ++i)
    {
        auto& t = mesh.triangles[i];
        PhysicalContactVec3 lo{FLT_MAX, FLT_MAX, FLT_MAX};
        PhysicalContactVec3 hi{-FLT_MAX, -FLT_MAX, -FLT_MAX};
        for (auto& p : t.vertices)
        {
            if (!(input >> p.x >> p.y >> p.z) || !PhysicalContactFinite(p)) return 4;
            expand(p, lo, hi);
            expand(p, minimum, maximum);
        }
        t.centre = (lo + hi) * .5f;
        t.halfExtents = (hi - lo) * .5f;
        for (auto p : t.vertices)
            t.boundRadius = std::max(t.boundRadius, PhysicalContactLength(p-t.centre));
    }
    mesh.triangleCount = static_cast<uint16_t>(count);
    mesh.groupCount = 1;
    mesh.groups[0].centre = (minimum + maximum) * .5f;
    mesh.groups[0].halfExtents = (maximum - minimum) * .5f;
    mesh.groups[0].boundRadius = PhysicalContactLength(mesh.groups[0].halfExtents);
    mesh.groups[0].triangleCount = mesh.triangleCount;
    if (!PhysicalContactTriangleMeshValid(mesh)) return 5;
    PhysicalContactCompoundShape sphere{};
    sphere.childCount = 1;
    sphere.children[0].vertexCount = 1;
    sphere.children[0].radius = .003f;
    PhysicalContactTransform identity{}, target{};
    unsigned centroidHits = 0;
    for (unsigned i = 0; i < count; ++i)
    {
        const auto& t = mesh.triangles[i];
        target.position = (t.vertices[0]+t.vertices[1]+t.vertices[2])*(1.f/3.f);
        centroidHits += PhysicalContactTriangleMeshCompoundIntersect(
            mesh, identity, sphere, target, 0).hit ? 1 : 0;
    }
    target.position = {.25f, 0, 0};
    const bool gapHit = PhysicalContactTriangleMeshCompoundIntersect(
        mesh, identity, sphere, target, 0).hit;
    // The authored fixture remains in model space. Exercise the same kernel
    // under nontrivial world transforms, including a swing whose endpoints
    // both miss. This is not an engine animation/visibility acceptance test.
    const auto pose = [](float angle, float scale) {
        PhysicalContactTransform result{};
        result.position = {13.25f, -4.5f, 2.75f};
        result.forward = {std::cos(angle), 0, std::sin(angle)};
        result.up = {-std::sin(angle), 0, std::cos(angle)};
        result.scale = scale;
        return result;
    };
    unsigned transformedHits = 0, transformedSamples = 0, transformedGapHits = 0;
    unsigned rotationHits = 0, rotationSamples = 0, rotationEndpointHits = 0;
    unsigned distal = 0;
    float distalX = -FLT_MAX;
    for (unsigned i = 0; i < count; ++i)
    {
        const auto& face = mesh.triangles[i];
        const auto centre = (face.vertices[0]+face.vertices[1]+face.vertices[2])*(1.f/3.f);
        if (centre.x > distalX) { distalX = centre.x; distal = i; }
    }
    const auto& distalFace = mesh.triangles[distal];
    const auto distalPoint = (distalFace.vertices[0]+distalFace.vertices[1]+
                             distalFace.vertices[2])*(1.f/3.f);
    for (float scale : {.5f, 1.f, 1.5f})
    {
        sphere.children[0].radius = .003f * scale;
        for (float angle : {-.7f, .3f, 1.2f})
        {
            const auto middle = pose(angle, scale);
            for (unsigned i = 0; i < count; ++i)
            {
                const auto& face = mesh.triangles[i];
                const auto centre = (face.vertices[0]+face.vertices[1]+face.vertices[2])*(1.f/3.f);
                target.position = PhysicalContactTransformPoint(middle, centre);
                transformedHits += PhysicalContactTriangleMeshCompoundIntersect(
                    mesh, middle, sphere, target, 0).hit ? 1 : 0;
                ++transformedSamples;
            }
            target.position = PhysicalContactTransformPoint(middle, {.25f, 0, 0});
            transformedGapHits += PhysicalContactTriangleMeshCompoundIntersect(
                mesh, middle, sphere, target, 0).hit ? 1 : 0;
            target.position = PhysicalContactTransformPoint(middle, distalPoint);
            const auto before = pose(angle - .2f, scale);
            const auto after = pose(angle + .2f, scale);
            rotationEndpointHits += PhysicalContactTriangleMeshCompoundIntersect(
                mesh, before, sphere, target, 0).hit ? 1 : 0;
            rotationEndpointHits += PhysicalContactTriangleMeshCompoundIntersect(
                mesh, after, sphere, target, 0).hit ? 1 : 0;
            rotationHits += PhysicalContactSweepTriangleMeshCompound(
                mesh, before, after, sphere, target, .001f * scale, 0).hit ? 1 : 0;
            ++rotationSamples;
        }
    }
    sphere.children[0].radius = .003f;
    PhysicalContactTransform inverseBind{};
    inverseBind.position.x = -.117739f; // Verified official sword fixture.
    const auto movingRoot = pose(.3f, 1.5f);
    auto movingBlade = pose(.8f, 1.5f);
    movingBlade.position = PhysicalContactTransformPoint(movingRoot, {.117739f,0,0});
    static PhysicalContactTriangleMesh animated{};
    if (!H3PrototypeBladeInRootFrame(mesh, inverseBind, movingBlade, movingRoot, animated)) return 7;
    unsigned animatedHits = 0;
    for (unsigned i = 0; i < count; ++i)
    {
        const auto& face = mesh.triangles[i];
        const auto centre = (face.vertices[0]+face.vertices[1]+face.vertices[2])*(1.f/3.f);
        target.position = PhysicalContactTransformPoint(movingBlade,
            PhysicalContactTransformPoint(inverseBind, centre));
        animatedHits += PhysicalContactTriangleMeshCompoundIntersect(
            animated, movingRoot, sphere, target, 0).hit ? 1 : 0;
    }
    target.position = PhysicalContactTransformPoint(movingBlade,
        PhysicalContactTransformPoint(inverseBind, {.25f,0,0}));
    const bool animatedGap = PhysicalContactTriangleMeshCompoundIntersect(
        animated, movingRoot, sphere, target, 0).hit;
    const auto& t = mesh.triangles[0];
    target.position = (t.vertices[0]+t.vertices[1]+t.vertices[2])*(1.f/3.f);
    auto previous = identity, current = identity;
    previous.position.z = -.03f;
    current.position.z = .03f;
    std::vector<double> micros;
    unsigned sweepHits = 0;
    for (unsigned i = 0; i < 600; ++i)
    {
        const auto start = std::chrono::steady_clock::now();
        const auto hit = PhysicalContactSweepTriangleMeshCompound(
            mesh, previous, current, sphere, target, .001f, 0);
        const auto end = std::chrono::steady_clock::now();
        if (i >= 100)
        {
            micros.push_back(std::chrono::duration<double,std::micro>(end-start).count());
            sweepHits += hit.hit ? 1 : 0;
        }
    }
    std::sort(micros.begin(), micros.end());
    static PhysicalContactCompoundShape bladeCompound{};
    static PhysicalContactTriangleMesh bladeMesh{};
    std::vector<double> constructionMicros;
    for (unsigned i = 0; i < 1100; ++i)
    {
        bladeCompound.childCount = bladeMesh.triangleCount = bladeMesh.groupCount = 0;
        const auto start = std::chrono::steady_clock::now();
        if (!Halo3AppendSwordBladeGeometry(inverseBind,movingBlade,movingRoot,
                bladeCompound,&bladeMesh)) return 8;
        const auto end = std::chrono::steady_clock::now();
        if (i >= 100) constructionMicros.push_back(
            std::chrono::duration<double,std::micro>(end-start).count());
    }
    std::sort(constructionMicros.begin(),constructionMicros.end());
    std::cout << "{\"triangles\":" << count << ",\"centroid_contacts\":" << centroidHits
              << ",\"gap_contact\":" << (gapHit ? "true" : "false")
              << ",\"transformed_contacts\":" << transformedHits
              << ",\"transformed_samples\":" << transformedSamples
              << ",\"transformed_gap_contacts\":" << transformedGapHits
              << ",\"rotation_contacts\":" << rotationHits
              << ",\"rotation_samples\":" << rotationSamples
              << ",\"rotation_endpoint_contacts\":" << rotationEndpointHits
              << ",\"animated_node_contacts\":" << animatedHits
              << ",\"animated_node_gap_contact\":" << (animatedGap ? "true" : "false")
              << ",\"sweep_contacts\":" << sweepHits << ",\"sweep_samples\":500"
              << ",\"sweep_p95_us\":" << micros[474]
              << ",\"sweep_max_us\":" << micros.back()
              << ",\"blade_children\":" << bladeCompound.childCount
              << ",\"construction_samples\":1000,\"construction_p95_us\":" << constructionMicros[949]
              << ",\"construction_max_us\":" << constructionMicros.back() << "}\n";
    return centroidHits == count && !gapHit && sweepHits == 500 &&
        transformedHits == transformedSamples && !transformedGapHits &&
        rotationHits == rotationSamples && !rotationEndpointHits &&
        animatedHits == count && !animatedGap ? 0 : 6;
}
