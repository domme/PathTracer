# PathTracer — Code Style Guide

## General

- C++17
- Headers use `#pragma once`
- Section separators: `//---------------------------------------------------------------------------//`
- Prefer flat includes over deep namespace qualification — bring types into scope with `using namespace Fancy;` (or equivalent) at the top of each file rather than sprinkling `Fancy::` qualifiers throughout

---

## Naming

| Entity | Convention | Example |
|---|---|---|
| Classes / Structs | PascalCase | `GpuBuffer`, `RtAccelerationStructure` |
| Member variables | `my` prefix + PascalCase | `myProperties`, `myNumElements` |
| Function parameters (singular) | `a`/`an` prefix + PascalCase | `aType`, `anOffset`, `aName` |
| Function parameters (plural) | `some` prefix + PascalCase | `someArguments`, `someBuffers` |
| Methods | PascalCase | `GetProperties()`, `Create()` |
| Constants / `enum` fields | ALL_CAPS_WITH_UNDERSCORES | `kNumCachedBarriers`, `PLANE_LEFT` |
| Local variables | camelCase | `startAddress`, `importSuccess` |
| Static / global variables | camelCase | `sceneLoadInfos` |

---

## Types

Use the project-defined type aliases instead of standard-library integers:

```cpp
uint, uint8, uint16, uint64
int16, int64
float64   // double
```

Use `glm` types for math (`glm::float3`, `glm::mat4`, etc.).  
Use EASTL containers — **prefer fixed/hybrid containers over dynamic ones** for predictable memory usage and cache locality:
- `eastl::fixed_vector< T, N >` (instead of `eastl::vector< T >` when capacity is known)
- `eastl::fixed_list< T, N >` (instead of `eastl::list< T >`)
- `eastl::fixed_string< char, N >` (instead of `eastl::string`)

ResourcePool uses this strategy: `ResourcePool< T, DestructorT, StaticCapacity >` allocates an internal `fixed_vector< Slot, StaticCapacity >` for predictable, stack-like memory behavior.

---

## Formatting & Code Style

All C++ code is formatted with **clang-format** (configuration in `.clang-format` at project root).

### Indentation & Spacing

- **Indentation:** 2 spaces per level
- **Namespace contents:** indented by 2 spaces
- **Opening braces:** same line as declaration (Attach style)
  ```cpp
  void SomeFunction() {
    // body
  }
  
  class Foo {
  };
  ```

### Brackets — Spacing

Insert a space after opening brackets and before closing brackets in **all bracket types**: `( )`, `[ ]`, `< >`.  
Empty brackets are left as-is: `()`, `[]`.

```cpp
// ✅ correct
alloca( sizeof( D3D12_PLACED_SUBRESOURCE_FOOTPRINT ) * aNumSubresources )
GetDevice()->CreateTexture( desc, someData )
myBuffers[ i ]
SharedPtr< GpuBuffer >
static_cast< D3D12_PLACED_SUBRESOURCE_FOOTPRINT * >( alloca( ... ) )

// ❌ wrong
alloca(sizeof(D3D12_PLACED_SUBRESOURCE_FOOTPRINT) * aNumSubresources)
SharedPtr<GpuBuffer>
myBuffers[i]
```

### Pointers & References — Alignment

Pointer/reference symbols go **next to the type**, with a space before the variable name:

```cpp
// ✅ correct
const GpuBuffer * aBuffer
const GpuBufferProperties & someProperties
GpuBuffer * GetBuffer() const;

// ❌ wrong
const GpuBuffer *aBuffer
const GpuBuffer* GetBuffer() const;
```

**Dereference/address-of in expressions:** symbol touches the operand:

```cpp
*someBuffer
&myResource
**ppData
```

### Column Limit & Line Length

- **Maximum line length:** 120 characters
- Favor **breaking long lines** over exceeding the limit

### Variable Alignment in Consecutive Declarations

When declaring multiple variables of the same type or related types in consecutive lines, align the variable names with tabs:

```cpp
// ✅ correct — names aligned
ResourcePool< Texture, 2048 >    RenderCore::ourTexturePool;
ResourcePool< GpuBuffer, 2048 >  RenderCore::ourBufferPool;
TextureHandle                    RenderCore::ourDefaultDiffuseTexture;

// Separator — empty line resets the alignment group
TextureSamplerHandle             RenderCore::ourLinearClampSampler;
```

### Function Separators in .cpp Files

Separate function definitions in `.cpp` files with a line of dashes:

```cpp
//---------------------------------------------------------------------------//
int Foo::CalculateValue() {
  return 42;
}

//---------------------------------------------------------------------------//
void Foo::DoSomething() {
  // ...
}
```

