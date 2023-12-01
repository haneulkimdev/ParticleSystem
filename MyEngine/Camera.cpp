#include "Camera.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

Camera::Camera()
    : m_position(0.0f, 0.0f, 0.0f),
      m_right(1.0f, 0.0f, 0.0f),
      m_up(0.0f, 1.0f, 0.0f),
      m_look(0.0f, 0.0f, 1.0f) {
  SetLens(0.25f * XM_PI, 1.0f, 1.0f, 1000.0f);
}

Camera::~Camera() {}

Vector3 Camera::GetPosition() const { return m_position; }

void Camera::SetPosition(float x, float y, float z) {
  m_position = Vector3(x, y, z);
}

void Camera::SetPosition(const Vector3& v) { m_position = v; }

Vector3 Camera::GetRight() const { return m_right; }

Vector3 Camera::GetUp() const { return m_up; }

Vector3 Camera::GetLook() const { return m_look; }

float Camera::GetNearZ() const { return m_nearZ; }

float Camera::GetFarZ() const { return m_farZ; }

float Camera::GetAspect() const { return m_aspect; }

float Camera::GetFovY() const { return m_fovY; }

float Camera::GetFovX() const {
  float halfWidth = 0.5f * GetNearWindowWidth();
  return 2.0f * atan(halfWidth / m_nearZ);
}

float Camera::GetNearWindowWidth() const {
  return m_aspect * m_nearWindowHeight;
}

float Camera::GetNearWindowHeight() const { return m_nearWindowHeight; }

float Camera::GetFarWindowWidth() const { return m_aspect * m_farWindowHeight; }

float Camera::GetFarWindowHeight() const { return m_farWindowHeight; }

void Camera::SetLens(float fovY, float aspect, float zn, float zf) {
  // cache properties
  m_fovY = fovY;
  m_aspect = aspect;
  m_nearZ = zn;
  m_farZ = zf;

  m_nearWindowHeight = 2.0f * m_nearZ * tanf(0.5f * m_fovY);
  m_farWindowHeight = 2.0f * m_farZ * tanf(0.5f * m_fovY);

  m_proj = XMMatrixPerspectiveFovLH(m_fovY, m_aspect, m_nearZ, m_farZ);
}

void Camera::LookAt(const Vector3& pos, const Vector3& target,
                    const Vector3& up) {
  Vector3 L = target - pos;
  L.Normalize();
  Vector3 R = up.Cross(L);
  R.Normalize();
  Vector3 U = L.Cross(R);

  m_position = pos;
  m_look = L;
  m_right = R;
  m_up = U;
}

Matrix Camera::View() const { return m_view; }

Matrix Camera::Proj() const { return m_proj; }

Matrix Camera::ViewProj() const { return View() * Proj(); }

void Camera::Strafe(float d) { m_position += d * m_right; }

void Camera::Walk(float d) { m_position += d * m_look; }

void Camera::Pitch(float angle) {
  // Rotate up and look vector about the right vector.

  Matrix R = Matrix::CreateFromAxisAngle(m_right, angle);

  m_up = Vector3::TransformNormal(m_up, R);
  m_look = Vector3::TransformNormal(m_look, R);
}

void Camera::RotateY(float angle) {
  // Rotate the basis vectors about the world y-axis.

  Matrix R = Matrix::CreateRotationY(angle);

  m_right = Vector3::TransformNormal(m_right, R);
  m_up = Vector3::TransformNormal(m_up, R);
  m_look = Vector3::TransformNormal(m_look, R);
}

void Camera::UpdateViewMatrix() {
  // Keep camera's axes orthogonal to each other and of unit length.
  m_look.Normalize();
  m_up = m_look.Cross(m_right);
  m_up.Normalize();

  // U, L already ortho-normal, so no need to normalize cross product.
  m_right = m_up.Cross(m_look);

  // Fill in the view matrix entries.
  float x = -m_position.Dot(m_right);
  float y = -m_position.Dot(m_up);
  float z = -m_position.Dot(m_look);

  m_view(0, 0) = m_right.x;
  m_view(1, 0) = m_right.y;
  m_view(2, 0) = m_right.z;
  m_view(3, 0) = x;

  m_view(0, 1) = m_up.x;
  m_view(1, 1) = m_up.y;
  m_view(2, 1) = m_up.z;
  m_view(3, 1) = y;

  m_view(0, 2) = m_look.x;
  m_view(1, 2) = m_look.y;
  m_view(2, 2) = m_look.z;
  m_view(3, 2) = z;

  m_view(0, 3) = 0.0f;
  m_view(1, 3) = 0.0f;
  m_view(2, 3) = 0.0f;
  m_view(3, 3) = 1.0f;
}