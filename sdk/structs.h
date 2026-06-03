#pragma once

#include <cmath>
#include <cstdint>

// ============================================================================
// Vector3
// ============================================================================
struct Vector3 {
  float x, y, z;

  Vector3() : x(0.f), y(0.f), z(0.f) {}
  Vector3(float x, float y, float z) : x(x), y(y), z(z) {}

  Vector3 operator+(const Vector3 &v) const {
    return {x + v.x, y + v.y, z + v.z};
  }
  Vector3 operator-(const Vector3 &v) const {
    return {x - v.x, y - v.y, z - v.z};
  }
  Vector3 operator*(float s) const { return {x * s, y * s, z * s}; }

  float Length() const { return std::sqrtf(x * x + y * y + z * z); }
  float Length2D() const { return std::sqrtf(x * x + y * y); }
  float Dot(const Vector3 &v) const { return x * v.x + y * v.y + z * v.z; }

  Vector3 Normalized() const {
    float len = Length();
    if (len == 0.f)
      return {};
    return {x / len, y / len, z / len};
  }
};

// ============================================================================
// View Matrix (column-major: matrix[col][row])
// ============================================================================
using view_matrix_t = float[4][4];

// ============================================================================
// WorldToScreen
// ============================================================================
inline bool WorldToScreen(const Vector3 &world, const view_matrix_t &matrix,
                          float screenW, float screenH, Vector3 &screenOut) {
  float w = matrix[3][0] * world.x + matrix[3][1] * world.y +
            matrix[3][2] * world.z + matrix[3][3];

  if (w < 0.001f)
    return false;

  float invW = 1.0f / w;
  float nx = matrix[0][0] * world.x + matrix[0][1] * world.y +
             matrix[0][2] * world.z + matrix[0][3];
  float ny = matrix[1][0] * world.x + matrix[1][1] * world.y +
             matrix[1][2] * world.z + matrix[1][3];

  screenOut.x = (screenW * 0.5f) + (nx * invW) * (screenW * 0.5f);
  screenOut.y = (screenH * 0.5f) - (ny * invW) * (screenH * 0.5f);
  screenOut.z = 0.f;
  return true;
}

// ============================================================================
// CS2 Color Struct
// ============================================================================
struct Color {
  uint8_t r, g, b, a;
  Color() : r(0), g(0), b(0), a(255) {}
  Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) : r(r), g(g), b(b), a(a) {}
};

// ============================================================================
// Angle Helpers
// ============================================================================

// Normalize yaw to [-180, 180]
inline float NormalizeYaw(float yaw) {
  while (yaw > 180.0f)
    yaw -= 360.0f;
  while (yaw < -180.0f)
    yaw += 360.0f;
  return yaw;
}

// Normalize pitch to [-89, 89] — this is a CLAMP for final angles only
inline float ClampPitch(float pitch) {
  if (pitch > 89.0f)
    pitch = 89.0f;
  if (pitch < -89.0f)
    pitch = -89.0f;
  return pitch;
}

// Normalize a pitch DELTA (wraps, does NOT clamp)
inline float NormalizePitch(float pitch) {
  while (pitch > 180.0f)
    pitch -= 360.0f;
  while (pitch < -180.0f)
    pitch += 360.0f;
  return pitch;
}

inline Vector3 ClampAngles(Vector3 angles) {
  angles.x = ClampPitch(angles.x);
  angles.y = NormalizeYaw(angles.y);
  angles.z = 0.f;
  return angles;
}

// Calculate the angle FROM src looking TO dst (Source 2 convention)
inline Vector3 CalcAngle(const Vector3 &src, const Vector3 &dst) {
  Vector3 delta = dst - src;
  float hyp = delta.Length2D();

  Vector3 angles;
  // Source 2: positive pitch = looking down
  angles.x = -std::atan2f(delta.z, hyp) * (180.0f / 3.14159265f);
  angles.y = std::atan2f(delta.y, delta.x) * (180.0f / 3.14159265f);
  angles.z = 0.f;
  return angles;
}

// ============================================================================
// Bone Matrix (stride = 32 bytes per bone)
// ============================================================================
constexpr int BONE_STRIDE = 32;

inline Vector3 GetBonePosition(uintptr_t boneArray, int boneIndex) {
  uintptr_t boneAddr = boneArray + boneIndex * BONE_STRIDE;
  return *reinterpret_cast<Vector3 *>(boneAddr);
}
