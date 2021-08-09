// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "DX12Buffers.h"
#include "SPPMesh.h"

namespace SPP
{	
	D3D12Buffer::D3D12Buffer(std::shared_ptr< ArrayResource > InCpuData) : GPUBuffer(InCpuData)
	{
		if (InCpuData)
		{
			auto pd3dDevice = GGraphicsDevice->GetDevice();
			auto heapProp = CD3DX12_HEAP_PROPERTIES{ D3D12_HEAP_TYPE_DEFAULT };
			auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(GetDataSize());

			pd3dDevice->CreateCommittedResource(
				&heapProp,
				D3D12_HEAP_FLAG_NONE,
				&bufferDesc,
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				IID_PPV_ARGS(&_buffer));

			_currentState = D3D12_RESOURCE_STATE_COPY_DEST;
		}
		else
		{
			_currentState = D3D12_RESOURCE_STATE_COMMON;
		}
	}

	D3D12Buffer::~D3D12Buffer()
	{

	}

	void D3D12Buffer::UpdateDirtyRegion(uint32_t Idx, uint32_t Count)
	{
		SE_ASSERT(_cpuLink);

		auto pUplCmdList = GGraphicsDevice->GetCommandList();
		auto currentFrame = GGraphicsDevice->GetFrameCount();

		if(_currentState != D3D12_RESOURCE_STATE_COPY_DEST)
		{
			auto resTrans = CD3DX12_RESOURCE_BARRIER::Transition(_buffer.Get(), _currentState, D3D12_RESOURCE_STATE_COPY_DEST);
			pUplCmdList->ResourceBarrier(1, &resTrans);
			_currentState = D3D12_RESOURCE_STATE_COPY_DEST;
		}

		uint32_t CopySize = _cpuLink->GetPerElementSize() * Count;
		auto perFrameSratchMem = GGraphicsDevice->GetPerDrawScratchMemory();
		auto memoryDirtyRegion = perFrameSratchMem->GetWritable(CopySize, currentFrame);

		// actually copy it over from the buff
		{			
			std::memcpy(memoryDirtyRegion.cpuAddr, GetData() + _cpuLink->GetPerElementSize() * Idx, CopySize);
		}

		pUplCmdList->CopyBufferRegion(_buffer.Get(),
			_cpuLink->GetPerElementSize() * Idx,
			memoryDirtyRegion.orgResource.Get(),
			memoryDirtyRegion.offsetOrgResource,
			CopySize);

		{
			auto newState = (_type == GPUBufferType::Index) ? D3D12_RESOURCE_STATE_INDEX_BUFFER : D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
			auto resTrans = CD3DX12_RESOURCE_BARRIER::Transition(_buffer.Get(), _currentState, newState);
			pUplCmdList->ResourceBarrier(1, &resTrans);
			_currentState = newState;
		}
	}

