#pragma once
#include <algorithm>
#include <d3d9types.h>
#include <mutex>
#include <numbers>
#include <unordered_map>
#include <vector>
#include <windows.h>
#include <src/memory/driver.h>
#include "offsets.h"

namespace numerics {

	inline int   screen_width         = GetSystemMetrics(0);
	inline int   screen_height        = GetSystemMetrics(1);
	inline int   screen_width_center  = screen_width  / 2;
	inline int   screen_height_center = screen_height / 2;
	inline float aspect_ratio         = (float)screen_width / screen_height;

	class fvector {
	public:
		double x = 0.0, y = 0.0, z = 0.0;

		fvector() = default;
		fvector(double a, double b, double c) : x(a), y(b), z(c) {}

		fvector operator+(const fvector& v) const { return { x + v.x, y + v.y, z + v.z }; }
		fvector operator-(const fvector& v) const { return { x - v.x, y - v.y, z - v.z }; }
		fvector operator*(double s)          const { return { x * s, y * s, z * s }; }
		fvector operator/(double s)          const { return { x / s, y / s, z / s }; }

		double dot     (const fvector& v) const { return x * v.x + y * v.y + z * v.z; }
		double length  ()                 const { return std::sqrt(x * x + y * y + z * z); }
		double distance(const fvector& v) const { return (*this - v).length(); }
	};

	class fvector2d {
	public:
		double x = 0.0, y = 0.0;

		fvector2d() = default;
		fvector2d(double a, double b) : x(a), y(b) {}
	};

	class frotator {
	public:
		double pitch = 0.0, yaw = 0.0, roll = 0.0;

		frotator() = default;
		frotator(double p, double y, double r) : pitch(p), yaw(y), roll(r) {}
	};

	struct fplane : fvector { double w = 0.0; };

	struct alignas(16) matrix_elements {
		double m11, m12, m13, m14;
		double m21, m22, m23, m24;
		double m31, m32, m33, m34;
		double m41, m42, m43, m44;
		matrix_elements()
			: m11(0), m12(0), m13(0), m14(0),
			  m21(0), m22(0), m23(0), m24(0),
			  m31(0), m32(0), m33(0), m34(0),
			  m41(0), m42(0), m43(0), m44(0) {}
	};

	struct alignas(16) dbl_matrix {
		union {
			matrix_elements elements;
			double m[4][4];
		};
		dbl_matrix() : elements() {}
	};

	struct alignas(16) fmatrix : public dbl_matrix {
		fplane x_plane;
		fplane y_plane;
		fplane z_plane;
		fplane w_plane;
		fmatrix() : dbl_matrix(), x_plane(), y_plane(), z_plane(), w_plane() {}
	};

	template<class T>
	class tarray {
	public:
		T*       data      = nullptr;
		uint32_t count     = 0;
		uint32_t max_count = 0;

		tarray() = default;

		T get(uintptr_t idx) {
			return read<T>((uintptr_t)data + idx * sizeof(T));
		}

		std::vector<T> get_all() {
			if (count <= 0 || count > max_count) return {};
			try {
				std::vector<T> buf(count);
				read2((uintptr_t)data, buf.data(), sizeof(T) * count);
				return buf;
			} catch (const std::bad_alloc&) {
				return {};
			}
		}
	};

}

namespace matrix_ops {

	inline D3DMATRIX multiply(const D3DMATRIX& a, const D3DMATRIX& b) {
		D3DMATRIX r{};
		r._11 = a._11*b._11 + a._12*b._21 + a._13*b._31 + a._14*b._41;
		r._12 = a._11*b._12 + a._12*b._22 + a._13*b._32 + a._14*b._42;
		r._13 = a._11*b._13 + a._12*b._23 + a._13*b._33 + a._14*b._43;
		r._14 = a._11*b._14 + a._12*b._24 + a._13*b._34 + a._14*b._44;

		r._21 = a._21*b._11 + a._22*b._21 + a._23*b._31 + a._24*b._41;
		r._22 = a._21*b._12 + a._22*b._22 + a._23*b._32 + a._24*b._42;
		r._23 = a._21*b._13 + a._22*b._23 + a._23*b._33 + a._24*b._43;
		r._24 = a._21*b._14 + a._22*b._24 + a._23*b._34 + a._24*b._44;

		r._31 = a._31*b._11 + a._32*b._21 + a._33*b._31 + a._34*b._41;
		r._32 = a._31*b._12 + a._32*b._22 + a._33*b._32 + a._34*b._42;
		r._33 = a._31*b._13 + a._32*b._23 + a._33*b._33 + a._34*b._43;
		r._34 = a._31*b._14 + a._32*b._24 + a._33*b._34 + a._34*b._44;

		r._41 = a._41*b._11 + a._42*b._21 + a._43*b._31 + a._44*b._41;
		r._42 = a._41*b._12 + a._42*b._22 + a._43*b._32 + a._44*b._42;
		r._43 = a._41*b._13 + a._42*b._23 + a._43*b._33 + a._44*b._43;
		r._44 = a._41*b._14 + a._42*b._24 + a._43*b._34 + a._44*b._44;
		return r;
	}

