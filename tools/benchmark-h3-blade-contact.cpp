// Offline native contact-kernel benchmark using official authored blade data.
// Never linked into the mod and never accesses a game process.
#include <cfloat>
#include <chrono>
#include <fstream>
#include <iostream>
#include <vector>
#include "../src/common/physical_contact_logic.h"

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
    std::cout << "{\"triangles\":" << count << ",\"centroid_contacts\":" << centroidHits
              << ",\"gap_contact\":" << (gapHit ? "true" : "false")
              << ",\"sweep_contacts\":" << sweepHits << ",\"sweep_samples\":500"
              << ",\"sweep_p95_us\":" << micros[474]
              << ",\"sweep_max_us\":" << micros.back() << "}\n";
    return centroidHits == count && !gapHit && sweepHits == 500 ? 0 : 6;
}
