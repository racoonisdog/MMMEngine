#include "DLLHotLoadHelper.h"
#include <string>

std::wstring MMMEngine::Editor::DLLHotLoadHelper::MakeHotDllName(const fs::path& originalDllPath)
{
    // UserScripts.dll -> UserScripts_copy_1700000000000.dll
    auto stem = originalDllPath.stem().wstring();       // "UserScripts"
    auto ext = originalDllPath.extension().wstring();  // ".dll"

    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    return stem + L"_copy_" + std::to_wstring(now) + ext;
}

fs::path MMMEngine::Editor::DLLHotLoadHelper::CopyDllForHotReload(const fs::path& dllPath, const fs::path& hotDir)
{
    namespace fs = std::filesystem;

    if (!fs::exists(dllPath))
        return {};

    std::error_code ec;
    fs::create_directories(hotDir, ec);
    if (ec) return {};

    const fs::path copied = hotDir / MakeHotDllName(dllPath);

    // DLL만 복사 (PDB는 복사하지 않음)
    fs::copy_file(dllPath, copied, fs::copy_options::overwrite_existing, ec);
    if (ec) return {};

    return copied;
}

void MMMEngine::Editor::DLLHotLoadHelper::CleanupHotReloadCopies(const fs::path& hotDir)
{
    namespace fs = std::filesystem;

    if (!fs::exists(hotDir)) return;

    for (auto& e : fs::directory_iterator(hotDir))
    {
        if (!e.is_regular_file()) continue;

        const auto p = e.path();
        const auto name = p.filename().wstring();

        // 규칙: *_copy_*.dll 삭제
        if (p.extension() == L".dll" && name.find(L"_copy_") != std::wstring::npos)
        {
            // 잠금 해제 지연 대비: 몇 번 리트라이
            for (int i = 0; i < 10; ++i)
            {
                std::error_code ec;
                fs::remove(p, ec);
                if (!ec) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }
    }
}