**Note:** clang-format cannot enforce this automatically; maintain manually for readability.

### Includes

- **Order:** No automatic sorting; maintain logical grouping by hand
- Preferred pattern: system headers, then engine headers, then local project headers
  ```cpp
  #include <cstring>
  #include <vector>
  
  #include "fancy_core/Rendering/RenderCore.h"
  #include "fancy_core/Common/Application.h"
  
  #include "MyLocal.h"
  ```

---

## Spacing — Pointers and References

Put the `*` / `&` symbol **next to the type**, with a space separating the symbol from the variable name:

```cpp
// ✅ correct — reference parameter
void Foo(const GpuBufferProperties & someProperties);

// ✅ correct — pointer parameter
void Bar(const GpuBuffer * aBuffer);

// ✅ correct — pointer member / return type
GpuBuffer * GetBuffer() const;
const GpuBuffer * myPtr;

// ❌ wrong — symbol touching the name
void Foo(const GpuBufferProperties &someProperties);
void Bar(const GpuBuffer *aBuffer);
```

**Dereference and address-of operators** in expressions are written with the symbol touching the operand (no leading space, no trailing space before the name):

```cpp
// ✅ correct — dereference / address-of in expressions
*someBuffer
&myResource
**ppData
```

---

## Namespaces

Prefer **not** qualifying types with a namespace prefix in implementation files and in headers that already pull the namespace into scope.  
Bring the namespace into scope once at the top instead:

```cpp
// ✅ preferred in .cpp files
using namespace Fancy;

void Foo(CommandList * aCommandList) { … }

// ❌ avoid unless disambiguation is required
void Foo(Fancy::CommandList * aCommandList) { … }
```

In headers where a `using namespace` would pollute other translation units, use a minimal forward-declaration block or qualify only where necessary.

---

## Classes

- `public` members before `protected` before `private`
- Deleted/defaulted special members are declared explicitly when the intent is non-obvious
- Virtual destructors are marked `= default` where there is no body needed

---

## Comments

Comment only code that genuinely needs clarification. Do not add explanatory prose for self-evident logic.  
Use `// TODO:` for outstanding work items.  
Use the section separator line before and after major declarations inside a namespace:

```cpp
namespace Fancy {
//---------------------------------------------------------------------------//
  class GpuBuffer : public GpuResource
  {
    …
  };
//---------------------------------------------------------------------------//
}
```

---

## Spacing — Brackets

Insert a space after every opening bracket and before every closing bracket. This applies to **all bracket types**: `( )`, `[ ]`, and `< >` (template angle brackets).  
Empty bracket pairs are left as-is: `()`, `[]`.

```cpp
// ✅ correct — function calls
alloca( sizeof( D3D12_PLACED_SUBRESOURCE_FOOTPRINT ) * aNumSubresources )
GetDevice()->CreateTexture( desc, someData )

// ✅ correct — C-style casts (space after closing bracket too)
( uint ) someValue
( D3D12_RESOURCE_STATES ) 0u
static_cast< D3D12_PLACED_SUBRESOURCE_FOOTPRINT * >( alloca( ... ) )

// ✅ correct — array subscripts
myBuffers[ i ]
someArray[ index + 1 ]

// ✅ correct — template angle brackets
SharedPtr< GpuBuffer >
eastl::vector< GpuRingBuffer * >
eastl::fixed_vector< glm::float3, 4 >
static_cast< D3D12_PLACED_SUBRESOURCE_FOOTPRINT * >( alloca( ... ) )

// ❌ wrong
alloca(sizeof(D3D12_PLACED_SUBRESOURCE_FOOTPRINT) * aNumSubresources)
( uint )someValue
myBuffers[i]
SharedPtr<GpuBuffer>
static_cast<D3D12_PLACED_SUBRESOURCE_FOOTPRINT*>(alloca(...))
```

Apply the same rule to function-template call sites and template parameter lists in declarations.

---

## Macros and Assertions

Use `ASSERT(condition)` for runtime assertions (not `assert()`).  
Existing helper macros (`SAFE_DELETE`, `ARRAY_LENGTH`, `VECTOR_BYTESIZE`, etc.) are preferred over reimplementing the same logic.

---

## Indentation

- 2 spaces per level
- Contents of a `namespace` block are indented by 2 spaces
- Opening brace `{` goes on the **same line** as the declaration or control statement:

```cpp
// ✅ correct
void SomeFunction() {
}

class Foo {
};

if (condition) {
}

// ❌ wrong
void SomeFunction()
{
}
```

---

