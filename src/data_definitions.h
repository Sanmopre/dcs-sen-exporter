#pragma once

// sen
#include <sen/core/base/numbers.h>

// std
#include <string>
#include<vector>

struct SpatialData
{
    f64 latitude;
    f64 longitude;
    f64 altitude;
    f64 yaw;
    f64 pitch;
    f64 roll;
};

struct PlatformData
{
    std::string name;
    u64 id;
    SpatialData spatial;
};

struct FrameData
{
    u64 frameNumber;
    f64 time;
    std::vector<PlatformData> platforms;
};

using Recording = std::vector<FrameData>;