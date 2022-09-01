// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPCamera.h"

namespace SPP
{
	//5 inches
	static const float NearClippingZ = 0.127f;
	//3 miles
	static const float FarClippingZ = 5000.0f;

	SPP_MATH_API void getBoundsForAxis(bool xAxis,
		const Vector3& center,
		float radius,
		float nearZ,
		const Matrix4x4& projMatrix,
		Vector4& U,
		Vector4& L)
	{
		bool trivialAccept = (center[2] + radius) < nearZ; // Entirely in back of nearPlane (Trivial Accept)
		const Vector3& a = xAxis ? Vector3(1, 0, 0) : Vector3(0, 1, 0);

		// given in coordinates (a,z), where a is in the direction of the vector a, and z is in the standard z direction
		Vector2 projectedCenter(a.dot(center), center[2]);
		Vector2 bounds_az[2];
		float tSquared = projectedCenter.dot(projectedCenter) - std::powf(radius,2);
		float t = 0, cLength = 0, costheta = 0, sintheta = 0;

		if (tSquared > 0) { // Camera is outside sphere
			// Distance to the tangent points of the sphere (points where a vector from the camera are tangent to the sphere) (calculated a-z space)
			t = std::sqrt(tSquared);
			cLength = projectedCenter.norm();

			// Theta is the angle between the vector from the camera to the center of the sphere and the vectors from the camera to the tangent points
			costheta = t / cLength;
			sintheta = radius / cLength;
		}
		float sqrtPart = 0.0f;
		if (!trivialAccept) 
			sqrtPart = std::sqrt(std::powf(radius,2) - std::powf(nearZ - projectedCenter[1], 2));

		for (int i = 0; i < 2; ++i) {
			if (tSquared > 0) {
				Matrix2x2 rotateTheta{
					{ costheta, -sintheta },
					{ sintheta, costheta } };

				bounds_az[i] = costheta * (projectedCenter * rotateTheta);
			}

			if (!trivialAccept && (tSquared <= 0 || bounds_az[i][1] > nearZ)) {
				bounds_az[i][0] = projectedCenter[0] + sqrtPart;
				bounds_az[i][1] = nearZ;
			}
			sintheta *= -1; // negate theta for B
			sqrtPart *= -1; // negate sqrtPart for B
		}
		U.head<3>() = bounds_az[0][0] * a;
		U[2] = bounds_az[0][1];
		U[3] = 1.0f;
		L.head<3>() = bounds_az[1][0] * a;
		L[2] = bounds_az[1][1];
		L[3] = 1.0f;
	}

	/** Center is in camera space */
	Vector4 getBoundingBox(const Vector3& center, float radius, float nearZ, const Matrix4x4& projMatrix) 
	{
		Vector4 maxXHomogenous, minXHomogenous, maxYHomogenous, minYHomogenous;
		getBoundsForAxis(true, center, radius, nearZ, projMatrix, maxXHomogenous, minXHomogenous);
		getBoundsForAxis(false, center, radius, nearZ, projMatrix, maxYHomogenous, minYHomogenous);
		// We only need one coordinate for each point, so we save computation by only calculating x(or y) and w
		float maxX = maxXHomogenous.dot(projMatrix.row(0)) / maxXHomogenous.dot(projMatrix.row(3));
		float minX = minXHomogenous.dot(projMatrix.row(0)) / minXHomogenous.dot(projMatrix.row(3));
		float maxY = maxYHomogenous.dot(projMatrix.row(1)) / maxYHomogenous.dot(projMatrix.row(3));
		float minY = minYHomogenous.dot(projMatrix.row(1)) / minYHomogenous.dot(projMatrix.row(3));
		return Vector4(minX, minY, maxX, maxY);
	}


	/** Center is in camera space */
	void tileClassification(int tileNumX,
		int tileNumY,
		int tileWidth,
		int tileHeight,
		const Vector3& center,
		float radius,
		float nearZ,
		const Matrix4x4& projMatrix)
	{
		Vector4 projectedBounds = getBoundingBox(center, radius, nearZ, projMatrix);

		int32_t minTileX = std::max< int32_t>(0, (int)(projectedBounds[0] / tileWidth));
		int32_t maxTileX = std::min< int32_t>(tileNumX - 1, (int)(projectedBounds[2] / tileWidth));

		int32_t minTileY = std::max< int32_t>(0, (int)(projectedBounds[1] / tileHeight));
		int32_t maxTileY = std::min< int32_t>(tileNumY - 1, (int)(projectedBounds[3] / tileHeight));

		for (int i = minTileX; i <= maxTileX; ++i) {
			for (int j = minTileY; j <= maxTileY; ++j) {
				// This tile is touched by the bounding box of the sphere
				// Put application specific tile-classification code here.
			}
		}
	}
		
	void Camera::Initialize(const Vector3d& InPosition, const Vector3& InEuler, float FoV, float AspectRatio)
	{
		_cameraPosition = InPosition;
		_eulerAngles = InEuler;
		_FoV = FoV;

		SetupStandardCorrection();
		GenerateLeftHandFoVPerspectiveMatrix(FoV, AspectRatio);		
		//GenerateOrthogonalMatrix({ 1920, 1080 });
		BuildCameraMatrices();		
	}

