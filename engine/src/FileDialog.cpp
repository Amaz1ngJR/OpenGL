#include "FileDialog.h"
#include <stdexcept>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#include <vector>

// Win32 的 W 接口使用 UTF-16；返回前转为 UTF-8，与 Assimp 的 Windows IO 接口一致。
std::string selectModelFile() {
    std::vector<wchar_t> path(32768, L'\0');
    const wchar_t filter[] =
        L"Model files (*.obj;*.fbx;*.gltf;*.glb;*.dae;*.ply;*.stl;*.blend;*.3ds)\0"
        L"*.obj;*.fbx;*.gltf;*.glb;*.dae;*.ply;*.stl;*.blend;*.3ds\0"
        L"All files (*.*)\0*.*\0\0";
    OPENFILENAMEW dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = GetActiveWindow();
    dialog.lpstrFilter = filter;
    dialog.nFilterIndex = 1;
    dialog.lpstrFile = path.data();
    dialog.nMaxFile = static_cast<DWORD>(path.size());
    dialog.lpstrTitle = L"Open model";
    // 仅单选文件；不改变进程工作目录，避免干扰其他资源的相对路径。
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
    if (!GetOpenFileNameW(&dialog)) {
        const DWORD error = CommDlgExtendedError();
        if (error == 0) return {}; // 取消不视为错误，调用者保持当前模型。
        throw std::runtime_error("Failed to open model file dialog (error " + std::to_string(error) + ")");
    }
    // 查询长度包含末尾 NUL，转换后移除它；保留 std::string 自己的终止符。
    const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, path.data(), -1,
                                         nullptr, 0, nullptr, nullptr);
    if (size == 0) throw std::runtime_error("Failed to measure model path as UTF-8");
    std::string result(static_cast<size_t>(size), '\0');
    if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, path.data(), -1,
                             result.data(), size, nullptr, nullptr))
        throw std::runtime_error("Failed to convert model path to UTF-8");
    result.pop_back();
    return result;
}
#elif !defined(__APPLE__)
// macOS 的实现放在 FileDialogMac.mm，其他平台明确报告不支持。
std::string selectModelFile() {
    throw std::runtime_error("Native model file selection is not supported on this platform");
}
#endif
