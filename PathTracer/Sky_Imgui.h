#pragma once

#include "DebugImage.h"
#include "Common/FancyCoreDefines.h"

namespace Fancy
{
  class TextureView;
}

class Sky;

class Sky_Imgui
{
public:
	bool Update(Sky* aSky);
	bool HaveSettingsChanged();
	
private:
	bool myImgui_windowOpen = false;
	bool myImgui_showTransmittanceLut = false;
	bool myImgui_settingsChanged = false;
  Fancy::ImGuiDebugImage myImgui_TransmittanceLutImg;
  Fancy::ImGuiDebugImage myImgui_SkyViewLutImg;
};

