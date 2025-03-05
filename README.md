# 适用于NVI SDK的FFmpeg解码插件

## 编译

有两种方式编译该插件库：

1. 通过nvi厂库统一编译，将`ffmpeg_codec`完整放置到`nvi`厂库的`plugin`目录中，然后在`plugin/CMakeLists.txt` 增加 `add_subdirectory(ffmpeg_codec)`。
2. 直接独立编译，可通过vcpkg或其它工具构建ffmpeg，然后直接配置编译。
