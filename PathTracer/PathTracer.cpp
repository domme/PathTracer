#include "PathTracer.h"

#include "imgui.h"
#include "imgui_impl_fancy.h"
#include "Sky.h"
#include "Common/Ptr.h"
#include "Common/StringUtil.h"
#include "Common/Window.h"
#include "Common/StaticString.h"
#include "Debug/Profiler.h"
#include "IO/MeshImporter.h"
#include "Rendering/CommandList.h"
#include "Rendering/DepthStencilState.h"
#include "Rendering/GraphicsResources.h"
#include "Rendering/RenderCore.h"
#include "Rendering/RenderOutput.h"
#include "Rendering/RtAccelerationStructure.h"
#include "Rendering/ShaderPipelineDesc.h"
#include "Rendering/Texture.h"

using namespace Fancy;

PathTracer::PathTracer(HINSTANCE anInstanceHandle, const char** someArguments, uint aNumArguments, const char* aName,
  const Fancy::RenderPlatformProperties& someRenderProperties, const Fancy::WindowParameters& someWindowParams)
  : Application(anInstanceHandle, someArguments, aNumArguments, aName, "../../../../", someRenderProperties, someWindowParams)
  , myImGuiContext(ImGui::CreateContext())
{
  ImGuiRendering::Init(myRenderOutput);

  mySupportsRaytracing = RenderCore::GetPlatformCaps().mySupportsRaytracing;

  DepthStencilStateProperties dsProps;
  dsProps.myDepthTestEnabled = false;
  dsProps.myDepthWriteEnabled = false;
  dsProps.myStencilEnabled = false;
  dsProps.myStencilWriteMask = 0u;
  myDepthTestOff = RenderCore::CreateDepthStencilState(dsProps);

  RenderCore::ourOnRtPipelineStateRecompiled.Connect(this, &PathTracer::OnRtPipelineRecompiled);

  UpdateDepthbuffer();
  UpdateOutputTexture();

  myUnlitMeshShader = RenderCore::CreateVertexPixelShaderPipeline("resources/shaders/unlit_mesh.hlsl");
  ASSERT(myUnlitMeshShader);

  myTonemapCompositShader = RenderCore::CreateVertexPixelShaderPipeline("resources/shaders/tonemap_composit.hlsl");
  ASSERT(myTonemapCompositShader);

  myClearTextureShader = RenderCore::CreateComputeShaderPipeline("resources/shaders/clear_texture.hlsl");
  ASSERT(myClearTextureShader);

  eastl::fixed_vector<VertexShaderAttributeDesc, 16> vertexAttributes = {
    { VertexAttributeSemantic::POSITION, 0, DataFormat::RGB_32F },
    { VertexAttributeSemantic::NORMAL, 0, DataFormat::RGB_32F },
    { VertexAttributeSemantic::TEXCOORD, 0, DataFormat::RG_32F }
  };

  SceneData sceneData;
  MeshImporter importer;
  const bool importSuccess = importer.Import("resources/models/CornellBox.obj", vertexAttributes, sceneData);
  ASSERT(importSuccess);
  
  if (mySupportsRaytracing)
    InitRtScene(sceneData);

  myScene = eastl::make_shared<Scene>(sceneData, myAssetManager.get());

  myCamera.myPosition = glm::float3(1.0f, 102.0f, -30.0f);
  myCamera.myOrientation = glm::quat_cast(glm::lookAt(glm::float3(0.0f, 0.0f, 10.0f), glm::float3(0.0f, 0.0f, 0.0f), glm::float3(0.0f, 1.0f, 0.0f)));

  myCameraController.myMoveSpeed = 5.0f;

  myCamera.myFovDeg = 60.0f;
  myCamera.myNear = 1.0f;
  myCamera.myFar = 10000.0f;
  myCamera.myWidth = (float) myRenderOutput->GetWindow()->GetWidth();
  myCamera.myHeight = (float) myRenderOutput->GetWindow()->GetHeight();
  myCamera.myIsOrtho = false;

  myCamera.UpdateView();
  myCamera.UpdateProjection();

  InitSky();
}

void PathTracer::InitSky()
{
  SkyParameters skyParams;
  mySky.reset(new Sky(skyParams));
}

glm::uvec2 GetOffsetSize(const VertexInputLayoutProperties& someVertexProps, VertexAttributeSemantic aSemantic, uint aSemanticIndex)
{
  uint offset = 0;
  for (const VertexInputAttributeDesc& attribute : someVertexProps.myAttributes)
  {
    uint size = DataFormatInfo::GetFormatInfo(attribute.myFormat).mySizeBytes;
    if (attribute.mySemantic == aSemantic && attribute.mySemanticIndex == aSemanticIndex)
    {
      return { offset, size };
    }
    offset += size;
  }

  ASSERT(false);
  return glm::uvec2(0, 0);
}

