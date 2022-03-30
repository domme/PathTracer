#include "PathTracer.h"

#include "imgui.h"
#include "imgui_impl_fancy.h"
#include "Common/Fancy.h"
#include "Common/Ptr.h"
#include "Common/StringUtil.h"
#include "Common/Window.h"
#include "Common/StaticString.h"
#include "Debug/Profiler.h"
#include "IO/MeshImporter.h"
#include "Rendering/CommandList.h"
#include "Rendering/GraphicsResources.h"
#include "Rendering/RenderCore.h"
#include "Rendering/RenderOutput.h"
#include "Rendering/RtAccelerationStructure.h"
#include "Rendering/ShaderPipelineDesc.h"
#include "Rendering/Texture.h"

using namespace Fancy;

static SharedPtr<ShaderPipeline> locLoadShader(const char* aShaderPath, const char* aMainVtxFunction = "main", const char* aMainFragmentFunction = "main", const char* someDefines = nullptr)
{
  eastl::vector<eastl::string> defines;
  if (someDefines)
    StringUtil::Tokenize(someDefines, ",", defines);

  ShaderPipelineDesc pipelineDesc;

  ShaderDesc* shaderDesc = &pipelineDesc.myShader[(uint)ShaderStage::SHADERSTAGE_VERTEX];
  shaderDesc->myPath = aShaderPath;
  shaderDesc->myMainFunction = aMainVtxFunction;
  for (const eastl::string& str : defines)
    shaderDesc->myDefines.push_back(str);

  shaderDesc = &pipelineDesc.myShader[(uint)ShaderStage::SHADERSTAGE_FRAGMENT];
  shaderDesc->myPath = aShaderPath;
  shaderDesc->myMainFunction = aMainFragmentFunction;
  for (const eastl::string& str : defines)
    shaderDesc->myDefines.push_back(str);

  return RenderCore::CreateShaderPipeline(pipelineDesc);
}

PathTracer::PathTracer(HINSTANCE anInstanceHandle, const char** someArguments, uint aNumArguments, const char* aName,
  const Fancy::RenderPlatformProperties& someRenderProperties, const Fancy::WindowParameters& someWindowParams)
  : Application(anInstanceHandle, someArguments, aNumArguments, aName, "../../../../", someRenderProperties, someWindowParams)
  , myImGuiContext(ImGui::CreateContext())
{
  ImGuiRendering::Init(myRuntime->GetRenderOutput(), myRuntime);

  myUnlitMeshShader = locLoadShader("resources/shaders/unlit_mesh.hlsl");
  ASSERT(myUnlitMeshShader);

  SceneData sceneData;
  MeshImporter importer;
  const bool importSuccess = importer.Import("resources/models/Cycles.obj", myUnlitMeshShader.get(), sceneData);
  ASSERT(importSuccess);

  InitRtScene(sceneData);

  myScene = eastl::make_shared<Scene>(sceneData);

  myCamera.myPosition = glm::float3(1.0f, -1.0f, -1460.0f);
  myCamera.myOrientation = glm::quat_cast(glm::lookAt(glm::float3(0.0f, 0.0f, 10.0f), glm::float3(0.0f, 0.0f, 0.0f), glm::float3(0.0f, 1.0f, 0.0f)));

  myCameraController.myMoveSpeed = 50.0f;

  myCamera.myFovDeg = 60.0f;
  myCamera.myNear = 1.0f;
  myCamera.myFar = 10000.0f;
  myCamera.myWidth = myWindow->GetWidth();
  myCamera.myHeight = myWindow->GetHeight();
  myCamera.myIsOrtho = false;

  myCamera.UpdateView();
  myCamera.UpdateProjection();
}

