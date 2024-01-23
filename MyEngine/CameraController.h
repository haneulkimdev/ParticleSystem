#pragma once

#include "Camera.h"
#include "SimpleMath.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

class CameraController {
 public:
  CameraController(Camera &camera) : m_targetCamera(camera) {}
  virtual ~CameraController() {}

 protected:
  Camera &m_targetCamera;
};

class FlyingFPSCamera : public CameraController {
 public:
  FlyingFPSCamera(Camera &camera) : CameraController(camera) {}

  void OnKeyboardInput(float dt);
};

class OrbitCamera : public CameraController {
 public:
  OrbitCamera(Camera &camera) : CameraController(camera) {}

  void OnMouseInput(int mouseButton, Vector2 lastMousePos, Vector2 mousePos,
                    Vector2 screenSize);

 private:
  Vector2 GetMouseNDC(Vector2 mousePos, Vector2 screenSize);
  Vector3 UnprojectOnTbPlane(Vector3 cameraPos, Vector2 mousePos,
                             Vector2 screenSize);
  Vector3 UnprojectOnTbSurface(Vector3 cameraPos, Vector2 mousePos,
                               float tbRadius, Vector2 screenSize);
};