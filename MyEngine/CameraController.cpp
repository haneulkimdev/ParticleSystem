#include "CameraController.h"

void FlyingFPSCamera::Update(float dt) { OnKeyboardInput(dt); }

void FlyingFPSCamera::OnKeyboardInput(float dt) {
  if (GetAsyncKeyState('W') & 0x8000) m_targetCamera.Walk(10.0f * dt);

  if (GetAsyncKeyState('S') & 0x8000) m_targetCamera.Walk(-10.0f * dt);

  if (GetAsyncKeyState('A') & 0x8000) m_targetCamera.Strafe(-10.0f * dt);

  if (GetAsyncKeyState('D') & 0x8000) m_targetCamera.Strafe(10.0f * dt);

  m_targetCamera.UpdateViewMatrix();
}