# Fancy Engine — Architecture Guide

## Repository Layout

```
FANCY/                      ← Engine submodule
  fancy_core/
    Common/                 ← Platform types, Application base, Camera, Input, math, allocators
    Debug/                  ← Log.h, ASSERT macro, Profiler (GPU + CPU markers)
    IO/                     ← Scene/mesh import, Assets, PathService, FileWatcher, BinaryCache
    Rendering/              ← All GPU-facing code (see below)
      DX12/                 ← DX12 backend implementation
      Vulkan/               ← Vulkan backend (partial)
  fancy_imgui/              ← ImGui integration (imgui_impl_fancy.h)
  resources/shaders/        ← Engine-level HLSL headers (GlobalResources.h, Encoding.h, common_types.h)

PathTracer/                 ← Application built on top of Fancy
  PathTracer.h / .cpp       ← Main app class (extends Application)
  PathTracer_main.cpp       ← WinMain entry point
  Sky.h / .cpp              ← Atmosphere/sky rendering
resources/
  shaders/                  ← Project HLSL shaders
    raytracing/             ← RT pipeline shaders (PathTracing.hlsl, Ao.hlsl, Common.hlsl, …)
    sky/                    ← Sky LUT compute shaders
```

---

## Application Framework

`Application` (`fancy_core/Common/Application.h`) is the base class for all Fancy apps.  
Override and call `Application::` super at the top of each:

```cpp
void BeginFrame() override;   // calls RenderCore::BeginFrame(), ImGuiRendering::NewFrame()
void Update() override;       // camera controller tick, input
void Render() override;       // main render pass — must begin/end a CommandList
void EndFrame() override;     // calls RenderCore::EndFrame(), present
```

The win32 message loop is hand-rolled in `PathTracer_main.cpp` (no framework magic).  
`Application` owns: `myRenderOutput`, `myCamera`, `myCameraController`, `myInputState`, `myRealTimeClock`.

---

## RenderCore — Central Rendering API

`RenderCore` (`Rendering/RenderCore.h`) is a **static-only** class; all methods are `static`.  
It owns the GPU device, command queues, resource caches, ring-buffer pool, and query heaps.

### Frame lifecycle

```cpp
RenderCore::BeginFrame();
// ... render work ...
RenderCore::EndFrame();
```

### Creating GPU resources

Always go through `RenderCore::Create*`; never use platform types directly.

```cpp
// Texture + views in one call (auto-registers in global bindless table)
SharedPtr< Texture > tex = RenderCore::CreateTexture( props, "MyTex", uploadDatas, numDatas );
SharedPtr< TextureView > srv = RenderCore::CreateTextureView( tex, viewProps, "MyTex_SRV" );

// Buffer + view combo helper (preferred when you have initial data)
SharedPtr< GpuBufferView > bufView = RenderCore::CreateBufferView( bufferProps, viewProps, "MyBuf", cpuData );

// Shader pipelines (cached by hash; hot-reloaded on file change)
SharedPtr< ShaderPipeline > gfxPso  = RenderCore::CreateVertexPixelShaderPipeline( "resources/shaders/foo.hlsl" );
SharedPtr< ShaderPipeline > csPso   = RenderCore::CreateComputeShaderPipeline( "resources/shaders/foo.hlsl" );
SharedPtr< RtPipelineState > rtPso  = RenderCore::CreateRtPipelineState( rtProps );
```

### Command lists

```cpp
CommandList * ctx = RenderCore::BeginCommandList( CommandListType::Graphics );
// ... record commands ...
RenderCore::ExecuteAndFreeCommandList( ctx );          // async submit (default)
RenderCore::ExecuteAndFreeCommandList( ctx, SyncMode::BLOCKING ); // CPU stall
```

`CommandListType` values: `Graphics`, `Compute`, `DMA`.

---

## CommandList API

`CommandList` (`Rendering/CommandList.h`) records all GPU work.

Key patterns:

