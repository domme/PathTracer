#pragma once

#include <EASTL/vector.h>

#include "Common/Application.h"
#include "Common/Ptr.h"

namespace Fancy
{
  class GpuBufferView;
  struct SceneData;
  class RtShaderBindingTable;
  class RtPipelineState;
  class RtAccelerationStructure;
  struct Scene;
  class ShaderPipeline;
  class Window;
  class FancyRuntime;
  class RenderOutput;
  struct RenderPlatformProperties;
  struct WindowParameters;
  class TextureView;
  class Texture;
}

using namespace Fancy;

struct BlasData
{
  SharedPtr<RtAccelerationStructure> myBLAS;
  SharedPtr<GpuBufferView> myTriangleIndices;
  SharedPtr<GpuBufferView> myVertexData;
};

struct RaytracingScene
{
  SharedPtr<GpuBufferView> myInstanceData;
  SharedPtr<GpuBufferView> myMaterialData;
  SharedPtr<RtPipelineState> myRtPso;
  SharedPtr<RtShaderBindingTable> mySBT;

  SharedPtr<RtAccelerationStructure> myTLAS;
  eastl::vector<BlasData> myBlasDatas;
};

class PathTracer : public Fancy::Application
{
public:
  PathTracer(HINSTANCE anInstanceHandle,
    const char** someArguments,
    uint aNumArguments,
    const char* aName,
    const Fancy::RenderPlatformProperties& someRenderProperties,
    const Fancy::WindowParameters& someWindowParams);

  void InitRtScene(const SceneData& aScene);

  ~PathTracer() override;
  void OnWindowResized(uint aWidth, uint aHeight) override;
  void BeginFrame() override;
  void Update() override;
  void Render() override;
  void EndFrame() override;

private:
  void UpdateDepthbuffer();

  void RenderRaster();
  void RenderRT();

  Fancy::SharedPtr<Fancy::Scene> myScene;
  Fancy::SharedPtr<Fancy::ShaderPipeline> myUnlitMeshShader;

  RaytracingScene myRtScene;
  
  SharedPtr<TextureView> myRtOutTextureRW;
  SharedPtr<TextureView> myDepthStencilDsv;
  
  ImGuiContext* myImGuiContext = nullptr;
  bool myRenderRaster = false;
};

