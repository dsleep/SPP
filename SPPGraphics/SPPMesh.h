// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPGraphics.h"
#include "SPPGPUResources.h"
#include "SPPMath.h"
#include "SPPSceneRendering.h"
#include <vector>
#include <cstddef>
#include <unordered_set>

namespace SPP
{
	struct Subset
	{
		uint32_t Offset;
		uint32_t Count;
	};

	struct MeshNode
	{
		AABB Bounds;
		uint32_t TriCount = 0;
		uint32_t ChildTriCount = 0;
		std::tuple<uint32_t, uint32_t> ChildrenRange;
		std::tuple<uint32_t, uint32_t> MeshletRange;
	};

	struct MeshVertex
	{
		Vector3 position;
		Vector3 normal;
		Vector3 tangent;
		Vector3 bitangent;
		Vector2 texcoord;
		Color4 color;
	};

	struct AdjTri
	{
		uint32_t connectedTris[12] =
		{
			uint32_t(-1),uint32_t(-1),uint32_t(-1),uint32_t(-1),uint32_t(-1),uint32_t(-1),
			uint32_t(-1),uint32_t(-1),uint32_t(-1),uint32_t(-1),uint32_t(-1),uint32_t(-1)
		};
		uint8_t triIdx = 0;
	};

	struct EdgeKey
	{
		union
		{
			struct
			{
				uint32_t edgeSmall;
				uint32_t edgeBig;
			};
			uint64_t edgePacked;
		};

		EdgeKey()
		{
			edgePacked = 0;
		}

		EdgeKey(uint32_t Edge0, uint32_t Edge1)
		{
			if (Edge0 < Edge1)
			{
				edgeSmall = Edge0;
				edgeBig = Edge1;
			}
			else
			{
				edgeSmall = Edge1;
				edgeBig = Edge0;
			}
		}

		size_t Hash() const
		{
			return edgePacked;
		}

		bool operator< (const EdgeKey& cmpTo) const
		{
			return edgePacked < cmpTo.edgePacked;
		}
		bool operator==(const EdgeKey& cmpTo) const
		{
			return edgePacked == cmpTo.edgePacked;
		}
		struct HASH
		{
			size_t operator()(const EdgeKey& InValue) const
			{
				return InValue.Hash();
			}
		};
	};

	struct MeshTranslator
	{
		std::unordered_set<uint32_t> ProtectedVertices;

		virtual Vector3& GetVertexPosition(uint32_t InIdx) const = 0;
		virtual Vector3& GetVertexNormal(uint32_t InIdx) const = 0;
		virtual Vector2& GetVertexUV(uint32_t InIdx) const = 0;

		virtual uint32_t GetVertexCount() const = 0;
		virtual void ResizeVertices(uint32_t NewCount) = 0;
		//virtual uint32_t GetVertexSpan() const = 0;

		virtual uint32_t& GetIndex(uint32_t InIdx) const = 0;
		virtual uint32_t GetIndexCount() const = 0;
		virtual void ResizeIndices(uint32_t NewCount) = 0;

		virtual void SetProtectedVertex(uint32_t InIdx) 
		{
			ProtectedVertices.insert(InIdx);
		}
		virtual bool IsVertexProtected(uint32_t InIdx) const
		{
			return ProtectedVertices.count(InIdx);
		}

		virtual std::shared_ptr< MeshTranslator > CreateCopy(bool bShareVertices, bool bShareIndices) = 0;

		virtual void Load(BinaryBlobSerializer &InSerializer) = 0;
		virtual void Store(BinaryBlobSerializer& InSerializer) const = 0;

		AABB GetBounds() const
		{
			AABB oBox;
			auto IndexCount = GetIndexCount();
			for (uint32_t Iter = 0; Iter < IndexCount; Iter++)
			{
				oBox += GetVertexPosition(GetIndex(Iter));
			}
			return oBox;
		}
	};

	template<>
	inline BinarySerializer& operator<< <MeshVertex> (BinarySerializer& Storage, const MeshVertex& Value)
	{
		Storage << Value.position;
		Storage << Value.normal;
		Storage << Value.tangent;
		Storage << Value.bitangent;
		Storage << Value.texcoord;
		return Storage;
	}

	template<>
	inline BinarySerializer& operator<< <Subset> (BinarySerializer& Storage, const Subset& Value)
	{
		Storage << Value.Offset;
		Storage << Value.Count;
		return Storage;
	}

