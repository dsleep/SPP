// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPPhysXSVVO.h"
#include "PxPhysicsAPI.h"
#include "SPPPlatformCore.h"
#include "SPPMemory.h"
#include "SPPHandledTimers.h"
#include "SPPSparseVirtualizedVoxelOctree.h"

#include "collision/PxCollisionDefs.h"
#include "PxImmediateMode.h"
#include "common/PxRenderOutput.h"
#include "geomutils/PxContactBuffer.h"

#include "characterkinematic/PxBoxController.h"
#include "characterkinematic/PxCapsuleController.h"
#include "characterkinematic/PxControllerManager.h"

#include <mutex>
#include <condition_variable>
#include <chrono>

using namespace physx;

namespace SPP
{
	class PhysXPrimitiveSVVO : public PhysicsPrimitive, public PxCustomGeometry::Callbacks, public PxUserAllocated
	{
	protected:

	public:
		PhysXPrimitiveSVVO(PxRigidActor* InActor)
		{

		}

		virtual ~PhysXPrimitiveSVVO()
		{
		}

		virtual bool IsDynamic() override
		{
			return false;// _pxActor->
		}

		virtual Vector3d GetPosition() override
		{
			return Vector3d(0,0,0);
		}
		virtual Vector3 GetRotation() override
		{
			return Vector3(0, 0, 0);
		}
		virtual Vector3 GetScale() override
		{
			return Vector3(1, 1, 1);
		}

		virtual void SetPosition(const Vector3d& InValue) override
		{

		}
		virtual void SetRotation(const Vector3& InValue) override
		{

		}
		virtual void SetScale(const Vector3& InValue) override
		{

		}

		PxVec3 voxelSize() const
		{
			return PxVec3(1, 1, 1);
		}
		PxVec3 extents() const
		{
			return PxVec3(1, 1, 1);
		}
		void getVoxelRegion(const physx::PxBounds3& b, int& sx, int& sy, int& sz, int& ex, int& ey, int& ez) const
		{

		}

		bool voxel(int x, int y, int z) const 
		{
			return false;
		}


		PxVec3 voxelPos(int x, int y, int z) const
		{
			return PxVec3(1, 1, 1);
		}


		void pointCoords(const physx::PxVec3& p, int& x, int& y, int& z) const
		{

		}

		DECLARE_CUSTOM_GEOMETRY_TYPE
		virtual physx::PxBounds3 getLocalBounds(const physx::PxGeometry&) const;
		virtual bool generateContacts(const physx::PxGeometry& geom0, const physx::PxGeometry& geom1, const physx::PxTransform& pose0, const physx::PxTransform& pose1,
			const physx::PxReal contactDistance, const physx::PxReal meshContactMargin, const physx::PxReal toleranceLength,
			physx::PxContactBuffer& contactBuffer) const;
		virtual physx::PxU32 raycast(const physx::PxVec3& origin, const physx::PxVec3& unitDir, const physx::PxGeometry& geom, const physx::PxTransform& pose,
			physx::PxReal maxDist, physx::PxHitFlags hitFlags, physx::PxU32 maxHits, physx::PxGeomRaycastHit* rayHits, physx::PxU32 stride, physx::PxRaycastThreadContext*) const;
		virtual bool overlap(const physx::PxGeometry& geom0, const physx::PxTransform& pose0, const physx::PxGeometry& geom1, const physx::PxTransform& pose1, physx::PxOverlapThreadContext*) const;
		virtual bool sweep(const physx::PxVec3& unitDir, const physx::PxReal maxDist,
			const physx::PxGeometry& geom0, const physx::PxTransform& pose0, const physx::PxGeometry& geom1, const physx::PxTransform& pose1,
			physx::PxGeomSweepHit& sweepHit, physx::PxHitFlags hitFlags, const physx::PxReal inflation, physx::PxSweepThreadContext*) const;
		virtual void visualize(const physx::PxGeometry&, physx::PxRenderOutput&, const physx::PxTransform&, const physx::PxBounds3&) const;
		virtual void computeMassProperties(const physx::PxGeometry&, physx::PxMassProperties&) const {}
		virtual bool usePersistentContactManifold(const physx::PxGeometry&, physx::PxReal&) const { return true; }
	};