	void D3D12Buffer::UploadToGpu()
	{
		if (_buffer)
		{
			auto pd3dDevice = GGraphicsDevice->GetDevice();
			auto pUplCmdList = GGraphicsDevice->GetUploadCommandList();
			auto currentFrame = GGraphicsDevice->GetFrameCount();

			if (_currentState != D3D12_RESOURCE_STATE_COPY_DEST)
			{
				auto resTrans = CD3DX12_RESOURCE_BARRIER::Transition(_buffer.Get(), _currentState, D3D12_RESOURCE_STATE_COPY_DEST);
				pUplCmdList->ResourceBarrier(1, &resTrans);
				_currentState = D3D12_RESOURCE_STATE_COPY_DEST;
			}

#if 1
			auto perFrameSratchMem = GGraphicsDevice->GetPerDrawScratchMemory();
			auto memChunk = perFrameSratchMem->Write(GetData(), GetDataSize(), currentFrame);
			pUplCmdList->CopyBufferRegion(_buffer.Get(), 0, memChunk.orgResource.Get(), memChunk.offsetOrgResource, GetDataSize());
#else	

			auto uploadHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
			auto uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(GetDataSize());
			ThrowIfFailed(pd3dDevice->CreateCommittedResource(
				&uploadHeap,
				D3D12_HEAP_FLAG_NONE,
				&uploadDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&_heapUpload)));

			{
				uint8_t* heapmemory = nullptr;
				_heapUpload->Map(0, nullptr, reinterpret_cast<void**>(&heapmemory));
				std::memcpy(heapmemory, GetData(), GetDataSize());
				_heapUpload->Unmap(0, nullptr);
			}

			pUplCmdList->CopyResource(_buffer.Get(), _heapUpload.Get());
#endif
			
			auto newState = (_type == GPUBufferType::Index) ? D3D12_RESOURCE_STATE_INDEX_BUFFER : D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
			auto resTrans = CD3DX12_RESOURCE_BARRIER::Transition(_buffer.Get(), _currentState, newState);
			pUplCmdList->ResourceBarrier(1, &resTrans);
			_currentState = newState;
		}
	}


	D3D12IndexBuffer::D3D12IndexBuffer(std::shared_ptr< ArrayResource > InCpuData) : D3D12Buffer(InCpuData) {}
	D3D12IndexBuffer::~D3D12IndexBuffer() {}

	void D3D12IndexBuffer::UploadToGpu()
	{
		D3D12Buffer::UploadToGpu();
		ConfigureView();
	}

	D3D12_INDEX_BUFFER_VIEW* D3D12IndexBuffer::GetView()
	{
		return &_view;
	}

	uint32_t D3D12IndexBuffer::GetCachedElementCount() const
	{
		return _numElements;
	}

	void D3D12IndexBuffer::ConfigureView()
	{
		_numElements = GetElementCount();
		_view.BufferLocation = _buffer->GetGPUVirtualAddress();
		_view.SizeInBytes = GetDataSize();
		_view.Format = DXGI_FORMAT_R32_UINT;
	}

	D3D12VertexBuffer::D3D12VertexBuffer(std::shared_ptr< ArrayResource > InCpuData) : D3D12Buffer(InCpuData) {}

	D3D12VertexBuffer::~D3D12VertexBuffer() {}

	void D3D12VertexBuffer::UploadToGpu()
	{
		D3D12Buffer::UploadToGpu();
		ConfigureView();
	}

	D3D12_VERTEX_BUFFER_VIEW* D3D12VertexBuffer::GetView()
	{
		return &_view;
	}

	void D3D12VertexBuffer::ConfigureView()
	{
		// Initialize the vertex buffer view.
		_view.BufferLocation = _buffer->GetGPUVirtualAddress();
		_view.StrideInBytes = GetPerElementSize();
		_view.SizeInBytes = GetDataSize();
	}


	DXGI_FORMAT D3D12InputLayout::LayouttoDXGIFormat(const InputLayoutElementType& InType)
	{
		switch (InType)
		{
		case InputLayoutElementType::Float3:
			return DXGI_FORMAT_R32G32B32_FLOAT;
			break;
		case InputLayoutElementType::Float2:
			return DXGI_FORMAT_R32G32_FLOAT;
			break;
		}
		return DXGI_FORMAT_UNKNOWN;
	}

	size_t D3D12InputLayout::GetCount()
	{
		return _layout.size();
	}

	D3D12_INPUT_ELEMENT_DESC* D3D12InputLayout::GetData()
	{
		return  _layout.data();
	}

	void D3D12InputLayout::UploadToGpu()
	{

	}

	void D3D12InputLayout::InitializeLayout(const std::vector< InputLayoutElement>& eleList)
	{
		_layout.clear();
		_layout.reserve(eleList.size());
		_storedList = eleList;
		for (auto& curele : _storedList)
		{
			_layout.push_back({ curele.Name.c_str(),  0, LayouttoDXGIFormat(curele.Type), 0, curele.Offset,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 });
		}
	}

	std::shared_ptr< GPUBuffer > CreateStaticBuffer(GPUBufferType InType, std::shared_ptr< ArrayResource > InCpuData)
	{
		switch (InType)
		{
		case GPUBufferType::Generic:
			return std::make_unique<D3D12Buffer>(InCpuData);
			break;
		case GPUBufferType::Index:
			return std::make_unique<D3D12IndexBuffer>(InCpuData);
			break;
		case GPUBufferType::Vertex:
			return std::make_unique<D3D12VertexBuffer>(InCpuData);
			break;
		case GPUBufferType::Global:
			return std::make_unique<D3D12GlobalBuffer>(InCpuData);
			break;
		}

		return nullptr;
	}

	#define MAX_MESH_ELEMENTS 1024

	class TextureGlobalSRV
	{

	};

	struct MeshInfo
	{
		uint32_t IndexBytes;
		uint32_t MeshletCount;
	};

	class MeshElementGlobalState
	{
	private:
		std::vector< std::shared_ptr < MeshElement > > _elements;
		std::vector< uint32_t > _freeIndices;
		std::vector< uint32_t > _pendingIndices;

		D3D12SimpleDescriptorBlock _meshInfoDescriptorTable;

		D3D12SimpleDescriptorBlock _verticesDescriptorTable;
		D3D12SimpleDescriptorBlock _meshletsDescriptorTable;
		D3D12SimpleDescriptorBlock _uniqueIndicesDescriptorTable;
		D3D12SimpleDescriptorBlock _primitiveIndicesDescriptorTable;


		std::shared_ptr< ArrayResource > MeshInfosArray;
		std::shared_ptr< GPUBuffer > MeshInfoResource;		

	public:
		void Initialize()
		{
			auto pd3dDevice = GGraphicsDevice->GetDevice();
			//meshes
			_meshInfoDescriptorTable = GGraphicsDevice->GetPresetDescriptorHeap(HDT_MeshInfos);

			MeshInfosArray = std::make_shared< ArrayResource >();
			auto pMeshInfos = MeshInfosArray->InitializeFromType<MeshInfo>(MAX_MESH_ELEMENTS);
			memset(pMeshInfos, 0, MeshInfosArray->GetTotalSize());
			MeshInfoResource = CreateStaticBuffer(GPUBufferType::Generic, MeshInfosArray);

			//their data
			_verticesDescriptorTable = GGraphicsDevice->GetPresetDescriptorHeap(HDT_MeshletVertices);
			_meshletsDescriptorTable = GGraphicsDevice->GetPresetDescriptorHeap(HDT_MeshletResource);
			_uniqueIndicesDescriptorTable = GGraphicsDevice->GetPresetDescriptorHeap(HDT_UniqueVertexIndices);
			_primitiveIndicesDescriptorTable = GGraphicsDevice->GetPresetDescriptorHeap(HDT_PrimitiveIndices);

			{
				D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
				srvDesc.Format = DXGI_FORMAT_UNKNOWN;
				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
				srvDesc.Buffer.FirstElement = 0;
				srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				auto currentTableElement = _meshInfoDescriptorTable[0];
				srvDesc.Buffer.StructureByteStride = MeshInfosArray->GetPerElementSize(); // We assume we'll only use the first vertex buffer
				srvDesc.Buffer.NumElements = MeshInfosArray->GetElementCount();
				pd3dDevice->CreateShaderResourceView(MeshInfoResource->GetAs<D3D12Buffer>().GetResource(), &srvDesc, currentTableElement.cpuHandle);
			}
		}	

		bool RegisterMeshElement(std::shared_ptr<MeshElement> InMeshElement)
		{
			auto pd3dDevice = GGraphicsDevice->GetDevice();

			//TODO _freeIndices

			auto currentIdx = _elements.size();

			InMeshElement->MeshIndex = currentIdx;
			_elements.push_back(InMeshElement);

			_pendingIndices.push_back(currentIdx);

			return true;
		}

		void UpdateGPUElements()
		{
			auto pd3dDevice = GGraphicsDevice->GetDevice();

			for (auto& resourceIdx : _pendingIndices)
			{
				std::shared_ptr<MeshElement> CurMeshElement = _elements[resourceIdx];

				SE_ASSERT(CurMeshElement->MeshIndex == resourceIdx);

				auto Meshinfos = MeshInfosArray->GetSpan< MeshInfo>();
				Meshinfos[resourceIdx].IndexBytes = 4;
				Meshinfos[resourceIdx].MeshletCount = CurMeshElement->MeshletSubsets.front().Count;
				MeshInfoResource->UpdateDirtyRegion(resourceIdx, 1);

				D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
				srvDesc.Format = DXGI_FORMAT_UNKNOWN;
				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
				srvDesc.Buffer.FirstElement = 0;
				srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

				{
					auto currentTableElement = _verticesDescriptorTable[resourceIdx];
					srvDesc.Buffer.StructureByteStride = CurMeshElement->VertexResource->GetPerElementSize(); // We assume we'll only use the first vertex buffer
					srvDesc.Buffer.NumElements = CurMeshElement->VertexResource->GetElementCount();
					pd3dDevice->CreateShaderResourceView(CurMeshElement->VertexResource->GetAs<D3D12Buffer>().GetResource(), &srvDesc, currentTableElement.cpuHandle);
				}
				{
					auto currentTableElement = _meshletsDescriptorTable[resourceIdx];
					srvDesc.Buffer.StructureByteStride = CurMeshElement->MeshletResource->GetPerElementSize(); // We assume we'll only use the first vertex buffer
					srvDesc.Buffer.NumElements = CurMeshElement->MeshletResource->GetElementCount();
					pd3dDevice->CreateShaderResourceView(CurMeshElement->MeshletResource->GetAs<D3D12Buffer>().GetResource(), &srvDesc, currentTableElement.cpuHandle);
				}
				{
					auto currentTableElement = _uniqueIndicesDescriptorTable[resourceIdx];					
					srvDesc.Buffer.StructureByteStride = CurMeshElement->UniqueVertexIndexResource->GetPerElementSize(); // We assume we'll only use the first vertex buffer
					srvDesc.Buffer.NumElements = CurMeshElement->UniqueVertexIndexResource->GetElementCount();
					pd3dDevice->CreateShaderResourceView(CurMeshElement->UniqueVertexIndexResource->GetAs<D3D12Buffer>().GetResource(), &srvDesc, currentTableElement.cpuHandle);
				}
				{
					auto currentTableElement = _primitiveIndicesDescriptorTable[resourceIdx];					
					srvDesc.Buffer.StructureByteStride = CurMeshElement->PrimitiveIndexResource->GetPerElementSize(); // We assume we'll only use the first vertex buffer
					srvDesc.Buffer.NumElements = CurMeshElement->PrimitiveIndexResource->GetElementCount();
					pd3dDevice->CreateShaderResourceView(CurMeshElement->PrimitiveIndexResource->GetAs<D3D12Buffer>().GetResource(), &srvDesc, currentTableElement.cpuHandle);
				}
			}
			_pendingIndices.clear();			
		}

		bool UnregisterMeshElement(std::shared_ptr<MeshElement> InMeshElement)
		{
			auto pd3dDevice = GGraphicsDevice->GetDevice();

			////TODO _freeIndices

			//auto currentIdx = _elements.size();
			//_elements.push_back(InMeshElement);

			return true;
		}

		bool ReadyMeshElement(std::shared_ptr<MeshElement> InMeshElement)
		{
			auto pd3dDevice = GGraphicsDevice->GetDevice();
			auto cmdList = GGraphicsDevice->GetCommandList();

			cmdList->SetGraphicsRootDescriptorTable(7, _meshInfoDescriptorTable.gpuHandle);

			cmdList->SetGraphicsRootDescriptorTable(8, _verticesDescriptorTable.gpuHandle);
			cmdList->SetGraphicsRootDescriptorTable(9, _meshletsDescriptorTable.gpuHandle);
			cmdList->SetGraphicsRootDescriptorTable(10, _uniqueIndicesDescriptorTable.gpuHandle);
			cmdList->SetGraphicsRootDescriptorTable(11, _primitiveIndicesDescriptorTable.gpuHandle);

			return true;
		}
	};

	std::unique_ptr<MeshElementGlobalState> GMeshState;

	void UpdateGPUMeshes()
	{
		if (GMeshState)
		{
			GMeshState->UpdateGPUElements();
		}		
	}

	bool ReadyMeshElement(std::shared_ptr<MeshElement> InMeshElement)
	{
		if (!GMeshState)
		{
			GMeshState = std::make_unique< MeshElementGlobalState>();
			GMeshState->Initialize();
		}
		return GMeshState->ReadyMeshElement(InMeshElement);
	}

	bool RegisterMeshElement(std::shared_ptr<MeshElement> InMeshElement)
	{
		if (!GMeshState)
		{
			GMeshState = std::make_unique< MeshElementGlobalState>();
			GMeshState->Initialize();
		}
		return GMeshState->RegisterMeshElement(InMeshElement);
	}

	bool UnregisterMeshElement(std::shared_ptr<MeshElement> InMeshElement)
	{
		if (!GMeshState)
		{
			GMeshState = std::make_unique< MeshElementGlobalState>();
			GMeshState->Initialize();
		}
		return GMeshState->UnregisterMeshElement(InMeshElement);
	}

	std::shared_ptr< GPUInputLayout > CreateInputLayout()
	{
		return std::make_unique<D3D12InputLayout>();		
	}

	class GlobalReservedBuffer
	{
	private:
		ComPtr<ID3D12Resource> _reservedResource;

	public:

		ID3D12Resource* GetResource() const
		{
			return _reservedResource.Get();
		}

		GlobalReservedBuffer()
		{
			auto pd3dDevice = GGraphicsDevice->GetDevice();
			auto pUplCmdList = GGraphicsDevice->GetUploadCommandList();

			D3D12_RESOURCE_DESC reservedBufferDesc = {};
			reservedBufferDesc.MipLevels = 1;
			reservedBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
			reservedBufferDesc.Width = 500 * 1024 * 1024;// std::numeric_limits<uint32_t>::max()
			reservedBufferDesc.Height = 1;
			reservedBufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
			reservedBufferDesc.DepthOrArraySize = 1;
			reservedBufferDesc.SampleDesc.Count = 1;
			reservedBufferDesc.SampleDesc.Quality = 0;
			reservedBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			reservedBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

			// set it up as 
			ThrowIfFailed(pd3dDevice->CreateReservedResource(
				&reservedBufferDesc,
				D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
				nullptr,
				IID_PPV_ARGS(&_reservedResource)));

			D3D12_PACKED_MIP_INFO m_packedMipInfo = {};
			UINT numTiles = 0;
			D3D12_TILE_SHAPE tileShape = {};
			UINT subresourceCount = reservedBufferDesc.MipLevels;
			std::vector<D3D12_SUBRESOURCE_TILING> tilings(subresourceCount);
			pd3dDevice->GetResourceTiling(_reservedResource.Get(), &numTiles, &m_packedMipInfo, &tileShape, &subresourceCount, 0, &tilings[0]);
		}


		void PushBufferTile(D3D12GlobalBuffer &InBuffer)
		{
			auto pd3dDevice = GGraphicsDevice->GetDevice();
			auto pUplCmdList = GGraphicsDevice->GetUploadCommandList();
			auto pCommandQueue = GGraphicsDevice->GetCommandQueue();

			pUplCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_reservedResource.Get(), D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_STATE_COPY_DEST));
			
			//
			UINT updatedRegions = 1;

			std::vector<D3D12_TILED_RESOURCE_COORDINATE> startCoordinates;
			startCoordinates.push_back(D3D12_TILED_RESOURCE_COORDINATE{ 0,0,0,0 });

			std::vector<D3D12_TILE_REGION_SIZE> regionSizes;

			auto TileCount = InBuffer.TileCount();
			D3D12_TILE_REGION_SIZE regionSize;
			regionSize.Width = TileCount;
			regionSize.Height = 1;
			regionSize.Depth = 1;
			regionSize.NumTiles = TileCount;
			regionSize.UseBox = false;
			regionSizes.push_back(regionSize);

			std::vector<D3D12_TILE_RANGE_FLAGS> rangeFlags;
			rangeFlags.push_back(D3D12_TILE_RANGE_FLAG_NONE);

			std::vector<UINT> heapRangeStartOffsets;
			heapRangeStartOffsets.push_back(0);

			std::vector<UINT> rangeTileCounts;
			rangeTileCounts.push_back(regionSizes[0].NumTiles);

			pCommandQueue->UpdateTileMappings(
				_reservedResource.Get(),
				updatedRegions,
				&startCoordinates[0],
				&regionSizes[0],
				InBuffer.GetHeap(),
				updatedRegions,
				&rangeFlags[0],
				&heapRangeStartOffsets[0],
				&rangeTileCounts[0],
				D3D12_TILE_MAPPING_FLAG_NONE
			);