	inline D3DMATRIX rotation_matrix(const numerics::frotator& rot) {
		float rx = (float)(rot.pitch * std::numbers::pi / 180.0);
		float ry = (float)(rot.yaw   * std::numbers::pi / 180.0);
		float rz = (float)(rot.roll  * std::numbers::pi / 180.0);

		float sp = sinf(rx), cp = cosf(rx);
		float sy = sinf(ry), cy = cosf(ry);
		float sr = sinf(rz), cr = cosf(rz);

		D3DMATRIX m{};
		m.m[0][0] = cp * cy;
		m.m[0][1] = cp * sy;
		m.m[0][2] = sp;
		m.m[0][3] = 0.f;

		m.m[1][0] = sr * sp * cy - cr * sy;
		m.m[1][1] = sr * sp * sy + cr * cy;
		m.m[1][2] = -sr * cp;
		m.m[1][3] = 0.f;

		m.m[2][0] = -(cr * sp * cy + sr * sy);
		m.m[2][1] = cy * sr - cr * sp * sy;
		m.m[2][2] = cr * cp;
		m.m[2][3] = 0.f;

		m.m[3][0] = m.m[3][1] = m.m[3][2] = 0.0f;
		m.m[3][3] = 1.0f;
		return m;
	}

	struct ftransform {
		numerics::fplane  rot;
		numerics::fvector translation;
		uint8_t           pad_38[8];
		numerics::fvector scale = { 1.0, 1.0, 1.0 };
		uint8_t           pad_58[8];

		D3DMATRIX to_matrix_with_scale() const {
			D3DMATRIX m{};
			const numerics::fvector adjusted_scale{
				scale.x == 0.0 ? 1.0 : scale.x,
				scale.y == 0.0 ? 1.0 : scale.y,
				scale.z == 0.0 ? 1.0 : scale.z
			};
			m._41 = (float)translation.x;
			m._42 = (float)translation.y;
			m._43 = (float)translation.z;

			auto x2 = rot.x + rot.x, y2 = rot.y + rot.y, z2 = rot.z + rot.z;
			auto xx = rot.x * x2, yy = rot.y * y2, zz = rot.z * z2;
			m._11 = (float)((1.0f - (yy + zz)) * adjusted_scale.x);
			m._22 = (float)((1.0f - (xx + zz)) * adjusted_scale.y);
			m._33 = (float)((1.0f - (xx + yy)) * adjusted_scale.z);

			auto yz = rot.y * z2, wx = rot.w * x2;
			m._32 = (float)((yz - wx) * adjusted_scale.z);
			m._23 = (float)((yz + wx) * adjusted_scale.y);

			auto xy = rot.x * y2, wz = rot.w * z2;
			m._21 = (float)((xy - wz) * adjusted_scale.y);
			m._12 = (float)((xy + wz) * adjusted_scale.x);

			auto xz = rot.x * z2, wy = rot.w * y2;
			m._31 = (float)((xz + wy) * adjusted_scale.z);
			m._13 = (float)((xz - wy) * adjusted_scale.x);

			m._14 = m._24 = m._34 = 0.0f;
			m._44 = 1.0f;
			return m;
		}
	};
	static_assert(sizeof(ftransform) == 0x60, "FTransform layout must match the game");

}

namespace bones {

	struct snapshot {
		uintptr_t              bone_array = 0;
		matrix_ops::ftransform c2w{};
		bool                   valid = false;
	};

	inline snapshot get_snapshot(uintptr_t mesh) {
		snapshot snap{};
		if (!mesh) return snap;

		const int32_t active_transforms =
			read<int32_t>(mesh + BONE_ARRAY + 0x48) * 0x10;
		snap.bone_array = read<uintptr_t>(mesh + BONE_ARRAY +
			static_cast<uintptr_t>(active_transforms));
		if (!snap.bone_array) {
			snap.bone_array = read<uintptr_t>(mesh +
				BONE_ARRAY + 0x10);
		}
		if (!snap.bone_array) return snap;

		snap.c2w = read<matrix_ops::ftransform>(mesh + COMPONENT_TO_WORLD);
		snap.valid = true;
		return snap;
	}

