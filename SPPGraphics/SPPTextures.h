// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPGraphics.h"
#include "SPPGPUResources.h"
#include "SPPMath.h"

namespace SPP
{
    struct SimpleRGB
    {
        uint8_t R, G, B;
    };
    struct SimpleRGBA
    {
        uint8_t R, G, B, A;
    };   

    SPP_GRAPHICS_API bool SaveImageToFile(const char* FilePath,
        uint32_t Width,
        uint32_t Height,
        TextureFormat Format,
        const uint8_t* ImageData
    );

    class SPP_GRAPHICS_API TextureObject
    {
    private:
        int32_t _width;
        int32_t _height;
        TextureFormat _format;
                
        std::shared_ptr< ArrayResource > _rawImgData;
        std::shared_ptr< ImageMeta > _metaInfo;

        std::shared_ptr<GPUTexture> _texture;

    public:
        bool LoadFromDisk(const AssetPath& FileName);
        bool Generate(int32_t InWidth, int32_t InHeight, TextureFormat InFormat );

        std::shared_ptr<GPUTexture> GetGPUTexture()
        {
            return _texture;
        }
    };
}