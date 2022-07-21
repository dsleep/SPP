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
		OElement(const std::string& InName, SPPDirectory* InParent) : SPPObject(InName, InParent) { }

		Sphere _bounds;
		Vector3d _translation = { 0,0,0 };
		Vector3 _rotation = { 0, 0, 0 };
		Vector3 _scale = { 1.0f, 1.0f, 1.0f };
		OctreeLinkPtr _octreeLink = nullptr;

		class OScene* _scene = nullptr;
		class OElement* _parent = nullptr;
		std::vector<OElement*> _children;

		bool _bIsStatic = true;

	public:
		struct TransformArgs
		{
			Vector3d translation;
			Vector3 rotation;
			Vector3 scale;
		};

		void SetTransformArgs(const TransformArgs &InArgs)
		{
			_translation = InArgs.translation;
			_rotation = InArgs.rotation;
			_scale = InArgs.scale;
		}

		virtual Sphere& Bounds()
		{
			return _bounds;
		}

		Vector3d& GetPosition()
		{
			return _translation;
		}
		Vector3& GetRotation()
		{
			return _rotation;
		}
		Vector3& GetScale()
		{
			return _scale;
		}

		virtual bool IsInOctree() const
		{
			return _octreeLink != nullptr;
		}

		OElement* GetParent()
		{
			return _parent;
		}
		OElement* GetTopBeforeScene();
		OElement* GetTop();
		Matrix4x4 GenerateLocalToWorld(bool bSkipTopTranslation = false) const;

		virtual ~OElement() { }

		virtual void AddedToScene(class OScene* InScene);
		virtual void RemovedFromScene();

		virtual void UpdateTransform();

		virtual void AddChild(OElement* InChild);
		virtual void RemoveChild(OElement* InChild);
		virtual void RemoveFromParent();

		virtual bool Intersect_Ray(const Ray& InRay, IntersectionInfo &oInfo) const;

		const std::vector<OElement*> GetChildren() const
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
		OScene(const std::string& InName, SPPDirectory* InParent);

		std::unique_ptr<LooseOctree> _octree;

	public:
		virtual void AddChild(OElement* InChild) override;
		virtual void RemoveChild(OElement* InChild) override;

		LooseOctree* GetOctree()
		{
			return _octree.get();
		}

		virtual ~OScene() { }
	};

	SPP_SCENE_API uint32_t GetSceneVersion();
}