void PathTracer::InitRtScene(const SceneData& aScene)
{
  for (uint iMesh = 0u; iMesh < (uint) aScene.myMeshes.size(); ++iMesh)
  {
    const MeshData& mesh = aScene.myMeshes[iMesh];

    eastl::fixed_vector<RtAccelerationStructureGeometryData, 4> meshPartsGeometryDatas;
    
    uint numMeshVertices = 0u;
    uint numMeshTriangles = 0u;
    
    for (const MeshPartData& meshPart : mesh.myParts)
    {
      const VertexInputLayoutProperties& vertexProps = meshPart.myVertexLayoutProperties;
      ASSERT(!vertexProps.myAttributes.empty() && vertexProps.myAttributes[0].mySemantic == VertexAttributeSemantic::POSITION);  // Assume there is no offset from the start of the vertex data to the first position
      ASSERT(vertexProps.myBufferBindings.size() == 1u);  // Assume the mesh is using only one interleaved buffer

      const uint numVertices = VECTOR_BYTESIZE(meshPart.myVertexData) / vertexProps.GetOverallVertexSize();

      RtAccelerationStructureGeometryData& geometryData = meshPartsGeometryDatas.push_back();
      geometryData.myType = RtAccelerationStructureGeometryType::TRIANGLES;
      geometryData.myFlags = (uint)RtAccelerationStructureGeometryFlags::OPAQUE_GEOMETRY;
      geometryData.myVertexFormat = vertexProps.myAttributes[0].myFormat;
      geometryData.myNumVertices = numVertices;
      geometryData.myVertexData.myType = RT_BUFFER_DATA_TYPE_CPU_DATA;
      geometryData.myVertexStride = vertexProps.GetOverallVertexSize();
      geometryData.myVertexData.myCpuData.myData = meshPart.myVertexData.data();
      geometryData.myVertexData.myCpuData.myDataSize = VECTOR_BYTESIZE(meshPart.myVertexData);

      geometryData.myIndexFormat = DataFormat::R_32UI;
      geometryData.myNumIndices = VECTOR_BYTESIZE(meshPart.myIndexData) / sizeof(uint);
      geometryData.myIndexData.myType = RT_BUFFER_DATA_TYPE_CPU_DATA;
      geometryData.myIndexData.myCpuData.myData = meshPart.myIndexData.data();
      geometryData.myIndexData.myCpuData.myDataSize = VECTOR_BYTESIZE(meshPart.myIndexData);

      numMeshVertices += numVertices;
      numMeshTriangles += geometryData.myNumIndices / 3;
    }

    struct VertexData
    {
      glm::float3 myNormal;
      glm::float2 myUv;
    };

    glm::uvec2 normalOffsetSize = GetOffsetSize(aScene.myVertexInputLayoutProperties, VertexAttributeSemantic::NORMAL, 0u);
    glm::uvec2 uvOffsetSize = GetOffsetSize(aScene.myVertexInputLayoutProperties, VertexAttributeSemantic::TEXCOORD, 0u);
    ASSERT(normalOffsetSize.y == sizeof(glm::float3));
    ASSERT(uvOffsetSize.y == sizeof(glm::float2));
    
    eastl::vector<VertexData> vertexData;
    vertexData.reserve(numMeshVertices);

    eastl::vector<glm::uvec3> triangleIndices;
    triangleIndices.resize(numMeshTriangles);
    glm::uvec3* dstTrianglePtr = triangleIndices.data();

    for (const MeshPartData& meshPart : mesh.myParts)
    {
      const VertexInputLayoutProperties& vertexProps = meshPart.myVertexLayoutProperties;

      const uint srcVertexStride = vertexProps.GetOverallVertexSize();
      const uint numVertices = VECTOR_BYTESIZE(meshPart.myVertexData) / srcVertexStride;
      const uint8* srcData = meshPart.myVertexData.data();
      for (uint i = 0u; i < numVertices; ++i)
      {
        VertexData& dstData = vertexData.push_back();
        memcpy(&dstData.myNormal, srcData + normalOffsetSize.x, sizeof(dstData.myNormal));
        memcpy(&dstData.myUv, srcData + uvOffsetSize.x, sizeof(dstData.myUv));
        srcData += srcVertexStride;
      }

      memcpy(dstTrianglePtr, meshPart.myIndexData.data(), VECTOR_BYTESIZE(meshPart.myIndexData));
      dstTrianglePtr += VECTOR_BYTESIZE(meshPart.myIndexData);
    }

    BlasData& blasData = myRtScene.myBlasDatas.push_back();

    GpuBufferProperties bufferProps;
    bufferProps.myBindFlags = (uint) GpuBufferBindFlags::SHADER_BUFFER;
    bufferProps.myNumElements = numMeshVertices;
    bufferProps.myElementSizeBytes = sizeof(VertexData);
    GpuBufferViewProperties bufferViewProps;
    bufferViewProps.myIsRaw = true;
    StaticString<64> name("Rt mesh vertexData %d", iMesh);
    blasData.myVertexData = RenderCore::CreateBufferView(bufferProps, bufferViewProps, name.GetBuffer(), vertexData.data());

    bufferProps.myNumElements = numMeshTriangles;
    bufferProps.myElementSizeBytes = sizeof(glm::uvec3);
    name.Format("Rt mesh triangles %d", iMesh);
    blasData.myTriangleIndices = RenderCore::CreateBufferView(bufferProps, bufferViewProps, name.GetBuffer(), triangleIndices.data());

    name.Format("BLAS mesh %d", iMesh);
    blasData.myBLAS = RenderCore::CreateRtBottomLevelAccelerationStructure(meshPartsGeometryDatas.data(), meshPartsGeometryDatas.size(), 0u, name.GetBuffer());
    ASSERT(blasData.myBLAS != nullptr);
  }

  struct PerInstanceData
  {
    uint myIndexBufferDescriptorIndex;
    uint myVertexBufferDescriptorIndex;
    uint myMaterialIndex;
  };
  eastl::vector<PerInstanceData> perInstanceDatas;
  perInstanceDatas.reserve((uint)aScene.myInstances.size());
    
  eastl::fixed_vector<RtAccelerationStructureInstanceData, 16> instanceDatas;
  for (uint iInstance = 0u; iInstance < (uint) aScene.myInstances.size(); ++iInstance)
  {
    const SceneMeshInstance& instance = aScene.myInstances[iInstance];

    PerInstanceData& perInstanceData = perInstanceDatas.push_back();
    perInstanceData.myMaterialIndex = instance.myMaterialIndex;
    perInstanceData.myIndexBufferDescriptorIndex = myRtScene.myBlasDatas[instance.myMeshIndex].myTriangleIndices->GetGlobalDescriptorIndex();
    perInstanceData.myVertexBufferDescriptorIndex = myRtScene.myBlasDatas[instance.myMeshIndex].myVertexData->GetGlobalDescriptorIndex();

    RtAccelerationStructureInstanceData& instanceData = instanceDatas.push_back();
    instanceData.myInstanceId = iInstance;
    instanceData.mySbtHitGroupOffset = 0;
    instanceData.myInstanceBLAS = myRtScene.myBlasDatas[instance.myMeshIndex].myBLAS;
    instanceData.myInstanceMask = UINT8_MAX;
    instanceData.myTransform = instance.myTransform;
    instanceData.myFlags = RT_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE | RT_INSTANCE_FLAG_FORCE_OPAQUE;
  }

  GpuBufferProperties bufferProps;
  bufferProps.myBindFlags = (uint)GpuBufferBindFlags::SHADER_BUFFER;
  bufferProps.myNumElements = (uint) perInstanceDatas.size();
  bufferProps.myElementSizeBytes = sizeof(PerInstanceData);
  GpuBufferViewProperties bufferViewProps;
  bufferViewProps.myIsRaw = true;
  myRtScene.myInstanceData = RenderCore::CreateBufferView(bufferProps, bufferViewProps, "Rt per instance data", perInstanceDatas.data());

  struct MaterialData
  {
    glm::float3 myEmission;
    uint myColor;
  };
  eastl::vector<MaterialData> materialDatas;
  materialDatas.reserve(aScene.myMaterials.size());

  for (const MaterialDesc& mat : aScene.myMaterials)
  {
    MaterialData& matData = materialDatas.push_back();
    matData.myEmission = mat.myParameters[(uint)MaterialParameterType::EMISSION];
    matData.myColor = MathUtil::Encode_Unorm_RGBA(mat.myParameters[(uint)MaterialParameterType::COLOR]);
  }

  bufferProps.myElementSizeBytes = sizeof(MaterialData);
  bufferProps.myNumElements = materialDatas.size();
  bufferViewProps.myIsRaw = true;
  myRtScene.myMaterialData = RenderCore::CreateBufferView(bufferProps, bufferViewProps, "Rt material buffer", materialDatas.data());

  myRtScene.myTLAS = RenderCore::CreateRtTopLevelAccelerationStructure(instanceDatas.data(), (uint) instanceDatas.size(), 0, "TLAS");

  // Ao RT pipeline + SBT
  {
    RtPipelineStateProperties rtPipelineProps;
    const uint raygenIdx = rtPipelineProps.AddRayGenShader("resources/shaders/raytracing/Ao.hlsl", "RayGen");
    const uint hitIdxPrimary = rtPipelineProps.AddHitGroup(L"HitGroup0", RT_HIT_GROUP_TYPE_TRIANGLES, nullptr,
      nullptr, nullptr, nullptr, "resources/shaders/raytracing/Ao.hlsl", "ClosestHitPrimary");
    const uint hitIdxAo = rtPipelineProps.AddHitGroup(L"HitGroup1", RT_HIT_GROUP_TYPE_TRIANGLES, nullptr,
      nullptr, nullptr, nullptr, "resources/shaders/raytracing/Ao.hlsl", "ClosestHitAo");
    rtPipelineProps.SetMaxAttributeSize(32u);
    rtPipelineProps.SetMaxPayloadSize(128u);
    rtPipelineProps.SetMaxRecursionDepth(RenderCore::GetPlatformCaps().myRaytracingMaxRecursionDepth);
    myRtScene.myAoRtPso = RenderCore::CreateRtPipelineState(rtPipelineProps);

    RtShaderBindingTableProperties sbtProps;
    sbtProps.myNumRaygenShaderRecords = 1;
    sbtProps.myNumMissShaderRecords = 5;
    sbtProps.myNumHitShaderRecords = 5;
    myRtScene.myAoSBT = RenderCore::CreateRtShaderTable(sbtProps);
    myRtScene.myAoSBT->AddShaderRecord(myRtScene.myAoRtPso->GetRayGenShaderIdentifier(raygenIdx));
    myRtScene.myAoSBT->AddShaderRecord(myRtScene.myAoRtPso->GetHitShaderIdentifier(hitIdxPrimary));
    myRtScene.myAoSBT->AddShaderRecord(myRtScene.myAoRtPso->GetHitShaderIdentifier(hitIdxAo));
  }

  // PathTracing RT pipeline + SBT
  {
    RtPipelineStateProperties rtPipelineProps;
    const uint raygenIdx = rtPipelineProps.AddRayGenShader("resources/shaders/raytracing/PathTracing.hlsl", "RayGen");
    const uint hitIdx = rtPipelineProps.AddHitGroup(L"HitGroup0", RT_HIT_GROUP_TYPE_TRIANGLES, nullptr,
      nullptr, nullptr, nullptr, "resources/shaders/raytracing/PathTracing.hlsl", "ClosestHit");
    rtPipelineProps.SetMaxAttributeSize(32u);
    rtPipelineProps.SetMaxPayloadSize(128u);
    rtPipelineProps.SetMaxRecursionDepth(RenderCore::GetPlatformCaps().myRaytracingMaxRecursionDepth);
    myRtScene.myRtPso = RenderCore::CreateRtPipelineState(rtPipelineProps);

    RtShaderBindingTableProperties sbtProps;
    sbtProps.myNumRaygenShaderRecords = 1;
    sbtProps.myNumMissShaderRecords = 5;
    sbtProps.myNumHitShaderRecords = 5;
    myRtScene.mySBT = RenderCore::CreateRtShaderTable(sbtProps);
    myRtScene.mySBT->AddShaderRecord(myRtScene.myRtPso->GetRayGenShaderIdentifier(raygenIdx));
    myRtScene.mySBT->AddShaderRecord(myRtScene.myRtPso->GetHitShaderIdentifier(hitIdx));
  }
  

  InitSampleSequences();
}

