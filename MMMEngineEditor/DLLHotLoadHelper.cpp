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
    if (!fs::exists(dllPath))
    {
        return fs::path{}; // 빈 경로 반환으로 "실패" 알림
    }

    fs::create_directories(hotDir);
    fs::path copied = hotDir / MakeHotDllName(dllPath);

    try
    {
        fs::copy_file(dllPath, copied, fs::copy_options::overwrite_existing);

        // PDB 처리 (생략 가능 로직)
        fs::path pdb = dllPath;
        pdb.replace_extension(L".pdb");
        if (fs::exists(pdb))
        {
            fs::path copiedPdb = copied;
            copiedPdb.replace_extension(L".pdb");
            fs::copy_file(pdb, copiedPdb, fs::copy_options::overwrite_existing);
        }

        return copied;
    }
    catch (const fs::filesystem_error& e)
    {
        // 복사 과정 중 발생한 에러 처리 (권한 문제 등)
        return fs::path{};
    }
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

            // pdb도 같이 삭제(있으면)
            fs::path pdb = p; pdb.replace_extension(L".pdb");
            std::error_code ec2;
            fs::remove(pdb, ec2);
        }
    }
}


