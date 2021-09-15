// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPObject.h"
#include "SPPMath.h"
#include "SPPOctree.h"

#if _WIN32 && !defined(SPP_SCENE_STATIC)

	#ifdef SPP_SCENEO_EXPORT
		#define SPP_SCENE_API __declspec(dllexport)
	#else
		#define SPP_SCENE_API __declspec(dllimport)
	#endif

	#else

		#define SPP_SCENE_API 

#endif

namespace SPP
{
	struct IntersectionInfo
	{
		Vector3 location;
		Vector3 normal;
		std::string hitName;
	};

	class SPP_SCENE_API OElement : public SPPObject, public IOctreeElement
	{
		RTTR_ENABLE(SPPObject);
		RTTR_REGISTRATION_FRIEND

	protected:
		OElement(const MetaPath& InPath) : SPPObject(InPath) { }

		class OElement* _parent = nullptr;
		std::unordered_set<OElement*> _children;

		Sphere _bounds;
		Vector3d _translation = { 0,0,0 };
		Vector3 _rotation = { 0, 0, 0 };
		float _scale = 1.0f;
		OctreeLinkPtr _octreeLink = nullptr;

	public:
		virtual Sphere& Bounds()
		{
			return _bounds;
		}

		Vector3d& GetPosition()
		{
			return _translation;
		}

		float& GetScale()
		{
			return _scale;
		}

		virtual bool IsInOctree() const
		{
			return _octreeLink != nullptr;
		}

		virtual ~OElement() { }

		virtual void AddedToScene(class OScene* InScene) {}
		virtual void RemovedFromScene(class OScene* InScene) {};

		virtual void AddChild(OElement* InChild);
		virtual void RemoveChild(OElement* InChild);
		virtual void RemoveFromParent();

		virtual bool Intersect_Ray(const Ray& InRay, IntersectionInfo &oInfo) const;

		const std::unordered_set<OElement*> GetChildren() const
		{
			return _children;
		}

		//
		virtual Spherei GetBounds() const
		{
			//hmm
			return Convert(_bounds);
		}
		virtual void SetOctreeLink(OctreeLinkPtr InOctree)
		{
			_octreeLink = InOctree;
		}
		virtual const OctreeLinkPtr GetOctreeLink()
		{
			return _octreeLink;
		}
	};

	class SPP_SCENE_API OScene : public OElement
	{
		RTTR_ENABLE(OElement);
		RTTR_REGISTRATION_FRIEND

	protected:
		OScene(const MetaPath& InPath);

		std::unique_ptr<LooseOctree> _octree;

	public:
		virtual void AddChild(OElement* InChild) override;
		virtual void RemoveChild(OElement* InChild) override;

		virtual ~OScene() { }
	};

	SPP_SCENE_API uint32_t GetSceneVersion();
}