void PathTracer::InitSampleSequences()
{
  uint sampleCount = 8192;
  eastl::vector<glm::float2> someSamples;
  someSamples.reserve(sampleCount);

  for (uint i = 0; i < sampleCount; ++i)
    someSamples.push_back({ MathUtil::Halton(i, 2), MathUtil::Halton(i, 3) });

  GpuBufferProperties props;
  props.myBindFlags = (uint)GpuBufferBindFlags::SHADER_BUFFER;
  props.myElementSizeBytes = sizeof(glm::float2);
  props.myNumElements = sampleCount;
  GpuBufferViewProperties viewProps;
  viewProps.myIsRaw = true;
  myRtScene.myHaltonSamples = RenderCore::CreateBufferView(props, viewProps, "Halton samples", someSamples.data());
}

PathTracer::~PathTracer()
{
  RenderCore::ourOnRtPipelineStateRecompiled.DetachObserver(this);
  ImGuiRendering::Shutdown();
  ImGui::DestroyContext(myImGuiContext);
  myImGuiContext = nullptr;
}

void PathTracer::OnWindowResized(uint aWidth, uint aHeight)
{
  Application::OnWindowResized(aWidth, aHeight);
  UpdateDepthbuffer();
  UpdateOutputTexture();
  RestartAccumulation();
}

