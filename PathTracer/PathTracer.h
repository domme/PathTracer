#pragma once

#include <EASTL/vector.h>

#include "Sky_Imgui.h"
#include "Common/Application.h"
#include "Rendering/ResourceHandle.h"
#include "DebugTextureList.h"

class Sky;

namespace Fancy {
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
}  // namespace Fancy

using namespace Fancy;

struct BlasData {
  RtAccelerationStructureHandle myBLAS;
  GpuBufferHandle               myTriangleIndicesBuf;
  GpuBufferViewHandle           myTriangleIndices;
  GpuBufferHandle               myVertexDataBuf;
  GpuBufferViewHandle           myVertexData;
};

struct RaytracingScene {
  ~RaytracingScene();

  GpuBufferHandle            myInstanceDataBuf;
  GpuBufferViewHandle        myInstanceData;
  GpuBufferHandle            myMaterialDataBuf;
  GpuBufferViewHandle        myMaterialData;
  GpuBufferHandle            myHaltonSamplesBuf;
  GpuBufferViewHandle        myHaltonSamples;
  RtPipelineStateHandle      myRtPso;
  RtShaderBindingTableHandle mySBT;
  RtPipelineStateHandle      myAoRtPso;
  RtShaderBindingTableHandle myAoSBT;

  RtAccelerationStructureHandle myTLAS;
  eastl::vector< BlasData >     myBlasDatas;
};

class PathTracer : public Fancy::Application {
public:
  PathTracer( HINSTANCE anInstanceHandle, const char ** someArguments, uint aNumArguments, const char * aName,
              const Fancy::RenderPlatformProperties & someRenderProperties, const Fancy::WindowParameters & someWindowParams );

  void LoadScene( const char * aPath, const glm::float3 & aCamPos );
  void InitSky();
  void InitRtScene( const SceneData & aScene );
  void InitSampleSequences();

  ~PathTracer() override;
  void OnWindowResized( uint aWidth, uint aHeight ) override;
  void BeginFrame() override;
  void Update() override;
  void Render() override;
  void EndFrame() override;

private:
  void OnRtPipelineRecompiled( const RtPipelineState * aRtPipeline );
  void UpdateOutputTexture();
  void UpdateDepthbuffer();
  void RestartAccumulation();
  bool CameraHasChanged();

  void UpdateMainMenuBar();
  void UpdatePathTracingSettings();

  void RenderRaster( CommandList * ctx );
  void RenderRT( CommandList * ctx );
  void TonemapComposit( CommandList * ctx );

  UniquePtr< Sky > mySky;
  Sky_Imgui        mySky_Imgui;

  SharedPtr< Scene >   myScene;
  ShaderPipelineHandle myUnlitMeshShader;
  ShaderPipelineHandle myTonemapCompositShader;
  ShaderPipelineHandle myClearTextureShader;

  UniquePtr< RaytracingScene > myRtScene;

  TextureHandle     myHdrLightTex;
  TextureViewHandle myHdrLightTexRtv;
  TextureViewHandle myHdrLightTexWrite;
  TextureViewHandle myHdrLightTexRead;

  TextureHandle     myDepthStencilTex;
  TextureViewHandle myDepthStencilDsv;

  DepthStencilStateHandle myDepthTestOff;

  TextureViewHandle myDdsTestSrv;

  float         myAoDistance = 1.0f;
  uint          myNumAccumulationFrames = 0u;
  bool          myAccumulationNeedsClear = true;
  glm::float4x4 myLastViewMat;

  ImGuiContext * myImGuiContext = nullptr;
  bool           myRenderRaster = false;
  bool           myRenderAo = false;
  bool           myAccumulate = true;
  bool           myHalfResRender = true;
  bool           mySampleSky = true;
  float          mySkyFallbackIntensity = 100.0f;
  int            myMaxRecursionDepth = 4;
  int            myLightInstanceIdx = 4;
  float64        myLastTimeMs = 0.0;
  bool           mySupportsRaytracing = false;
  bool           myLightEnabled = true;
  glm::float3    myLightColor = glm::float3( 1.0f );
  float          myLightStrength = 100.0f;
  float          myPhongSpecularPower = 10.0f;

  UniquePtr< ImGuiMippedDebugImage > myDdsDebugImage;
  DebugTextureList                   myTextureList;
};