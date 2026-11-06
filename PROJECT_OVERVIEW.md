# Project overview for coding agents

This repository contains two connected C++ codebases:

- **PathTracer** is the application and rendering demo.
- **FANCY** is the reusable rendering framework used by the application. It is a Git submodule at `FANCY/`.

The combined project is configured and built from the repository root. This guide is intended as a practical map for understanding the code and making changes without first reverse-engineering the build.

## Purpose and design

FANCY is a personal-learning and graphics-demo framework, not a general-purpose production engine. Its central architectural goal is to offer a common rendering API over Direct3D 12 and Vulkan. Most renderer-facing application code should use the API-independent types in `FANCY/fancy_core/Rendering/`; implementations live in the `DX12/` and `Vulkan/` subdirectories.

The framework also groups shared application services with the renderer: window/application lifecycle, camera and input, asset and scene loading, profiling, GPU resources, shader compilation, and ImGui integration. Resource handles and backend caches are used to manage GPU objects. The framework advertises automatic resource hazard tracking, async compute, shader hot reload, CPU/GPU profiling, and hardware ray tracing. Some areas are explicitly works in progress; check the implementation and both backends before assuming a feature is equally complete everywhere.

PathTracer is a Windows desktop demo built on this API. It loads a mesh scene, constructs GPU acceleration structures when ray tracing is supported, and offers both raster and ray-traced rendering paths. It includes progressive accumulation, ambient-occlusion/ray-tracing controls, an atmospheric sky, tonemapping, and ImGui controls/debug views. Shader code is HLSL; FANCY uses DirectX Shader Compiler (DXC), including HLSL-to-SPIR-V compilation for Vulkan.

The separation is deliberate: application/demo logic belongs in `PathTracer/` and its root `resources/`; reusable rendering and supporting facilities belong in the FANCY submodule. Avoid putting app-specific logic in FANCY unless it is genuinely reusable.

## Repository map

| Path | Responsibility |
| --- | --- |
| `CMakeLists.txt` | Root CMake project; sets C++17 and includes FANCY and PathTracer. Also defines the root `format` target when `clang-format` is found. |
| `CMakePresets.json` | Visual Studio 2022 x64 configure preset and Debug/Release build presets. |
| `PathTracer/` | Windows GUI executable: application entry point, path-tracing scene setup/render loop, sky model, and ImGui integration. |
| `resources/models/` | PathTracer scenes and model assets (including Cornell Box and Sun Temple). |
| `resources/shaders/` | PathTracer raster, ray-tracing, sky, and post-processing HLSL. |
| `resources/textures/` | PathTracer textures. |
| `FANCY/fancy_core/Common/` | Application/window/input/time, camera/math, utility containers, and memory allocation. |
| `FANCY/fancy_core/Debug/` | Logging, annotations, and CPU/GPU profiler. |
| `FANCY/fancy_core/IO/` | Asset paths, file/image loading, mesh import, materials, and scene data. |
| `FANCY/fancy_core/Rendering/` | API-independent GPU resource, command, shader, pipeline, render-output, and ray-tracing interfaces, plus common hazard/resource management. |
| `FANCY/fancy_core/Rendering/DX12/` | Direct3D 12 renderer implementation. |
| `FANCY/fancy_core/Rendering/Vulkan/` | Vulkan renderer implementation. |
| `FANCY/fancy_imgui/` | ImGui sources and FANCY rendering adapter, profiler window, and GPU debug views. |
| `FANCY/Tests/` | Standalone interactive framework test application and GPU/rendering tests. |
| `FANCY/resources/` | Framework models and shader/test assets; separate from the application’s root `resources/`. |
| `FANCY/cmake/` | Shared CMake helpers and WinPixEventRuntime imported-target setup. |
| `FANCY/vcpkg.json` | Manifest for third-party C++ dependencies. |
| `FANCY/external/vcpkg/` | Local vcpkg checkout/toolchain and its package ports. It is package-manager infrastructure, not project source. |
| `FANCY/bootstrap_winpix.ps1` | Downloads the separately distributed WinPixEventRuntime package into `FANCY/external/WinPixEventRuntime/`. |

## Important code entry points

- `PathTracer/PathTracer_main.cpp` is the Win32 `WinMain`, creates a `PathTracer`, then drives `BeginFrame()`, `Update()`, `Render()`, and `EndFrame()` while processing window messages. It exports the Direct3D 12 Agility SDK version/path symbols.
- `PathTracer/PathTracer.h` declares the app and its `RaytracingScene` GPU-resource bundle.
- `PathTracer/PathTracer.cpp` owns scene import and BLAS/TLAS setup, render-target and depth management, frame/UI logic, raster rendering, ray tracing, tonemapping, and accumulation reset. Its method layout is a good first read when changing application rendering behavior.
- `PathTracer/Sky.h` and `Sky.cpp` implement atmospheric parameter setup and transmittance/sky-view lookup-table generation/rendering.
- `FANCY/fancy_core/Rendering/RenderCore.h` and `GraphicsResources.h` are useful starting points for the common renderer/resource API. Follow a common interface into its corresponding DX12 and Vulkan implementation when making backend changes.
- `FANCY/fancy_core/IO/Scene.h` and `MeshImporter.h` define scene/mesh input concepts used by the app.
- `FANCY/fancy_imgui/imgui_impl_fancy.*` is the framework integration point for ImGui rendering.

