#pragma once
#include "Vec.hpp"

namespace cxo {

// An ellipse in space. Phase 0 sits on the major axis and phase advances toward the minor axis.
struct Ring {
	// What a ring can turn about: one of its own axes, or a world axis through its center.
	enum Axis { NORMAL, MAJOR, MINOR, WORLD_X, WORLD_Y, WORLD_Z, AXES };
	static constexpr const char* AXIS_NAMES[AXES] = {"NORMAL", "MAJOR", "MINOR", "X", "Y", "Z"};

	Vec3 center;
	Mat3 basis; // x: major axis, y: minor axis, z: normal
	double major = 1, minor = 1;

	Vec3 at(double phase) const {
		const double a = TAU * phase;
		return center + basis.x * (major * std::cos(a)) + basis.y * (minor * std::sin(a));
	}

	// Tilt lifts the plane about the line of nodes, and azimuth swings that line around world Z.
	// Both zero lays the ring flat in XY with its major axis along X.
	static Mat3 orient(double tilt, double azimuth) { return Mat3::rotZ(azimuth) * Mat3::rotX(tilt); }

	Vec3 axis(Axis a) const {
		const Vec3 axes[AXES] = {basis.z, basis.x, basis.y, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
		return axes[a];
	}

	// Turned by `angle` radians about a unit axis through its center.
	Ring turned(Vec3 axis, double angle) const { return {center, Mat3::rotate(axis, angle) * basis, major, minor}; }
	Ring shifted(Vec3 by) const { return {center + by, basis, major, minor}; }
	Ring scaled(double scale) const { return {center, basis, major * scale, minor * scale}; }

	// Scaled about its own point at `phase`: every scale passes through that point with the same tangent,
	// so a family of them meets there and fans apart toward the opposite phase.
	Ring pinched(double phase, double scale) const {
		const Vec3 p = at(phase);
		return {p + (center - p) * scale, basis, major * scale, minor * scale};
	}

	// Turned about its diameter through `phase`: a family of them shares that diameter's ends
	// and fans apart a quarter cycle on either side.
	Ring hinged(double phase, double angle) const {
		const Vec3 d = at(phase) - center;
		return turned(length(d) > 1e-9 ? normalize(d) : basis.x, angle);
	}
};

} // namespace cxo
