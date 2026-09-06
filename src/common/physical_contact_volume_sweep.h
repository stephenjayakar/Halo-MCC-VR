#pragma once
#include "physical_contact_volume_logic.h"

struct PhysicalContactVolumePath
{
    PhysicalContactTransform from{}, to{};
    PhysicalContactVec3 axis{1,0,0};
    float angle = 0;
};

inline bool PhysicalContactVolumeRigid(const PhysicalContactTransform& t)
{
    return PhysicalContactTransformFinite(t) &&
        std::abs(PhysicalContactDot(t.forward,t.forward)-1.f) < 1.e-4f &&
        std::abs(PhysicalContactDot(t.left,t.left)-1.f) < 1.e-4f &&
        std::abs(PhysicalContactDot(t.up,t.up)-1.f) < 1.e-4f &&
        std::abs(PhysicalContactDot(t.forward,t.left)) < 1.e-4f &&
        std::abs(PhysicalContactDot(t.forward,t.up)) < 1.e-4f &&
        std::abs(PhysicalContactDot(t.left,t.up)) < 1.e-4f &&
        PhysicalContactDot(PhysicalContactCross(t.forward,t.left),t.up) > .9999f;
}

inline bool PhysicalContactBuildVolumePath(const PhysicalContactTransform& from,
    const PhysicalContactTransform& to, PhysicalContactVolumePath& path)
{
    if (!PhysicalContactVolumeRigid(from) || !PhysicalContactVolumeRigid(to) ||
        from.scale != to.scale) return false;
    // Relative world rotation R_to * transpose(R_from). Extract a unit
    // quaternion using the largest diagonal branch, including exact half-turns.
    const double a[3][3]{{from.forward.x,from.left.x,from.up.x},
        {from.forward.y,from.left.y,from.up.y},{from.forward.z,from.left.z,from.up.z}};
    const double b[3][3]{{to.forward.x,to.left.x,to.up.x},
        {to.forward.y,to.left.y,to.up.y},{to.forward.z,to.left.z,to.up.z}};
    double m[3][3]{};
    for (int i=0;i<3;++i) for (int j=0;j<3;++j)
        for (int k=0;k<3;++k) m[i][j] += b[i][k]*a[j][k];
    double q[4]{};
    const double trace = m[0][0]+m[1][1]+m[2][2];
    if (trace > 0)
    {
        const double s = 2*std::sqrt(trace+1);
        q[0]=(m[2][1]-m[1][2])/s; q[1]=(m[0][2]-m[2][0])/s;
        q[2]=(m[1][0]-m[0][1])/s; q[3]=s*.25;
    }
    else
    {
        int i = m[1][1] > m[0][0] ? 1 : 0;
        if (m[2][2] > m[i][i]) i=2;
        const int j=(i+1)%3, k=(i+2)%3;
        const double s=2*std::sqrt(std::max(0.0,1+m[i][i]-m[j][j]-m[k][k]));
        if (s < 1.e-12) return false;
        q[i]=s*.25; q[j]=(m[i][j]+m[j][i])/s;
        q[k]=(m[i][k]+m[k][i])/s; q[3]=(m[k][j]-m[j][k])/s;
    }
    const double length = std::sqrt(q[0]*q[0]+q[1]*q[1]+q[2]*q[2]+q[3]*q[3]);
    if (!std::isfinite(length) || length < .5) return false;
    const double sign = q[3] < 0 ? -1.0 : 1.0;
    for (auto& value : q) value *= sign/length;
    const double sine = std::sqrt(q[0]*q[0]+q[1]*q[1]+q[2]*q[2]);
    path = {from,to};
    path.angle = static_cast<float>(2*std::atan2(sine,q[3]));
    if (sine > 1.e-12)
        path.axis = {static_cast<float>(q[0]/sine),static_cast<float>(q[1]/sine),static_cast<float>(q[2]/sine)};
    return std::isfinite(path.angle) && path.angle <= 3.141593f;
}

inline PhysicalContactTransform PhysicalContactVolumePathAt(const PhysicalContactVolumePath& path, float t)
{
    if (t <= 0) return path.from;
    if (t >= 1) return path.to;
    auto result = path.from;
    result.position = path.from.position + (path.to.position-path.from.position)*t;
    result.forward = PhysicalContactRotateAxisAngle(path.from.forward,path.axis,path.angle*t);
    result.left = PhysicalContactRotateAxisAngle(path.from.left,path.axis,path.angle*t);
    result.up = PhysicalContactRotateAxisAngle(path.from.up,path.axis,path.angle*t);
    return result;
}