void PathTracer::InitRtScene(const SceneData& aScene)
{
  for (uint iMesh = 0u; iMesh < (uint) aScene.myMeshes.size(); ++iMesh)
  {
    const MeshData& mesh = aScene.myMeshes[iMesh];

    eastl::fixed_vector<RtAccelerationStructureGeometryData, 4> meshPartsGeometryDatas;

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
    }

    StaticString<64> name("BLAS mesh %d", iMesh);
    myBLAS.push_back(RenderCore::CreateRtBottomLevelAccelerationStructure(meshPartsGeometryDatas.data(), meshPartsGeometryDatas.size(), 0u, name.GetBuffer()));
    ASSERT(myBLAS.back() != nullptr);
  }

  struct PerInstanceData
  {
    uint myMaterialIndex;
  };
  eastl::vector<PerInstanceData> perInstanceDatas;
  
  eastl::fixed_vector<RtAccelerationStructureInstanceData, 16> instanceDatas;
  for (uint iInstance = 0u; iInstance < (uint) aScene.myInstances.size(); ++iInstance)
  {
    const SceneMeshInstance& instance = aScene.myInstances[iInstance];

    PerInstanceData& perInstanceData = perInstanceDatas.push_back();
    perInstanceData.myMaterialIndex = instance.myMaterialIndex;

    RtAccelerationStructureInstanceData& instanceData = instanceDatas.push_back();
    instanceData.myInstanceId = iInstance;
    instanceData.mySbtHitGroupOffset = 0;
    instanceData.myInstanceBLAS = myBLAS[instance.myMeshIndex];
    instanceData.myInstanceMask = UINT8_MAX;
    instanceData.myTransform = instance.myTransform;
    instanceData.myFlags = RT_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE | RT_INSTANCE_FLAG_FORCE_OPAQUE;
  }

  myTLAS = RenderCore::CreateRtTopLevelAccelerationStructure(instanceDatas.data(), (uint) instanceDatas.size(), 0, "TLAS");

  RtPipelineStateProperties rtPipelineProps;
  const uint raygenIdx = rtPipelineProps.AddRayGenShader("resources/shaders/raytracing/RayGen.hlsl", "RayGen");
  const uint missIdx = rtPipelineProps.AddMissShader("resources/shaders/raytracing/Miss.hlsl", "Miss");
  const uint hitIdx = rtPipelineProps.AddHitGroup(L"HitGroup0", RT_HIT_GROUP_TYPE_TRIANGLES, nullptr, nullptr, nullptr, nullptr, "resources/shaders/raytracing/Hit.hlsl", "ClosestHit");
  rtPipelineProps.SetMaxAttributeSize(32u);
  rtPipelineProps.SetMaxPayloadSize(128u);
  rtPipelineProps.SetMaxRecursionDepth(2u);
  myRtPso = RenderCore::CreateRtPipelineState(rtPipelineProps);

  RtShaderBindingTableProperties sbtProps;
  sbtProps.myNumRaygenShaderRecords = 1;
  sbtProps.myNumMissShaderRecords = 5;
  sbtProps.myNumHitShaderRecords = 5;
  mySBT = RenderCore::CreateRtShaderTable(sbtProps);
  mySBT->AddShaderRecord(myRtPso->GetRayGenShaderIdentifier(raygenIdx));
  mySBT->AddShaderRecord(myRtPso->GetMissShaderIdentifier(missIdx));
  mySBT->AddShaderRecord(myRtPso->GetHitShaderIdentifier(hitIdx));

  struct MaterialData
  {
    uint myColor;
  };
  eastl::vector<MaterialData> materialDatas;
  materialDatas.reserve(aScene.myMaterials.size());

  for (const MaterialDesc& mat : aScene.myMaterials)
  {
    MaterialData& matData = materialDatas.push_back();
    matData.myColor = MathUtil::Encode_Unorm_RGBA(mat.myParameters[(uint)MaterialParameterType::COLOR]);
  }

  GpuBufferProperties bufferProps;
  bufferProps.myBindFlags = (uint) GpuBufferBindFlags::SHADER_BUFFER;
  bufferProps.myElementSizeBytes = sizeof(MaterialData);
  bufferProps.myNumElements = materialDatas.size();

  GpuBufferViewProperties bufferViewProps;
  bufferViewProps.myIsRaw = true;
  bufferViewProps.myOffset = 0;
  bufferViewProps.mySize = VECTOR_BYTESIZE(materialDatas);
  myMaterialBuffer = RenderCore::CreateBufferView(bufferProps, bufferViewProps, "Rt material buffer", materialDatas.data());

  bufferProps.myElementSizeBytes = sizeof(PerInstanceData);
  bufferProps.myNumElements = perInstanceDatas.size();
  bufferViewProps.mySize = VECTOR_BYTESIZE(perInstanceDatas);
  myPerInstanceData = RenderCore::CreateBufferView(bufferProps, bufferViewProps, "Rt per instance data", perInstanceDatas.data());
}

PathTracer::~PathTracer()
{
  ImGuiRendering::Shutdown();
  ImGui::DestroyContext(myImGuiContext);
  myImGuiContext = nullptr;
}

void PathTracer::OnWindowResized(uint aWidth, uint aHeight)
{
  Application::OnWindowResized(aWidth, aHeight);
}

void PathTracer::BeginFrame()
{
  Application::BeginFrame();
  ImGuiRendering::NewFrame();
}

void PathTracer::Update()
{
  Application::Update();
  ImGui::Checkbox("Render Raster", &myRenderRaster);
}

