# SPP

## Requirements

- VS2019 with .Net Desktop Develop and C++ Desktop Development
- Latest CMake *MUST BE ON PATH*
- Latest Python *MUST BE ON PATH*

## Building

- run "UpdatePrereqs.bat" will update python with necessary modules and download the PreReqs into 3rdParty Folder
- run "GenerateProjects.bat" will run cmake with 'cmake . -BCMAKEBUILD -G "Visual Studio 16 2019" -A x64' creating solution into CMAKEBUILD folder

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