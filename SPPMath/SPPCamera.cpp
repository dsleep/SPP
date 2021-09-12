// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPCamera.h"

namespace SPP
{
	static const double NearClippingZ = 10.0;
	static const double FarClippingZ = 50000.0;
		
	void Camera::Initialize(const Vector3d& InPosition, const Vector3& InYPR, double FoV, double AspectRatio)
	{
		_cameraPosition = InPosition;
		_eulerAnglesYPR = InYPR;
		_FoV = FoV;

		SetupStandardCorrection();
		GenerateLeftHandFoVPerspectiveMatrix(FoV, AspectRatio);		
		BuildCameraMatrices();		
	}

	void Camera::BuildCameraMatrices()
	{
		const float degToRad = 0.0174533f;
		Eigen::AngleAxisf yawAngle(_eulerAnglesYPR[0] * degToRad, Vector3::UnitY());
		Eigen::AngleAxisf pitchAngle(_eulerAnglesYPR[1] * degToRad, Vector3::UnitX());
		Eigen::AngleAxisf rollAngle(_eulerAnglesYPR[2] * degToRad, Vector3::UnitZ());
		Eigen::Quaternion<float> q = rollAngle * (pitchAngle * yawAngle);

		Matrix3x3 rotationMatrix = q.matrix();

		_cameraMatrix = Matrix4x4::Identity();
		_cameraMatrix.block<3, 3>(0, 0) = rotationMatrix;
		//_cameraMatrix.block<1, 3>(3, 0) = Vector3(InPosition[0], InPosition[1], InPosition[2]);

		_viewProjMatrix = _cameraMatrix.inverse() * _correctionMatrix * _projectionMatrix;
		_invViewProjMatrix = _viewProjMatrix.inverse();
	}

	void Camera::GenerateLeftHandFoVPerspectiveMatrix(double FoV, double AspectRatio)
	{
		_projectionMatrix = Matrix4x4::Identity();

		float xscale = 1.0f / (float)std::tan(DegToRad(FoV) * 0.5);
		float yscale = xscale * AspectRatio;

		float fnDelta = FarClippingZ - NearClippingZ;
		_projectionMatrix(0, 0) = xscale; // scale the x coordinates of the projected point 
		_projectionMatrix(1, 1) = yscale; // scale the y coordinates of the projected point 
		_projectionMatrix(2, 2) = FarClippingZ / fnDelta; // used to remap z to [0,1] 
		_projectionMatrix(3, 2) = (-NearClippingZ * FarClippingZ) / fnDelta; // used to remap z [0,1] 
		_projectionMatrix(2, 3) = 1; // set w = z 
		_projectionMatrix(3, 3) = 0;		
	}

	float Camera::GetRecipTanHalfFovy() const
	{
		return _projectionMatrix(1, 1);
	}

	//turns a coordinate syatem from from Up +Y Right +X Front +Z to Up +Z   from Up +Y Right +X Front +Z to Up +Z 
	void Camera::SetupStandardCorrection()
	{		
		_correctionMatrix = Matrix4x4::Identity();
		//EX: to flip coordinates
		//_correctionMatrix.block<1, 3>(0, 0) = Vector3(0, 0, 1);
		//_correctionMatrix.block<1, 3>(1, 0) = Vector3(1, 0, 0);
		//_correctionMatrix.block<1, 3>(2, 0) = Vector3(0, 1, 0);		
	}

	Vector3 Camera::CameraDirection()
	{
		return _cameraMatrix.block<1, 3>(0, 0);
	}

	void Camera::TurnCamera(const Vector2& CameraTurn)
	{
		_eulerAnglesYPR[0] += CameraTurn[0] * _turnSpeedModifier;
		_eulerAnglesYPR[1] += CameraTurn[1] * _turnSpeedModifier;

		if (CameraTurn[0] || CameraTurn[1])
		{
			BuildCameraMatrices();
		}
	}

