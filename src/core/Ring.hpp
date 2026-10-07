#pragma once
#include "Vec.hpp"

namespace cxo {

// An ellipse in space. Phase 0 sits on the major axis and phase advances toward the minor axis.
struct Ring {
	Vec3 center;
	Mat3 basis; // x: major axis, y: minor axis, z: normal
	double major = 1, minor = 1;

	Vec3 at(double phase) const {
		const double a = TAU * phase;
		return center + basis.x * (major * std::cos(a)) + basis.y * (minor * std::sin(a));
	}

	// Spin turns the ellipse within its plane, tilt lifts the plane about the line of nodes,
	// and azimuth swings that line around world Z. All zero lays the ring flat in XY.
	static Mat3 orient(double spin, double tilt, double azimuth) {
		return Mat3::rotZ(azimuth) * Mat3::rotX(tilt) * Mat3::rotZ(spin);
	}

	// Scaled about its own point at `phase`: every scale passes through that point with the same tangent,
	// so a family of them meets there and fans apart toward the opposite phase.
	Ring pinched(double phase, double scale) const {
		const Vec3 p = at(phase);
		return {p + (center - p) * scale, basis, major * scale, minor * scale};
	}
};

} // namespace cxo
