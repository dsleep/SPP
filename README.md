# SPP

## Requirements

- VS2019 with .Net Desktop Develop and C++ Desktop Development
- Latest CMake
- Latest Python

## Building

- run "UpdatePrereqs.bat" will update python with necessary modules and download the PreReqs into 3rdParty Folder
- run "GenerateProjects.bat" will run cmake with 'cmake . -BCMAKEBUILD -G "Visual Studio 16 2019" -A x64' creating solution into CMAKEBUILD folder