#if 1
			{
				uint8_t* heapmemory = nullptr;
				InBuffer.GetUploadHeap()->Map(0, nullptr, reinterpret_cast<void**>(&heapmemory));
				std::memcpy(heapmemory, InBuffer. GetData(), InBuffer.GetDataSize());
				InBuffer.GetUploadHeap()->Unmap(0, nullptr);
			}

			pUplCmdList->CopyResource(_reservedResource.Get(), InBuffer.GetUploadHeap());
#else
			D3D12_SUBRESOURCE_DATA vertexData = {};
			vertexData.pData = InBuffer.GetData();
			vertexData.RowPitch = InBuffer.GetDataSize();
			vertexData.SlicePitch = vertexData.RowPitch;
			UpdateSubresources<1>(pUplCmdList, _reservedResource.Get(), InBuffer.GetUploadHeap(), 0, 0, 1, &vertexData);
#endif

			pUplCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_reservedResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));
		}
	};

	//
	D3D12GlobalBuffer::D3D12GlobalBuffer(std::shared_ptr< ArrayResource > InCpuData) : GPUBuffer(InCpuData) {}
	D3D12GlobalBuffer::~D3D12GlobalBuffer() {}	

	std::unique_ptr< GlobalReservedBuffer> GBuffer;

	void D3D12GlobalBuffer::UploadToGpu()
	{
		auto pd3dDevice = GGraphicsDevice->GetDevice();
		auto pUplCmdList = GGraphicsDevice->GetUploadCommandList();

		auto uploadHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		auto uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(GetDataSize());
		ThrowIfFailed(pd3dDevice->CreateCommittedResource(
			&uploadHeap,
			D3D12_HEAP_FLAG_NONE,
			&uploadDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&_heapUpload)));

		_numberOfTiles = DivRoundUp<uint32_t>(GetDataSize(), D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
		auto HeapSize = _numberOfTiles * D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
		
		CD3DX12_HEAP_DESC heapDesc(HeapSize, D3D12_HEAP_TYPE_DEFAULT, 0, D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS);
		ThrowIfFailed(pd3dDevice->CreateHeap(&heapDesc, IID_PPV_ARGS(&_heap)));		

		if (!GBuffer)
		{
			GBuffer = std::make_unique< GlobalReservedBuffer>();
		}
		GBuffer->PushBufferTile(*this);
	}


	ID3D12Resource* D3D12GlobalBuffer::GetResource() const
	{
		return GBuffer->GetResource();
	}
}