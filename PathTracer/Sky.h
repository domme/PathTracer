#pragma once

#include "Common/FancyCoreDefines.h"
#include "Common/MathIncludes.h"
#include "Common/Ptr.h"

namespace Fancy
{
	class Camera;
  class CommandList;
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
	class TextureSampler;
}

struct AtmosphereParameters
{
	// Rayleigh scattering coefficients
	glm::float3 RayleighScattering;
	// Rayleigh scattering exponential distribution scale in the atmosphere
	float RayleighDensityExpScale;

	// This other medium only absorb light, e.g. useful to represent ozone in the earth atmosphere
	glm::float3 AbsorptionExtinction;
	// Radius of the planet (center to ground)
	float BottomRadius;

	// The albedo of the ground.
	glm::float3 GroundAlbedo;
	// Maximum considered atmosphere height (center to atmosphere top)
	float TopRadius;

	// Mie scattering coefficients
	glm::float3 MieScattering;
	// Mie scattering exponential distribution scale in the atmosphere
	float MieDensityExpScale;

	// Mie extinction coefficients
	glm::float3 MieExtinction;
	// Mie phase function excentricity
	float MiePhaseG;

	// Mie absorption coefficients
	glm::float3 MieAbsorption;
	// Another medium type in the atmosphere
	float AbsorptionDensity0LayerWidth;

	float AbsorptionDensity0ConstantTerm;
	float AbsorptionDensity0LinearTerm;
	float AbsorptionDensity1ConstantTerm;
	float AbsorptionDensity1LinearTerm;
};

using namespace Fancy;

struct SkyParameters
{
  
};

class Sky
{
public:
  Sky(const SkyParameters& someParams, Camera& aCamera);
  ~Sky();

	void UpdateImgui();

  void ComputeLuts(CommandList* ctx);
	
	float myMultiScatteringFactor = 0.0f;
	glm::float3 mySunDir = glm::float3(0, 1, 0);
	glm::float3 mySunIlluminance = glm::float3(1.0f, 1.0f, 1.0f);
	AtmosphereParameters myAtmosphereParams;

private:
	SharedPtr<TextureView> myTransmittanceLutRead;
	SharedPtr<TextureView> myTransmittanceLutWrite;
	SharedPtr<TextureView> mySkyViewLutRead;
	SharedPtr<TextureView> mySkyViewLutWrite;
	SharedPtr<ShaderPipeline> myComputeTransmittanceLut;
	SharedPtr<ShaderPipeline> myComputeSkyViewLut;
	SharedPtr<TextureSampler> myLinearClampSampler;
	Camera& myCamera;
	SkyParameters myParams;

	bool myImgui_windowOpen = false;
	bool myImgui_showTransmittanceLut = false;
};

