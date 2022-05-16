// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPCore.h"
#include "SPPReferenceCounter.h"
#include "SPPArrayResource.h"
#include "SPPObject.h"
#include "SPPGraphics.h"
#include "SPPMath.h"
#include "SPPFileSystem.h"
//#include "SPPSceneRendering.h"
#include <vector>
#include <memory>
#include <string>

namespace DirectX
{
    struct DDS_HEADER;
}

namespace SPP
{
    

    enum class EDrawingTopology
    {
        PointList = 0,
        LineList,
        TriangleList,
        TriangleStrip,
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

    class GPUResource;
    
    template<typename T>
    class GPUReferencer : public Referencer<T>
    {
    protected:
        virtual void DestroyObject() override
        {
            Referencer<T>::DestroyObject();
        }

    public:
        GPUReferencer() : Referencer<T>()
        {
        }

        GPUReferencer(T* obj) : Referencer<T>(obj)
        {
            SE_ASSERT(IsOnGPUThread());
            static_assert(std::is_base_of_v<GPUResource, T>, "Only for gpu refs");
        }

        GPUReferencer(GPUReferencer<T>& orig) : Referencer<T>(orig) 
        { 
            SE_ASSERT(IsOnGPUThread());
        }
        GPUReferencer(const GPUReferencer<T>& orig) : Referencer<T>(orig) 
        { 
            SE_ASSERT(IsOnGPUThread());
        }

        template<typename K = T>
        GPUReferencer(const GPUReferencer<K>& orig) : Referencer<T>(orig)
        { 
            SE_ASSERT(IsOnGPUThread());
            static_assert(std::is_base_of_v<GPUResource, T>, "Only for gpu refs");
        }

        //template<typename K>
        //GPUReferencer(K* obj = nullptr) : Referencer(obj) 
        //{
        //    static_assert(std::is_base_of_v<GPUResource, K>, "Only for gpu refs");
        //}

        //template<typename K>
        //GPUReferencer(GPUReferencer<K>& orig) : Referencer(orig) { }
    };


    template<typename T>
    GPUReferencer<T> Make_GPU()
    {
        return GPUReferencer<T>(new T());
    }

    template<typename T, typename ... Args>
    GPUReferencer<T> Make_GPU(Args&& ... args)
    {
        return GPUReferencer<T>(new T(args...));
    }

    class SPP_GRAPHICS_API GPUResource : public ReferenceCounted
    {
    protected:
        bool _gpuResident = false;
        GPUResource* _prevResource = nullptr;
        GPUResource* _nextResource = nullptr;

    public:
        GPUResource();
        virtual ~GPUResource();

        GPUResource* GetNext()
        {
            return _nextResource;
        }

        virtual const char* GetName() const = 0;       
        virtual void UploadToGpu() = 0;

        bool IsGPUResident() const { return _gpuResident; }

        //crazy ugly no verification TODO improve....
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
        std::string _entryPoint;

    public:
        GPUShader(EShaderType InType) : _type(InType)
        {

        }

        const std::string &GetEntryPoint() 
        {
            return _entryPoint;
        }

        virtual const char* GetName() const override
        {
            return "Shader";
        }

        virtual int32_t GetInstructionCount() const
        {
            return 0;
        }

        virtual bool CompileShaderFromFile(const AssetPath& FileName, const char* EntryPoint = "main", std::string* oErrorMsgs = nullptr)
        {
            std::string loadSrc;
            if (LoadFileToString(*FileName, loadSrc))
            {
                return CompileShaderFromString(loadSrc, FileName.GetName().c_str(), EntryPoint, oErrorMsgs);
            }
            else
            {
                return false;
            }
        }
        virtual bool CompileShaderFromString(const std::string& ShaderSource, const char* ShaderName, const char* EntryPoint = "main", std::string* oErrorMsgs = nullptr) = 0;
    };

    enum class TextureFormat
    {
        UNKNOWN,
        RGB_888,
        RGBA_8888,
        RGBA_BC7,
        DDS_UNKNOWN,
        RG_BC5,
        GRAY_BC4,
        RGB_BC1,
        D24_S8,
        R32G32B32A32F,
        R32G32B32A32
    };

    class SPP_GRAPHICS_API GPUTexture : public GPUResource
    {
    protected:
        int32_t _width = -1;
        int32_t _height = -1;
        TextureFormat _format = TextureFormat::UNKNOWN;
        bool _bCubemap = false;
        std::shared_ptr< ArrayResource > _rawImgData;
        std::shared_ptr< ImageMeta > _metaInfo;
        uint32_t _uniqueID = 0;

    public:
        GPUTexture(int32_t Width, int32_t Height, TextureFormat Format,
            std::shared_ptr< ArrayResource > RawData = nullptr, 
            std::shared_ptr< ImageMeta > InMetaInfo = nullptr);
        virtual ~GPUTexture();
        uint32_t GetID() const { return _uniqueID; }
        virtual const char* GetName() const override
        {
            return "Texture";
        }
    };

   

    class SPP_GRAPHICS_API GPUBuffer : public GPUResource
    {
    protected:
        GPUBufferType _type = GPUBufferType::Simple;
        std::shared_ptr< ArrayResource > _cpuLink;

