#pragma once

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
	
private:
	struct ImGuiDebugImage
	{
    Fancy::TextureView* myTexture = nullptr;
		float myZoom = 1.0f;

		void Update(Fancy::TextureView* aTexture, const char* aName);
	};

	bool myImgui_windowOpen = false;
	bool myImgui_showTransmittanceLut = false;
	ImGuiDebugImage myImgui_TransmittanceLutImg;
	ImGuiDebugImage myImgui_SkyViewLutImg;
};