bool PathTracer::CameraHasChanged()
{
  for (int i = 0; i < 4; ++i)
    for (int k = 0; k < 4; ++k)
      if (glm::abs(myCamera.myViewProj[i][k] - myLastViewMat[i][k]) > 0.0001f)
        return true;

  return false;
}

void PathTracer::BeginFrame()
{
  Application::BeginFrame();
  ImGuiRendering::NewFrame();
}

static float64 SampleTimeMs()
{
  const std::chrono::duration<float64, std::milli> now(std::chrono::system_clock::now().time_since_epoch());
  return now.count();
}

void PathTracer::Update()
{
  Application::Update();
  bool skyParamsChanged = mySky_Imgui.Update(mySky.get());

  ImGui::Text("View Pos: %.3f, %.3f, %.3f", myCamera.myPosition.x, myCamera.myPosition.y, myCamera.myPosition.z);
  ImGui::SliderFloat("Move Speed", &myCameraController.myMoveSpeed, 1.0f, 10000.0f);

  float64 msNow = SampleTimeMs();
  float64 frameTime = msNow - myLastTimeMs;
  myLastTimeMs = msNow;
  ImGui::Text("Frame Time: %.3f ms", (float)frameTime);

  if (ImGui::Checkbox("Render Half Res", &myHalfResRender))
  {
    UpdateOutputTexture();
    RestartAccumulation();
  }
   
  if (mySupportsRaytracing)
  {
    if (ImGui::Checkbox("Render Raster", &myRenderRaster) && !myRenderRaster)
      RestartAccumulation();

    if (!myRenderRaster)
    {
      if (ImGui::Checkbox("Render AO", &myRenderAo))
        RestartAccumulation();
      
      if (ImGui::Checkbox("Accumulate", &myAccumulate))
        RestartAccumulation();

      if (ImGui::Checkbox("Sample Sky", &mySampleSky))
        RestartAccumulation();

      if (!mySampleSky)
      {
        if (ImGui::SliderFloat("Sky Fallback Intensity", &mySkyFallbackIntensity, 0.0f, 1000.0f))
          RestartAccumulation();
      }

      if (!myAccumulate)
        RestartAccumulation(); // Always render frame 0 only

      ImGui::Text("Accumulation Frame %i", myNumAccumulationFrames);
  
      if (myRenderAo)
      {
        if (ImGui::DragFloat("Ao Distance", &myAoDistance))
          RestartAccumulation();
      }
      else
      {
        if (ImGui::SliderFloat("Phong Spec Power", &myPhongSpecularPower, 0.0f, 1000.0f))
          RestartAccumulation();

        if (ImGui::InputInt("Max Recursion Depth", &myMaxRecursionDepth, 1))
          RestartAccumulation();

        if (ImGui::Checkbox("Enable Light", &myLightEnabled))
          RestartAccumulation();

        if (myLightEnabled)
        {
          if (ImGui::SliderFloat("Light Strength", &myLightStrength, 0, 5000))
            RestartAccumulation();

          if (ImGui::InputInt("Light Instance Idx", &myLightInstanceIdx, 1))
            RestartAccumulation();

          float col[3] = { myLightColor.x, myLightColor.y, myLightColor.z };
          if(ImGui::ColorPicker3("Light Color", col ))
          {
            myLightColor = glm::float3(col[0], col[1], col[2]);
            RestartAccumulation();
          }

          
        }
      }

      if (CameraHasChanged() || skyParamsChanged)
        RestartAccumulation();
    }
  }

  myLastViewMat = myCamera.myViewProj;
}

