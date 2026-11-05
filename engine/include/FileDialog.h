#pragma once

#include <string>

// 打开原生单文件选择器，不创建 GLFW 窗口。请从应用的主/UI 线程调用。
// 成功时返回 UTF-8 编码的完整路径（包含中文路径）；取消时返回空字符串。
// 平台不支持或系统调用失败时抛出 std::runtime_error，由调用者在界面中提示。
std::string selectModelFile();
