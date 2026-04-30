#include "PathTracer.h"

#include <chrono>
#include "imgui.h"
#include "imgui_impl_fancy.h"
#include "Sky.h"
#include "Common/Ptr.h"
#include "Common/StringUtil.h"
#include "Common/Window.h"
#include "Common/StaticString.h"
#include "Debug/Profiler.h"
#include "IO/Assets.h"
#include "IO/ImageLoader.h"
#include "IO/MeshImporter.h"
#include "IO/PathService.h"
#include "IO/Scene.h"
#include "Rendering/CommandList.h"
#include "Rendering/DepthStencilState.h"
#include "Rendering/GraphicsResources.h"
#include "Rendering/RenderCore.h"
#include "Rendering/RenderOutput.h"
#include "Rendering/RtAccelerationStructure.h"
#include "Rendering/ShaderPipelineDesc.h"
#include "Rendering/Texture.h"

using namespace Fancy;

static float64 SampleTimeMs() {
  const std::chrono::duration< float64, std::milli > now( std::chrono::system_clock::now().time_since_epoch() );
  return now.count();
}

struct SceneLoadInfo {
  eastl::fixed_string< char, 64, false > myDisplayName;
  eastl::fixed_string< char, 256, false > myPath;
  glm::float3 myCamPos = glm::float3( 0.0f );
};

SceneLoadInfo sceneLoadInfos[] = {
  { "Cornell Box", "resources/models/CornellBox.obj", glm::float3( 1.0f, 102.0f, -30.0f ) },
  { "Epic Sun Temple", "resources/models/SunTemple_v4/SunTemple/SunTemple_Reduced.fbx",
    glm::float3( -13.0f, 513.7f, -1191.5f ) },
};

RaytracingScene::~RaytracingScene() {
  for ( BlasData & blas : myBlasDatas ) {
    if ( blas.myVertexData.IsValid() )
      RenderCore::DeleteBufferView( blas.myVertexData );
    if ( blas.myVertexDataBuf.IsValid() )
      RenderCore::DeleteBuffer( blas.myVertexDataBuf );
    if ( blas.myTriangleIndices.IsValid() )
      RenderCore::DeleteBufferView( blas.myTriangleIndices );
    if ( blas.myTriangleIndicesBuf.IsValid() )
      RenderCore::DeleteBuffer( blas.myTriangleIndicesBuf );
    if ( blas.myBLAS.IsValid() )
      RenderCore::DeleteRtAccelerationStructure( blas.myBLAS );
  }
  if ( myInstanceData.IsValid() )
    RenderCore::DeleteBufferView( myInstanceData );
  if ( myInstanceDataBuf.IsValid() )
    RenderCore::DeleteBuffer( myInstanceDataBuf );
  if ( myMaterialData.IsValid() )
    RenderCore::DeleteBufferView( myMaterialData );
  if ( myMaterialDataBuf.IsValid() )
    RenderCore::DeleteBuffer( myMaterialDataBuf );
  if ( myHaltonSamples.IsValid() )
    RenderCore::DeleteBufferView( myHaltonSamples );
  if ( myHaltonSamplesBuf.IsValid() )
    RenderCore::DeleteBuffer( myHaltonSamplesBuf );
  // RtPipelineState is a cached resource; not owned
  if ( mySBT.IsValid() )
    RenderCore::DeleteRtShaderBindingTable( mySBT );
  if ( myAoSBT.IsValid() )
    RenderCore::DeleteRtShaderBindingTable( myAoSBT );
  if ( myTLAS.IsValid() )
    RenderCore::DeleteRtAccelerationStructure( myTLAS );
}

PathTracer::PathTracer( HINSTANCE anInstanceHandle, const char ** someArguments, uint aNumArguments, const char * aName,
                        const Fancy::RenderPlatformProperties & someRenderProperties,
                        const Fancy::WindowParameters & someWindowParams )
    : Application( anInstanceHandle, someArguments, aNumArguments, aName, "../../../../", someRenderProperties,
                   someWindowParams ),
      myImGuiContext( ImGui::CreateContext() ) {
  ImGuiRendering::Init( myRenderOutput );

  mySupportsRaytracing = RenderCore::GetPlatformCaps().mySupportsRaytracing;

  DepthStencilStateProperties dsProps;
  dsProps.myDepthTestEnabled = false;
  dsProps.myDepthWriteEnabled = false;
  dsProps.myStencilEnabled = false;
  dsProps.myStencilWriteMask = 0u;
  myDepthTestOff = RenderCore::CreateDepthStencilState( dsProps );

  RenderCore::ourOnRtPipelineStateRecompiled.Connect( this, &PathTracer::OnRtPipelineRecompiled );

  UpdateDepthbuffer();
  UpdateOutputTexture();

  myUnlitMeshShader = RenderCore::CreateVertexPixelShaderPipeline( "resources/shaders/unlit_mesh.hlsl" );
  ASSERT( myUnlitMeshShader.IsValid() );

  myTonemapCompositShader = RenderCore::CreateVertexPixelShaderPipeline( "resources/shaders/tonemap_composit.hlsl" );
  ASSERT( myTonemapCompositShader.IsValid() );

  myClearTextureShader = RenderCore::CreateComputeShaderPipeline( "resources/shaders/clear_texture.hlsl" );
  ASSERT( myClearTextureShader.IsValid() );

  InitSky();

  LoadScene( sceneLoadInfos[ 0 ].myPath.c_str(), sceneLoadInfos[ 0 ].myCamPos );
}

