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

        GPUReferencer(int line, const char* file, T* obj) : Referencer<T>(line,file,obj)
        {
            SE_ASSERT(IsOnGPUThread());
            static_assert(std::is_base_of_v<GPUResource, T>, "Only for gpu refs");
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
    GPUReferencer<T> _Make_GPU(int line, const char* file)
    {
        auto Tp = new T();
        Tp->SetDebug(line, file);
        return GPUReferencer<T>(line, file, Tp);
    }

    template<typename T, typename ... Args>
    GPUReferencer<T> _Make_GPU(int line, const char* file, Args&& ... args)
    {
        auto Tp = new T(args...);
        Tp->SetDebug(line, file);
        return GPUReferencer<T>(line, file, Tp);
    }

    #define Make_GPU(T,...) _Make_GPU<T>( __LINE__, __FILE__, ##__VA_ARGS__); 

    class SPP_GRAPHICS_API GPUResource : public ReferenceCounted
    {
        NO_COPY_ALLOWED(GPUResource);

    protected:
        bool _gpuResident = false;
        bool _dying = false;
        GraphicsDevice* _owner = nullptr;


        int32_t _cppLine = -1;
        std::string _cppFile;

        virtual void _MakeResident() {}
        virtual void _MakeUnresident() {}

    public:
        GPUResource(GraphicsDevice* InOwner);
        virtual ~GPUResource();

        void SetDebug(int line, const char* file)
        {
            _cppLine = line;
            _cppFile = file;
        }

        virtual void NoMoreReferences()
        {
            _dying = true;
            _owner->DyingResource(this);
        }

        virtual const char* GetName() const {
            return "GPUResource";
        }
       
        void MakeResident()
        {
            SE_ASSERT(IsOnGPUThread());
            if (!_gpuResident)
            {
                _MakeResident();
                _gpuResident = true;
            }
        }

        void MakeUnresident()
        {
            SE_ASSERT(IsOnGPUThread());
            if (_gpuResident)
            {
                _MakeUnresident();
                _gpuResident = false;
            }
        }

        bool IsGPUResident() const { return _gpuResident; }

        //crazy ugly no verification TODO improve....
        template<typename T>
        T& GetAs()
        {
            return *(T*)this;
        }
    };

    class SPP_GRAPHICS_API GlobalGraphicsResource : public InternalLinkedList<GlobalGraphicsResource>
    {
    protected:

    public:
        GlobalGraphicsResource();
        virtual ~GlobalGraphicsResource();

        // called on render thread
        virtual void Initialize(class GraphicsDevice* InOwner) = 0;
        virtual void Shutdown(class GraphicsDevice* InOwner) = 0;
    };

    //SPP_GRAPHICS_API void MakeResidentAllGPUResources();

    class SPP_GRAPHICS_API GPUShader : public GPUResource
    {
    protected:
        EShaderType _type;
        std::string _entryPoint;

    public:
        GPUShader(GraphicsDevice* InOwner, EShaderType InType) : GPUResource(InOwner), _type(InType)
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
        GPUTexture(GraphicsDevice* InOwner, 
            int32_t Width, int32_t Height, TextureFormat Format,
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
        GPUBuffer(GraphicsDevice* InOwner, GPUBufferType InType, std::shared_ptr< ArrayResource > InCpuData) : GPUResource(InOwner), _type(InType), _cpuLink(InCpuData) {}

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
        Float,
        UInt32,
        UInt8_4,
        UInt8_3,
    };

    struct InputLayoutElement
    {
        std::string Name;
        InputLayoutElementType Type;
        uint16_t Offset;
    };

    struct VertexAttribute
    {
        InputLayoutElementType Type;
        uint16_t Offset;
    };

    struct VertexStream
    {  
        uint16_t Size;
        std::vector<VertexAttribute> Attributes;
    };

    template <typename T, typename... Args>
    void AddVertexAttributes(std::vector<VertexAttribute> &oAttributes, uintptr_t baseAddr, const T& first, const Args&... args)
    {
        VertexAttribute curAttribute;

        if constexpr (std::is_same_v<T, Vector3>)
        {
            curAttribute.Type = InputLayoutElementType::Float3;
        }
        else if constexpr (std::is_same_v<T, Vector2>)
        {
            curAttribute.Type = InputLayoutElementType::Float2;
        }
        else if constexpr (std::is_same_v<T, uint32_t>)
        {
            curAttribute.Type = InputLayoutElementType::UInt32;
        }
        else if constexpr (std::is_same_v<T, float>)
        {
            curAttribute.Type = InputLayoutElementType::Float;
        }
        else if constexpr (std::is_same_v<T, Color4>)
        {
            curAttribute.Type = InputLayoutElementType::UInt8_4;
        }
        else
        {
            struct DummyType {};
            static_assert(std::is_same_v<T, DummyType>, "AddVertexAttributes: Unknown type");
        }

        curAttribute.Offset = (uint32_t)(reinterpret_cast<uintptr_t>(&first) - baseAddr);

        // todo real proper check
        SE_ASSERT(curAttribute.Offset < 128);

        oAttributes.push_back(curAttribute);

        if constexpr (sizeof...(args) >= 1)
        {
            AddVertexAttributes(oAttributes, baseAddr, args...);
        }
    }

    template<class VertexType, typename... VertexMembers>
    VertexStream CreateVertexStream(const VertexType& InType, const VertexMembers&... inMembers)
    {
        VertexStream currentStream = { .Size = sizeof(VertexType) };

        AddVertexAttributes(currentStream.Attributes, reinterpret_cast<uintptr_t>(&InType), inMembers...);

        return currentStream;
    }

    class SPP_GRAPHICS_API GPUInputLayout : public GPUResource
    {
    public:
        GPUInputLayout(GraphicsDevice* InOwner) : GPUResource(InOwner)
        {

        }

        virtual const char* GetName() const override
        {
            return "InputLayout";
        }

        virtual void InitializeLayout(const std::vector<VertexStream>& vertexStreams) = 0;
        virtual ~GPUInputLayout() { }
    };

    

    class SPP_GRAPHICS_API PipelineState : public GPUResource
    {
    public:
        PipelineState(GraphicsDevice* InOwner) :
            GPUResource(InOwner) { }
        virtual ~PipelineState() { }
    };

    class SPP_GRAPHICS_API GPURenderTarget : public GPUTexture
    {
    public:
        GPURenderTarget(GraphicsDevice* InOwner, int32_t Width, int32_t Height, TextureFormat Format) :
            GPUTexture(InOwner, Width, Height, Format) { }
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

        GPUReferencer< GPUShader > GetGPURef()
        {
            return _shader;
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
            _texture = _owner->_gxCreateTexture(Width, Height, Format, RawData, InMetaInfo);
        }
        GPUReferencer< GPUTexture > GetGPUTexture()
        {
            return _texture;
        }
    };

    class SPP_GRAPHICS_API GD_Material : public GD_Resource
    {
    protected:
        std::shared_ptr< GD_Shader > _vertexShader;
        std::shared_ptr< GD_Shader > _pixelShader;

        std::vector< std::shared_ptr<GD_Texture> > _textureArray;

        EBlendState _blendState = EBlendState::Disabled;
        ERasterizerState _rasterizerState = ERasterizerState::BackFaceCull;
        EDepthState _depthState = EDepthState::Enabled;

    public:
        GD_Material(GraphicsDevice* InOwner) : GD_Resource(InOwner) {}
        virtual ~GD_Material() {}

        struct Args
        {
            std::shared_ptr< GD_Shader > vertexShader;
            std::shared_ptr< GD_Shader > pixelShader;
            std::vector< std::shared_ptr<GD_Texture> > textureArray;
        };

        void SetMaterialArgs(const Args &InArgs)
        {
            _vertexShader = InArgs.vertexShader;
            _pixelShader = InArgs.pixelShader;
            _textureArray = InArgs.textureArray;
        }

        std::vector< std::shared_ptr<GD_Texture> > &GetTextureArray()
        {
            return _textureArray;
        }
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

        GPUReferencer< GPUBuffer > GetGPUBuffer()
        {
            return _buffer;
        }
    };
   

    class SPP_GRAPHICS_API GD_ComputeDispatch : public GD_Resource
    {
    protected:        
        std::vector< std::shared_ptr< ArrayResource> > _constants;
        std::vector< GPUReferencer< GPUTexture > > _textures;
        GPUReferencer< GPUShader > _compute;

    public:
        GD_ComputeDispatch(GraphicsDevice* InOwner, GPUReferencer< GPUShader> InCS) : GD_Resource(InOwner), _compute(InCS) { }

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
}