	inline numerics::fvector bone_from_snapshot(const snapshot& s, int bone_id) {
		if (!s.valid || !s.bone_array || bone_id < 0)
			return { 0.0, 0.0, 0.0 };
		const auto b = read<matrix_ops::ftransform>(s.bone_array +
			(static_cast<uintptr_t>(bone_id) * 0x60));
		auto mat = matrix_ops::multiply(b.to_matrix_with_scale(), s.c2w.to_matrix_with_scale());
		return { mat._41, mat._42, mat._43 };
	}

}

namespace udata {

	struct world {
		uintptr_t gworld;
		uintptr_t owning_game_instance;
		uintptr_t local_player;
		uintptr_t player_controller;
		uintptr_t player_state;
		uintptr_t local_pawn;
		int       team_id;
		uintptr_t game_state;
		std::vector<u64> player_array;
	};

	struct actor {
		uintptr_t player_state;
		uintptr_t pawn_private;
		uintptr_t mesh;
		int       team_id;
		int       local_team;
		numerics::fvector location;
	};

	inline uintptr_t closest_mesh;
	inline float closest_distance;

	inline world world_t;
	inline std::mutex world_mutex;
	inline std::vector<actor> actor_t;
	inline std::mutex actor_mutex;
}

struct FNRot
{
	double a;
	char pad_0008[24];
	double b;
	char pad_0028[424];
	double c;
};

namespace camera {
	struct view_point {
		numerics::fvector  location{};
		numerics::frotator rotation{};
		float               fov = 0.0f;
	};

	inline numerics::fvector location{};
	inline numerics::frotator rotation{};
	inline float             fov = 0.0f;

	inline view_point get_view_point() {
		view_point result{};
		uintptr_t gworld = 0;
		uintptr_t player_controller = 0;
		{
			std::lock_guard<std::mutex> lock(udata::world_mutex);
			gworld = udata::world_t.gworld;
			player_controller = udata::world_t.player_controller;
		}

		FNRot fnrot{};
		if (gworld && CAMROT) {
			const uintptr_t rotation_pointer =
				read<uintptr_t>(gworld + CAMROT);
			if (rotation_pointer)
				read_struct(rotation_pointer, fnrot);
		}

		if (gworld && CAMPOS) {
			const uintptr_t location_pointer =
				read<uintptr_t>(gworld + CAMPOS);
			if (location_pointer)
				read_struct(location_pointer, result.location);
		}

		constexpr double radians_to_degrees = 180.0 / std::numbers::pi;
		result.rotation.pitch = std::asin(std::clamp(fnrot.c, -1.0, 1.0)) * radians_to_degrees;
		result.rotation.yaw   = std::atan2(-fnrot.a, fnrot.b) * radians_to_degrees;
		result.rotation.roll  = 0.0;

		if (player_controller && FOV) {
			result.fov = read<float>(player_controller + FOV) * 90.0f;
		}

		return result;
	}

	inline void update() {
		const view_point current = get_view_point();
		location = current.location;
		rotation = current.rotation;
		fov      = current.fov;
	}

}

namespace w2s {

	inline D3DMATRIX rotation_matrix;
	inline float     inv_fov = 0.0f;

	inline numerics::fvector2d project(const numerics::fvector& loc) {
		auto& ax = rotation_matrix.m[0];
		auto& ay = rotation_matrix.m[1];
		auto& az = rotation_matrix.m[2];

		auto d = loc - camera::location;

		auto tx = d.dot(numerics::fvector(ay[0], ay[1], ay[2]));
		auto ty = d.dot(numerics::fvector(az[0], az[1], az[2]));
		auto tz = max(d.dot(numerics::fvector(ax[0], ax[1], ax[2])), 1.0);

		double sx = numerics::screen_width_center  + tx * (numerics::screen_height_center / inv_fov) / tz;
		double sy = numerics::screen_height_center - ty * (numerics::screen_height_center / inv_fov) / tz;
		return numerics::fvector2d(sx, sy);
	}

	inline bool on_screen(const numerics::fvector2d& p) {
		return p.x >= 0 && p.x <= numerics::screen_width &&
		       p.y >= 0 && p.y <= numerics::screen_height;
	}

}

inline double getCrossDistance(double x1, double y1, double x2, double y2)
{
	return std::hypot(x2 - x1, y2 - y1);
}