void PathTracer::LoadScene( const char * aPath, const glm::float3 & aCamPos ) {
  eastl::fixed_vector< VertexShaderAttributeDesc, 16 > vertexAttributes = {
    { VertexAttributeSemantic::POSITION, 0, DataFormat::RGB_32F },
    { VertexAttributeSemantic::NORMAL, 0, DataFormat::RGB_32F },
    { VertexAttributeSemantic::TEXCOORD, 0, DataFormat::RG_32F }
  };

  SceneData sceneData;
  MeshImporter importer;
  const bool importSuccess = importer.Import( aPath, vertexAttributes, sceneData );
  if ( !importSuccess ) {
    Log( "Failed importing scene %s", aPath );
  }

  if ( mySupportsRaytracing )
    InitRtScene( sceneData );

  myScene = eastl::make_shared< Scene >( sceneData );

  myCamera.myPosition = aCamPos;
  myCamera.myOrientation = glm::quat_cast( glm::lookAt(
      glm::float3( 0.0f, 0.0f, 10.0f ), glm::float3( 0.0f, 0.0f, 0.0f ), glm::float3( 0.0f, 1.0f, 0.0f ) ) );

  myCameraController.myMoveSpeed = 50.0f;

  myCamera.myFovDeg = 60.0f;
  myCamera.myNear = 1.0f;
  myCamera.myFar = 10000.0f;
  myCamera.myWidth = ( float ) RenderCore::GetRenderOutput( myRenderOutput )->GetWindow()->GetWidth();
  myCamera.myHeight = ( float ) RenderCore::GetRenderOutput( myRenderOutput )->GetWindow()->GetHeight();
  myCamera.myIsOrtho = false;

  myCamera.UpdateView();
  myCamera.UpdateProjection();
}

void PathTracer::InitSky() {
  SkyParameters skyParams;
  mySky.reset( new Sky( skyParams ) );
}

glm::uvec2 GetOffsetSize( const VertexInputLayoutProperties & someVertexProps, VertexAttributeSemantic aSemantic,
                          uint aSemanticIndex ) {
  uint offset = 0;
  for ( const VertexInputAttributeDesc & attribute : someVertexProps.myAttributes ) {
    uint size = BITS_TO_BYTES( DataFormatInfo::GetFormatInfo( attribute.myFormat ).myBitsPerPixel );
    if ( attribute.mySemantic == aSemantic && attribute.mySemanticIndex == aSemanticIndex ) {
      return { offset, size };
    }
    offset += size;
  }

  ASSERT( false );
  return glm::uvec2( 0, 0 );
}