void PathTracer::Render()
{
  Application::Render();

  CommandList* ctx = RenderCore::BeginCommandList(CommandListType::Graphics);
  {
    GPU_SCOPED_PROFILER_FUNCTION(ctx, 0u);

    mySky->ComputeTranmittanceLut(ctx);

    if (myRenderRaster || !mySupportsRaytracing)
    {
      RenderRaster(ctx);
    }
    else
    {
      RenderRT(ctx);
    }

    TonemapComposit(ctx);
  }
  
  RenderCore::ExecuteAndFreeCommandList(ctx);
  
  ImGui::Render();
}

void PathTracer::RenderRaster(CommandList* ctx)
{
  GPU_SCOPED_PROFILER_FUNCTION(ctx, 0u);

  mySky->ComputeSkyViewLut(ctx, myCamera);

  uint dstTexWidth = myHdrLightTexRead->GetTexture()->GetProperties().myWidth;
  uint dstTexHeight = myHdrLightTexRead->GetTexture()->GetProperties().myHeight;

  ctx->SetViewport(glm::uvec4(0, 0, dstTexWidth, dstTexHeight));
  ctx->SetClipRect(glm::uvec4(0, 0, dstTexWidth, dstTexHeight));
  ctx->ClearDepthStencilTarget(myDepthStencilDsv.get(), 1.0f, 0u, (uint)DepthStencilClearFlags::CLEAR_ALL);
  glm::float4 clearColor(0.0f);
  ctx->ClearRenderTarget(myHdrLightTexRtv.get(), &clearColor[0]);

  mySky->Render(ctx, myHdrLightTexWrite.get(), nullptr, myCamera);

  ctx->SetRenderTarget(myHdrLightTexRtv.get() , myDepthStencilDsv.get());

  ctx->SetDepthStencilState(nullptr);
  ctx->SetBlendState(nullptr);
  ctx->SetCullMode(CullMode::NONE);
  ctx->SetFillMode(FillMode::SOLID);
  ctx->SetWindingOrder(WindingOrder::CCW);

  ctx->SetTopologyType(TopologyType::TRIANGLE_LIST);
  ctx->SetShaderPipeline(myUnlitMeshShader.get());

  auto RenderMesh = [ctx](Mesh* mesh)
  {
    for (SharedPtr<MeshPart>& meshPart : mesh->myParts)
    {
      const VertexInputLayout* layout = meshPart->myVertexInputLayout.get();
      
      const GpuBuffer* vertexBuffer = meshPart->myVertexBuffer.get();
      uint64 offset = 0ull;
      uint64 size = vertexBuffer->GetByteSize();
      ctx->BindVertexBuffers(&vertexBuffer, &offset, &size, 1u, layout);
      ctx->BindIndexBuffer(meshPart->myIndexBuffer.get(), meshPart->myIndexBuffer->GetProperties().myElementSizeBytes);

      ctx->DrawIndexedInstanced(meshPart->myIndexBuffer->GetProperties().myNumElements, 1u, 0, 0, 0);
    }
  };

  for (SceneMeshInstance& meshInstance : myScene->myInstances)
  {
    Mesh* mesh = myScene->myMeshes[meshInstance.myMeshIndex].get();
    glm::float4x4 transform = meshInstance.myTransform;
    Material* material = myScene->myMaterials[meshInstance.myMaterialIndex].get();

    struct Cbuffer_PerObject
    {
      glm::float4x4 myWorldViewProj;
      glm::float4 myColor;
    };
    Cbuffer_PerObject cbuffer_perObject
    {
      myCamera.myViewProj * transform,
      material->myParameters[(uint)MaterialParameterType::COLOR]
    };
    ctx->BindConstantBuffer(&cbuffer_perObject, sizeof(cbuffer_perObject), 0);

    RenderMesh(mesh);
  }
  
}