	template<>
	inline BinarySerializer& operator>> <Subset> (BinarySerializer& Storage, Subset& Value)
	{
		Storage >> Value.Offset;
		Storage >> Value.Count;
		return Storage;
	}

	struct LoadedMeshes
	{
		struct MeshLayer
		{
			std::shared_ptr< ArrayResource > VertexResource;
			std::shared_ptr< ArrayResource > IndexResource;
			int32_t MaterialID = 0;
		};

		std::vector< MeshLayer > Layers;
	};

	template<>
	inline BinarySerializer& operator<< <ArrayResource> (BinarySerializer& Storage, const ArrayResource& Value)
	{
		Storage << Value.GetPerElementSize();
		Storage << Value.GetElementCount();
		Storage << Value.GetRawByteArray();

		return Storage;
	}
	template<>
	inline BinarySerializer& operator>> <ArrayResource> (BinarySerializer& Storage, ArrayResource& Value)
	{
		size_t perEleSize = 0, eleCount = 0;

		Storage >> perEleSize;
		Storage >> eleCount;
		Value = ArrayResource(perEleSize, eleCount);
		Storage >> Value.GetRawByteArray();

		return Storage;
	}

	struct MeshVertexTranslator : public MeshTranslator
	{
		std::shared_ptr<ArrayResource> ourVertices;
		std::shared_ptr<ArrayResource> ourIndices;

		TSpan<MeshVertex> ourVertSpan;
		TSpan<uint32_t> ourIndexSpan;

		MeshVertexTranslator(std::shared_ptr<ArrayResource> InVerts, std::shared_ptr<ArrayResource> InIndices) : ourVertices(InVerts), ourIndices(InIndices)
		{			
			ourVertSpan = ourVertices->GetSpan<MeshVertex>();
			ourIndexSpan = ourIndices->GetSpan<uint32_t>();
		}		

		virtual void Store(BinaryBlobSerializer& InSerializer) const override
		{
			InSerializer << *ourVertices;
			InSerializer << *ourIndices;
		}

		virtual void Load(BinaryBlobSerializer& InSerializer) override
		{
			InSerializer >> *ourVertices;
			InSerializer >> *ourIndices;
			ourVertSpan = ourVertices->GetSpan<MeshVertex>();
			ourIndexSpan = ourIndices->GetSpan<uint32_t>();
		}

		virtual std::shared_ptr< MeshTranslator > CreateCopy(bool bShareVertices, bool bShareIndices)
		{
			return std::make_shared< MeshVertexTranslator >(
				bShareVertices ? ourVertices : std::make_shared< ArrayResource > (ourVertices->GetPerElementSize(),0),
				bShareIndices ? ourIndices : std::make_shared< ArrayResource >(ourIndices->GetPerElementSize(), 0)
				);
		}

		virtual Vector3& GetVertexPosition(uint32_t InIdx) const override
		{
			return ourVertSpan[InIdx].position;
		}
		virtual Vector3& GetVertexNormal(uint32_t InIdx) const override
		{
			return ourVertSpan[InIdx].normal;
		}
		virtual Vector2& GetVertexUV(uint32_t InIdx) const override
		{
			return ourVertSpan[InIdx].texcoord;
		}

		virtual uint32_t GetVertexCount() const override
		{
			return (uint32_t) ourVertSpan.GetCount();
		}
		virtual void ResizeVertices(uint32_t NewCount) override
		{
			ourVertSpan = ourVertices->InitializeFromType< MeshVertex>(NewCount);
		}

		virtual uint32_t& GetIndex(uint32_t InIdx) const override
		{
			return ourIndexSpan[InIdx];
		}
		virtual uint32_t GetIndexCount() const override
		{
			return (uint32_t) ourIndexSpan.GetCount();
		}
		virtual void ResizeIndices(uint32_t NewCount) override
		{
			ourIndexSpan = ourIndices->InitializeFromType<uint32_t>(NewCount);
		}
	};

	


	inline const Vector3& GetPosition(const MeshVertex& InVertex)
	{
		return InVertex.position;
	}

	inline Vector3& GetPosition(MeshVertex& InVertex)
	{
		return InVertex.position;
	}

	inline Vector3& GetPosition(Vector3& InVertex)
	{
		return InVertex;
	}

