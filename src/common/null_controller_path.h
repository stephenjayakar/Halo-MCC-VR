#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

inline constexpr uintptr_t kNullControllerPoseMessage = 0x48335053u; // H3PS
struct NullControllerPoseCommand
{
    uint32_t version = 1;
    uint32_t durationMs = 1000;
    float position[3]{.18f, -.18f, -.65f};
    float orientation[4]{0, 0, 0, 1};
};
static_assert(sizeof(NullControllerPoseCommand) == 36);
struct NullControllerPose
{
    float position[3]{.18f, -.18f, -.65f};
    float orientation[4]{0, 0, 0, 1};
};
struct NullControllerPath
{
    uint64_t startUs = 0;
    uint32_t durationMs = 0, generation = 0;
    NullControllerPose from{}, to{};
};
struct NullControllerPathSample
{
    NullControllerPose pose{};
    float linearVelocity[3]{}, angularVelocity[3]{};
    bool moving = false;
};
inline std::array<float, 4> NullControllerMultiply(
    const std::array<float, 4>& a, const std::array<float, 4>& b)
{
    return {a[3]*b[0]+a[0]*b[3]+a[1]*b[2]-a[2]*b[1],
        a[3]*b[1]-a[0]*b[2]+a[1]*b[3]+a[2]*b[0],
        a[3]*b[2]+a[0]*b[1]-a[1]*b[0]+a[2]*b[3],
        a[3]*b[3]-a[0]*b[0]-a[1]*b[1]-a[2]*b[2]};
}
inline std::array<float, 4> NullControllerDelta(
    const NullControllerPose& from, const NullControllerPose& to)
{
    float dot = 0;
    for (int i=0; i<4; ++i) dot += from.orientation[i]*to.orientation[i];
    const float sign = dot < 0 ? -1.0f : 1.0f;
    return NullControllerMultiply(
        {to.orientation[0]*sign, to.orientation[1]*sign,
         to.orientation[2]*sign, to.orientation[3]*sign},
        {-from.orientation[0], -from.orientation[1], -from.orientation[2], from.orientation[3]});
}
inline bool NullControllerMakeTarget(const NullControllerPoseCommand& command,
    const NullControllerPose& from, NullControllerPose& target)
{
    if (command.version != 1 || command.durationMs < 200 || command.durationMs > 5000)
        return false;
    constexpr float lower[3]{-1.5f, -1.8f, -1.6f}, upper[3]{1.5f, .8f, .4f};
    float distance2 = 0, norm2 = 0;
    for (int i=0; i<3; ++i)
    {
        const float p=command.position[i];
        if (!std::isfinite(p) || !std::isfinite(from.position[i]) || p<lower[i] || p>upper[i]) return false;
        target.position[i]=p;
        distance2 += (p-from.position[i])*(p-from.position[i]);
    }
    float fromNorm2=0;
    for (int i=0; i<4; ++i)
    {
        if (!std::isfinite(command.orientation[i]) || !std::isfinite(from.orientation[i])) return false;
        norm2 += command.orientation[i]*command.orientation[i];
        fromNorm2 += from.orientation[i]*from.orientation[i];
    }
    if (norm2 < .25f || norm2 > 4.0f || std::abs(fromNorm2-1.0f)>.001f) return false;
    for (int i=0; i<4; ++i) target.orientation[i]=command.orientation[i]/std::sqrt(norm2);
    const auto delta=NullControllerDelta(from,target);
    const float angle=2.0f*std::atan2(std::sqrt(delta[0]*delta[0]+delta[1]*delta[1]+delta[2]*delta[2]),
                                    std::clamp(delta[3], 0.0f, 1.0f));
    const float peakRate=1875.0f/static_cast<float>(command.durationMs);
    return std::sqrt(distance2)*peakRate <= 5.0f && angle*peakRate <= 12.0f;
}
inline NullControllerPathSample NullControllerSamplePath(const NullControllerPath& path, uint64_t nowUs)
{
    NullControllerPathSample out{};
    if (!path.durationMs || nowUs >= path.startUs + uint64_t{path.durationMs}*1000)
    { out.pose=path.to; return out; }
    if (nowUs <= path.startUs) { out.pose=path.from; return out; }
    const double u=static_cast<double>(nowUs-path.startUs)/(path.durationMs*1000.0);
    // Minimum-jerk interpolation: zero velocity and acceleration at either
    // endpoint, with analytic velocity used by the normal melee/contact path.
    const float s=static_cast<float>(u*u*u*(10.0+u*(-15.0+6.0*u)));
    const float rate=static_cast<float>(30.0*u*u*(1-u)*(1-u)*1000.0/path.durationMs);
    out.moving=true;
    for (int i=0; i<3; ++i)
    {
        const float d=path.to.position[i]-path.from.position[i];
        out.pose.position[i]=path.from.position[i]+s*d;
        out.linearVelocity[i]=rate*d;
    }
    const auto delta=NullControllerDelta(path.from,path.to);
    const float vectorLength=std::sqrt(delta[0]*delta[0]+delta[1]*delta[1]+delta[2]*delta[2]);
    if (vectorLength < 1.0e-7f)
    { for (int i=0; i<4; ++i) out.pose.orientation[i]=path.from.orientation[i]; return out; }
    const float angle=2.0f*std::atan2(vectorLength,std::clamp(delta[3],0.0f,1.0f));
    const float half=angle*s*.5f, factor=std::sin(half)/vectorLength;
    const auto q=NullControllerMultiply(
        {delta[0]*factor,delta[1]*factor,delta[2]*factor,std::cos(half)},
        {path.from.orientation[0],path.from.orientation[1],path.from.orientation[2],path.from.orientation[3]});
    for (int i=0; i<4; ++i) out.pose.orientation[i]=q[i];
    for (int i=0; i<3; ++i) out.angularVelocity[i]=delta[i]/vectorLength*angle*rate;
    return out;
}