void PathTracer::RenderRT(CommandList* ctx)
{
  GPU_SCOPED_PROFILER_FUNCTION(ctx, 0u);

  uint dstTexWidth = myHdrLightTexRead->GetTexture()->GetProperties().myWidth;
  uint dstTexHeight = myHdrLightTexRead->GetTexture()->GetProperties().myHeight;

  if (myAccumulationNeedsClear)
  {
    ctx->PrepareResourceShaderAccess(myHdrLightTexWrite.get());

    uint texIdx = myHdrLightTexWrite->GetGlobalDescriptorIndex();
    ctx->BindConstantBuffer(&texIdx, sizeof(texIdx), 0);
    ctx->SetShaderPipeline(myClearTextureShader.get());
    ctx->Dispatch(glm::ivec3(dstTexWidth, dstTexHeight, 1));
    ctx->ResourceUAVbarrier(myHdrLightTexWrite->GetTexture());
    myAccumulationNeedsClear = false;
    myNumAccumulationFrames = 0u;
  }
  
  RtPipelineState* rtPso = myRenderAo ? myRtScene.myAoRtPso.get() : myRtScene.myRtPso.get();
  RtShaderBindingTable* rtSbt = myRenderAo ? myRtScene.myAoSBT.get() : myRtScene.mySBT.get();
  ctx->SetRaytracingPipelineState(rtPso);

  eastl::fixed_vector<glm::float3, 4> nearPlaneVertices;
  myCamera.GetVerticesOnNearPlane(nearPlaneVertices);

  struct SkyConstants
  {
    AtmosphereParameters myAtmosphere;

    glm::float3 mySunDirection;
    uint myTransmissionLutTexIdx;

    glm::float3 mySunIlluminance;
    float _unused;

    glm::float2 myRayMarchMinMaxSPP;
    glm::float2 _unused2;
  } skyConsts;

  skyConsts.myAtmosphere = mySky->myAtmosphereParams;
  skyConsts.mySunDirection = mySky->mySunDir;
  skyConsts.myTransmissionLutTexIdx = ctx->GetPrepareDescriptorIndex(mySky->myTransmittanceLutRead.get());
  skyConsts.mySunIlluminance = mySky->mySunIlluminance;
  skyConsts.myRayMarchMinMaxSPP = glm::float2(4.0f, 14.0f);

  struct RtConsts
  {
    glm::float3 myNearPlaneCorner;
    float myAoDistance;

    glm::float3 myXAxis;
    uint myOutTexIndex;

    glm::float3 myYAxis;
    uint myAsIndex;

    glm::float3 myCameraPos;
    uint myInstanceDataBufferIndex;

    uint myMaterialDataBufferIndex;
    uint mySampleBufferIndex;
    uint myFrameRandomSeed;
    uint myNumAccumulationFrames;

    uint myLinearClampSamplerIndex;
    uint myMaxRecursionDepth;
    uint myLightInstanceId;
    uint myNumHaltonSamples;

    glm::float3 myLightEmission;
    uint mySampleSky;

    glm::float3 mySkyFallbackEmission;
    float myPhongSpecularPower;

    SkyConstants mySkyConsts;

  } rtConsts;

  rtConsts.myNearPlaneCorner = nearPlaneVertices[0];
  rtConsts.myAoDistance = myAoDistance;
  rtConsts.myXAxis = nearPlaneVertices[1] - nearPlaneVertices[0];
  rtConsts.myYAxis = nearPlaneVertices[3] - nearPlaneVertices[0];
  rtConsts.myOutTexIndex = myHdrLightTexWrite->GetGlobalDescriptorIndex();
  rtConsts.myAsIndex = myRtScene.myTLAS->GetBufferRead()->GetGlobalDescriptorIndex();
  rtConsts.myCameraPos = myCamera.myPosition;
  rtConsts.myInstanceDataBufferIndex = myRtScene.myInstanceData->GetGlobalDescriptorIndex();
  rtConsts.myMaterialDataBufferIndex = myRtScene.myMaterialData->GetGlobalDescriptorIndex();
  rtConsts.mySampleBufferIndex = myRtScene.myHaltonSamples->GetGlobalDescriptorIndex();
  rtConsts.myLightInstanceId = myLightInstanceIdx;
  rtConsts.myNumHaltonSamples = myRtScene.myHaltonSamples->GetBuffer()->GetProperties().myNumElements;
  rtConsts.myLightEmission = myLightEnabled ? myLightColor * myLightStrength : glm::float3(0.0f);
  rtConsts.mySampleSky = mySampleSky ? 1u : 0u;
  rtConsts.mySkyFallbackEmission = glm::float3(mySkyFallbackIntensity);
  rtConsts.myPhongSpecularPower = myPhongSpecularPower;
  rtConsts.myFrameRandomSeed = (uint)Time::ourFrameIdx;
  rtConsts.myNumAccumulationFrames = myNumAccumulationFrames++;
  rtConsts.myLinearClampSamplerIndex = RenderCore::ourLinearClampSampler->GetGlobalDescriptorIndex();
  rtConsts.myMaxRecursionDepth = (uint)myMaxRecursionDepth;
  rtConsts.mySkyConsts = skyConsts;
  ctx->BindConstantBuffer(&rtConsts, sizeof(rtConsts), 0);

  ctx->PrepareResourceShaderAccess(myHdrLightTexWrite.get());
  ctx->PrepareResourceShaderAccess(myRtScene.myTLAS->GetBufferRead());
  ctx->PrepareResourceShaderAccess(myRtScene.myInstanceData.get());
  ctx->PrepareResourceShaderAccess(myRtScene.myMaterialData.get());
  ctx->PrepareResourceShaderAccess(myRtScene.myHaltonSamples.get());

  DispatchRaysDesc desc;
  desc.myRayGenShaderTableRange = rtSbt->GetRayGenRange();
  desc.myMissShaderTableRange = rtSbt->GetMissRange();
  desc.myHitGroupTableRange = rtSbt->GetHitRange();
  desc.myWidth = dstTexWidth;
  desc.myHeight = dstTexHeight;
  desc.myDepth = 1;
  ctx->DispatchRays(desc);

  ctx->ResourceUAVbarrier(myHdrLightTexWrite->GetTexture());
}