	IMPLEMENT_CUSTOM_GEOMETRY_TYPE(PhysXPrimitiveSVVO)

	PxBounds3 PhysXPrimitiveSVVO::getLocalBounds(const PxGeometry&) const
	{
		return PxBounds3::centerExtents(PxVec3(0), extents());
	}

	bool PhysXPrimitiveSVVO::generateContacts(const PxGeometry& /*geom0*/, const PxGeometry& geom1, const PxTransform& pose0, const PxTransform& pose1,
		const PxReal contactDistance, const PxReal meshContactMargin, const PxReal toleranceLength,
		PxContactBuffer& contactBuffer) const
	{
		PxBoxGeometry voxelGeom(voxelSize() * 0.5f);
		PxGeometry* pGeom0 = &voxelGeom;

		const PxGeometry* pGeom1 = &geom1;
		PxTransform pose1in0 = pose0.transformInv(pose1);
		PxBounds3 bounds1; PxGeometryQuery::computeGeomBounds(bounds1, geom1, pose1in0, contactDistance);

		struct ContactRecorder : immediate::PxContactRecorder
		{
			PxContactBuffer* contactBuffer;
			ContactRecorder(PxContactBuffer& _contactBuffer) : contactBuffer(&_contactBuffer) {}
			virtual bool recordContacts(const PxContactPoint* contactPoints, const PxU32 nbContacts, const PxU32 /*index*/)
			{
				for (PxU32 i = 0; i < nbContacts; ++i)
					contactBuffer->contact(contactPoints[i]);
				return true;
			}
		}
		contactRecorder(contactBuffer);

		PxCache contactCache;

		struct ContactCacheAllocator : PxCacheAllocator
		{
			PxU8 buffer[1024];
			ContactCacheAllocator() { memset(buffer, 0, sizeof(buffer)); }
			virtual PxU8* allocateCacheData(const PxU32 /*byteSize*/) { return reinterpret_cast<PxU8*>(size_t(buffer + 0xf) & ~0xf); }
		}
		contactCacheAllocator;

		int sx, sy, sz, ex, ey, ez;
		getVoxelRegion(bounds1, sx, sy, sz, ex, ey, ez);
		for (int x = sx; x <= ex; ++x)
			for (int y = sy; y <= ey; ++y)
				for (int z = sz; z <= ez; ++z)
					if (voxel(x, y, z))
					{
						PxTransform p0 = pose0.transform(PxTransform(voxelPos(x, y, z)));
						immediate::PxGenerateContacts(&pGeom0, &pGeom1, &p0, &pose1, &contactCache, 1, contactRecorder,
							contactDistance, meshContactMargin, toleranceLength, contactCacheAllocator);
					}

		return true;
	}

