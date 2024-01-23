#include "CameraController.h"

void FlyingFPSCamera::OnKeyboardInput(float dt) {
  if (GetAsyncKeyState('W') & 0x8000) m_targetCamera.Walk(10.0f * dt);

  if (GetAsyncKeyState('S') & 0x8000) m_targetCamera.Walk(-10.0f * dt);

  if (GetAsyncKeyState('A') & 0x8000) m_targetCamera.Strafe(-10.0f * dt);

  if (GetAsyncKeyState('D') & 0x8000) m_targetCamera.Strafe(10.0f * dt);

  m_targetCamera.UpdateViewMatrix();
}

void OrbitCamera::OnMouseInput(int mouseButton, Vector2 lastMousePos,
                               Vector2 mousePos, Vector2 screenSize) {
  switch (mouseButton) {
    case 0: {
      float tbRadius = 1.0f;

      Vector3 p0 = UnprojectOnTbSurface(Vector3(0.0f, 0.0f, -5.0f),
                                        lastMousePos, tbRadius, screenSize);
      Vector3 p1 = UnprojectOnTbSurface(Vector3(0.0f, 0.0f, -5.0f), mousePos,
                                        tbRadius, screenSize);

      Vector3 axis = p0.Cross(p1);

      if (axis.LengthSquared() < 0.01f) break;

      axis.Normalize();

      float distance = Vector3::Distance(p0, p1);
      float angle = distance / tbRadius;

      if (angle < 0.01f) break;

      m_targetCamera.TransformView(Matrix::CreateFromAxisAngle(axis, -angle));
      break;
    }
    case 1: {
      Vector3 p0 = UnprojectOnTbPlane(Vector3(0.0f, 0.0f, -5.0f), lastMousePos,
                                      screenSize);
      Vector3 p1 =
          UnprojectOnTbPlane(Vector3(0.0f, 0.0f, -5.0f), mousePos, screenSize);

      Vector3 movement = p1 - p0;

      m_targetCamera.TransformView(Matrix::CreateTranslation(movement));
      break;
    }
  }

  m_targetCamera.UpdateViewMatrix();
}

Vector2 OrbitCamera::GetMouseNDC(Vector2 mousePos, Vector2 screenSize) {
  Vector2 mouseNDC;
  mouseNDC.x = (2.0f * mousePos.x) / screenSize.x - 1.0f;
  mouseNDC.y = 1.0f - (2.0f * mousePos.y) / screenSize.y;
  return mouseNDC;
}

Vector3 OrbitCamera::UnprojectOnTbPlane(Vector3 cameraPos, Vector2 mousePos,
                                        Vector2 screenSize) {
  Vector2 mouseNDC = GetMouseNDC(mousePos, screenSize);

  // unproject cursor on the near plane
  Vector3 rayDir(mouseNDC.x, mouseNDC.y, -1.0f);
  rayDir = Vector3::Transform(rayDir, m_targetCamera.Proj().Invert());

  rayDir.Normalize();  // unprojected ray direction

  //	  camera
  //		|\
  //		| \
  //		|  \
  //	h	|	\
  //		| 	 \
  //		| 	  \
  //	_ _ | _ _ _\ _ _  near plane
  //			l

  float h = rayDir.z;
  float l = sqrtf(rayDir.x * rayDir.x + rayDir.y * rayDir.y);
  float cameraSpehreDistance = Vector3::Distance(cameraPos, Vector3());

  /*
   * calculate intersection point between unprojected ray and the plane
   *|y = mx + q
   *|y = 0
   *
   * x = -q/m
   */
  if (l == 0.0f) {
    // ray aligned with camera
    rayDir = Vector3();
    return rayDir;
  }

  float m = h / l;
  float q = cameraSpehreDistance;
  float x = -q / m;

  float rayLength = sqrtf(q * q + x * x);
  rayDir *= rayLength;
  rayDir.z = 0.0f;
  return rayDir;
}

Vector3 OrbitCamera::UnprojectOnTbSurface(Vector3 cameraPos, Vector2 mousePos,
                                          float tbRadius, Vector2 screenSize) {
  // unproject cursor on the near plane
  Vector2 mouseNDC = GetMouseNDC(mousePos, screenSize);

  Vector3 rayDir(mouseNDC.x, mouseNDC.y, -1.0f);
  rayDir = Vector3::Transform(rayDir, m_targetCamera.Proj().Invert());

  rayDir.Normalize();  // unprojected ray direction
  float cameraSpehreDistance = Vector3::Distance(cameraPos, Vector3());
  float radius2 = tbRadius * tbRadius;

  //	  camera
  //		|\
  //		| \
  //		|  \
  //	h	|	\
  //		| 	 \
  //		| 	  \
  //	_ _ | _ _ _\ _ _  near plane
  //			l

  float h = rayDir.z;
  float l = sqrtf(rayDir.x * rayDir.x + rayDir.y * rayDir.y);

  if (l == 0.0f) {
    // ray aligned with camera
    rayDir = Vector3(rayDir.x, rayDir.y, tbRadius);
    return rayDir;
  }

  float m = h / l;
  float q = cameraSpehreDistance;

  /*
   * calculate intersection point between unprojected ray and trackball surface
   *|y = m * x + q
   *|x^2 + y^2 = r^2
   *
   * (m^2 + 1) * x^2 + (2 * m * q) * x + q^2 - r^2 = 0
   */
  float a = m * m + 1.0f;
  float b = 2.0f * m * q;
  float c = q * q - radius2;
  float delta = b * b - (4.0f * a * c);

  if (delta >= 0.0f) {
    // intersection with sphere
    float x = (-b - sqrtf(delta)) / (2.0f * a);
    float y = m * x + q;

    float angle = atan2f(-y, -x) + XM_PI;

    if (angle >= XM_PIDIV4) {
      // if angle between intersection point and X' axis is >= 45, return that
      // point otherwise, calculate intersection point with hyperboloid

      float rayLength = sqrtf(x * x + (cameraSpehreDistance - y) *
                                          (cameraSpehreDistance - y));
      rayDir *= rayLength;
      rayDir.z += cameraSpehreDistance;
      return rayDir;
    }
  }

  // intersection with hyperboloid
  /*
   *|y = m * x + q
   *|y = (1 / x) * (r^2 / 2)
   *
   * m * x^2 + q * x - r^2 / 2 = 0
   */

  a = m;
  b = q;
  c = -radius2 * 0.5f;
  delta = b * b - (4.0f * a * c);
  float x = (-b - sqrtf(delta)) / (2.0f * a);
  float y = m * x + q;

  float rayLength =
      sqrtf(x * x + (cameraSpehreDistance - y) * (cameraSpehreDistance - y));

  rayDir *= rayLength;
  rayDir.z += cameraSpehreDistance;
  return rayDir;
}