// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPCore.h"
#include "SPPArrayResource.h"
#include "SPPObject.h"
#include "SPPGraphics.h"
#include "SPPMath.h"
#include <vector>
#include <memory>
#include <string>

namespace DirectX
{
    struct DDS_HEADER;
}

namespace SPP
{
    enum class EShaderType
    {
        Pixel = 0,
        Vertex,
        Compute,
        Hull,
        Domain,
        Mesh,
        Amplification,
        NumValues
    };

    enum class EDrawingTopology
    {
        PointList = 0,
        LineList,
        TriangleList,
        PatchList_4ControlPoints,

        NumValues
    };

    enum class EBlendState
    {
        Disabled = 0,
        Additive,
        AlphaBlend,
        PreMultiplied,
        NoColorWrites,
        PreMultipliedRGB,

        NumValues
    };

    enum class ERasterizerState 
    {
        NoCull = 0,
        BackFaceCull,
        BackFaceCullNoZClip,
        FrontFaceCull,
        NoCullNoMS,
        Wireframe,

        NumValues
    };

    enum class EDepthState 
    {
        Disabled = 0,
        Enabled,
        Reversed,
        WritesEnabled,
        ReversedWritesEnabled,

        NumValues
    };

    enum class ESamplerState
    {
        Linear = 0,
        LinearClamp,
        LinearBorder,
        Point,
        Anisotropic,
        ShadowMap,
        ShadowMapPCF,
        ReversedShadowMap,
        ReversedShadowMapPCF,

        NumValues
    };

    enum class ECmdListMode
    {
        Graphics = 0,
        Copy,
        Compute,
        NumValues
    };

    struct ImageMeta
    {
        ImageMeta() = default;
        virtual ~ImageMeta() {}
    };
    
    
    struct DDSImageMeta : public ImageMeta
    {
        const DirectX::DDS_HEADER* header = nullptr;
        const uint8_t* bitData = nullptr;
        size_t bitSize = 0;

        DDSImageMeta() = default;
        virtual ~DDSImageMeta() {}
    };

    class SPP_GRAPHICS_API GPUResource 
    {
    protected:
        bool _gpuResident = false;

    public:
        GPUResource();
        virtual ~GPUResource();

        virtual const char* GetName() const = 0;

        bool IsGPUResident() const { return _gpuResident; }
        virtual void UploadToGpu() = 0;

        template<typename T>
        T& GetAs()
        {
            return *(T*)this;
        }
    };

    SPP_GRAPHICS_API void MakeResidentAllGPUResources();

    class SPP_GRAPHICS_API GPUShader : public GPUResource
    {
    protected:
        EShaderType _type;

    public:
        GPUShader(EShaderType InType) : _type(InType)
        {

        }

        virtual const char* GetName() const override
        {
            return "Shader";
        }

        virtual bool CompileShaderFromFile(const AssetPath& FileName, const char* EntryPoint = "main") = 0;
    };

    enum class TextureFormat
    {
        RGB_888,
        RGBA_8888,
        RGBA_BC7,
        DDS_UNKNOWN,
        RG_BC5,
        GRAY_BC4,
        RGB_BC1
    };

    class SPP_GRAPHICS_API GPUTexture : public GPUResource
    {
    protected:
        int32_t _width;
        int32_t _height;
        TextureFormat _format;
        std::shared_ptr< ArrayResource > _rawImgData;
        std::shared_ptr< ImageMeta > _metaInfo;

    public:

        GPUTexture(int32_t Width, int32_t Height, TextureFormat Format, std::shared_ptr< ArrayResource > RawData, std::shared_ptr< ImageMeta > InMetaInfo) :
            _width(Width), _height(Height), _format(Format), _rawImgData(RawData), _metaInfo(InMetaInfo)
        {
        }

        virtual const char* GetName() const override
        {
            return "Texture";
        }
    };

    enum class GPUBufferType
    {
        Generic,
        Index,
        Vertex,
        Global
    };

    class SPP_GRAPHICS_API GPUBuffer : public GPUResource
    {
    protected:
        GPUBufferType _type = GPUBufferType::Generic;
        size_t _size = 0;
        std::shared_ptr< ArrayResource > _cpuLink;

    public:
        GPUBuffer(std::shared_ptr< ArrayResource > InCpuData = nullptr) :_cpuLink(InCpuData) {}

        size_t GetDataSize() const
        {
            SE_ASSERT(_cpuLink);
            return _cpuLink->GetTotalSize();
        }
        uint8_t* GetData() 
        {
            SE_ASSERT(_cpuLink);
            return _cpuLink->GetElementData();
        }
        size_t GetPerElementSize() const
        {
            SE_ASSERT(_cpuLink);
            return _cpuLink->GetPerElementSize();
        }
        size_t GetElementCount() const
        {
            SE_ASSERT(_cpuLink);
            return _cpuLink->GetElementCount();
        }


        virtual void UpdateDirtyRegion(uint32_t Idx, uint32_t Count) { }

        virtual void SetLink(std::shared_ptr < ArrayResource> InLink)
        {
            _cpuLink = InLink;
        }

        virtual const char* GetName() const override
        {
            return "Buffer";
        }
    };