	inline void SetPosition(MeshVertex& InMeshVertex, const Vector3& InVertex)
	{
		InMeshVertex.position = InVertex;
	}
	inline void SetPosition(Vector3& InMeshVertex, const Vector3& InVertex)
	{
		InMeshVertex = InVertex;
	}

	struct FullscreenVertex
	{
		Vector2 position;
	};

	struct DebugVertex
	{
		Vector3 position;
		Vector3 color;
	};
	
	class SPP_GRAPHICS_API MeshLayout
	{
	private:
		std::unique_ptr<GPUInputLayout> _meshVertexLayout;
	public:
		MeshLayout()
		{
			_meshVertexLayout->InitializeLayout({
				{ "POSITION",  InputLayoutElementType::Float3, offsetof(MeshVertex,position) },
				{ "NORMAL",    InputLayoutElementType::Float3, offsetof(MeshVertex,normal) },
				{ "TANGENT",   InputLayoutElementType::Float3, offsetof(MeshVertex,tangent) },
				{ "BITANGENT", InputLayoutElementType::Float3, offsetof(MeshVertex,bitangent) },
				{ "TEXCOORD",  InputLayoutElementType::Float2, offsetof(MeshVertex,texcoord) } });
		}
	};

	struct SPP_GRAPHICS_API MeshMaterial
	{
		std::vector< std::shared_ptr<GPUTexture> > textureArray;

		std::shared_ptr<GPUShader> vertexShader;
		std::shared_ptr<GPUShader> pixelShader;
		std::shared_ptr<GPUShader> domainShader;
		std::shared_ptr<GPUShader> hullShader;
		std::shared_ptr<GPUShader> meshShader;
		std::shared_ptr<GPUShader> amplificationShader;

		std::shared_ptr<GPUInputLayout> layout;

		EBlendState blendState = EBlendState::Disabled;
		ERasterizerState rasterizerState = ERasterizerState::BackFaceCull;
		EDepthState depthState = EDepthState::Enabled;

		void SetTextureUnit(uint8_t Idx, std::shared_ptr<GPUTexture> InTexture)
		{
			if (Idx >= textureArray.size())
			{
				textureArray.resize((size_t)Idx + 1);
			}

			textureArray[Idx] = InTexture;
		}
	};
	
	//ugly update eventuallly
	enum class MeshTypes
	{
		Simple,
		Meshlets
	};

	struct SPP_GRAPHICS_API MeshElement
	{
		EDrawingTopology topology = EDrawingTopology::TriangleList;

		AABB Bounds;

		int32_t MeshIndex = -1;
		//
		std::shared_ptr< GPUBuffer > VertexResource;
		std::shared_ptr< GPUBuffer > IndexResource;
				
		std::shared_ptr< MeshMaterial > material;

		virtual MeshTypes GetType() const
		{
			return MeshTypes::Simple;
		}
		virtual ~MeshElement()
		{

		}
	};	

	struct SPP_GRAPHICS_API MeshletedElement : public MeshElement
	{
		// meshlet related
		std::vector<struct MeshNode> MeshletNodes;
		std::vector<struct Subset> MeshletSubsets;

		std::shared_ptr< GPUBuffer > MeshletResource;
		std::shared_ptr< GPUBuffer > UniqueVertexIndexResource;
		std::shared_ptr< GPUBuffer > PrimitiveIndexResource;
		std::shared_ptr< GPUBuffer > CullDataResource;

		virtual MeshTypes GetType() const
		{
			return MeshTypes::Meshlets;
		}

		virtual ~MeshletedElement()
		{

		}
	};

	class SPP_GRAPHICS_API Mesh 
	{
	protected:
		std::vector< std::shared_ptr<MeshElement> > _elements;
		AABB _bounds;

	public:
		bool LoadMesh(const AssetPath& FileName);

		std::vector< std::shared_ptr<MeshElement> > &GetMeshElements()
		{
			return _elements;
		}
	};

	class SPP_GRAPHICS_API RenderableMesh : public Renderable
	{
	protected:
		std::vector< std::shared_ptr<MeshElement> > _meshElements;

	public:
		void SetMeshData(const std::vector< std::shared_ptr<MeshElement> >& InMeshData)
		{
			_meshElements = InMeshData;
		}
	};

	SPP_GRAPHICS_API std::shared_ptr<RenderableMesh> CreateRenderableMesh(bool bIsStatic = false);
}