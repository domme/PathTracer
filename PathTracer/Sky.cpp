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
	
	void SetupEarthAtmosphere(AtmosphereParameters& someParams)
	{
		// All units in kilometers
		const float EarthBottomRadius = 6360000.0f;
		const float EarthTopRadius = 6460000.0f;   // 100km atmosphere radius, less edge visible and it contain 99.99% of the atmosphere medium https://en.wikipedia.org/wiki/K%C3%A1rm%C3%A1n_line
		const float EarthRayleighScaleHeight = 8000.0f;
		const float EarthMieScaleHeight = 1200.0f;

		someParams.RayleighScattering = { 0.000005802f, 0.000013558f, 0.000033100f };		// 1/km
		someParams.RayleighDensityExpScale = -1.0f / EarthRayleighScaleHeight;
		someParams.AbsorptionExtinction = { 0.000000650f, 0.000001881f, 0.0000000085f };	// 1/km
		someParams.BottomRadius = EarthBottomRadius;
		someParams.GroundAlbedo = { 0.0f, 0.0f, 0.0f };
		someParams.TopRadius = EarthTopRadius;
		someParams.MieScattering = { 0.000003996f, 0.000003996f, 0.000003996f };			// 1/km
		someParams.MieDensityExpScale = -1.0f / EarthMieScaleHeight;
		someParams.MieExtinction = { 0.000004440f, 0.000004440f, 0.000004440f };			// 1/km
		someParams.MiePhaseG = 0.8f;
		someParams.MieAbsorption = glm::max(glm::float3(0, 0, 0), someParams.MieExtinction - someParams.MieScattering);
		someParams.AbsorptionDensity0LayerWidth = 25000.0f;
		someParams.AbsorptionDensity0ConstantTerm = -2.0f / 3.0f;
		someParams.AbsorptionDensity0LinearTerm = 1.0f / 15.0f;
		someParams.AbsorptionDensity1ConstantTerm = 8.0f / 3.0f;
		someParams.AbsorptionDensity1LinearTerm = -1.0f / 15.0f;
	}
}

