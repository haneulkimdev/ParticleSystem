#pragma once

#include "Camera.h"
#include "SimpleMath.h"

using namespace DirectX::SimpleMath;

class CameraController {
 public:
  CameraController(Camera &camera) : m_targetCamera(camera) {}
  virtual ~CameraController() {}
  virtual void Update(float dt) = 0;

 protected:
  Camera &m_targetCamera;
};

class FlyingFPSCamera : public CameraController {
 public:
  FlyingFPSCamera(Camera &camera) : CameraController(camera) {}

  virtual void Update(float dt) override;

  void OnKeyboardInput(float dt);
};