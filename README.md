# SPP

## Requirements

- VS2019 with .Net Desktop Develop and C++ Desktop Development
- Latest CMake *MUST BE ON PATH*
- Latest Python *MUST BE ON PATH*

## Building

- run "UpdatePrereqs.bat" will update python with necessary modules and download the PreReqs into 3rdParty Folder
- run "GenerateProjects.bat" will run cmake with 'cmake . -BCMAKEBUILD -G "Visual Studio 16 2019" -A x64' creating solution into CMAKEBUILD folder

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
  - needs GC!!!!
  
## Use Cmake Flags

- USE_CEF 
- USE_GRAPHICS
- USE_DX12 
- USE_OPENGL 
- USE_CRYPTOCPP 
- USE_CRYPTOCPP_NETCONN 
- USE_CUDA 

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