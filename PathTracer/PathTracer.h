#pragma once

#include <EASTL/vector.h>

#include "Sky_Imgui.h"
#include "Common/Application.h"
#include "Common/Ptr.h"

class Sky;

namespace Fancy
{
  class DepthStencilState;
  class GpuBufferView;
  struct SceneData;
  class RtShaderBindingTable;
  class RtPipelineState;
  class RtAccelerationStructure;
  struct Scene;
  class ShaderPipeline;
  class Window;
  class RenderOutput;
  struct RenderPlatformProperties;
  struct WindowParameters;
  class TextureView;
  class Texture;
  class CommandList;
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
  SharedPtr<GpuBufferView> myHaltonSamples;
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

  void InitSky();
  void InitRtScene(const SceneData& aScene);
  void InitSampleSequences();

  ~PathTracer() override;
  void OnWindowResized(uint aWidth, uint aHeight) override;
  void BeginFrame() override;
  void Update() override;
  void Render() override;
  void EndFrame() override;

private:
  void OnRtPipelineRecompiled(const RtPipelineState* aRtPipeline);
  void UpdateOutputTexture();
  void UpdateDepthbuffer();
  void RestartAccumulation();
  bool CameraHasChanged();

  void RenderRaster(CommandList* ctx);
  void RenderRT(CommandList* ctx);
  void TonemapComposit(CommandList* ctx);

  SharedPtr<Sky> mySky;
  Sky_Imgui mySky_Imgui;

  SharedPtr<Scene> myScene;
  SharedPtr<ShaderPipeline> myUnlitMeshShader;
  SharedPtr<ShaderPipeline> myTonemapCompositShader;
  SharedPtr<ShaderPipeline> myClearTextureShader;

  RaytracingScene myRtScene;

  SharedPtr<TextureView> myHdrLightTexRtv;
  SharedPtr<TextureView> myHdrLightTexWrite;
  SharedPtr<TextureView> myHdrLightTexRead;
  SharedPtr<TextureView> myDepthStencilDsv;
  SharedPtr<DepthStencilState> myDepthTestOff;

  float myAoDistance = 50.0f;
  uint myNumAccumulationFrames = 0u;
  bool myAccumulationNeedsClear = true;
  glm::float4x4 myLastViewMat;
  
  ImGuiContext* myImGuiContext = nullptr;
  bool myRenderRaster = false;
  int myMaxNumAccumulationFrames = 50;
};