	PxU32 PhysXPrimitiveSVVO::raycast(const PxVec3& origin, const PxVec3& unitDir, const PxGeometry& /*geom*/, const PxTransform& pose,
		PxReal maxDist, PxHitFlags hitFlags, PxU32 maxHits, PxGeomRaycastHit* rayHits, PxU32 stride, PxRaycastThreadContext*) const
	{
		PxVec3 p = pose.transformInv(origin);
		PxVec3 n = pose.rotateInv(unitDir);
		PxVec3 s = voxelSize() * 0.5f;
		int x, y, z; pointCoords(p, x, y, z);
		int hitCount = 0;
		PxU8* hitBuffer = reinterpret_cast<PxU8*>(rayHits);
		float currDist = 0;
		PxVec3 hitN(0);

		while (currDist < maxDist)
		{
			PxVec3 v = voxelPos(x, y, z);
			if (voxel(x, y, z))
			{
				PxGeomRaycastHit& h = *reinterpret_cast<PxGeomRaycastHit*>(hitBuffer + hitCount * stride);
				h.distance = currDist;
				if (hitFlags.isSet(PxHitFlag::ePOSITION))
					h.position = origin + unitDir * currDist;
				if (hitFlags.isSet(PxHitFlag::eNORMAL))
					h.normal = hitN;
				if (hitFlags.isSet(PxHitFlag::eFACE_INDEX))
					h.faceIndex = (x) | (y << 10) | (z << 20);
				hitCount += 1;
			}
			if (hitCount == int(maxHits))
				break;
			float step = FLT_MAX;
			int dx = 0, dy = 0, dz = 0;
			if (n.x > FLT_EPSILON)
			{
				float d = (v.x + s.x - p.x) / n.x;
				if (d < step) { step = d; dx = 1; dy = 0; dz = 0; }
			}
			if (n.x < -FLT_EPSILON)
			{
				float d = (v.x - s.x - p.x) / n.x;
				if (d < step) { step = d; dx = -1; dy = 0; dz = 0; }
			}
			if (n.y > FLT_EPSILON)
			{
				float d = (v.y + s.y - p.y) / n.y;
				if (d < step) { step = d; dx = 0; dy = 1; dz = 0; }
			}
			if (n.y < -FLT_EPSILON)
			{
				float d = (v.y - s.y - p.y) / n.y;
				if (d < step) { step = d; dx = 0; dy = -1; dz = 0; }
			}
			if (n.z > FLT_EPSILON)
			{
				float d = (v.z + s.z - p.z) / n.z;
				if (d < step) { step = d; dx = 0; dy = 0; dz = 1; }
			}
			if (n.z < -FLT_EPSILON)
			{
				float d = (v.z - s.z - p.z) / n.z;
				if (d < step) { step = d; dx = 0; dy = 0; dz = -1; }
			}
			x += dx; y += dy; z += dz;
			hitN = PxVec3(float(-dx), float(-dy), float(-dz));
			currDist = step;
		}

		return hitCount;
	}

	bool PhysXPrimitiveSVVO::overlap(const PxGeometry& /*geom0*/, const PxTransform& pose0, const PxGeometry& geom1, const PxTransform& pose1, PxOverlapThreadContext*) const
	{
		PxBoxGeometry voxelGeom(voxelSize() * 0.5f);

		PxTransform pose1in0 = pose0.transformInv(pose1);
		PxBounds3 bounds1; PxGeometryQuery::computeGeomBounds(bounds1, geom1, pose1in0);

		int sx, sy, sz, ex, ey, ez;
		getVoxelRegion(bounds1, sx, sy, sz, ex, ey, ez);
		for (int x = sx; x <= ex; ++x)
			for (int y = sy; y <= ey; ++y)
				for (int z = sz; z <= ez; ++z)
					if (voxel(x, y, z))
					{
						PxTransform p0 = pose0.transform(PxTransform(voxelPos(x, y, z)));
						if (PxGeometryQuery::overlap(voxelGeom, p0, geom1, pose1, PxGeometryQueryFlags(0)))
							return true;
					}

		return false;
	}