void PathTracer::Render()
{
  Application::Render();

  if (myRenderRaster)
  {
    RenderRaster();
  }
  else
  {
    RenderRT();
  }

  ImGui::Render();
}

void PathTracer::RenderRaster()
{
  CommandList* ctx = RenderCore::BeginCommandList(CommandListType::Graphics);
  GPU_BEGIN_PROFILE(ctx, "Render scene", 0u);
  ctx->SetViewport(glm::uvec4(0, 0, myWindow->GetWidth(), myWindow->GetHeight()));
  ctx->SetClipRect(glm::uvec4(0, 0, myWindow->GetWidth(), myWindow->GetHeight()));
  ctx->SetRenderTarget(myRenderOutput->GetBackbufferRtv(), myRenderOutput->GetDepthStencilDsv());

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
      ctx->SetVertexInputLayout(layout);

      ctx->BindVertexBuffer(meshPart->myVertexBuffer.get());
      ctx->BindIndexBuffer(meshPart->myIndexBuffer.get(), meshPart->myIndexBuffer->GetProperties().myElementSizeBytes);

      ctx->Render(meshPart->myIndexBuffer->GetProperties().myNumElements, 1u, 0, 0, 0);
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
  GPU_END_PROFILE(ctx);

  RenderCore::ExecuteAndFreeCommandList(ctx);
}

void PathTracer::RenderRT()
{
  TextureResourceProperties texProps;
  texProps.myIsShaderWritable = true;
  texProps.myIsRenderTarget = false;
  texProps.myTextureProperties.myFormat = DataFormat::RGBA_8;
  texProps.myTextureProperties.myWidth = myWindow->GetWidth();
  texProps.myTextureProperties.myHeight = myWindow->GetHeight();
  TempTextureResource rtOutputTex = RenderCore::AllocateTempTexture(texProps, 0u, "RT Test Result Texture");

  CommandList* ctx = RenderCore::BeginCommandList(CommandListType::Graphics);
  
  ctx->SetRaytracingPipelineState(myRtPso.get());
  ctx->PrepareResourceShaderAccess(rtOutputTex.myWriteView);
  ctx->PrepareResourceShaderAccess(myTLAS->GetBufferRead());

  eastl::fixed_vector<glm::float3, 4> nearPlaneVertices;
  myCamera.GetVerticesOnNearPlane(nearPlaneVertices);

  struct RayGenConsts
  {
    glm::float3 myNearPlaneCorner;
    bool myIsBGR;

    glm::float3 myXAxis;
    uint myOutTexIndex;

    glm::float3 myYAxis;
    uint myAsIndex;

    glm::float3 myCameraPos;
  } rayGenConsts;

  rayGenConsts.myNearPlaneCorner = nearPlaneVertices[0];
  rayGenConsts.myIsBGR = false;
  rayGenConsts.myXAxis = nearPlaneVertices[1] - nearPlaneVertices[0];
  rayGenConsts.myYAxis = nearPlaneVertices[3] - nearPlaneVertices[0];
  rayGenConsts.myOutTexIndex = rtOutputTex.myWriteView->GetGlobalDescriptorIndex();
  rayGenConsts.myAsIndex = myTLAS->GetBufferRead()->GetGlobalDescriptorIndex();
  rayGenConsts.myCameraPos = myCamera.myPosition;
  ctx->BindConstantBuffer(&rayGenConsts, sizeof(rayGenConsts), 0);

  GPU_BEGIN_PROFILE(ctx, "RT", 0u);
  DispatchRaysDesc desc;
  desc.myRayGenShaderTableRange = mySBT->GetRayGenRange();
  desc.myMissShaderTableRange = mySBT->GetMissRange();
  desc.myHitGroupTableRange = mySBT->GetHitRange();
  desc.myWidth = texProps.myTextureProperties.myWidth;
  desc.myHeight = texProps.myTextureProperties.myHeight;
  desc.myDepth = 1;
  ctx->DispatchRays(desc);
  GPU_END_PROFILE(ctx);

  ctx->ResourceUAVbarrier(rtOutputTex.myTexture);
  SubresourceLocation subresourceLoc;
  TextureRegion region = { glm::uvec3(0), glm::uvec3(texProps.myTextureProperties.myWidth, texProps.myTextureProperties.myHeight, 1u) };

  Texture* backbuffer = myRenderOutput->GetBackbuffer();
  ctx->CopyTexture(backbuffer, subresourceLoc, region, rtOutputTex.myTexture, subresourceLoc, region);

  RenderCore::ExecuteAndFreeCommandList(ctx, SyncMode::BLOCKING);
}

void PathTracer::EndFrame()
{
  Application::EndFrame();
}