void PathTracer::InitRtScene( const SceneData & aScene ) {
  myRtScene.reset( new RaytracingScene() );

  myRtScene->myBlasDatas.reserve( aScene.myMeshes.size() );

  for ( uint iMesh = 0u; iMesh < ( uint ) aScene.myMeshes.size(); ++iMesh ) {
    const MeshData & mesh = aScene.myMeshes[ iMesh ];

    eastl::fixed_vector< RtAccelerationStructureGeometryData, 4 > meshPartsGeometryDatas;

    uint numMeshVertices = 0u;
    uint numMeshTriangles = 0u;

    for ( const MeshPartData & meshPart : mesh.myParts ) {
      const VertexInputLayoutProperties & vertexProps = meshPart.myVertexLayoutProperties;
      ASSERT( !vertexProps.myAttributes.empty() &&
              vertexProps.myAttributes[ 0 ].mySemantic ==
                  VertexAttributeSemantic::POSITION );  // Assume there is no offset from the start of the vertex data
                                                        // to the first position
      ASSERT( vertexProps.myBufferBindings.size() == 1u );  // Assume the mesh is using only one interleaved buffer

      const uint numVertices = VECTOR_BYTESIZE( meshPart.myVertexData ) / vertexProps.GetOverallVertexSize();

      RtAccelerationStructureGeometryData & geometryData = meshPartsGeometryDatas.push_back();
      geometryData.myType = RtAccelerationStructureGeometryType::TRIANGLES;
      geometryData.myFlags = ( uint ) RtAccelerationStructureGeometryFlags::OPAQUE_GEOMETRY;
      geometryData.myVertexFormat = vertexProps.myAttributes[ 0 ].myFormat;
      geometryData.myNumVertices = numVertices;
      geometryData.myVertexData.myType = RT_BUFFER_DATA_TYPE_CPU_DATA;
      geometryData.myVertexStride = vertexProps.GetOverallVertexSize();
      geometryData.myVertexData.myCpuData.myData = meshPart.myVertexData.data();
      geometryData.myVertexData.myCpuData.myDataSize = VECTOR_BYTESIZE( meshPart.myVertexData );

      geometryData.myIndexFormat = DataFormat::R_32UI;
      geometryData.myNumIndices = VECTOR_BYTESIZE( meshPart.myIndexData ) / sizeof( uint );
      geometryData.myIndexData.myType = RT_BUFFER_DATA_TYPE_CPU_DATA;
      geometryData.myIndexData.myCpuData.myData = meshPart.myIndexData.data();
      geometryData.myIndexData.myCpuData.myDataSize = VECTOR_BYTESIZE( meshPart.myIndexData );

      numMeshVertices += numVertices;
      numMeshTriangles += geometryData.myNumIndices / 3;
    }

    struct VertexData {
      glm::float3 myNormal;
      glm::float2 myUv;
    };

    glm::uvec2 normalOffsetSize =
        GetOffsetSize( aScene.myVertexInputLayoutProperties, VertexAttributeSemantic::NORMAL, 0u );
    glm::uvec2 uvOffsetSize =
        GetOffsetSize( aScene.myVertexInputLayoutProperties, VertexAttributeSemantic::TEXCOORD, 0u );
    ASSERT( normalOffsetSize.y == sizeof( glm::float3 ) );
    ASSERT( uvOffsetSize.y == sizeof( glm::float2 ) );

    eastl::vector< VertexData > vertexData;
    vertexData.reserve( numMeshVertices );

    eastl::vector< glm::uvec3 > triangleIndices;
    triangleIndices.resize( numMeshTriangles );
    glm::uvec3 * dstTrianglePtr = triangleIndices.data();

    for ( const MeshPartData & meshPart : mesh.myParts ) {
      const VertexInputLayoutProperties & vertexProps = meshPart.myVertexLayoutProperties;

      const uint srcVertexStride = vertexProps.GetOverallVertexSize();
      const uint numVertices = VECTOR_BYTESIZE( meshPart.myVertexData ) / srcVertexStride;
      const uint8 * srcData = meshPart.myVertexData.data();
      for ( uint i = 0u; i < numVertices; ++i ) {
        VertexData & dstData = vertexData.push_back();
        memcpy( &dstData.myNormal, srcData + normalOffsetSize.x, sizeof( dstData.myNormal ) );
        memcpy( &dstData.myUv, srcData + uvOffsetSize.x, sizeof( dstData.myUv ) );
        srcData += srcVertexStride;
      }

      memcpy( dstTrianglePtr, meshPart.myIndexData.data(), VECTOR_BYTESIZE( meshPart.myIndexData ) );
      dstTrianglePtr += VECTOR_BYTESIZE( meshPart.myIndexData );
    }

    BlasData & blasData = myRtScene->myBlasDatas.push_back();

    GpuBufferProperties bufferProps;
    bufferProps.myBindFlags = ( uint ) GpuBufferBindFlags::SHADER_BUFFER;
    bufferProps.myNumElements = numMeshVertices;
    bufferProps.myElementSizeBytes = sizeof( VertexData );
    GpuBufferViewProperties bufferViewProps;
    bufferViewProps.myIsRaw = true;
    StaticString< 64 > name( "Rt mesh vertexData %d", iMesh );
    blasData.myVertexDataBuf = RenderCore::CreateBuffer( bufferProps, name.GetBuffer(), vertexData.data() );
    blasData.myVertexData = RenderCore::CreateBufferView( RenderCore::GetBuffer( blasData.myVertexDataBuf ),
                                                          bufferViewProps, name.GetBuffer() );

    bufferProps.myNumElements = numMeshTriangles;
    bufferProps.myElementSizeBytes = sizeof( glm::uvec3 );
    name.Format( "Rt mesh triangles %d", iMesh );
    blasData.myTriangleIndicesBuf = RenderCore::CreateBuffer( bufferProps, name.GetBuffer(), triangleIndices.data() );
    blasData.myTriangleIndices = RenderCore::CreateBufferView( RenderCore::GetBuffer( blasData.myTriangleIndicesBuf ),
                                                               bufferViewProps, name.GetBuffer() );

    name.Format( "BLAS mesh %d", iMesh );
    blasData.myBLAS = RenderCore::CreateRtBottomLevelAccelerationStructure(
        meshPartsGeometryDatas.data(), meshPartsGeometryDatas.size(), 0u, name.GetBuffer() );
    ASSERT( blasData.myBLAS.IsValid() );
  }

  struct PerInstanceData {
    uint myIndexBufferDescriptorIndex;
    uint myVertexBufferDescriptorIndex;
    uint myMaterialIndex;
  };
  eastl::vector< PerInstanceData > perInstanceDatas;
  perInstanceDatas.reserve( ( uint ) aScene.myInstances.size() );

  eastl::fixed_vector< RtAccelerationStructureInstanceData, 16 > instanceDatas;
  for ( uint iInstance = 0u; iInstance < ( uint ) aScene.myInstances.size(); ++iInstance ) {
    const SceneMeshInstance & instance = aScene.myInstances[ iInstance ];

    PerInstanceData & perInstanceData = perInstanceDatas.push_back();
    perInstanceData.myMaterialIndex = instance.myMaterialIndex;
    perInstanceData.myIndexBufferDescriptorIndex =
        RenderCore::GetBufferView( myRtScene->myBlasDatas[ instance.myMeshIndex ].myTriangleIndices )
            ->GetGlobalDescriptorIndex();
    perInstanceData.myVertexBufferDescriptorIndex =
        RenderCore::GetBufferView( myRtScene->myBlasDatas[ instance.myMeshIndex ].myVertexData )
            ->GetGlobalDescriptorIndex();

    RtAccelerationStructureInstanceData & instanceData = instanceDatas.push_back();
    instanceData.myInstanceId = iInstance;
    instanceData.mySbtHitGroupOffset = 0;
    instanceData.myInstanceBLAS =
        RenderCore::GetRtAccelerationStructure( myRtScene->myBlasDatas[ instance.myMeshIndex ].myBLAS );
    instanceData.myInstanceMask = UINT8_MAX;
    instanceData.myTransform = instance.myTransform;
    instanceData.myFlags = RT_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE | RT_INSTANCE_FLAG_FORCE_OPAQUE;
  }

  GpuBufferProperties bufferProps;
  bufferProps.myBindFlags = ( uint ) GpuBufferBindFlags::SHADER_BUFFER;
  bufferProps.myNumElements = ( uint ) perInstanceDatas.size();
  bufferProps.myElementSizeBytes = sizeof( PerInstanceData );
  GpuBufferViewProperties bufferViewProps;
  bufferViewProps.myIsRaw = true;
  myRtScene->myInstanceDataBuf =
      RenderCore::CreateBuffer( bufferProps, "Rt per instance data", perInstanceDatas.data() );
  myRtScene->myInstanceData = RenderCore::CreateBufferView( RenderCore::GetBuffer( myRtScene->myInstanceDataBuf ),
                                                            bufferViewProps, "Rt per instance data" );

  struct MaterialData {
    glm::float3 myEmission;
    uint myColor;
  };
  eastl::vector< MaterialData > materialDatas;
  materialDatas.reserve( aScene.myMaterials.size() );

  for ( const MaterialDesc & mat : aScene.myMaterials ) {
    MaterialData & matData = materialDatas.push_back();
    matData.myEmission = mat.myParameters[ ( uint ) MaterialParameterType::EMISSION ];
    matData.myColor = MathUtil::Encode_Unorm_RGBA( mat.myParameters[ ( uint ) MaterialParameterType::COLOR ] );
  }

  bufferProps.myElementSizeBytes = sizeof( MaterialData );
  bufferProps.myNumElements = materialDatas.size();
  bufferViewProps.myIsRaw = true;
  myRtScene->myMaterialDataBuf = RenderCore::CreateBuffer( bufferProps, "Rt material buffer", materialDatas.data() );
  myRtScene->myMaterialData = RenderCore::CreateBufferView( RenderCore::GetBuffer( myRtScene->myMaterialDataBuf ),
                                                            bufferViewProps, "Rt material buffer" );

  myRtScene->myTLAS = RenderCore::CreateRtTopLevelAccelerationStructure( instanceDatas.data(),
                                                                         ( uint ) instanceDatas.size(), 0, "TLAS" );

  // Ao RT pipeline + SBT
  {
    RtPipelineStateProperties rtPipelineProps;
    const uint raygenIdx = rtPipelineProps.AddRayGenShader( "resources/shaders/raytracing/Ao.hlsl", "RayGen" );
    const uint hitIdxPrimary =
        rtPipelineProps.AddHitGroup( L"HitGroup0", RT_HIT_GROUP_TYPE_TRIANGLES, nullptr, nullptr, nullptr, nullptr,
                                     "resources/shaders/raytracing/Ao.hlsl", "ClosestHitPrimary" );
    const uint hitIdxAo =
        rtPipelineProps.AddHitGroup( L"HitGroup1", RT_HIT_GROUP_TYPE_TRIANGLES, nullptr, nullptr, nullptr, nullptr,
                                     "resources/shaders/raytracing/Ao.hlsl", "ClosestHitAo" );
    rtPipelineProps.SetMaxAttributeSize( 32u );
    rtPipelineProps.SetMaxPayloadSize( 128u );
    rtPipelineProps.SetMaxRecursionDepth( RenderCore::GetPlatformCaps().myRaytracingMaxRecursionDepth );
    myRtScene->myAoRtPso = RenderCore::CreateRtPipelineState( rtPipelineProps );

    RtShaderBindingTableProperties sbtProps;
    sbtProps.myNumRaygenShaderRecords = 1;
    sbtProps.myNumMissShaderRecords = 5;
    sbtProps.myNumHitShaderRecords = 5;
    myRtScene->myAoSBT = RenderCore::CreateRtShaderTable( sbtProps );
    RenderCore::GetRtShaderBindingTable( myRtScene->myAoSBT )
        ->AddShaderRecord(
            RenderCore::GetRtPipelineState( myRtScene->myAoRtPso )->GetRayGenShaderIdentifier( raygenIdx ) );
    RenderCore::GetRtShaderBindingTable( myRtScene->myAoSBT )
        ->AddShaderRecord(
            RenderCore::GetRtPipelineState( myRtScene->myAoRtPso )->GetHitShaderIdentifier( hitIdxPrimary ) );
    RenderCore::GetRtShaderBindingTable( myRtScene->myAoSBT )
        ->AddShaderRecord( RenderCore::GetRtPipelineState( myRtScene->myAoRtPso )->GetHitShaderIdentifier( hitIdxAo ) );
  }

  // PathTracing RT pipeline + SBT
  {
    RtPipelineStateProperties rtPipelineProps;
    const uint raygenIdx = rtPipelineProps.AddRayGenShader( "resources/shaders/raytracing/PathTracing.hlsl", "RayGen" );
    const uint hitIdx =
        rtPipelineProps.AddHitGroup( L"HitGroup0", RT_HIT_GROUP_TYPE_TRIANGLES, nullptr, nullptr, nullptr, nullptr,
                                     "resources/shaders/raytracing/PathTracing.hlsl", "ClosestHit" );
    rtPipelineProps.SetMaxAttributeSize( 32u );
    rtPipelineProps.SetMaxPayloadSize( 128u );
    rtPipelineProps.SetMaxRecursionDepth( RenderCore::GetPlatformCaps().myRaytracingMaxRecursionDepth );
    myRtScene->myRtPso = RenderCore::CreateRtPipelineState( rtPipelineProps );

    RtShaderBindingTableProperties sbtProps;
    sbtProps.myNumRaygenShaderRecords = 1;
    sbtProps.myNumMissShaderRecords = 5;
    sbtProps.myNumHitShaderRecords = 5;
    myRtScene->mySBT = RenderCore::CreateRtShaderTable( sbtProps );
    RenderCore::GetRtShaderBindingTable( myRtScene->mySBT )
        ->AddShaderRecord(
            RenderCore::GetRtPipelineState( myRtScene->myRtPso )->GetRayGenShaderIdentifier( raygenIdx ) );
    RenderCore::GetRtShaderBindingTable( myRtScene->mySBT )
        ->AddShaderRecord( RenderCore::GetRtPipelineState( myRtScene->myRtPso )->GetHitShaderIdentifier( hitIdx ) );
  }

  InitSampleSequences();
}

