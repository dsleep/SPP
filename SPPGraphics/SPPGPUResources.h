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

    class ReferenceCounted
    {
        uint32_t _refCnt = 0;

    public:
       
        uint32_t incRefCnt()
        {
            return ++_refCnt;
        }

        uint32_t decRefCnt()
        {
            return --_refCnt;
        }
    };

    template< typename T>
    class Referencer
    {
    protected:
        T* obj = nullptr;

        virtual void DestroyObject()
        {
            delete obj;
            obj = nullptr;
        }

        void decRef() 
        {
            if (obj && obj->decRefCnt() == 0)
            {
                DestroyObject();
            }
        }

    public:       
      
        Referencer(T* obj = nullptr) : obj(obj)
        {
            if (obj)
            {
                obj->incRefCnt();
            }
        }



        Referencer(Referencer<T>& orig) 
        {
            obj = orig.RawGet();
            if (obj)
            {
                obj->incRefCnt();
            }
        }

        Referencer(const Referencer<T>& orig) 
        {
            obj = orig.RawGet();
            if (obj)
            {
                obj->incRefCnt();
            }
        }

        template<typename K=T>
        Referencer(const Referencer<K>& orig)
        {
            static_assert(std::is_base_of_v<T, K>, "must be base of");

            obj = orig.RawGet();
            if (obj)
            {
                obj->incRefCnt();
            }
        }

        T& operator* () 
        {
            SE_ASSERT(obj);
            return *obj;
        }

        T* operator-> () 
        {
            SE_ASSERT(obj);
            return obj;
        }

        T* RawGet() const
        {
            return obj;
        }
        T* get()
        {
            return obj;
        }

        template<typename K>
        bool operator== (const Referencer<T>& right) const
        {
            return obj == right.obj;
        }

        Referencer<T>& operator= (Referencer<T>& right)
        {
            if (this == &right)
            {
                return *this;
            }
            if (right.obj)
            {
                right.obj->incRefCnt();
            }
            decRef();
            obj = right.obj;
            return *this;
        }

        template<typename K>
        Referencer<T>& operator= (Referencer<K>& right)
        {
            static_assert(std::is_base_of_v(T, K));

            *this = reinterpret_cast<Referencer<T>>(right);
        }

      

        operator bool()
        {
            return (obj != NULL);
        }

        virtual ~Referencer()
        {
            decRef();
        }
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
        
        GPUReferencer(T* obj = nullptr) : Referencer(obj)
        {
            static_assert(std::is_base_of_v<GPUResource, T>, "Only for gpu refs");
        }
        

        GPUReferencer(GPUReferencer<T>& orig) : Referencer(orig) { }
        GPUReferencer(const GPUReferencer<T>& orig) : Referencer(orig) { }

        template<typename K = T>
        GPUReferencer(const GPUReferencer<K>& orig) : Referencer(orig) 
        { 
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

    public:
        GPUResource();
        virtual ~GPUResource();

        virtual const char* GetName() const = 0;

        bool IsGPUResident() const { return _gpuResident; }
        virtual void UploadToGpu() = 0;

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

    class SPP_GRAPHICS_API ComputeDispatch
    {
    protected:        
        std::vector< std::shared_ptr< ArrayResource> > _constants;
        std::vector< GPUReferencer< GPUTexture > > _textures;
        GPUReferencer< GPUShader > _compute;

    public:
        ComputeDispatch(GPUReferencer< GPUShader> InCS) : _compute(InCS) { }

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
        GPUReferencer<GPUShader> _shader;

    public:
        void LoadFromDisk(const AssetPath &FileName, const char* EntryPoint, EShaderType InType);
        GPUReferencer<GPUShader> GetGPUShader()
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

    struct IGraphicsInterface
    {
        virtual GPUReferencer< GPUShader > CreateShader(EShaderType InType) = 0;

        virtual GPUReferencer< GPUBuffer > CreateStaticBuffer(GPUBufferType InType, std::shared_ptr< ArrayResource > InCpuData = nullptr) = 0;

        virtual GPUReferencer< GPUInputLayout > CreateInputLayout() = 0;
        virtual GPUReferencer< GPUTexture > CreateTexture(int32_t Width, int32_t Height, TextureFormat Format, std::shared_ptr< ArrayResource > RawData = nullptr, std::shared_ptr< ImageMeta > InMetaInfo = nullptr) = 0;
        virtual GPUReferencer< GPURenderTarget > CreateRenderTarget() = 0;


        virtual std::shared_ptr< GraphicsDevice > CreateGraphicsDevice() = 0;

        virtual std::shared_ptr< class ComputeDispatch > CreateComputeDispatch(GPUReferencer< GPUShader> InCS) = 0;
        virtual std::shared_ptr< class RenderScene > CreateRenderScene() = 0;
        virtual std::shared_ptr< class RenderableMesh > CreateRenderableMesh() = 0;
        virtual std::shared_ptr< class RenderableSignedDistanceField > CreateRenderableSDF() = 0;

        virtual void BeginResourceCopies() { }
        virtual void EndResourceCopies() { }

        virtual bool RegisterMeshElement(std::shared_ptr<class MeshElement> InMeshElement) { return true; };
        virtual bool UnregisterMeshElement(std::shared_ptr<class MeshElement> InMeshElement) { return true; };
    };

    // global graphics interface
    SPP_GRAPHICS_API IGraphicsInterface* GGI();
    SPP_GRAPHICS_API void SET_GGI(IGraphicsInterface *InGraphicsIterface);
}