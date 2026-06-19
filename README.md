# PathTracer

A small GPU-based pathtracer using hardware raytracing, built on top of the `FANCY` rendering framework.

![Screenshot 2022-11-05 142222](https://user-images.githubusercontent.com/94477/200122499-4c9c80cb-0d4d-4f16-abe9-868f204909fa.png)

## Prerequisites

1. Visual Studio 2022 with C++ workload
2. Git (with submodule support)
3. CMake 4.3 or newer
4. PowerShell (for bootstrap scripts)

## Clone and initialize

Clone with submodules so `FANCY` and its dependencies are available:

```bat
git clone --recurse-submodules https://github.com/domme/PathTracer.git
cd PathTracer
```

If you already cloned without submodules:

```bat
git submodule update --init --recursive
```

## One-time dependency bootstrap

`FANCY` requires WinPixEventRuntime to be downloaded into `FANCY\external\WinPixEventRuntime`.

From repository root:

```powershell
powershell -ExecutionPolicy Bypass -File .\FANCY\bootstrap_winpix.ps1
```

Bootstrap `vcpkg` (required once per checkout):

```bat
.\FANCY\external\vcpkg\bootstrap-vcpkg.bat
```

## Generate Visual Studio projects/solutions

### Preferred (full PathTracer solution)

From repository root:

```bat
generate_projects.bat
```

This runs:

```bat
cmake --preset vs2022-win64
```

and generates the VS solution in:

```text
_cmake_build\PathTracerSolution.sln
```

### Alternative (FANCY-only solution)

If you only want to work on `FANCY`, run:

```bat
cd FANCY
generate_solution.bat
```

This generates:

```text
FANCY\_cmake_build\Fancy.sln
```

## Build the project

After generating (root solution), build from repository root:

```bat
cmake --build _cmake_build --config Debug
```

or:

```bat
cmake --build _cmake_build --config Release
```

You can also use the build presets from `CMakePresets.json`:

```bat
cmake --build --preset debug
cmake --build --preset release
```

To build just `fancy_core` in Debug:

```bat
cmake --build _cmake_build --target fancy_core --config Debug -- /nologo /m
```

## Visual Studio startup project

Open `_cmake_build\PathTracerSolution.sln` in Visual Studio.  
You can select either `PathTracer` or `Tests` as startup project.

## Script quick reference

| Script | Run from | Purpose |
| --- | --- | --- |
| `generate_projects.bat` | repository root | Generates full PathTracer + FANCY VS solution (`_cmake_build`) |
| `FANCY\generate_solution.bat` | `FANCY` folder | Generates standalone FANCY VS solution (`FANCY\_cmake_build`) |
| `FANCY\bootstrap_winpix.ps1` | anywhere (path-adjusted) | Downloads/extracts WinPixEventRuntime into `FANCY\external\WinPixEventRuntime` |
| `FANCY\external\vcpkg\bootstrap-vcpkg.bat` | `FANCY\external\vcpkg` (or root with full path) | Builds/bootstraps the local `vcpkg.exe` tool |