```cpp
// State setup before draw
ctx->SetShaderPipeline( myPso.get() );
ctx->SetRenderTarget( colorView, depthView );
ctx->SetViewport( glm::uvec4( 0, 0, width, height ) );
ctx->SetClipRect( glm::uvec4( 0, 0, width, height ) );
ctx->SetCullMode( CullMode::NONE );
ctx->SetFillMode( FillMode::SOLID );
ctx->SetTopologyType( TopologyType::TRIANGLE_LIST );

// Upload small constant data inline (uses ring buffer internally)
ctx->BindConstantBuffer( &myCbufData, sizeof( myCbufData ), 0 /*register*/ );

// Bind geometry buffers
ctx->BindVertexBuffers( &vtxBuf, &offset, &size, 1u, layout );
ctx->BindIndexBuffer( idxBuf, elementSizeBytes );
ctx->DrawIndexedInstanced( numIndices, numInstances, 0, 0, 0 );

// Compute dispatch
ctx->PrepareResourceShaderAccess( uavView );          // makes resource accessible to shader
ctx->SetShaderPipeline( myCsPso.get() );
ctx->BindConstantBuffer( &cbuf, sizeof( cbuf ), 0 );
ctx->Dispatch( glm::ivec3( width, height, 1 ) );
ctx->ResourceUAVbarrier( tex );                       // UAV → UAV dependency

// RT dispatch
ctx->SetRaytracingPipelineState( myRtPso.get() );
ctx->BindConstantBuffer( &rtConsts, sizeof( rtConsts ), 0 );
ctx->DispatchRays( dispatchDesc );
```

**Resource access before shader reads/writes:** call `PrepareResourceShaderAccess( view )` before binding a view whose state isn't known (first use in a frame or after a UAV write).

---

## Bindless Resource Model (HLSL side)

All textures, buffers, and samplers live in global unbounded arrays declared in `fancy/resources/shaders/GlobalResources.h`.  
Include it in every shader that needs resources:

```hlsl
#include "fancy/resources/shaders/GlobalResources.h"

// Read a float4 2-D texture by its runtime index
float4 val = theTextures2D[myDescriptorIndex].Sample(theSamplers[mySamplerIdx], uv);

// Read a byte-address buffer (raw/structured data)
MyStruct s = theBuffers[myBufferIndex].Load<MyStruct>(byteOffset);

// Write to a UAV texture
theRwTextures2D[myWriteIdx][dispatchThreadId.xy] = result;

// Raytracing acceleration structure
RayQuery<...> q; q.TraceRayInline(theRtAccelerationStructures[myAsIndex], ...);
```

Global array names: `theTextures2D`, `theBuffers`, `theRwTextures2D`, `theRwBuffers`, `theSamplers`, `theRtAccelerationStructures`, etc. — see `GlobalResources.h` for the full list.

**Register spaces** (HLSL):
| Space | Meaning |
|---|---|
| `Space_LocalBuffer` (`space0`) | per-draw read buffers `t`-register |
| `Space_LocalRWBuffer` (`space1`) | per-draw UAV buffers `u`-register |
| `Space_LocalCBuffer` (`space2`) | per-draw constant buffers `b`-register |
| `space3`–`space26` | global resource arrays |
| `space27` | global sampler array |

Descriptor indices are obtained at C++ side via `view->GetGlobalDescriptorIndex()`.

---

## Buffer & Texture Properties Recipes

### Shader-read-only GPU buffer (SRV, raw)
```cpp
GpuBufferProperties bufProps;
bufProps.myBindFlags      = (uint) GpuBufferBindFlags::SHADER_BUFFER;
bufProps.myNumElements    = count;
bufProps.myElementSizeBytes = sizeof( MyStruct );

GpuBufferViewProperties viewProps;
viewProps.myIsRaw = true;   // ByteAddressBuffer in HLSL

SharedPtr< GpuBufferView > view =
    RenderCore::CreateBufferView( bufProps, viewProps, "MyBuf", cpuInitData );
```

### Shader-writable texture (UAV + SRV)
```cpp
TextureProperties texProps;
texProps.myWidth           = width;
texProps.myHeight          = height;
texProps.myFormat          = DataFormat::RGBA_16F;
texProps.myIsShaderWritable = true;
texProps.myIsRenderTarget  = false;

TextureViewProperties rtvProps;   // SRV
TextureViewProperties uavProps;
uavProps.myIsShaderWritable = true;

SharedPtr< TextureView > srvView = RenderCore::CreateTextureView( tex, rtvProps, "MyTex_SRV" );
SharedPtr< TextureView > uavView = RenderCore::CreateTextureView( tex, uavProps, "MyTex_UAV" );
```

---

## Temporary (frame-lifetime) Resources

Use `RenderCore::AllocateTempTexture` / `AllocateTempBuffer` for transient per-frame resources.  
They are returned via `TempTextureResource` / `TempBufferResource` which hold raw pointers `myTexture`, `myReadView`, `myWriteView`, `myRenderTargetView`. Lifetime is managed by a `SharedPtr< TempResourceKeepAlive >`.

---

## Scene & IO

