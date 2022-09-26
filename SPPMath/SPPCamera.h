// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPMath.h"
#include "SPPPrimitiveShapes.h"

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

	struct alignas(16) CameraCullInfo
	{
		float P00, P11, znear, zfar; // symmetric projection parameters
		float frustum[4]; // data for left/right/top/bottom frustum planes
	};

	SPP_MATH_API void getBoundsForAxis(bool xAxis,
		const Vector3& center,
		float radius,
		float nearZ,
		const Matrix4x4& projMatrix,
		Vector3& U,
		Vector3& L);

	/** Center is in camera space */
	SPP_MATH_API Vector4 getBoundingBox(const Vector3& center, float radius, float nearZ, const Matrix4x4& projMatrix);

	/** Center is in camera space */
	SPP_MATH_API void tileClassification(int tileNumX,
		int tileNumY,
		int tileWidth,
		int tileHeight,
		const Vector3& center,
		float radius,
		float nearZ,
		const Matrix4x4& projMatrix);

	struct BoxOfCorners
	{
		std::array< Vector3, 8 > Points;
	};

	// default is a left handed coordinate system
	// right X+
	// up Y+
	// front Z+
	class SPP_MATH_API Camera
	{
	protected:
		Vector3d _cameraPosition;
		//rotation and translation of local camera matrix
		Matrix4x4 _cameraMatrix;
		Matrix4x4 _invCameraMatrix;
		//projection matrix into normalized view space
		Matrix4x4 _projectionMatrix;
		Matrix4x4 _invProjectionMatrix;
		//swaps us to the expected facing directions
		Matrix4x4 _correctionMatrix;
		//composited matrix
		Matrix4x4 _viewProjMatrix; 
		Matrix4x4 _invViewProjMatrix;

		float _FoV = 0.0f;

		float _speed = 25.0f;
		float _turnSpeedModifier = 0.1f;
		Vector3 _eulerAngles;

		bool bIsInvertedZ = false;
				
	public:
		Camera() { }
		
		void Initialize(const Vector3d& InPosition, const Vector3 &InEuler, float FoV, float AspectRatio);
		void Initialize(const Vector3d& InPosition, const Vector3& InEuler, Vector2 &Extents, Vector2& NearFar);

		void BuildCameraMatrices();

		float GetFoV() const
		{
			return _FoV;
		}
		
		float GetRecipTanHalfFovy() const;

		void GenerateLeftHandFoVPerspectiveMatrix(float FoV, float AspectRatio);


		void GenerateLHInverseZPerspectiveMatrix(float FoV, float AspectRatio);

		void GenerateOrthogonalMatrix(const Vector2& InSize, const Vector2& InDepthRange);

		void SetupStandardCorrection();

		// without Camera Location Offset, but has rotation
		void GetFrustumSpheresForRanges(const std::vector<float>& DepthRanges, std::vector<Sphere>& OutFrustumSpheres);

		void SphereProjectionTest(const Sphere &InSphere);

		Vector3 CameraDirection();

		CameraCullInfo GetCullingData();

		void GetFrustumCorners(Vector3 OutFrustumCorners[8]);
		void GetFrustumPlanes(Planed planes[5]);

		const Matrix4x4 &GetWorldToCameraMatrix() const { return _invCameraMatrix; }
		const Matrix4x4 &GetProjectionMatrix() const { return _projectionMatrix; }
		const Matrix4x4 &GetInvProjectionMatrix() const { return _invProjectionMatrix; }
		const Matrix4x4 &GetCorrectionMatrix() const { return _correctionMatrix; }
		const Matrix4x4 &GetViewProjMatrix() const { return _viewProjMatrix; }
		const Matrix4x4 &GetInvViewProjMatrix() const { return _invViewProjMatrix; }
		Vector3d &GetCameraPosition() { return _cameraPosition; }

		void TurnCamera(const Vector2 &CameraTurn);
		void MoveCamera(float DeltaTime, ERelativeDirection Direction);

		Vector3 GetCameraMoveDelta(float DeltaTime, ERelativeDirection Direction);
		void SetCameraPosition(const Vector3d &InPosition);
	};
}