void PathTracer::TonemapComposit(CommandList* ctx)
{
  GPU_SCOPED_PROFILER_FUNCTION(ctx, 0u);

  ctx->SetShaderPipeline(myTonemapCompositShader.get());
  ctx->SetDepthStencilState(myDepthTestOff);
  ctx->SetCullMode(CullMode::NONE);
  ctx->SetViewport(glm::uvec4(0, 0, myRenderOutput->GetWindow()->GetWidth(), myRenderOutput->GetWindow()->GetHeight()));
  ctx->SetClipRect(glm::uvec4(0, 0, myRenderOutput->GetWindow()->GetWidth(), myRenderOutput->GetWindow()->GetHeight()));

  struct TonemapConsts
  {
    bool myIsBGR;
    uint mySrcTextureIdx;
    uint myLinearClampSamplerIndex;
    uint myNumAccumulationFrames;

    glm::float2 myPixelToUv;
  } tonemapConsts;
  tonemapConsts.myIsBGR = myRenderOutput->GetBackbuffer()->GetProperties().myFormat == DataFormat::BGRA_8 ? 1 : 0;
  tonemapConsts.mySrcTextureIdx = myHdrLightTexRead->GetGlobalDescriptorIndex();
  tonemapConsts.myLinearClampSamplerIndex = RenderCore::ourLinearClampSampler->GetGlobalDescriptorIndex();
  tonemapConsts.myPixelToUv = glm::float2(1.0f) / glm::float2(myRenderOutput->GetWindow()->GetWidth(), myRenderOutput->GetWindow()->GetHeight());
  tonemapConsts.myNumAccumulationFrames = myNumAccumulationFrames;
  ctx->BindConstantBuffer(&tonemapConsts, sizeof(tonemapConsts), 0);
  ctx->PrepareResourceShaderAccess(myHdrLightTexRead.get());

  glm::float2 fsTriangleVerts[] = {
    { -4.0f, -1.0f }, { 1.0f, -1.0f }, { 1.0f, 4.0f }
  };
  ctx->BindVertexBuffer(fsTriangleVerts, sizeof(fsTriangleVerts));
  ctx->SetRenderTarget(myRenderOutput->GetBackbufferRtv(), nullptr);
  ctx->DrawInstanced(3, 1, 0, 0);
}