void PathTracer::InitSampleSequences() {
  uint sampleCount = 8192;
  eastl::vector< glm::float2 > someSamples;
  someSamples.reserve( sampleCount );

  for ( uint i = 0; i < sampleCount; ++i )
    someSamples.push_back( { MathUtil::Halton( i, 2 ), MathUtil::Halton( i, 3 ) } );

  GpuBufferProperties props;
  props.myBindFlags = ( uint ) GpuBufferBindFlags::SHADER_BUFFER;
  props.myElementSizeBytes = sizeof( glm::float2 );
  props.myNumElements = sampleCount;
  GpuBufferViewProperties viewProps;
  viewProps.myIsRaw = true;
  myRtScene->myHaltonSamplesBuf = RenderCore::CreateBuffer( props, "Halton samples", someSamples.data() );
  myRtScene->myHaltonSamples = RenderCore::CreateBufferView( RenderCore::GetBuffer( myRtScene->myHaltonSamplesBuf ),
                                                             viewProps, "Halton samples" );
}

PathTracer::~PathTracer() {
  RenderCore::ourOnRtPipelineStateRecompiled.DetachObserver( this );
  ImGuiRendering::Shutdown();
  ImGui::DestroyContext( myImGuiContext );
  myImGuiContext = nullptr;

  if ( myHdrLightTexRead.IsValid() )
    RenderCore::DeleteTextureView( myHdrLightTexRead );
  if ( myHdrLightTexWrite.IsValid() )
    RenderCore::DeleteTextureView( myHdrLightTexWrite );
  if ( myHdrLightTexRtv.IsValid() )
    RenderCore::DeleteTextureView( myHdrLightTexRtv );
  if ( myHdrLightTex.IsValid() )
    RenderCore::DeleteTexture( myHdrLightTex );
  if ( myDepthStencilDsv.IsValid() )
    RenderCore::DeleteTextureView( myDepthStencilDsv );
  if ( myDepthStencilTex.IsValid() )
    RenderCore::DeleteTexture( myDepthStencilTex );
  if ( myDdsTestSrv.IsValid() )
    RenderCore::DeleteTextureView( myDdsTestSrv );
}

