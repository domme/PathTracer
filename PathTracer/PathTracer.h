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

  eastl::vector<SharedPtr<RtAccelerationStructure>> myBLAS;
  SharedPtr<RtAccelerationStructure> myTLAS;
  SharedPtr<RtPipelineState> myRtPso;
  SharedPtr<RtShaderBindingTable> mySBT;
  SharedPtr<GpuBufferView> myPerInstanceData;
  SharedPtr<GpuBufferView> myMaterialBuffer;
  SharedPtr<TextureView> myRtOutTextureRW;
  SharedPtr<TextureView> myDepthStencilDsv;

  ImGuiContext* myImGuiContext = nullptr;
  bool myRenderRaster = false;
};

