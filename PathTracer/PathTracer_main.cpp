#include "imgui.h"
#include "imgui_impl_fancy.h"
#include "Windows.h"
#include "Common/Fancy.h"
#include "Common/Input.h"
#include "Common/StringUtil.h"
#include "Common/Window.h"
#include "Debug/Profiler.h"
#include "Rendering/CommandList.h"
#include "Rendering/RenderCore.h"
#include "Rendering/RenderOutput.h"

#include <array>

extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 4; }

extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = u8".\\"; }

using namespace Fancy;

FancyRuntime* myRuntime = nullptr;
Window* myWindow = nullptr;
RenderOutput* myRenderOutput = nullptr;
InputState myInputState;
ImGuiContext* myImGuiContext = nullptr;

bool test_profiler = false;
bool test_imgui = false;
bool test_gpuMemoryAllocs = false;
bool test_sychronization = false;
bool test_asyncCompute = false;
bool test_mipmapping = false;
bool test_modelviewer = false;
bool test_sharedQueueResources = false;
bool test_hazardTracking = false;
bool test_raytracing = false;

void OnWindowResized(uint aWidth, uint aHeight)
{
}

void Init(HINSTANCE anInstanceHandle, const char** someArguments, uint aNumArguments)
{
  Fancy::WindowParameters windowParams;
  windowParams.myWidth = 1280;
  windowParams.myHeight = 720;
  windowParams.myTitle = "Fancy Engine Tests";

  Fancy::RenderPlatformProperties renderProperties;
  myRuntime = FancyRuntime::Init(anInstanceHandle, someArguments, aNumArguments, windowParams, renderProperties, "../../../../");

  myRenderOutput = myRuntime->GetRenderOutput();
  myWindow = myRenderOutput->GetWindow();

  std::function<void(uint, uint)> onWindowResized = &OnWindowResized;
  myWindow->myOnResize.Connect(onWindowResized);
  myWindow->myWindowEventHandler.Connect(&myInputState, &InputState::OnWindowEvent);

  myImGuiContext = ImGui::CreateContext();
  ImGuiRendering::Init(myRuntime->GetRenderOutput(), myRuntime);
}

void Update()
{
  myRuntime->BeginFrame();
  ImGuiRendering::NewFrame();
  myRuntime->Update(0.016f);

  ImGui::Checkbox("Log resource barriers", &RenderCore::ourDebugLogResourceBarriers);

  ImGui::Separator();
}

void Render()
{
  CommandList* ctx = RenderCore::BeginCommandList(CommandListType::Graphics);
  GPU_BEGIN_PROFILE(ctx, "ClearRenderTarget", 0u);
  float clearColor[] = { 0.3f, 0.3f, 0.3f, 0.0f };
  ctx->ClearRenderTarget(myRenderOutput->GetBackbufferRtv(), clearColor);
  ctx->ClearDepthStencilTarget(myRenderOutput->GetDepthStencilDsv(), 1.0f, 0u, (uint)DepthStencilClearFlags::CLEAR_ALL);
  GPU_END_PROFILE(ctx);
  RenderCore::ExecuteAndFreeCommandList(ctx);

  ImGui::Render();

  myRuntime->EndFrame();
}

void Shutdown()
{
  ImGuiRendering::Shutdown();
  ImGui::DestroyContext(myImGuiContext);
  myImGuiContext = nullptr;
  FancyRuntime::Shutdown();
  myRuntime = nullptr;
}

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

  Init(hInstance, cStrings.data(), cStrings.size());

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

    Update();
    Render();
  }

  Shutdown();

  return 0;
}