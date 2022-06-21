#include "Sky.h"

#include "Common/Camera.h"
#include "Rendering/RenderCore.h"
#include "Common/MathIncludes.h"
#include "Debug/Profiler.h"
#include "Rendering/CommandList.h"
#include "Rendering/Texture.h"
#include "imgui.h"

namespace Priv_Sky
{
	struct SkyLutConsts
	{
		enum
		{
			TRANSMITTANCE_TEXTURE_WIDTH = 256,
			TRANSMITTANCE_TEXTURE_HEIGHT = 64,
			SKY_VIEW_TEXTURE_WIDTH = 192,
			SKY_VIEW_TEXTURE_HEIGHT = 108,
			SCATTERING_TEXTURE_R_SIZE = 32,
			SCATTERING_TEXTURE_MU_SIZE = 128,
			SCATTERING_TEXTURE_MU_S_SIZE = 32,
			SCATTERING_TEXTURE_NU_SIZE = 8,
			IRRADIANCE_TEXTURE_WIDTH = 64,
			IRRADIANCE_TEXTURE_HEIGHT = 16,
			MULTI_SCATTERING_LUT_RES = 32,

			SCATTERING_TEXTURE_WIDTH = SCATTERING_TEXTURE_NU_SIZE * SCATTERING_TEXTURE_MU_S_SIZE,
			SCATTERING_TEXTURE_HEIGHT = SCATTERING_TEXTURE_MU_SIZE,
			SCATTERING_TEXTURE_DEPTH = SCATTERING_TEXTURE_R_SIZE,
		};
	};

	struct SkyAtmosphereCbuffer
	{
		AtmosphereParameters myAtmosphereParameters;

		int TRANSMITTANCE_TEXTURE_WIDTH;
		int TRANSMITTANCE_TEXTURE_HEIGHT;
		int IRRADIANCE_TEXTURE_WIDTH;
		int IRRADIANCE_TEXTURE_HEIGHT;

		int SCATTERING_TEXTURE_R_SIZE;
		int SCATTERING_TEXTURE_MU_SIZE;
		int SCATTERING_TEXTURE_MU_S_SIZE;
		int SCATTERING_TEXTURE_NU_SIZE;

		glm::float3 SKY_SPECTRAL_RADIANCE_TO_LUMINANCE;
		int SKY_VIEW_TEXTURE_WIDTH;
		glm::float3 SUN_SPECTRAL_RADIANCE_TO_LUMINANCE;
		int SKY_VIEW_TEXTURE_HEIGHT;

		//
		// Other globals
		//
		glm::float4x4 gSkyViewProjMat;
		glm::float4x4 gSkyInvViewProjMat;
		glm::float4x4 gShadowmapViewProjMat;

		glm::float3 camera;
		float  pad5;
		glm::float3 sun_direction;
		float  pad6;
		glm::float3 view_ray;
		float  pad7;

		float MultipleScatteringFactor;
		float MultiScatteringLUTRes;
		float pad9;
		float pad10;
	};

	struct CommonCbuffer
	{
    glm::float3 gSunIlluminance;
		int gScatteringMaxPathDepth;

		glm::uvec2 gResolution;
		float gFrameTimeSec;
		float gTimeSec;

    glm::float2 RayMarchMinMaxSPP;
		uint gFrameId;

		uint myOutTexIndex;
		uint myTransmittanceLutTextureIndex;
		uint myLinearClampSamplerIndex;
	};

	void SetupEarthAtmosphere(AtmosphereParameters& someParams)
	{
		// All units in kilometers
		const float EarthBottomRadius = 6360.0f;
		const float EarthTopRadius = 6460.0f;   // 100km atmosphere radius, less edge visible and it contain 99.99% of the atmosphere medium https://en.wikipedia.org/wiki/K%C3%A1rm%C3%A1n_line
		const float EarthRayleighScaleHeight = 8.0f;
		const float EarthMieScaleHeight = 1.2f;

		someParams.RayleighScattering = { 0.005802f, 0.013558f, 0.033100f };		// 1/km
		someParams.RayleighDensityExpScale = -1.0f / EarthRayleighScaleHeight;
		someParams.AbsorptionExtinction = { 0.000650f, 0.001881f, 0.000085f };	// 1/km
		someParams.BottomRadius = EarthBottomRadius;
		someParams.GroundAlbedo = { 0.0f, 0.0f, 0.0f };
		someParams.TopRadius = EarthTopRadius;
		someParams.MieScattering = { 0.003996f, 0.003996f, 0.003996f };			// 1/km
		someParams.MieDensityExpScale = -1.0f / EarthMieScaleHeight;
		someParams.MieExtinction = { 0.004440f, 0.004440f, 0.004440f };			// 1/km
		someParams.MiePhaseG = 0.8f;
		someParams.MieAbsorption = glm::max(glm::float3(0, 0, 0), someParams.MieExtinction - someParams.MieScattering);
		someParams.AbsorptionDensity0LayerWidth = 25.0f;
		someParams.AbsorptionDensity0ConstantTerm = -2.0f / 3.0f;
		someParams.AbsorptionDensity0LinearTerm = 1.0f / 15.0f;
		someParams.AbsorptionDensity1ConstantTerm = 8.0f / 3.0f;
		someParams.AbsorptionDensity1LinearTerm = -1.0f / 15.0f;
	}
}

