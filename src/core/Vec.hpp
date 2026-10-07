#pragma once
#include <cmath>

namespace cxo {

constexpr double PI = 3.14159265358979323846;
constexpr double TAU = 2 * PI;

struct Vec2f {
	float x = 0, y = 0;
};

struct Vec3 {
	double x = 0, y = 0, z = 0;

	Vec3 operator+(Vec3 b) const { return {x + b.x, y + b.y, z + b.z}; }
	Vec3 operator-(Vec3 b) const { return {x - b.x, y - b.y, z - b.z}; }
	Vec3 operator-() const { return {-x, -y, -z}; }
	Vec3 operator*(double k) const { return {x * k, y * k, z * k}; }
};

inline double dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(Vec3 a, Vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline double length(Vec3 v) { return std::sqrt(dot(v, v)); }
inline Vec3 normalize(Vec3 v) { return v * (1 / length(v)); }

// Rotation stored as the images of the X, Y and Z axes.
struct Mat3 {
	Vec3 x{1, 0, 0}, y{0, 1, 0}, z{0, 0, 1};

	Vec3 operator*(Vec3 v) const { return x * v.x + y * v.y + z * v.z; }
	Mat3 operator*(const Mat3& m) const { return {*this * m.x, *this * m.y, *this * m.z}; }

	static Mat3 rotX(double a) {
		const double c = std::cos(a), s = std::sin(a);
		return {{1, 0, 0}, {0, c, s}, {0, -s, c}};
	}
	static Mat3 rotZ(double a) {
		const double c = std::cos(a), s = std::sin(a);
		return {{c, s, 0}, {-s, c, 0}, {0, 0, 1}};
	}
	// About the unit vector `u`, right-handed.
	static Mat3 rotate(Vec3 u, double a) {
		const double c = std::cos(a), s = std::sin(a);
		const auto image = [&](Vec3 v) { return v * c + cross(u, v) * s + u * (dot(u, v) * (1 - c)); };
		return {image({1, 0, 0}), image({0, 1, 0}), image({0, 0, 1})};
	}
};

} // namespace cxo