void PathTracer::EndFrame()
{
  Application::EndFrame();
}

void PathTracer::OnRtPipelineRecompiled(const RtPipelineState* aRtPipeline)
{
  RestartAccumulation();
}

void PathTracer::UpdateOutputTexture()
{
  RenderCore::WaitForIdle(CommandListType::Graphics);
  uint width = myRenderOutput->GetWindow()->GetWidth();
  uint height = myRenderOutput->GetWindow()->GetHeight();

  if (myHalfResRender)
  {
    width = (width + 1) / 2;
    height = (height + 1) / 2;
  }

  TextureProperties props;
  props.myDimension = GpuResourceDimension::TEXTURE_2D;
  props.myFormat = DataFormat::RGBA_32F;
  props.myIsShaderWritable = true;
  props.myIsRenderTarget = true;
  props.myWidth = width;
  props.myHeight = height;
  props.myNumMipLevels = 1u;
  SharedPtr<Texture> texture = RenderCore::CreateTexture(props, "Light output texture");
  ASSERT(texture);

  TextureViewProperties viewProps;
  myHdrLightTexRead = RenderCore::CreateTextureView(texture, viewProps, "Light output texture read");
  ASSERT(myHdrLightTexRead);

  viewProps.myIsShaderWritable = true;
  myHdrLightTexWrite = RenderCore::CreateTextureView(texture, viewProps, "Light output texture write");
  ASSERT(myHdrLightTexWrite);

  viewProps.myIsRenderTarget = true;
  viewProps.myIsShaderWritable = false;
  myHdrLightTexRtv = RenderCore::CreateTextureView(texture, viewProps, "Light output texture rtv");
  ASSERT(myHdrLightTexRtv);
}

void PathTracer::UpdateDepthbuffer()
{
  uint width = myRenderOutput->GetWindow()->GetWidth();
  uint height = myRenderOutput->GetWindow()->GetHeight();

  TextureProperties dsTexProps;
  dsTexProps.myDimension = GpuResourceDimension::TEXTURE_2D;
  dsTexProps.bIsDepthStencil = true;
  dsTexProps.myFormat = DataFormat::D_24UNORM_S_8UI;
  dsTexProps.myIsRenderTarget = true;
  dsTexProps.myIsShaderWritable = false;
  dsTexProps.myWidth = width;
  dsTexProps.myHeight = height;
  dsTexProps.myNumMipLevels = 1u;

  SharedPtr<Texture> dsTexture = RenderCore::CreateTexture(dsTexProps, "Backbuffer DepthStencil Texture");
  ASSERT(dsTexture != nullptr);

  TextureViewProperties props;
  props.myDimension = GpuResourceDimension::TEXTURE_2D;
  props.myIsRenderTarget = true;
  props.myFormat = DataFormat::D_24UNORM_S_8UI;
  props.mySubresourceRange = dsTexture->mySubresources;
  myDepthStencilDsv = RenderCore::CreateTextureView(dsTexProps, props, "DepthStencil Texture");
  ASSERT(myDepthStencilDsv != nullptr);
}

void PathTracer::RestartAccumulation()
{
  myNumAccumulationFrames = 0u;
  myAccumulationNeedsClear = true;
}