void PathTracer::OnWindowResized( uint aWidth, uint aHeight ) {
  Application::OnWindowResized( aWidth, aHeight );
  UpdateDepthbuffer();
  UpdateOutputTexture();
  RestartAccumulation();
}

bool PathTracer::CameraHasChanged() {
  for ( int i = 0; i < 4; ++i )
    for ( int k = 0; k < 4; ++k )
      if ( glm::abs( myCamera.myViewProj[ i ][ k ] - myLastViewMat[ i ][ k ] ) > 0.0001f )
        return true;

  return false;
}

void PathTracer::UpdateMainMenuBar() {
  if ( ImGui::BeginMainMenuBar() ) {
    if ( ImGui::BeginMenu( "Load Scene" ) ) {
      for ( int i = 0; i < ARRAY_LENGTH( sceneLoadInfos ); ++i ) {
        const SceneLoadInfo & loadInfo = sceneLoadInfos[ i ];
        if ( ImGui::MenuItem( loadInfo.myDisplayName.c_str() ) ) {
          LoadScene( loadInfo.myPath.c_str(), loadInfo.myCamPos );
        }
      }
      ImGui::EndMenu();
    }

    if ( ImGui::BeginMenu( "Sky" ) ) {
      mySky_Imgui.Update( mySky.get() );
      ImGui::EndMenu();
    }

    if ( ImGui::BeginMenu( "PathTracing" ) ) {
      UpdatePathTracingSettings();
      ImGui::EndMenu();
    }

    if ( ImGui::BeginMenu( "Texture List" ) ) {
      myTextureList.Update();
      ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
  }
}

void PathTracer::UpdatePathTracingSettings() {
  ImGui::Text( "View Pos: %.3f, %.3f, %.3f", myCamera.myPosition.x, myCamera.myPosition.y, myCamera.myPosition.z );
  ImGui::SliderFloat( "Move Speed", &myCameraController.myMoveSpeed, 1.0f, 10000.0f );

  float64 msNow = SampleTimeMs();
  float64 frameTime = msNow - myLastTimeMs;
  myLastTimeMs = msNow;
  ImGui::Text( "Frame Time: %.3f ms", ( float ) frameTime );

  if ( ImGui::Checkbox( "Render Half Res", &myHalfResRender ) ) {
    UpdateOutputTexture();
    RestartAccumulation();
  }

  if ( mySupportsRaytracing ) {
    if ( ImGui::Checkbox( "Render Raster", &myRenderRaster ) && !myRenderRaster )
      RestartAccumulation();

    if ( !myRenderRaster ) {
      if ( ImGui::Checkbox( "Render AO", &myRenderAo ) )
        RestartAccumulation();

      if ( ImGui::Checkbox( "Accumulate", &myAccumulate ) )
        RestartAccumulation();

      if ( ImGui::Checkbox( "Sample Sky", &mySampleSky ) )
        RestartAccumulation();

      if ( !mySampleSky ) {
        if ( ImGui::SliderFloat( "Sky Fallback Intensity", &mySkyFallbackIntensity, 0.0f, 1000.0f ) )
          RestartAccumulation();
      }

      ImGui::Text( "Accumulation Frame %i", myNumAccumulationFrames );

      if ( myRenderAo ) {
        if ( ImGui::DragFloat( "Ao Distance", &myAoDistance ) )
          RestartAccumulation();
      } else {
        if ( ImGui::SliderFloat( "Phong Spec Power", &myPhongSpecularPower, 0.0f, 1000.0f ) )
          RestartAccumulation();

        if ( ImGui::InputInt( "Max Recursion Depth", &myMaxRecursionDepth, 1 ) )
          RestartAccumulation();

        if ( ImGui::Checkbox( "Enable Light", &myLightEnabled ) )
          RestartAccumulation();

        if ( myLightEnabled ) {
          if ( ImGui::SliderFloat( "Light Strength", &myLightStrength, 0, 5000 ) )
            RestartAccumulation();

          if ( ImGui::InputInt( "Light Instance Idx", &myLightInstanceIdx, 1 ) )
            RestartAccumulation();

          float col[ 3 ] = { myLightColor.x, myLightColor.y, myLightColor.z };
          if ( ImGui::ColorPicker3( "Light Color", col ) ) {
            myLightColor = glm::float3( col[ 0 ], col[ 1 ], col[ 2 ] );
            RestartAccumulation();
          }
        }
      }
    }
  }
}

void PathTracer::BeginFrame() {
  Application::BeginFrame();
  ImGuiRendering::NewFrame();
}

void PathTracer::Update() {
  Application::Update();

  UpdateMainMenuBar();

  if ( !myAccumulate ) {
    RestartAccumulation();  // Always render frame 0 only
  }

  if ( CameraHasChanged() || mySky_Imgui.HaveSettingsChanged() ) {
    RestartAccumulation();
  }

  myLastViewMat = myCamera.myViewProj;
}

void PathTracer::Render() {
  Application::Render();

  CommandList * ctx = RenderCore::BeginCommandList( CommandListType::Graphics );
  {
    GPU_SCOPED_PROFILER_FUNCTION( ctx, 0u );

    mySky->ComputeTranmittanceLut( ctx );

    if ( myRenderRaster || !mySupportsRaytracing ) {
      RenderRaster( ctx );
    } else {
      RenderRT( ctx );
    }

    TonemapComposit( ctx );
  }

  RenderCore::ExecuteAndFreeCommandList( ctx );

  ImGui::Render();
  ImGuiRendering::RenderDrawLists( ImGui::GetDrawData() );
}

void PathTracer::RenderRaster( CommandList * ctx ) {
  GPU_SCOPED_PROFILER_FUNCTION( ctx, 0u );

  mySky->ComputeSkyViewLut( ctx, myCamera );

  TextureView * hdrLightTexRead = RenderCore::GetTextureView( myHdrLightTexRead );
  uint dstTexWidth = hdrLightTexRead->GetTexture()->GetProperties().myWidth;
  uint dstTexHeight = hdrLightTexRead->GetTexture()->GetProperties().myHeight;

  ctx->SetViewport( glm::uvec4( 0, 0, dstTexWidth, dstTexHeight ) );
  ctx->SetClipRect( glm::uvec4( 0, 0, dstTexWidth, dstTexHeight ) );
  ctx->ClearDepthStencilTarget( RenderCore::GetTextureView( myDepthStencilDsv ), 1.0f, 0u,
                                ( uint ) DepthStencilClearFlags::CLEAR_ALL );
  glm::float4 clearColor( 0.0f );
  ctx->ClearRenderTarget( RenderCore::GetTextureView( myHdrLightTexRtv ), &clearColor[ 0 ] );

  mySky->Render( ctx, RenderCore::GetTextureView( myHdrLightTexWrite ), nullptr, myCamera );

  ctx->SetRenderTarget( RenderCore::GetTextureView( myHdrLightTexRtv ),
                        RenderCore::GetTextureView( myDepthStencilDsv ) );

  ctx->SetDepthStencilState( nullptr );
  ctx->SetBlendState( nullptr );
  ctx->SetCullMode( CullMode::NONE );
  ctx->SetFillMode( FillMode::SOLID );
  ctx->SetWindingOrder( WindingOrder::CCW );

  ctx->SetTopologyType( TopologyType::TRIANGLE_LIST );
  ctx->SetShaderPipeline( RenderCore::GetShaderPipeline( myUnlitMeshShader ) );

  auto RenderMesh = [ ctx ]( Mesh * mesh ) {
    for ( SharedPtr< MeshPart > & meshPart : mesh->myParts ) {
      const VertexInputLayout * layout = RenderCore::GetVertexInputLayout( meshPart->myVertexInputLayout );

      const GpuBuffer * vertexBuffer = RenderCore::GetBuffer( meshPart->myVertexBuffer );
      uint64 offset = 0ull;
      uint64 size = vertexBuffer->GetByteSize();
      ctx->BindVertexBuffers( &vertexBuffer, &offset, &size, 1u, layout );
      const GpuBuffer * indexBuffer = RenderCore::GetBuffer( meshPart->myIndexBuffer );
      ctx->BindIndexBuffer( indexBuffer, indexBuffer->GetProperties().myElementSizeBytes );

      ctx->DrawIndexedInstanced( indexBuffer->GetProperties().myNumElements, 1u, 0, 0, 0 );
    }
  };

  for ( SceneMeshInstance & meshInstance : myScene->myInstances ) {
    Mesh * mesh = myScene->myMeshes[ meshInstance.myMeshIndex ].get();
    glm::float4x4 transform = meshInstance.myTransform;
    Material * material = myScene->myMaterials[ meshInstance.myMaterialIndex ].get();

    struct Cbuffer_PerObject {
      glm::float4x4 myWorldViewProj;
      glm::float4 myColor;
    };
    Cbuffer_PerObject cbuffer_perObject{ myCamera.myViewProj * transform,
                                         material->myParameters[ ( uint ) MaterialParameterType::COLOR ] };
    ctx->BindConstantBuffer( &cbuffer_perObject, sizeof( cbuffer_perObject ), 0 );

    RenderMesh( mesh );
  }
}

void PathTracer::RenderRT( CommandList * ctx ) {
  GPU_SCOPED_PROFILER_FUNCTION( ctx, 0u );

  TextureView * hdrLightTexRead = RenderCore::GetTextureView( myHdrLightTexRead );
  uint dstTexWidth = hdrLightTexRead->GetTexture()->GetProperties().myWidth;
  uint dstTexHeight = hdrLightTexRead->GetTexture()->GetProperties().myHeight;

  TextureView * hdrLightTexWrite = RenderCore::GetTextureView( myHdrLightTexWrite );

  if ( myAccumulationNeedsClear ) {
    ctx->PrepareResourceShaderAccess( hdrLightTexWrite );

    uint texIdx = hdrLightTexWrite->GetGlobalDescriptorIndex();
    ctx->BindConstantBuffer( &texIdx, sizeof( texIdx ), 0 );
    ctx->SetShaderPipeline( RenderCore::GetShaderPipeline( myClearTextureShader ) );
    ctx->Dispatch( glm::ivec3( dstTexWidth, dstTexHeight, 1 ) );
    ctx->ResourceUAVbarrier( hdrLightTexWrite->GetTexture() );
    myAccumulationNeedsClear = false;
    myNumAccumulationFrames = 0u;
  }

  RtPipelineState * rtPso = myRenderAo ? RenderCore::GetRtPipelineState( myRtScene->myAoRtPso )
                                       : RenderCore::GetRtPipelineState( myRtScene->myRtPso );
  RtShaderBindingTable * rtSbt = myRenderAo ? RenderCore::GetRtShaderBindingTable( myRtScene->myAoSBT )
                                            : RenderCore::GetRtShaderBindingTable( myRtScene->mySBT );
  ctx->SetRaytracingPipelineState( rtPso );

  eastl::fixed_vector< glm::float3, 4 > nearPlaneVertices;
  myCamera.GetVerticesOnNearPlane( nearPlaneVertices );

  struct SkyConstants {
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
  skyConsts.myTransmissionLutTexIdx =
      ctx->GetPrepareDescriptorIndex( RenderCore::GetTextureView( mySky->myTransmittanceLutRead ) );
  skyConsts.mySunIlluminance = mySky->mySunIlluminance;
  skyConsts.myRayMarchMinMaxSPP = glm::float2( 4.0f, 14.0f );

  struct RtConsts {
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

  GpuBufferView * instanceData = RenderCore::GetBufferView( myRtScene->myInstanceData );
  GpuBufferView * materialData = RenderCore::GetBufferView( myRtScene->myMaterialData );
  GpuBufferView * haltonSamples = RenderCore::GetBufferView( myRtScene->myHaltonSamples );
  RtAccelerationStructure * tlas = RenderCore::GetRtAccelerationStructure( myRtScene->myTLAS );

  rtConsts.myNearPlaneCorner = nearPlaneVertices[ 0 ];
  rtConsts.myAoDistance = myAoDistance;
  rtConsts.myXAxis = nearPlaneVertices[ 1 ] - nearPlaneVertices[ 0 ];
  rtConsts.myYAxis = nearPlaneVertices[ 3 ] - nearPlaneVertices[ 0 ];
  rtConsts.myOutTexIndex = hdrLightTexWrite->GetGlobalDescriptorIndex();
  rtConsts.myAsIndex = tlas->GetBufferRead()->GetGlobalDescriptorIndex();
  rtConsts.myCameraPos = myCamera.myPosition;
  rtConsts.myInstanceDataBufferIndex = instanceData->GetGlobalDescriptorIndex();
  rtConsts.myMaterialDataBufferIndex = materialData->GetGlobalDescriptorIndex();
  rtConsts.mySampleBufferIndex = haltonSamples->GetGlobalDescriptorIndex();
  rtConsts.myLightInstanceId = myLightInstanceIdx;
  rtConsts.myNumHaltonSamples = haltonSamples->GetBuffer()->GetProperties().myNumElements;
  rtConsts.myLightEmission = myLightEnabled ? myLightColor * myLightStrength : glm::float3( 0.0f );
  rtConsts.mySampleSky = mySampleSky ? 1u : 0u;
  rtConsts.mySkyFallbackEmission = glm::float3( mySkyFallbackIntensity );
  rtConsts.myPhongSpecularPower = myPhongSpecularPower;
  rtConsts.myFrameRandomSeed = ( uint ) Time::ourFrameIdx;
  rtConsts.myNumAccumulationFrames = myNumAccumulationFrames++;
  rtConsts.myLinearClampSamplerIndex =
      RenderCore::GetTextureSampler( RenderCore::ourLinearClampSampler )->GetGlobalDescriptorIndex();
  rtConsts.myMaxRecursionDepth = ( uint ) myMaxRecursionDepth;
  rtConsts.mySkyConsts = skyConsts;
  ctx->BindConstantBuffer( &rtConsts, sizeof( rtConsts ), 0 );

  ctx->PrepareResourceShaderAccess( hdrLightTexWrite );
  ctx->PrepareResourceShaderAccess( tlas->GetBufferRead() );
  ctx->PrepareResourceShaderAccess( instanceData );
  ctx->PrepareResourceShaderAccess( materialData );
  ctx->PrepareResourceShaderAccess( haltonSamples );

  DispatchRaysDesc desc;
  desc.myRayGenShaderTableRange = rtSbt->GetRayGenRange();
  desc.myMissShaderTableRange = rtSbt->GetMissRange();
  desc.myHitGroupTableRange = rtSbt->GetHitRange();
  desc.myWidth = dstTexWidth;
  desc.myHeight = dstTexHeight;
  desc.myDepth = 1;
  ctx->DispatchRays( desc );

  ctx->ResourceUAVbarrier( hdrLightTexWrite->GetTexture() );
}

void PathTracer::TonemapComposit( CommandList * ctx ) {
  GPU_SCOPED_PROFILER_FUNCTION( ctx, 0u );

  RenderOutput * renderOutput = RenderCore::GetRenderOutput( myRenderOutput );

  ctx->SetShaderPipeline( RenderCore::GetShaderPipeline( myTonemapCompositShader ) );
  ctx->SetDepthStencilState( RenderCore::GetDepthStencilState( myDepthTestOff ) );
  ctx->SetCullMode( CullMode::NONE );
  ctx->SetViewport( glm::uvec4( 0, 0, renderOutput->GetWindow()->GetWidth(), renderOutput->GetWindow()->GetHeight() ) );
  ctx->SetClipRect( glm::uvec4( 0, 0, renderOutput->GetWindow()->GetWidth(), renderOutput->GetWindow()->GetHeight() ) );

  struct TonemapConsts {
    bool myIsBGR;
    uint mySrcTextureIdx;
    uint myLinearClampSamplerIndex;
    uint myNumAccumulationFrames;

    glm::float2 myPixelToUv;
  } tonemapConsts;

  TextureView * hdrLightTexRead = RenderCore::GetTextureView( myHdrLightTexRead );
  tonemapConsts.myIsBGR = renderOutput->GetBackbuffer()->GetProperties().myFormat == DataFormat::BGRA_8 ? 1 : 0;
  tonemapConsts.mySrcTextureIdx = hdrLightTexRead->GetGlobalDescriptorIndex();
  tonemapConsts.myLinearClampSamplerIndex =
      RenderCore::GetTextureSampler( RenderCore::ourLinearClampSampler )->GetGlobalDescriptorIndex();
  tonemapConsts.myPixelToUv = glm::float2( 1.0f ) / glm::float2( renderOutput->GetWindow()->GetWidth(),
                                                                 renderOutput->GetWindow()->GetHeight() );
  tonemapConsts.myNumAccumulationFrames = myNumAccumulationFrames;
  ctx->BindConstantBuffer( &tonemapConsts, sizeof( tonemapConsts ), 0 );
  ctx->PrepareResourceShaderAccess( hdrLightTexRead );

  glm::float2 fsTriangleVerts[] = { { -4.0f, -1.0f }, { 1.0f, -1.0f }, { 1.0f, 4.0f } };
  ctx->BindVertexBuffer( fsTriangleVerts, sizeof( fsTriangleVerts ) );
  ctx->SetRenderTarget( renderOutput->GetBackbufferRtv(), nullptr );
  ctx->DrawInstanced( 3, 1, 0, 0 );
}

void PathTracer::EndFrame() {
  Application::EndFrame();
}

void PathTracer::OnRtPipelineRecompiled( const RtPipelineState * aRtPipeline ) {
  RestartAccumulation();
}

void PathTracer::UpdateOutputTexture() {
  RenderCore::WaitForIdle( CommandListType::Graphics );

  if ( myHdrLightTexRead.IsValid() )
    RenderCore::DeleteTextureView( myHdrLightTexRead );
  if ( myHdrLightTexWrite.IsValid() )
    RenderCore::DeleteTextureView( myHdrLightTexWrite );
  if ( myHdrLightTexRtv.IsValid() )
    RenderCore::DeleteTextureView( myHdrLightTexRtv );
  if ( myHdrLightTex.IsValid() )
    RenderCore::DeleteTexture( myHdrLightTex );

  uint width = RenderCore::GetRenderOutput( myRenderOutput )->GetWindow()->GetWidth();
  uint height = RenderCore::GetRenderOutput( myRenderOutput )->GetWindow()->GetHeight();

  if ( myHalfResRender ) {
    width = ( width + 1 ) / 2;
    height = ( height + 1 ) / 2;
  }

  TextureProperties props;
  props.myDimension = GpuResourceDimension::TEXTURE_2D;
  props.myFormat = DataFormat::RGBA_32F;
  props.myIsShaderWritable = true;
  props.myIsRenderTarget = true;
  props.myWidth = width;
  props.myHeight = height;
  props.myNumMipLevels = 1u;
  myHdrLightTex = RenderCore::CreateTexture( props, "Light output texture" );
  ASSERT( myHdrLightTex.IsValid() );
  Texture * lightTex = RenderCore::GetTexture( myHdrLightTex );

  TextureViewProperties viewProps;
  myHdrLightTexRead = RenderCore::CreateTextureView( lightTex, viewProps, "Light output texture read" );
  ASSERT( myHdrLightTexRead.IsValid() );

  viewProps.myIsShaderWritable = true;
  myHdrLightTexWrite = RenderCore::CreateTextureView( lightTex, viewProps, "Light output texture write" );
  ASSERT( myHdrLightTexWrite.IsValid() );

  viewProps.myIsRenderTarget = true;
  viewProps.myIsShaderWritable = false;
  myHdrLightTexRtv = RenderCore::CreateTextureView( lightTex, viewProps, "Light output texture rtv" );
  ASSERT( myHdrLightTexRtv.IsValid() );
}

void PathTracer::UpdateDepthbuffer() {
  if ( myDepthStencilDsv.IsValid() )
    RenderCore::DeleteTextureView( myDepthStencilDsv );
  if ( myDepthStencilTex.IsValid() )
    RenderCore::DeleteTexture( myDepthStencilTex );

  uint width = RenderCore::GetRenderOutput( myRenderOutput )->GetWindow()->GetWidth();
  uint height = RenderCore::GetRenderOutput( myRenderOutput )->GetWindow()->GetHeight();

  TextureProperties dsTexProps;
  dsTexProps.myDimension = GpuResourceDimension::TEXTURE_2D;
  dsTexProps.bIsDepthStencil = true;
  dsTexProps.myFormat = DataFormat::D_24UNORM_S_8UI;
  dsTexProps.myIsRenderTarget = true;
  dsTexProps.myIsShaderWritable = false;
  dsTexProps.myWidth = width;
  dsTexProps.myHeight = height;
  dsTexProps.myNumMipLevels = 1u;

  myDepthStencilTex = RenderCore::CreateTexture( dsTexProps, "Backbuffer DepthStencil Texture" );
  ASSERT( myDepthStencilTex.IsValid() );
  Texture * dsTex = RenderCore::GetTexture( myDepthStencilTex );

  TextureViewProperties props;
  props.myDimension = GpuResourceDimension::TEXTURE_2D;
  props.myIsRenderTarget = true;
  props.myFormat = DataFormat::D_24UNORM_S_8UI;
  props.mySubresourceRange = dsTex->mySubresources;
  myDepthStencilDsv = RenderCore::CreateTextureView( dsTex, props, "DepthStencil Texture" );
  ASSERT( myDepthStencilDsv.IsValid() );
}

void PathTracer::RestartAccumulation() {
  myNumAccumulationFrames = 0u;
  myAccumulationNeedsClear = true;
}