Sky::Sky(const SkyParameters& someParams)
  : myParams(someParams)
{
  Priv_Sky::SetupEarthAtmosphere(myAtmosphereParams);

  myComputeTransmittanceLut = RenderCore::CreateComputeShaderPipeline("resources/shaders/sky/compute_transmittance_lut.hlsl", "main", "OPTICAL_DEPTH_ONLY");
	myComputeSkyViewLut = RenderCore::CreateComputeShaderPipeline("resources/shaders/sky/compute_skyView_lut.hlsl", "main");
	//myComputeRaymarching = RenderCore::CreateComputeShaderPipeline("resources/shaders/sky/RenderSkyRayMarching.hlsl", "ComputeRaymarching");
	myRenderSkyShader = RenderCore::CreateComputeShaderPipeline("resources/shaders/render_sky.hlsl");

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

void Sky::ComputeTranmittanceLut(CommandList* ctx)
{
	using namespace Priv_Sky;

	GPU_SCOPED_PROFILER_FUNCTION(ctx, 0u);

	struct Constants
	{
		AtmosphereParameters myAtmosphereParameters;
		glm::uvec2 myTransmittanceTextureRes;
		uint myOutTexIdx;
	} consts;

	consts.myAtmosphereParameters = myAtmosphereParams;
	consts.myTransmittanceTextureRes = { SkyLutConsts::TRANSMITTANCE_TEXTURE_WIDTH, SkyLutConsts::TRANSMITTANCE_TEXTURE_HEIGHT };
	consts.myOutTexIdx = ctx->GetPrepareDescriptorIndex(myTransmittanceLutWrite.get());
	ctx->BindConstantBuffer(&consts, sizeof(consts), 0);

	ctx->SetShaderPipeline(myComputeTransmittanceLut.get());
	ctx->Dispatch({ SkyLutConsts::TRANSMITTANCE_TEXTURE_WIDTH, SkyLutConsts::TRANSMITTANCE_TEXTURE_HEIGHT, 1 });

	ctx->ResourceUAVbarrier(myTransmittanceLutWrite->GetTexture());
}

void Sky::ComputeSkyViewLut(CommandList* ctx, const Camera& aCamera)
{
	using namespace Priv_Sky;

	GPU_SCOPED_PROFILER_FUNCTION(ctx, 0u);

	struct Constants
	{
		AtmosphereParameters myAtmosphereParameters;

		glm::float4x4 myInvViewProj;

		glm::float3 mySunIlluminance;
		uint myLinearClampSamplerIdx;

		glm::float3 mySunDirection;
		uint myTransmissionLutTexIdx;

		glm::float3 myCameraPos;
		uint myOutTexIdx;

		glm::float2 myRayMarchMinMaxSPP;
		glm::uvec2 mySkyViewTextureRes;
	} consts;

	consts.myAtmosphereParameters = myAtmosphereParams;
	consts.myInvViewProj = glm::inverse(aCamera.myViewProj);
	consts.mySunIlluminance = mySunIlluminance;
	consts.myLinearClampSamplerIdx = myLinearClampSampler->GetGlobalDescriptorIndex();
	consts.mySunDirection = mySunDir;
	consts.myTransmissionLutTexIdx = ctx->GetPrepareDescriptorIndex(myTransmittanceLutRead.get());
	consts.myCameraPos = aCamera.myPosition;
	consts.myOutTexIdx = ctx->GetPrepareDescriptorIndex(mySkyViewLutWrite.get());
	consts.myRayMarchMinMaxSPP = glm::float2(4.0f, 14.0f);
	consts.mySkyViewTextureRes = { SkyLutConsts::SKY_VIEW_TEXTURE_WIDTH, SkyLutConsts::SKY_VIEW_TEXTURE_HEIGHT };
	ctx->BindConstantBuffer(&consts, sizeof(consts), 0u);

	ctx->SetShaderPipeline(myComputeSkyViewLut.get());
	ctx->Dispatch({ SkyLutConsts::SKY_VIEW_TEXTURE_WIDTH, SkyLutConsts::SKY_VIEW_TEXTURE_HEIGHT, 1 });
	ctx->ResourceUAVbarrier(mySkyViewLutWrite->GetTexture());

}

void Sky::Render(CommandList* ctx, TextureView* aDestTextureWrite, TextureView* aDepthBufferRead, const Camera& aCamera)
{
	GPU_SCOPED_PROFILER_FUNCTION(ctx, 0u);

	using namespace Priv_Sky;

	glm::uvec2 texSize = { aDestTextureWrite->GetTexture()->GetProperties().myWidth, aDestTextureWrite->GetTexture()->GetProperties().myHeight };
	
	struct Cbuffer
	{
    glm::float2 myInvResolution;
		uint myOutTexIdx;
		uint mySkyViewLutTextureIndex;

    glm::float4x4 myInvViewProj;

    glm::float3 myViewPos;
		float myAtmosphereBottomRadius;

    glm::float3 mySunDirection;
		uint myLinearClampSamplerIndex;

		glm::uvec2 mySkyViewLutTextureRes;
	} cbuffer;

	cbuffer.myInvResolution = glm::float2(1.0f, 1.0f) / glm::float2(texSize);
	cbuffer.myOutTexIdx = ctx->GetPrepareDescriptorIndex(aDestTextureWrite);
	cbuffer.mySkyViewLutTextureIndex = ctx->GetPrepareDescriptorIndex(mySkyViewLutRead.get());
	cbuffer.myInvViewProj = glm::inverse(aCamera.myViewProj);
	cbuffer.myViewPos = aCamera.myPosition;
	cbuffer.myAtmosphereBottomRadius = myAtmosphereParams.BottomRadius;
	cbuffer.mySunDirection = mySunDir;
	cbuffer.myLinearClampSamplerIndex = myLinearClampSampler->GetGlobalDescriptorIndex();
	cbuffer.mySkyViewLutTextureRes = { mySkyViewLutRead->GetTexture()->GetProperties().myWidth, mySkyViewLutRead->GetTexture()->GetProperties().myHeight };
	ctx->BindConstantBuffer(&cbuffer, sizeof(cbuffer), 0);

	ctx->SetShaderPipeline(myRenderSkyShader.get());
	
	ctx->Dispatch({ texSize.x, texSize.y, 1 });

	ctx->ResourceUAVbarrier(aDestTextureWrite->GetTexture());
}