	bool PhysXPrimitiveSVVO::sweep(const PxVec3& unitDir, const PxReal maxDist,
		const PxGeometry& /*geom0*/, const PxTransform& pose0, const PxGeometry& geom1, const PxTransform& pose1,
		PxGeomSweepHit& sweepHit, PxHitFlags hitFlags, const PxReal inflation, PxSweepThreadContext*) const
	{
		PxBoxGeometry voxelGeom(voxelSize() * 0.5f);

		PxTransform pose1in0 = pose0.transformInv(pose1);
		PxBounds3 b; PxGeometryQuery::computeGeomBounds(b, geom1, pose1in0, 0, 1.0f, PxGeometryQueryFlags(0));
		PxVec3 n = pose0.rotateInv(unitDir);
		PxVec3 s = voxelSize();

		int sx, sy, sz, ex, ey, ez;
		getVoxelRegion(b, sx, sy, sz, ex, ey, ez);
		int sx1, sy1, sz1, ex1, ey1, ez1;
		sx1 = sy1 = sz1 = -1; ex1 = ey1 = ez1 = 0;
		float currDist = 0;
		sweepHit.distance = FLT_MAX;

		while (currDist < maxDist && currDist < sweepHit.distance)
		{
			for (int x = sx; x <= ex; ++x)
				for (int y = sy; y <= ey; ++y)
					for (int z = sz; z <= ez; ++z)
						if (voxel(x, y, z))
						{
							if (x >= sx1 && x <= ex1 && y >= sy1 && y <= ey1 && z >= sz1 && z <= ez1)
								continue;

							PxGeomSweepHit hit;
							PxTransform p0 = pose0.transform(PxTransform(voxelPos(x, y, z)));
							if (PxGeometryQuery::sweep(unitDir, maxDist, geom1, pose1, voxelGeom, p0, hit, hitFlags, inflation, PxGeometryQueryFlags(0)))
								if (hit.distance < sweepHit.distance)
									sweepHit = hit;
						}

			PxVec3 mi = b.minimum, ma = b.maximum;
			PxVec3 bs = voxelPos(sx, sy, sz) - s, be = voxelPos(ex, ey, ez) + s;
			float dist = FLT_MAX;
			if (n.x > FLT_EPSILON)
			{
				float d = (be.x - ma.x) / n.x;
				if (d < dist) dist = d;
			}
			if (n.x < -FLT_EPSILON)
			{
				float d = (bs.x - mi.x) / n.x;
				if (d < dist) dist = d;
			}
			if (n.y > FLT_EPSILON)
			{
				float d = (be.y - ma.y) / n.y;
				if (d < dist) dist = d;
			}
			if (n.y < -FLT_EPSILON)
			{
				float d = (bs.y - mi.y) / n.y;
				if (d < dist) dist = d;
			}
			if (n.z > FLT_EPSILON)
			{
				float d = (be.z - ma.z) / n.z;
				if (d < dist) dist = d;
			}
			if (n.z < -FLT_EPSILON)
			{
				float d = (bs.z - mi.z) / n.z;
				if (d < dist) dist = d;
			}
			sx1 = sx; sy1 = sy; sz1 = sz; ex1 = ex; ey1 = ey; ez1 = ez;
			PxBounds3 b1 = b; b1.minimum += n * dist; b1.maximum += n * dist;
			getVoxelRegion(b1, sx, sy, sz, ex, ey, ez);
			currDist = dist;
		}

		return sweepHit.distance < FLT_MAX;
	}

	void PhysXPrimitiveSVVO::visualize(const physx::PxGeometry& /*geom*/, physx::PxRenderOutput& render, const physx::PxTransform& transform, const physx::PxBounds3& /*bound*/) const
	{
		//PxVec3 extents = voxelSize() * 0.5f;

		//render << transform;

		//for (int x = 0; x < dimX(); ++x)
		//	for (int y = 0; y < dimY(); ++y)
		//		for (int z = 0; z < dimZ(); ++z)
		//			if (voxel(x, y, z))
		//			{
		//				if (voxel(x + 1, y, z) &&
		//					voxel(x - 1, y, z) &&
		//					voxel(x, y + 1, z) &&
		//					voxel(x, y - 1, z) &&
		//					voxel(x, y, z + 1) &&
		//					voxel(x, y, z - 1))
		//					continue;

		//				PxVec3 pos = voxelPos(x, y, z);

		//				PxBounds3 bounds(pos - extents, pos + extents);

		//				physx::PxDebugBox box(bounds, true);
		//				render << box;
		//			}
	}

}