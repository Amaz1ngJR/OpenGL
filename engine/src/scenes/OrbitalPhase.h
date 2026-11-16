#pragma once
#include <algorithm>
#include <cmath>

namespace solarSystem {
// Wrap each body's own phase, never a shared angle before multiplying its rate.
// Keep double precision over long sessions; speed affects increments only.
struct OrbitalPhase {
    double degrees=0;
    void advance(double seconds,double degreesPerSecond) {
        degrees=std::fmod(degrees+std::max(0.0,seconds)*degreesPerSecond,360.0);
        if(degrees<0) degrees+=360.0;
    }
    float radians(double initialDegrees=0) const {
        return static_cast<float>((degrees+initialDegrees)*0.017453292519943295);
    }
};
} // namespace solarSystem
