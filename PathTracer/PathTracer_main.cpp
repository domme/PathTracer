#include "Windows.h"

#include <array>
#include <EASTL/vector.h>

#include "PathTracer.h"
#include "Common/StringUtil.h"
#include "Common/Window.h"
#include "Rendering/RendererPrerequisites.h"

extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 4; }

extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = u8".\\"; }

using namespace Fancy;

Fancy::UniquePtr<PathTracer> myApp;

_Use_decl_annotations_
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
  int numArgs = 0;
  LPWSTR* commandLineArgs = CommandLineToArgvW(GetCommandLineW(), &numArgs);

  eastl::vector<eastl::string> commandLineArgStrings(numArgs);
  eastl::vector<const char*> cStrings(numArgs);
  for (uint i = 0u; i < numArgs; ++i)
  {
    commandLineArgStrings[i] = StringUtil::ToNarrowString(commandLineArgs[i]);
    cStrings[i] = commandLineArgStrings[i].c_str();
  }

  LocalFree(commandLineArgs);

  RenderPlatformProperties renderProperties;
  WindowParameters windowParams;
  windowParams.myWidth = 1280;
  windowParams.myHeight = 720;
  myApp.reset(new PathTracer(hInstance, cStrings.data(), cStrings.size(), "Path Tracer", renderProperties, windowParams));

  MSG msg = { 0 };
  while (true)
  {
    // Process any messages in the queue.
    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
      TranslateMessage(&msg);
      DispatchMessage(&msg);

      if (msg.message == WM_QUIT)
        break;
    }

    myApp->BeginFrame();
    myApp->Update();
    myApp->Render();
    myApp->EndFrame();
  }

  myApp.reset();

  return 0;
}