# wiliwili：斐讯 N1 优化分支

基于 [xfangfang/wiliwili v1.6.0](https://github.com/xfangfang/wiliwili/tree/v1.6.0)，目标环境为 S905D、2GB 内存、Debian 12 arm64、Mali450 / Mesa Lima / GLES2。`n1-optimized` 保存已在实机使用的源码；上游分支保持原样。

## 已包含的修改

- GLES2 framebuffer 函数加载修复。
- 手柄槽位扫描、鼠标/手柄切换，以及独立三横线菜单键的画质菜单映射。
- 硬解失败时的 seek 恢复。
- `N1_LOW_MEMORY`：缩小封面请求、64MiB 纹理缓存预算、修复复用视图和取消加载的纹理引用释放。
- FFmpeg V4L2 SOURCE_CHANGE / flush 修复、可复用帧复制池及 AArch64 拷贝路径；实机配置使用 24 个 capture buffer 并关闭额外 detach copy。
- 启动器通过用户 systemd scope 限制应用内存 768MiB、应用 swap 64MiB。系统已有 512MiB swap 不作修改。

这仍有 CPU 可读 NV12 到 GLES 的上传，并非完整 DMA-BUF 零拷贝。30fps 1080p 实测输出掉帧约从 20.6 降至 1.1 帧/秒，音画同步最大偏差约 12ms。不能据此保证所有码流、4K 或长时间无泄漏。

## 源码与依赖

N1 实际使用并修改的依赖（含 borealis / GLFW）直接收进仓库，避免普通 fork 的子模块仍指向未修改版本。未在 N1 使用的 OpenCC / MemoryModule 保留上游子模块；N1 构建使用系统 OpenCC。原始依赖提交在 `source-pins.json`，修改后的关键文件有 SHA256。

`ffmpeg/ffmpeg-5.1.9-n1-complete.patch` 可直接应用到原始 FFmpeg 5.1.9，含此前保留容器里的 V4L2 修改和 Debian rangecoder 修复，避免仅保存最后一层补丁。`patches/` 保留分层补丁作为修改记录，两种方式不要重复应用。

## 编译

在 Mac 的 ARM64 Linux Docker 容器中编译，不要在 2GB N1 上并行编译。

应用使用 Debian 12 arm64 开发环境，需要 CMake、C/C++ 编译器、pkg-config、libmpv-dev、libcurl4-openssl-dev、libopencc-dev、libwebp-dev、libssl-dev，以及 X11 / Xrandr / Xinerama / Xcursor / Xi / Mesa EGL/GLES 开发包：

```sh
sh n1/build-app.sh /tmp/wiliwili-build /tmp/wiliwili-stage
```

缺少字体时，安装 `fonts-noto-cjk`，并在运行机器上让 `resources/font/switch_font.ttf` 指向 `/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc`。不要提交个人字体或 B 站账户配置。

FFmpeg：解压官方 `ffmpeg-5.1.9.tar.xz`，应用完整补丁，再按 `ffmpeg/configure-command.txt` 在独立构建目录配置。此配置依赖 Debian 的 FFmpeg build-dep 包；只构建 `make -j4 libavcodec/libavcodec.so.59` 即可得到替换库。与系统 FFmpeg 5.1 ABI 匹配后，将库单独放到 `/opt/n1-wiliwili/lib/`，由启动器的 `LD_LIBRARY_PATH` 加载。不要覆盖系统库。

将 `n1/mpv.conf` 合并到用户 wiliwili 配置。开启 H264 硬解，使用 `v4l2m2m-copy`。此配置的 `n1_copy_frames` 需要本仓库补丁库，不能直接给未修改的系统 FFmpeg 使用。

`retained-build/` 保存此次实际使用的 Mac Docker 构建脚本，依赖原来保留的容器和 `/out` 包缓存，属于实机构建记录，不是全新环境的一键安装器。

## 验证

`tests/` 包含 guarded plane copy、纹理引用和 5000 图 churn、持有旧硬件帧跨 seek 的回归测试。`validation/memory-opt-validation.json` 保存脱敏后的实机结果。应用与 codec SHA256 对应已安装版本。

许可证沿用根目录及各依赖原许可证。FFmpeg 补丁对应 FFmpeg 的许可证。没有提交账户 Cookie、Token、SSH 信息、运行日志或视频文件。
