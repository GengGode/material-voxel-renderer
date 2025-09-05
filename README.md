# Material-Voxel-Renderer

material x-ray volume renderer using OpenGL


## Features
- CMake for build system
- vcpkg for dependency management
- Unsealed CMake for easy configuration
- Version control with Git

## Usage

0. Unseal Template   
If you are using this template, you need to unseal it first.  
You can do this by running the following command in the root directory of the project:
```shell
powerhell unseal_template.ps1
```
This will replace the template names and create a new project with the same name as the repository.

1. setting up vcpkg  
Set Environment Variable `VCPKG_ROOT` to the path of vcpkg directory.  
Or use .vcpkg-root file in the project root directory.
```shell
$env:VCPKG_ROOT="C:\vcpkg"
          or
echo "C:\vcpkg" > .vcpkg-root
```

2. Cmake configure and build
```shell
cmake -B build && cmake --build build
```

3. Run the executable
```shell
.\build\bin\Release\test.exe
```

## Dependencies

- Fmt
- Spdlog

## Using GitHub Codespaces

1. Open the repository in GitHub Codespaces.
2. Codespaces will automatically set up the development environment.
3. CMake configure and build
```shell
cmake -B build && cmake --build build
```
4. Run the executable
```shell
.\build\bin\Release\test.exe
```
