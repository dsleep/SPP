// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPEngine.h"
#include "SPPMath.h"

namespace SPP
{
	enum class ERelativeDirection
	{
		Forward,
		Back,
		Right,
		Left,
		Up,
		Down
	};

	//-X East +X West
	//-Y South +Y North
	//Z+ is Altitude
	class SPP_ENGINE_API Camera
	{
	protected:
		Vector3d _cameraPosition;
		//rotation and translation of local camera matrix
		Matrix4x4 _cameraMatrix;
		//projection matrix into normalized view space
		Matrix4x4 _projectionMatrix;
		//swaps us to the expected facing directions
		Matrix4x4 _correctionMatrix;
		//composited matrix
		Matrix4x4 _viewProjMatrix; 
		Matrix4x4 _invViewProjMatrix;

		float _FoV = 0.0f;

		float _speed = 500.0f;
		float _turnSpeedModifier = 0.1f;
		Vector3 _eulerAnglesYPR;
				
	public:
		Camera() { }
		
		void Initialize(const Vector3d& InPosition, const Vector3 &InYPR, double FoV, double AspectRatio);
		void BuildCameraMatrices();

		float GetFoV() const
		{
			return _FoV;
		}
		
		float GetRecipTanHalfFovy() const;

		void GenerateLeftHandFoVPerspectiveMatrix(double FoV, double AspectRatio);

		void SetupStandardCorrection();

		Vector3 CameraDirection();

		void GetFrustumCorners(Vector3 OutFrustumCorners[8]);
		void GetFrustumPlanes(Planed planes[6]);

		const Matrix4x4 &GetCameraMatrix() const { return _cameraMatrix; }
		const Matrix4x4 &GetProjectionMatrix() const { return _projectionMatrix; }
		const Matrix4x4 &GetCorrectionMatrix() const { return _correctionMatrix; }
		const Matrix4x4 &GetViewProjMatrix() const { return _viewProjMatrix; }
		const Matrix4x4 &GetInvViewProjMatrix() const { return _invViewProjMatrix; }
		Vector3d &GetCameraPosition() { return _cameraPosition; }

		void TurnCamera(const Vector2 &CameraTurn);
		void MoveCamera(float DeltaTime, ERelativeDirection Direction);
	};
}