struct PhysicalContactVolumeCast
{
    bool valid = false, hit = false;
    float fraction = 1;
    PhysicalContactVec3 normal{};
};

struct PhysicalContactVolumeSweep
{
    bool valid = false, blocked = false, exhausted = false;
    PhysicalContactTransform pose{};
    float progress = 0;
    uint32_t queries = 0;
    PhysicalContactVolumeCast contact{};
};

// The caller MUST establish that the entire starting cover (including the
// fixed skin) is clear in the current static world. A forward-only native cast
// cannot establish this. No hit proves a segment only from such a start.
//
// Cast accepts (center, displacement, radius) and returns the first hit.
// Every accepted pose is one rigid transform shared by all spheres. Rotation
// uses exact shortest-arc interpolation, with its chord error inside the skin.
// A fractional rotated stop is re-cast; the original chord's fraction alone
// cannot prove clearance of the different endpoint on the rotation arc.
template<class Cast>
inline PhysicalContactVolumeSweep PhysicalContactSweepVolume(
    const PhysicalContactVolumeCover& cover, const PhysicalContactTransform& from,
    const PhysicalContactTransform& to, float skin, uint32_t maximumQueries, Cast&& cast)
{
    PhysicalContactVolumeSweep result{};
    result.pose = from;
    PhysicalContactVolumePath path{};
    if (!cover.count || cover.count > cover.kMaximumSpheres ||
        !std::isfinite(skin) || skin <= 0 || !maximumQueries || maximumQueries > 4096 ||
        !PhysicalContactBuildVolumePath(from,to,path)) return result;
    float lever = 0;
    for (uint16_t i=0;i<cover.count;++i)
    {
        const auto& sphere=cover.spheres[i];
        if (!PhysicalContactFinite(sphere.center) || !std::isfinite(sphere.radius) ||
            sphere.radius <= 0 || sphere.radius > 100) return result;
        lever = std::max(lever,PhysicalContactLength(sphere.center)*from.scale);
    }
    const uint32_t steps = PhysicalContactVolumeAngularSteps(lever,path.angle,skin);
    result.valid = true;
    if (!steps) { result.exhausted = true; return result; }
    for (uint32_t step=1;step<=steps;++step)
    {
        float desired = static_cast<float>(step)/steps;
        // Up to four refinements, each from the same last-proven clear pose.
        // On failure/budget exhaustion we keep that pose, never an untested TOI.
        for (uint32_t attempt=0;attempt<5;++attempt)
        {
            const auto target = PhysicalContactVolumePathAt(path,desired);
            PhysicalContactVolumeCast earliest{true,false,1,{}};
            float maximumTravel = 0;
            for (uint16_t i=0;i<cover.count;++i)
            {
                if (result.queries >= maximumQueries)
                { result.exhausted=true; return result; }
                const auto& sphere=cover.spheres[i];
                const auto start=PhysicalContactTransformPoint(result.pose,sphere.center);
                const auto end=PhysicalContactTransformPoint(target,sphere.center);
                const float radius=sphere.radius*from.scale+skin;
                if (!PhysicalContactFinite(start) || !PhysicalContactFinite(end) || !std::isfinite(radius))
                { result.valid=false; return result; }
                const auto delta=end-start;
                maximumTravel=std::max(maximumTravel,PhysicalContactLength(delta));
                ++result.queries;
                const auto hit=cast(start,delta,radius);
                if (!hit.valid || !std::isfinite(hit.fraction) || hit.fraction < 0 || hit.fraction > 1 ||
                    (hit.hit && (!PhysicalContactFinite(hit.normal) ||
                     std::abs(PhysicalContactLengthSquared(hit.normal)-1) > .002f)))
                { result.valid=false; return result; }
                if (hit.hit && (!earliest.hit || hit.fraction < earliest.fraction)) earliest=hit;
            }
            if (!earliest.hit)
            {
                result.pose=target; result.progress=desired;
                if (result.blocked) return result;
                break;
            }
            result.blocked=true; result.contact=earliest;
            // Back off by a fraction of the query skin. Recasting, rather than
            // the backoff itself, is what proves the resulting endpoint clear.
            const float retreat = maximumTravel > 0 ? std::min(.1f,skin*.1f/maximumTravel) : 1;
            const float fraction = std::max(0.f,earliest.fraction-retreat);
            desired=result.progress+(desired-result.progress)*fraction;
            if (desired-result.progress <= 1.e-6f) return result;
            if (attempt==4) { result.exhausted=true; return result; }
        }
    }
    return result;
}
