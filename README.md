# SPP

## Requirements

- VS2022 with C++20
- Latest CMake *MUST BE ON PATH*
- Latest Python *MUST BE ON PATH*

## Building

- run "UpdatePrereqs.bat" will update python with necessary modules and download the PreReqs into 3rdParty Folder
- run "GenerateProjects.bat" will run cmake with 'cmake . -BCMAKEBUILD -G "Visual Studio 17 2022" -A x64' creating solution into CMAKEBUILD folder

## WARNING

This big of work is a crazy WIP, use at your own riskk!

## Optional

- CUDA

## Descriptions

- SPPCore
  - logging
  - some generic info of system
  - no dependecies
- SPPNetworking
- SPPEngine
- SPPGraphics
- SPPCapture
- SPPVideo
- SPPObject
- 
  
## Use Cmake Flags

- USE_CEF:BOOL=ON
- USE_CRYPTOCPP:BOOL=ON
- USE_CRYPTOCPP_NETCONN:BOOL=ON
- USE_CUDA:BOOL=ON
- USE_DX12:BOOL=OFF
- USE_GRAPHICS:BOOL=ON
- USE_LIBDATACHANNEL:BOOL=OFF
- USE_OBJECTS:BOOL=ON
- USE_OPENGL:BOOL=OFF
- USE_VS_MULTIPROCESS:BOOL=ON
- USE_VULKAN:BOOL=ON
- USE_WINRT:BOOL=ON

## Useful Commands
```
where APPNAME
python --version
cmake --version
```

## Primary Third Parties

- CEF

https://github.com/chromiumembedded/cef

- RTTR

https://github.com/rttrorg/rttr
https://www.rttr.org/

- AssImp

https://github.com/assimp/assimp

- jsoncpp

https://github.com/open-source-parsers/jsoncpp

- D3D12Allocator

https://github.com/GPUOpen-LibrariesAndSDKs/D3D12MemoryAllocator

- CUDA
- CRYPTOCPP

## AUTHOR

David Sleeper
david.sleeper@gmail.com