    public:
        GPUBuffer(GPUBufferType InType, std::shared_ptr< ArrayResource > InCpuData) : _type(InType), _cpuLink(InCpuData) {}

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
        UInt,
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
        virtual ~GPUInputLayout() { }
    };

    

    class SPP_GRAPHICS_API PipelineState : public GPUResource
    {
    public:
    };

    class SPP_GRAPHICS_API GPURenderTarget : public GPUTexture
    {
    public:
        GPURenderTarget(int32_t Width, int32_t Height, TextureFormat Format) : 
            GPUTexture(Width, Height, Format) { }
        virtual ~GPURenderTarget() { }
    };

    class SPP_GRAPHICS_API GD_Shader : public GD_Resource
    {
    protected:
        GPUReferencer< GPUShader > _shader;

       
    public:
        GD_Shader(GraphicsDevice* InOwner) : GD_Resource(InOwner) {}
        virtual ~GD_Shader() {}
        virtual void Initialize(EShaderType InType)
        {
            _shader = _owner->_gxCreateShader(InType);
        }
        virtual bool CompileShaderFromFile(const AssetPath& FileName, const char* EntryPoint = "main", std::string* oErrorMsgs = nullptr)
        {
            return _shader->CompileShaderFromFile(FileName, EntryPoint, oErrorMsgs);
        }
        virtual bool CompileShaderFromString(const std::string& ShaderSource, const char* ShaderName, const char* EntryPoint = "main", std::string* oErrorMsgs = nullptr)
        {
            return _shader->CompileShaderFromString(ShaderSource, ShaderName, EntryPoint, oErrorMsgs);
        }
    };

    class SPP_GRAPHICS_API GD_Texture : public GD_Resource
    {
    protected:
        GPUReferencer< GPUTexture > _texture;

    public:
        GD_Texture(GraphicsDevice* InOwner) : GD_Resource(InOwner) {}
        virtual ~GD_Texture() {}
        virtual void Initialize(int32_t Width, int32_t Height, TextureFormat Format, std::shared_ptr< ArrayResource > RawData = nullptr, std::shared_ptr< ImageMeta > InMetaInfo = nullptr)
        {

        }
    };

    class SPP_GRAPHICS_API GD_Material : public GD_Resource
    {
    protected:
        std::vector< GPUReferencer<GPUTexture> > _textureArray;

        GPUReferencer<GPUShader> _vertexShader;
        GPUReferencer<GPUShader> _pixelShader;

        EBlendState _blendState = EBlendState::Disabled;
        ERasterizerState _rasterizerState = ERasterizerState::BackFaceCull;
        EDepthState _depthState = EDepthState::Enabled;

    public:
        GD_Material(GraphicsDevice* InOwner) : GD_Resource(InOwner) {}
        virtual ~GD_Material() {}
    };

   

    class SPP_GRAPHICS_API GD_Buffer : public GD_Resource
    {
    protected:
        GPUReferencer< GPUBuffer > _buffer;

    public:
        GD_Buffer(GraphicsDevice* InOwner) : GD_Resource(InOwner) {}

        virtual void Initialize(GPUBufferType InType, std::shared_ptr< ArrayResource > InCpuData)
        {
            _buffer = _owner->_gxCreateBuffer(InType, InCpuData);
        }
    };
   

    class SPP_GRAPHICS_API GD_ComputeDispatch : public GD_Resource
    {
    protected:        
        std::vector< std::shared_ptr< ArrayResource> > _constants;
        std::vector< GPUReferencer< GPUTexture > > _textures;
        GPUReferencer< GPUShader > _compute;

    public:
        GD_ComputeDispatch(GPUReferencer< GPUShader> InCS) : _compute(InCS) { }

        void SetTextures(const std::vector< GPUReferencer<GPUTexture> > &InTextures)
        {
            _textures = InTextures;
        }

        void SetConstants(const std::vector< std::shared_ptr< ArrayResource> > &InConstants)
        {
            _constants = InConstants;
        }
        virtual void Dispatch(const Vector3i& ThreadGroupCounts) = 0;
    };

    class SPP_GRAPHICS_API ShaderObject 
    {
    private:
        std::shared_ptr<GD_Shader> _shader;

    public:
        void LoadFromDisk(const AssetPath &FileName, const char* EntryPoint, EShaderType InType);
        auto GetShader()
        {
            return _shader;
        }
    };

    class SPP_GRAPHICS_API MaterialObject 
    {
    public:
        EBlendState blendState = EBlendState::Disabled;
        ERasterizerState rasterizerState = ERasterizerState::BackFaceCull;
        EDepthState depthState = EDepthState::Enabled;

        ShaderObject* meshShader = nullptr;
        ShaderObject* vertexShader = nullptr;
        ShaderObject* pixelShader = nullptr;
    };

    class SPP_GRAPHICS_API TesslationMaterialObject 
    {
    public:
        ShaderObject* domainShader = nullptr;
        ShaderObject* hullShader = nullptr;
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

    // global graphics interface
    SPP_GRAPHICS_API IGraphicsInterface* GGI();
    SPP_GRAPHICS_API void SET_GGI(IGraphicsInterface *InGraphicsIterface);
    // should this be handled better?
    SPP_GRAPHICS_API GraphicsDevice* GGD();
    SPP_GRAPHICS_API void SET_GGD(GraphicsDevice* InGraphicsDevice);
}