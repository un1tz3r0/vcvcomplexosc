#pragma once
#include "Vec.hpp"

namespace cxo {

// Pinhole camera projecting to screen pixels with y pointing down.
struct Camera {
	Vec3 eye, forward, right, up;
	double focal;
	float width, height;

	Camera(Vec3 eye, Vec3 target, Vec3 sky, float width, float height, double fovY = 0.75)
		: eye(eye), forward(normalize(target - eye)), right(normalize(cross(forward, sky))), up(cross(right, forward)),
		  focal(0.5 * height / std::tan(0.5 * fovY)), width(width), height(height) {}

	// Looks at `target` from `distance` away, with azimuth and elevation measured in `frame`, whose Z is the sky.
	// Elevation must stay short of ±90°.
	static Camera orbit(Vec3 target, double distance, double azimuth, double elevation, float width, float height, const Mat3& frame = {}) {
		const double ce = std::cos(elevation);
		const Vec3 dir = frame * Vec3{ce * std::cos(azimuth), ce * std::sin(azimuth), std::sin(elevation)};
		return {target + dir * distance, target, frame.z, width, height};
	}

	Vec2f project(Vec3 p) const {
		const Vec3 d = p - eye;
		const double s = focal / dot(d, forward);
		return {float(0.5 * width + s * dot(d, right)), float(0.5 * height - s * dot(d, up))};
	}

	// Orientation only, for gizmos: a unit world direction as a screen-space vector of length ≤ 1.
	Vec2f projectDir(Vec3 d) const { return {float(dot(d, right)), float(-dot(d, up))}; }
};

} // namespace cxo
