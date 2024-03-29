#include "Sky_Imgui.h"
#include "Sky.h"

#include "imgui.h"

#include "Rendering/Texture.h"
#include "Common/MathUtil.h"

bool Sky_Imgui::Update(Sky* aSky)
{
	myImgui_settingsChanged = false;
	// ImGui::Begin("Sky", &myImgui_windowOpen);

	myImgui_TransmittanceLutImg.Update(aSky->myTransmittanceLutRead.get(), "Transmittance LUT");
	myImgui_SkyViewLutImg.Update(aSky->mySkyViewLutRead.get(), "Sky-View LUT");

	bool skyParamsChanged = false;

	glm::float2 sunDirThetaPhi = MathUtil::ToSpherical(aSky->mySunDir);
	skyParamsChanged |= ImGui::DragFloat("Sun dir theta", &sunDirThetaPhi.x, 0.01f, 0.0f, glm::half_pi<float>());
	skyParamsChanged |= ImGui::DragFloat("Sun dir phi", &sunDirThetaPhi.y, 0.01f, 0.0f, glm::two_pi<float>());
	aSky->mySunDir = MathUtil::ToCartesian(sunDirThetaPhi);

	skyParamsChanged |= ImGui::DragFloat("Sun illum", &aSky->mySunIlluminance.x);
	aSky->mySunIlluminance.y = aSky->mySunIlluminance.x;
	aSky->mySunIlluminance.z = aSky->mySunIlluminance.x;

	skyParamsChanged |= ImGui::DragFloat("Bottom Radius", &aSky->myAtmosphereParams.BottomRadius);
	skyParamsChanged |= ImGui::DragFloat("Top Radius", &aSky->myAtmosphereParams.TopRadius);
	skyParamsChanged |= ImGui::DragFloat3("Ground Albedo", &aSky->myAtmosphereParams.GroundAlbedo[0]);
	skyParamsChanged |= ImGui::DragFloat3("Rayleigh Scattering", &aSky->myAtmosphereParams.RayleighScattering[0]);
	skyParamsChanged |= ImGui::DragFloat("Rayleigh ExpScale", &aSky->myAtmosphereParams.RayleighDensityExpScale);
	skyParamsChanged |= ImGui::DragFloat3("Absorption Extinction", &aSky->myAtmosphereParams.AbsorptionExtinction[0]);
	skyParamsChanged |= ImGui::DragFloat3("Mie Scattering", &aSky->myAtmosphereParams.MieScattering[0]);
	skyParamsChanged |= ImGui::DragFloat("Mie ExpScale", &aSky->myAtmosphereParams.MieDensityExpScale);
	skyParamsChanged |= ImGui::DragFloat("Mie PhaseG", &aSky->myAtmosphereParams.MiePhaseG);
	skyParamsChanged |= ImGui::DragFloat3("Mie Absorption", &aSky->myAtmosphereParams.MieAbsorption[0]);

	// ImGui::End();

	myImgui_settingsChanged = skyParamsChanged;
	return skyParamsChanged;
}

bool Sky_Imgui::HaveSettingsChanged()
{
	return myImgui_settingsChanged;
}


