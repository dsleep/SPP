// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPCore.h"
#include "SPPReferenceCounter.h"
#include "SPPMath.h"
#include "SPPSceneO.h"
#include <coroutine>
#include <list>

#if _WIN32 && !defined(SPP_PHYSX_STATIC)

	#ifdef SPP_PHYSX_EXPORT
		#define SPP_PHYSX_API __declspec(dllexport)
	#else
		#define SPP_PHYSX_API __declspec(dllimport)
	#endif

#else

	#define SPP_PHYSX_API 

#endif

namespace SPP
{		
	SPP_PHYSX_API uint32_t GetPhysXVersion();
	SPP_PHYSX_API void InitializePhysX();

	struct DataView
	{
		void* Data = nullptr;
		size_t DataSize = 0;
	};

	class PhysicsTriangleMesh
	{
	protected:
	public:
		virtual DataView GetData() = 0;
	};

	enum class PrimitiveType
	{
		Box,
		Sphere,
		Plane,
		Capsule,
		TriangleMesh,
		Convex
	};

	class PhysicsCharacter
	{
	protected:

	public:
		virtual ~PhysicsCharacter() {}

		virtual Vector3d GetPosition() = 0;
		virtual Vector3 GetRotation() = 0;
		virtual Vector3 GetScale() = 0;

		virtual void SetPosition(const Vector3d& InValue) = 0;
		virtual void SetRotation(const Vector3& InValue) = 0;
		virtual void SetScale(const Vector3& InValue) = 0;
	};

	class PhysicsPrimitive
	{
	protected:

	public:
		virtual ~PhysicsPrimitive() {}

		virtual bool IsDynamic() = 0;

		virtual Vector3d GetPosition() = 0;
		virtual Vector3 GetRotation() = 0;
		virtual Vector3 GetScale() = 0;

		virtual void SetPosition(const Vector3d &InValue) = 0;
		virtual void SetRotation(const Vector3 &InValue) = 0;
		virtual void SetScale(const Vector3& InValue) = 0;
	};

	class PhysicsScene
	{
	protected:

	public:

		virtual ~PhysicsScene() {}

		virtual void Update(float DeltaTime) = 0;

		virtual std::shared_ptr< PhysicsPrimitive > CreateBoxPrimitive(const Vector3d& InPosition, 
			const Vector3& InRotationEuler,
			const Vector3& Extents,
			OElement *eleRef,
			bool bIsDynamic = false) = 0;

		virtual std::shared_ptr< PhysicsPrimitive > CreateTriangleMeshPrimitive(const Vector3d& InPosition,
			const Vector3& InRotationEuler,
			const Vector3& InScale,
			std::shared_ptr< PhysicsTriangleMesh > InTriMesh) = 0;

		virtual std::shared_ptr< PhysicsCharacter > CreateCharacterCapsule(const Vector3& Extents,
			OElement* InElement ) = 0;
	};

	class PhysicsAPI
	{
	protected:

	public:
		virtual ~PhysicsAPI() {}
		virtual std::shared_ptr< PhysicsScene > CreatePhysicsScene() = 0;

		virtual std::shared_ptr< PhysicsTriangleMesh > CreateTriangleMesh(uint32_t numVertices,
			const void* vertData,
			uint32_t stride,
			uint32_t numTriangles,
			const void* indices,
			uint32_t indexStride ) = 0;

	};

	SPP_PHYSX_API PhysicsAPI *GetPhysicsAPI();
}