| Type | Location | Purpose |
|---|---|---|
| `MeshImporter` | `IO/MeshImporter.h` | Imports .obj/.fbx into `SceneData` |
| `SceneData` | `IO/Scene.h` | Raw data: mesh arrays, instances, materials, vertex layout |
| `Scene` | `IO/Scene.h` | GPU-side scene: `SharedPtr<Mesh>`, `SharedPtr<Material>`, `SharedPtr<VertexInputLayout>` |
| `Assets` | `IO/Assets.h` | Asset manager/cache |
| `PathService` | `IO/PathService.h` | Resolves paths relative to the app root |

`MeshImporter::Import` takes a path and a `fixed_vector` of `VertexShaderAttributeDesc` to specify which attributes to extract and in what format.

---

## Raytracing Setup Pattern

```cpp
// 1. Create BLAS per mesh
RenderCore::CreateRtBottomLevelAccelerationStructure( geometryDatas.data(), count, flags, "BLAS_name" );

// 2. Create TLAS from instance list
RenderCore::CreateRtTopLevelAccelerationStructure( instanceDatas.data(), count, flags, "TLAS" );

// 3. Build RT pipeline state
RtPipelineStateProperties rtProps;
uint raygenIdx = rtProps.AddRayGenShader( "path/to/shader.hlsl", "RayGen" );
uint hitIdx    = rtProps.AddHitGroup( L"HitGroup0", RT_HIT_GROUP_TYPE_TRIANGLES, nullptr, nullptr,
                                       nullptr, nullptr, "path/to/shader.hlsl", "ClosestHit" );
rtProps.SetMaxPayloadSize( 128u );
rtProps.SetMaxAttributeSize( 32u );
rtProps.SetMaxRecursionDepth( RenderCore::GetPlatformCaps().myRaytracingMaxRecursionDepth );
SharedPtr< RtPipelineState > rtPso = RenderCore::CreateRtPipelineState( rtProps );

// 4. Build SBT
RtShaderBindingTableProperties sbtProps;
sbtProps.myNumRaygenShaderRecords = 1;
sbtProps.myNumMissShaderRecords   = 5;
sbtProps.myNumHitShaderRecords    = 5;
SharedPtr< RtShaderBindingTable > sbt = RenderCore::CreateRtShaderTable( sbtProps );
sbt->AddShaderRecord( rtPso->GetRayGenShaderIdentifier( raygenIdx ) );
sbt->AddShaderRecord( rtPso->GetHitShaderIdentifier( hitIdx ) );
```

Hot-reload of RT pipelines is signalled through `RenderCore::ourOnRtPipelineStateRecompiled` (a `Slot<>`).

---

## Event System (Slot / Callback)

`Slot< ReturnT( Args... ) >` is a multicast delegate in `Common/Slot.h`.

```cpp
// Declaration (usually a public static on the owner class)
static Slot< void( const RtPipelineState * ) > ourOnRtPipelineStateRecompiled;

// Subscribe (in constructor or Init())
RenderCore::ourOnRtPipelineStateRecompiled.Connect( this, &PathTracer::OnRtPipelineRecompiled );

// Unsubscribe (in destructor)
RenderCore::ourOnRtPipelineStateRecompiled.DetachObserver( this );

// Fire (by the owner)
ourOnRtPipelineStateRecompiled( pipelinePtr );
```

---

## Logging, Assertions & Profiling

```cpp
// Logging — defined in Debug/Log.h (included via fancy_core_precompile.h)
Log( "Simple message %d", value );
LOG_WARNING( "Something fishy: %s", name );
LOG_ERROR( "Fatal: %s", msg );

// Assertions — use ASSERT, never raw assert()
ASSERT( ptr != nullptr );

// GPU profiler markers (visible in PIX/RenderDoc)
GPU_SCOPED_PROFILER_FUNCTION( ctx, 0u );   // scoped region named after __FUNCTION__
```

---

## Utility Types

| Type | Header | Use |
|---|---|---|
| `StaticString< N >` | `Common/StaticString.h` | Stack-allocated formatted string (`.Format(...)`, `.Append(...)`, `.GetBuffer()`) |
| `StaticFilePath` | same | `StaticString< 260 >` alias for paths |
| `CircularStringBuffer` | same | Pool-backed temp string formatting |
| `SharedPtr< T >` / `UniquePtr< T >` | `Common/Ptr.h` | EASTL smart pointers |
| `FixedShortString` | `FancyCoreDefines.h` | `eastl::fixed_string< char, 32 >` |

`ARRAY_LENGTH( arr )` — safe compile-time array size (preferred over `sizeof`).  
`VECTOR_BYTESIZE( vec )` — `vec.size() * sizeof(element)`.  
`BITS_TO_BYTES( bits )` — bits / 8.