Sky::Sky(const SkyParameters& someParams, Camera& aCamera)
  : myParams(someParams)
  , myCamera(aCamera)
{
  Priv_Sky::SetupEarthAtmosphere(myAtmosphereParams);

  myComputeTransmittanceLut = RenderCore::CreateComputeShaderPipeline("resources/shaders/sky/RenderSkyRayMarching.hlsl", "ComputeTransmittanceLut", "OPTICAL_DEPTH_ONLY");
	myComputeSkyViewLut = RenderCore::CreateComputeShaderPipeline("resources/shaders/sky/RenderSkyRayMarching.hlsl", "ComputeSkyViewLut");

  ASSERT(myComputeTransmittanceLut);

	// Transmittance lut texture
  {
		TextureProperties props;
		props.myWidth = Priv_Sky::SkyLutConsts::TRANSMITTANCE_TEXTURE_WIDTH;
		props.myHeight = Priv_Sky::SkyLutConsts::TRANSMITTANCE_TEXTURE_HEIGHT;
		props.myFormat = DataFormat::RGBA_16F;
		props.myIsShaderWritable = true;
		SharedPtr<Texture> transmittanceLutTex = RenderCore::CreateTexture(props, "Transmittance Lut");

		TextureViewProperties viewProps;
		myTransmittanceLutRead = RenderCore::CreateTextureView(transmittanceLutTex, viewProps, "Transmittance Lut Srv");

		viewProps.myIsShaderWritable = true;
		myTransmittanceLutWrite = RenderCore::CreateTextureView(transmittanceLutTex, viewProps, "Transmittance Lut Uav");
  }

	// Sky view lut texture
  {
		TextureProperties props;
		props.myWidth = Priv_Sky::SkyLutConsts::SKY_VIEW_TEXTURE_WIDTH;
		props.myHeight = Priv_Sky::SkyLutConsts::SKY_VIEW_TEXTURE_HEIGHT;
		props.myFormat = DataFormat::RGB_11_11_10F;
		props.myIsShaderWritable = true;
		SharedPtr<Texture> skyViewLutTex = RenderCore::CreateTexture(props, "Sky view Lut");

		TextureViewProperties viewProps;
		mySkyViewLutRead = RenderCore::CreateTextureView(skyViewLutTex, viewProps, "Sky view Lut Srv");

		viewProps.myIsShaderWritable = true;
		mySkyViewLutWrite = RenderCore::CreateTextureView(skyViewLutTex, viewProps, "Sky view Lut Uav");
  }

	// Linear clamp sampler
  {
		TextureSamplerProperties samplerProps;
		samplerProps.myMinFiltering = SamplerFilterMode::BILINEAR;
		samplerProps.myMagFiltering = SamplerFilterMode::BILINEAR;
		myLinearClampSampler = RenderCore::CreateTextureSampler(samplerProps);
  }
}

Sky::~Sky()
{
}

void Sky::UpdateImgui()
{
	ImGui::Begin("Sky", &myImgui_windowOpen);

	ImGui::Text("Transmittance LUT");
	ImGui::Image((ImTextureID)myTransmittanceLutRead.get()
		, { (float) myTransmittanceLutRead->GetTexture()->GetProperties().myWidth, (float) myTransmittanceLutRead->GetTexture()->GetProperties().myHeight });

	ImGui::Text("Sky-View LUT");
	ImGui::Image((ImTextureID)mySkyViewLutRead.get()
		, { (float)mySkyViewLutRead->GetTexture()->GetProperties().myWidth, (float)mySkyViewLutRead->GetTexture()->GetProperties().myHeight });

	ImGui::End();
}