	void Camera::MoveCamera(float DeltaTime, ERelativeDirection Direction)
	{
		Vector3 movementDir(0, 0, 0);
		switch (Direction)
		{
		case ERelativeDirection::Forward:
			movementDir = _cameraMatrix.block<1, 3>(2, 0);
			break;
		case ERelativeDirection::Back:
			movementDir = -_cameraMatrix.block<1, 3>(2, 0);
			break;
		case ERelativeDirection::Left:
			movementDir = -_cameraMatrix.block<1, 3>(0, 0);
			break;
		case ERelativeDirection::Right:
			movementDir = _cameraMatrix.block<1, 3>(0, 0);
			break;
		case ERelativeDirection::Up:
			movementDir = _cameraMatrix.block<1, 3>(1, 0);
			break;
		case ERelativeDirection::Down:
			movementDir = -_cameraMatrix.block<1, 3>(1, 0);
			break;
		}

		movementDir *= DeltaTime * _speed;
		_cameraPosition += Vector3d(movementDir[0], movementDir[1], movementDir[2]);
	}

	void Camera::GetFrustumCorners(Vector3 OutFrustumCorners[8])
	{
		Vector4 viewSpacefrustumCorners[8] = {
			//near
			Vector4(-1, -1, 0, 1),
			Vector4(-1, +1, 0, 1),
			Vector4(+1, +1, 0, 1),
			Vector4(+1, -1, 0, 1),
			//far 
			Vector4(-1, -1, 1, 1),
			Vector4(-1, +1, 1, 1),
			Vector4(+1, +1, 1, 1),
			Vector4(+1, -1, 1, 1)
		};

		_viewProjMatrix = _cameraMatrix.inverse() * _correctionMatrix * _projectionMatrix;
		_invViewProjMatrix = _viewProjMatrix.inverse();

		for (int32_t Iter = 0; Iter < 8; Iter++)
		{
			Vector4 currentCorner = (viewSpacefrustumCorners[Iter] * _invViewProjMatrix);
			currentCorner /= currentCorner[3];
			
			OutFrustumCorners[Iter] = currentCorner.block<1, 3>(0, 0);
		}
	}

	void Camera::GetFrustumPlanes(Planed planes[6])
	{				
		// left
		auto& coeff0 = planes[0].coeffs();
		auto& coeff1 = planes[1].coeffs();
		auto& coeff2 = planes[2].coeffs();
		auto& coeff3 = planes[3].coeffs();
		auto& coeff4 = planes[4].coeffs();
		auto& coeff5 = planes[5].coeffs();

		Matrix4x4d cameraMatrixWithTranslation = _cameraMatrix.cast<double>();
		cameraMatrixWithTranslation.block<1, 3>(3, 0) = Vector3d(_cameraPosition[0], _cameraPosition[1], _cameraPosition[2]);

		Matrix4x4d viewProjMatrixWithTranslation = cameraMatrixWithTranslation.inverse() * _correctionMatrix.cast<double>() * _projectionMatrix.cast<double>();

		// left
		coeff0 = viewProjMatrixWithTranslation.block<1, 4>(3, 0) + viewProjMatrixWithTranslation.block<1, 4>(0, 0);
		// right
		coeff1 = viewProjMatrixWithTranslation.block<1, 4>(3, 0) - viewProjMatrixWithTranslation.block<1, 4>(0, 0);
		// bottom
		coeff2 = viewProjMatrixWithTranslation.block<1, 4>(3, 0) + viewProjMatrixWithTranslation.block<1, 4>(1, 0);
		// top
		coeff3 = viewProjMatrixWithTranslation.block<1, 4>(3, 0) - viewProjMatrixWithTranslation.block<1, 4>(1, 0);		
		// near
		coeff4 = viewProjMatrixWithTranslation.block<1, 4>(3, 0) + viewProjMatrixWithTranslation.block<1, 4>(2, 0);
		// far
		coeff5 = viewProjMatrixWithTranslation.block<1, 4>(3, 0) - viewProjMatrixWithTranslation.block<1, 4>(2, 0);

		// normalize all planes
		for (int32_t Iter = 0; Iter < 6; ++Iter)
		{
			planes[Iter].normalize();
		}		
	}

}