#include <imgui.h>
#include <framework/KaraPuyo.h>

using namespace Hagine;
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    //_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    //_CrtSetBreakAlloc(152);

    std::unique_ptr<Framework> pGame = std::make_unique<KaraPuyo>();

    pGame->Run();

    return 0;
}