void Sky::ComputeLuts(CommandList* ctx)
{
	using namespace Priv_Sky;
	
	SkyAtmosphereCbuffer atmoCbuffer = {};
	atmoCbuffer.myAtmosphereParameters = myAtmosphereParams;
	atmoCbuffer.TRANSMITTANCE_TEXTURE_WIDTH = SkyLutConsts::TRANSMITTANCE_TEXTURE_WIDTH;
	atmoCbuffer.TRANSMITTANCE_TEXTURE_HEIGHT = SkyLutConsts::TRANSMITTANCE_TEXTURE_HEIGHT;
	atmoCbuffer.IRRADIANCE_TEXTURE_WIDTH = SkyLutConsts::IRRADIANCE_TEXTURE_WIDTH;
	atmoCbuffer.IRRADIANCE_TEXTURE_HEIGHT = SkyLutConsts::IRRADIANCE_TEXTURE_HEIGHT;
	atmoCbuffer.SCATTERING_TEXTURE_R_SIZE = SkyLutConsts::SCATTERING_TEXTURE_R_SIZE;
	atmoCbuffer.SCATTERING_TEXTURE_MU_SIZE = SkyLutConsts::SCATTERING_TEXTURE_MU_SIZE;
	atmoCbuffer.SCATTERING_TEXTURE_MU_S_SIZE = SkyLutConsts::SCATTERING_TEXTURE_MU_S_SIZE;
	atmoCbuffer.SCATTERING_TEXTURE_NU_SIZE = SkyLutConsts::SCATTERING_TEXTURE_NU_SIZE;
	atmoCbuffer.SKY_SPECTRAL_RADIANCE_TO_LUMINANCE = glm::float3(114974.916437f, 71305.954816f, 65310.548555f); // Not used if using LUTs as transfert
	atmoCbuffer.SUN_SPECTRAL_RADIANCE_TO_LUMINANCE = glm::float3(98242.786222f, 69954.398112f, 66475.012354f);  // idem
	atmoCbuffer.SKY_VIEW_TEXTURE_WIDTH = SkyLutConsts::SKY_VIEW_TEXTURE_WIDTH;
	atmoCbuffer.SKY_VIEW_TEXTURE_HEIGHT = SkyLutConsts::SKY_VIEW_TEXTURE_HEIGHT;
	atmoCbuffer.gSkyViewProjMat = myCamera.myViewProj;
	atmoCbuffer.gSkyInvViewProjMat = glm::inverse(myCamera.myViewProj);
	atmoCbuffer.gShadowmapViewProjMat = glm::float4x4(); // Shadow map not used at the moment
	atmoCbuffer.camera = myCamera.myPosition;
	atmoCbuffer.view_ray = myCamera.myViewInv[3];
	atmoCbuffer.sun_direction = mySunDir;
	atmoCbuffer.MultipleScatteringFactor = myMultiScatteringFactor;
	atmoCbuffer.MultiScatteringLUTRes = SkyLutConsts::MULTI_SCATTERING_LUT_RES;
	ctx->BindConstantBuffer(&atmoCbuffer, sizeof(atmoCbuffer), 0);

	CommonCbuffer commonCBuffer = {};
	// Most data not actually read by the shader
	commonCBuffer.myOutTexIndex = myTransmittanceLutWrite->GetGlobalDescriptorIndex();
	ctx->PrepareResourceShaderAccess(myTransmittanceLutWrite.get());

	ctx->BindConstantBuffer(&commonCBuffer, sizeof(commonCBuffer), 1);

	ctx->SetShaderPipeline(myComputeTransmittanceLut.get());

	GPU_BEGIN_PROFILE(ctx, "ComputeTransmissionLut", 0u);
	ctx->Dispatch({ SkyLutConsts::TRANSMITTANCE_TEXTURE_WIDTH, SkyLutConsts::TRANSMITTANCE_TEXTURE_HEIGHT, 1 });
	GPU_END_PROFILE(ctx);

	ctx->ResourceUAVbarrier(myTransmittanceLutWrite->GetTexture());

	// Compute Sky view lut
	ctx->BindConstantBuffer(&atmoCbuffer, sizeof(atmoCbuffer), 0);

	commonCBuffer.gSunIlluminance = mySunIlluminance;
	commonCBuffer.RayMarchMinMaxSPP = glm::float2(4.0f, 14.0f);
	commonCBuffer.gScatteringMaxPathDepth = 4;
	commonCBuffer.myOutTexIndex = mySkyViewLutWrite->GetGlobalDescriptorIndex();
	commonCBuffer.myTransmittanceLutTextureIndex = myTransmittanceLutRead->GetGlobalDescriptorIndex();
	commonCBuffer.myLinearClampSamplerIndex = myLinearClampSampler->GetGlobalDescriptorIndex();
	ctx->BindConstantBuffer(&commonCBuffer, sizeof(commonCBuffer), 1);
	ctx->PrepareResourceShaderAccess(myTransmittanceLutRead.get());
	ctx->PrepareResourceShaderAccess(mySkyViewLutWrite.get());
	ctx->SetShaderPipeline(myComputeSkyViewLut.get());

	GPU_BEGIN_PROFILE(ctx, "ComputeSkyViewLut", 0u);
	ctx->Dispatch({ SkyLutConsts::SKY_VIEW_TEXTURE_WIDTH, SkyLutConsts::SKY_VIEW_TEXTURE_HEIGHT, 1 });
	GPU_END_PROFILE(ctx);
}