	void Camera::BuildCameraMatrices()
	{
		Eigen::Quaternion<float> q = EulerAnglesToQuaternion(_eulerAngles);

		Matrix3x3 rotationMatrix = q.matrix();

		_cameraMatrix = Matrix4x4::Identity();
		_cameraMatrix.block<3, 3>(0, 0) = rotationMatrix;
		//_cameraMatrix.block<1, 3>(3, 0) = Vector3(InPosition[0], InPosition[1], InPosition[2]);

		_viewProjMatrix = _cameraMatrix.inverse() * _correctionMatrix * _projectionMatrix;
		_invViewProjMatrix = _viewProjMatrix.inverse();
	}

	void Camera::GenerateLeftHandFoVPerspectiveMatrix(float FoV, float AspectRatio)
	{
		_projectionMatrix = Matrix4x4::Identity();

		float xscale = 1.0f / (float)std::tan(DegToRad(FoV) * 0.5f);
		float yscale = xscale * AspectRatio;

		float fnDelta = FarClippingZ - NearClippingZ;
		_projectionMatrix(0, 0) = xscale; // scale the x coordinates of the projected point 
		_projectionMatrix(1, 1) = yscale; // scale the y coordinates of the projected point 
		_projectionMatrix(2, 2) = FarClippingZ / fnDelta; // used to remap z to [0,1] 
		_projectionMatrix(3, 2) = (-NearClippingZ * FarClippingZ) / fnDelta; // used to remap z [0,1] 
		_projectionMatrix(2, 3) = 1; // set w = z 
		_projectionMatrix(3, 3) = 0;		

		_invProjectionMatrix = _projectionMatrix.inverse();
	}

	void Camera::GenerateOrthogonalMatrix(const Vector2i& InSize)
	{
		_projectionMatrix = Matrix4x4::Identity();

		float fnDelta = FarClippingZ - NearClippingZ;
		_projectionMatrix(0, 0) = 2.0f / InSize[0]; // scale the x coordinates of the projected point 
		_projectionMatrix(1, 1) = 2.0f / InSize[1]; // scale the y coordinates of the projected point 
		_projectionMatrix(2, 2) = 2 / fnDelta; // used to remap z to [0,1] 

		_invProjectionMatrix = _projectionMatrix.inverse();
	}

	float Camera::GetRecipTanHalfFovy() const
	{
		return _projectionMatrix(1, 1);
	}

	
	void Camera::SetupStandardCorrection()
	{		
		_correctionMatrix = Matrix4x4::Identity();
		//EX: to flip coordinates
		_correctionMatrix.block<1, 3>(0, 0) = Vector3(1, 0, 0);
		_correctionMatrix.block<1, 3>(1, 0) = Vector3(0, -1, 0);
		_correctionMatrix.block<1, 3>(2, 0) = Vector3(0, 0, 1);		
	}

	Vector3 Camera::CameraDirection()
	{
		return _cameraMatrix.block<1, 3>(0, 0);
	}

	void Camera::TurnCamera(const Vector2& CameraTurn)
	{
		_eulerAngles[1] += CameraTurn[0] * _turnSpeedModifier;
		_eulerAngles[0] += CameraTurn[1] * _turnSpeedModifier;

		if (CameraTurn[0] || CameraTurn[1])
		{
			BuildCameraMatrices();
		}
	}

	void Camera::SetCameraPosition(const Vector3d& InPosition)
	{
		_cameraPosition = InPosition;
	}

	Vector3 Camera::GetCameraMoveDelta(float DeltaTime, ERelativeDirection Direction)
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
		return movementDir;
	}

	void Camera::MoveCamera(float DeltaTime, ERelativeDirection Direction)
	{
		auto moveDelta = GetCameraMoveDelta(DeltaTime, Direction);		
		_cameraPosition += Vector3d(moveDelta[0], moveDelta[1], moveDelta[2]);
	}

	CameraCullInfo Camera::GetCullingData()
	{
		float znear = 0.5f;
		Matrix4x4 tProj = _projectionMatrix.transpose();

		Plane planeX;
		Plane planeY;

		planeX.coeffs() = tProj.block<1, 4>(3, 0) + tProj.block<1, 4>(0, 0); // x + w < 0
		planeY.coeffs() = tProj.block<1, 4>(3, 0) + tProj.block<1, 4>(1, 0); // y + w < 0

		planeX.normalize();
		planeY.normalize();

		CameraCullInfo cullData = {};
		cullData.P00 = _projectionMatrix(0, 0);
		cullData.P11 = _projectionMatrix(1, 1);
		cullData.znear = NearClippingZ;
		cullData.zfar = FarClippingZ;
		cullData.frustum[0] = planeX.coeffs()[0];
		cullData.frustum[1] = planeX.coeffs()[2];
		cullData.frustum[2] = planeY.coeffs()[1];
		cullData.frustum[3] = planeY.coeffs()[2];
		return cullData;
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

		viewProjMatrixWithTranslation.transposeInPlace();

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