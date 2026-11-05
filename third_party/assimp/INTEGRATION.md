# Assimp 源码说明

- 上游：https://github.com/assimp/assimp
- 版本：`v5.4.3`
- 提交：`c35200e38ea8f058812b83de2ef32c6093b0ece2`
- 许可证：见本目录 `LICENSE`、`CREDITS` 及各 `contrib/` 组件的许可证。

本目录保留构建需要的 `code/`、`include/`、`contrib/`、`cmake-modules/`、顶层 CMake 文件及许可证，不包含上游的大型测试模型、样例、工具与文档目录。构建配置位于项目的 `cmake/AssimpOptions.cmake`，上游源码没有定制修改。

两套构建都使用静态库、内置 zlib、单精度、默认导入器，不编译导出器、测试、工具或 USD/Draco 扩展。首次构建会生成 `config.h`/`revision.h`，这些头文件位于对应构建目录，不能只把 `include/` 复制出来使用。

注意：上游 zlib 的 CMake 在首次配置时会把源码中的 `contrib/zlib/zconf.h` 改名为 `zconf.h.included`，避免遮蔽构建目录生成的配置头；仓库保留该改名后的文件。其他生成文件应位于项目的 `build/` 下。

升级时须同时更新版本记录，验证 CMake/xmake 的 Debug 与 Release 链接，并重新检查 Windows MSVC 运行库与生成库名。