## Build and dependency model

### Supported local build

The checked-in preset targets **Visual Studio 2022, x64, Windows**, with Debug and Release configurations. The root project and subprojects require **CMake 4.3 or newer** and set **C++17**. Although the renderer has Vulkan code, the documented build, application entry point, compiler flags, output layout, and bootstrap scripts are Windows/MSVC-oriented; do not assume this repository currently provides a tested cross-platform build.

Dependencies are resolved in two ways:

1. **vcpkg manifest mode:** `FANCY/vcpkg.json` declares Assimp, EASTL, GLM, xxHash, DirectX Shader Compiler, DirectX headers, DirectX 12 Agility SDK, Vulkan, and SPIRV-Reflect. The root preset selects the vcpkg toolchain in `FANCY/external/vcpkg/scripts/buildsystems/vcpkg.cmake`, the `x64-windows` triplet, manifest root `FANCY/`, and install root `FANCY/vcpkg_installed/`. CMake’s vcpkg integration installs/resolves these packages during configure as needed.
2. **Separate WinPixEventRuntime package:** `FANCY/cmake/WinPixEventRuntime.cmake` expects headers, import library, and DLL under `FANCY/external/WinPixEventRuntime/`. Run `FANCY/bootstrap_winpix.ps1` once to fetch and extract it from NuGet. CMake fails configuration if this package is absent.

The Vulkan SDK must also be installed and available to CMake (the FANCY build documentation calls out `VK_SDK_PATH`). Git submodules must be initialized so the FANCY source is present. vcpkg bootstrapping builds the local `vcpkg.exe`; it is separate from installing the manifest dependencies.

### First configure and build (PowerShell from repository root)

```powershell
git submodule update --init --recursive
.\FANCY\external\vcpkg\bootstrap-vcpkg.bat
powershell -ExecutionPolicy Bypass -File .\FANCY\bootstrap_winpix.ps1
cmake --preset vs2022-win64
cmake --build --preset debug
```

Use `cmake --build --preset release` for Release. The equivalent direct build command is `cmake --build _cmake_build --config Debug`. Open `_cmake_build\PathTracerSolution.sln` to work in Visual Studio. `PathTracer` and `Tests` are executable targets; `fancy_core` and `fancy_imgui` are static libraries.

The manifest directory and install directory are intentionally set in `CMakePresets.json`; prefer the root preset over running vcpkg from an arbitrary working directory or hand-configuring a second package install location. If dependencies appear missing, first check that the configure used this preset and that the `FANCY/vcpkg_installed/` tree is populated.

### Build outputs and runtime assets

- The combined CMake build tree and Visual Studio solution are under `_cmake_build/`.
- The PathTracer executable is placed under `bin/win64/<configuration>/PathTracer/` at the repository root. The Tests executable is placed under `FANCY/bin/win64/<configuration>/Tests/`; framework static libraries go under `FANCY/lib/win64/<configuration>/...`.
- The app and test targets set their working directory to their executable output directory. Runtime shader/model/texture lookups use relative `resources/...` paths, so keep the expected working directory in mind when launching manually.
- FANCY’s CMake helper adds post-build copies of the vcpkg runtime DLLs, DXC/Agility DLLs, and WinPix runtime needed by these executables.
- Root `generate_projects.bat` runs the root configure preset. `FANCY/generate_solution.bat` is the FANCY-only entry point documented by that subproject.

## Development notes for AI coding tools

- Read the root `.copilot-instructions.md` for C++ style expectations; it applies to both the app and FANCY. The build is configured for C++17, but the style guide favors familiar, restrained C++ idioms unless a newer feature materially helps.
- Follow the common rendering API for cross-backend behavior. Keep backend-specific code in `Rendering/DX12/` or `Rendering/Vulkan/` and check whether both implementations need the same change.
- Resource ownership is explicit in many GPU-facing classes: handles generally need to be released through the appropriate `RenderCore::Delete...` API; some pipelines and cached objects are explicitly non-owned. Follow nearby lifetime patterns.
- Shader changes usually require checking the HLSL resource bindings/constant layouts against the C++ setup and checking shader compilation/reload behavior for both graphics APIs.
- Keep changes to `FANCY/` coordinated with the submodule: commit/push changes in the FANCY repository and then update the submodule reference in PathTracer when that is the intended workflow. Do not edit `FANCY/external/vcpkg/` as if it were project-owned engine code.
- CMake source collection uses `GLOB`/`GLOB_RECURSE` in several targets. When adding files, verify they match the relevant glob or explicit source list; the Tests executable uses an explicit list.
- The FANCY tests are interactive GPU executables, not a conventional headless unit-test suite. Select a specific test and target hardware/graphics API when validating rendering changes.

## Project documentation

- Root build quick start: `README.md`
- Framework background and declared features: `FANCY/README.md`
- Shared code style: `.copilot-instructions.md`