    enum class InputLayoutElementType
    {
        Float3,
        Float2,
        UInt8_3,
        UInt8_4,
    };

    struct InputLayoutElement
    {
        std::string Name;
        InputLayoutElementType Type;
        uint16_t Offset;
    };

    class SPP_GRAPHICS_API GPUInputLayout : public GPUResource
    {
    public:
        GPUInputLayout()
        {

        }

        virtual const char* GetName() const override
        {
            return "InputLayout";
        }

        virtual void InitializeLayout(const std::vector< InputLayoutElement>& eleList) = 0;
    };

    

    class SPP_GRAPHICS_API PipelineState
    {
    public:

        PipelineState()
        {

        }

    };

    class SPP_GRAPHICS_API GPURenderTarget : public GPUTexture
    {

    };

    class SPP_GRAPHICS_API GraphicsDevice
    {
    protected:

    public:
        virtual void Initialize(int32_t InitialWidth, int32_t InitialHeight, void* OSWindow) = 0;
        virtual void ResizeBuffers(int32_t NewWidth, int32_t NewHeight) = 0;
        
        virtual int32_t GetDeviceWidth() const = 0;
        virtual int32_t GetDeviceHeight() const = 0;

        virtual void BeginFrame() = 0;
        virtual void EndFrame() = 0;
        virtual void MoveToNextFrame() { };
    };

    class SPP_GRAPHICS_API GPUComputeDispatch
    {
    protected:        
        std::vector< std::shared_ptr< ArrayResource> > _constants;
        std::vector< std::shared_ptr<GPUTexture> > _textures;
        std::shared_ptr< GPUShader > _compute;

    public:
        GPUComputeDispatch(std::shared_ptr< GPUShader> InCS) : _compute(InCS) { }

        void SetTextures(const std::vector< std::shared_ptr<GPUTexture> > &InTextures)
        {
            _textures = InTextures;
        }

        void SetConstants(const std::vector< std::shared_ptr< ArrayResource> > &InConstants)
        {
            _constants = InConstants;
        }
        virtual void Dispatch(const Vector3i& ThreadGroupCounts) = 0;
    };

    SPP_GRAPHICS_API std::shared_ptr< GPUShader > CreateShader(EShaderType InType);
    SPP_GRAPHICS_API std::shared_ptr< GPUComputeDispatch > CreateComputeDispatch(std::shared_ptr< GPUShader> InCS);

    SPP_GRAPHICS_API std::shared_ptr< GPUBuffer > CreateStaticBuffer(GPUBufferType InType, std::shared_ptr< ArrayResource > InCpuData = nullptr);

    SPP_GRAPHICS_API bool RegisterMeshElement(std::shared_ptr<struct MeshElement> InMeshElement);
    SPP_GRAPHICS_API bool UnregisterMeshElement(std::shared_ptr<struct MeshElement> InMeshElement);

    SPP_GRAPHICS_API std::shared_ptr< GPUInputLayout > CreateInputLayout();
    SPP_GRAPHICS_API std::shared_ptr< GPUTexture > CreateTexture(int32_t Width, int32_t Height, TextureFormat Format, std::shared_ptr< ArrayResource > RawData = nullptr, std::shared_ptr< ImageMeta > InMetaInfo = nullptr);
    SPP_GRAPHICS_API std::shared_ptr< GPURenderTarget > CreateRenderTarget();
    SPP_GRAPHICS_API std::shared_ptr< GraphicsDevice > CreateGraphicsDevice(const char* InType = nullptr);

    class SPP_GRAPHICS_API ShaderObject : public SPPObject
    {
        DEFINE_SPP_OBJECT(ShaderObject, SPPObject);

    private:
        std::shared_ptr<GPUShader> _shader;

    public:
        void LoadFromDisk(const AssetPath &FileName, const char* EntryPoint, EShaderType InType);
        std::shared_ptr<GPUShader> GetGPUShader()
        {
            return _shader;
        }
    };

    class SPP_GRAPHICS_API MaterialObject : public SPPObject
    {
        DEFINE_SPP_OBJECT(MaterialObject, SPPObject);

    public:
        EBlendState blendState = EBlendState::Disabled;
        ERasterizerState rasterizerState = ERasterizerState::BackFaceCull;
        EDepthState depthState = EDepthState::Enabled;

        TObjectReference<ShaderObject> meshShader;
        TObjectReference<ShaderObject> vertexShader;
        TObjectReference<ShaderObject> pixelShader;
    };

    class SPP_GRAPHICS_API TesslationMaterialObject : public MaterialObject
    {
        DEFINE_SPP_OBJECT(TesslationMaterialObject, MaterialObject);

    public:
        TObjectReference<ShaderObject> domainShader;
        TObjectReference<ShaderObject> hullShader;
    };
   
    struct SimpleColoredLine
    {
        Vector2 lineEnds[2];
        Color3 color;
    };

    class SPP_GRAPHICS_API DebugDrawer
    {
    private:
        std::vector< SimpleColoredLine > _lines;


    public:
        void AddLine(Vector2 Start, Vector2 End, const Color3 InColor = Color3(255, 255, 255));
    };
}