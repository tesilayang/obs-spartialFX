# v3.2 音频离线分析模块

入口为 `src/audio/beat_analyzer.hpp`，命名空间 `spatial::audio`。

```cpp
std::atomic_bool cancelled{false};
auto result = spatial::audio::analyzeFile(localPath, cancelled, onProgress);
if (!result.error.empty()) { /* 显示错误，允许手动 BPM */ }
else if (!result.cancelled && result.confidence >= .7 && !result.beats.empty()) {
    auto preview = spatial::audio::candidates(result.beats, .5, 0, timelineOffset);
}
```

`analyzeFile` 是同步、可由工作线程调用的函数；进度回调在调用线程执行，不能直接操作 Qt 控件。调用方持有取消标记直至函数返回，取消后拒绝写入旧结果。函数独立读取文件，不调用 OBS 的播放、定位、静音或创建源接口。

解码使用 Windows Media Foundation Source Reader，输出原采样率、原声道数的交错 32 位浮点 PCM。每次只持有当前解码样本，分块累计约 5ms 的 RMS 包络；先对每个声道求平方再汇总，反相双声道不会抵消。`envelopeStep` 为包络时间步长，`duration` 由实际解码帧数计算。文件时间戳中的明显正向间断补静音。

分析使用包络正向变化、局部自适应起音阈值、峰值抑制、拍间隔周期估计和相邻间隔连续度，再用局部间隔状态的动态规划选择一致拍点路径。孤立的额外瞬态不直接计为一拍；后续连续的局部间隔可以确认真实变速。`beats` 保存路径中实际观察到的峰值时间，保留前导静音和变速，不由 BPM 从零机械均分。静音、持续单音和不足四个有效起音不产生自动拍点。置信度是启发式指标，低于 `.45` 不输出拍点；v3.2 UI在算法已输出拍点且显示置信度后允许预览，写入始终需要用户确认，并保留手动校正；这不是音乐识别准确率保证。

`candidates` 按拍坐标采样：`.25/.5/1/2/4` 分别表示四分之一拍、半拍、一拍、两拍、四拍。细分和非整数比例使用相邻已检测拍点的局部插值，属于候选网格；不能把这些插值点标成检测到的鼓击。先插值，再按 `minSeconds` 过滤密度，最后应用时间轴偏移；不向最后一个已观察拍点之外外推。非递增/非有限时间、非正比例、负最小间隔或超过一百万个候选点的请求返回空列表，调用方需给出可见的参数/容量提示。

容量为六小时音频及单个解码样本 64 MiB；超限返回具体错误，不截断后冒充成功。保留的是低频包络和分析临时数组，内存随分析时长增长但不随完整多声道 PCM 体积增长。取消在读取样本之间、8192 帧处理块之间和峰值/周期分析中检查；同步系统 `ReadSample` 正在阻塞时不能强制中断该系统调用。

## 本机实测格式矩阵（2026-10-31）

以下只证明本机系统解码器与所列夹具可用，不代表所有编码变体或其他 Windows 安装。

| 夹具 | 结果 | 解码时长 | 首拍 | BPM |
| --- | --- | ---: | ---: | ---: |
| Unicode 路径 WAV / 反相立体声 PCM16 | PASS | 13.000s | .300s | 120 |
| MP3 / libmp3lame | PASS | 13.021s | .310s | 120 |
| MP4 / AAC | PASS | 13.034667s | .320s | 120 |
| FLAC | PASS | 13.000s | .300s | 120 |
| MKV / AAC | PASS | 13.034667s | .320s | 120 |
| 无音轨 MP4 | 正确拒绝 | HRESULT `0xC00D36B3` | — | — |
| 非媒体内容的 MP3 | 正确拒绝 | HRESULT `0xC00D36C4` | — | — |
| 不存在文件 | 正确拒绝 | HRESULT `0x80070002` | — | — |
| 30 分钟静音 WAV，8kHz 单声道 | PASS，无伪造节拍 | 1800s | — | 0 |

MP3/AAC 的编码器延迟和解码器补齐使时长与首拍有差异；本组夹具首拍误差分别为 10/20ms。不能将其当作 OBS 媒体播放同步已验证的证据。

实测证据为 `work/v3.2/audio/probe-results.json`，生成脚本和无界面探针保存在同目录。探针只链接系统 MF 库；FFmpeg 可执行程序仅用于制作测试夹具，不是插件的运行依赖。

## 集成边界

本单元不枚举 OBS 源、不读写场景、不控制播放、不持有 Qt 对象。源 UUID、文件身份/mtime/大小和算法版本的缓存键、源失效后的结果拒绝、实时窗口采集、媒体速度/裁切映射、手动 BPM、候选关键帧事务由调用方实现。`algorithmVersion` 当前为 3（真实拍点路径选择并保留多个分离段落），可用于缓存失效。

真实音乐的人工标注精度/召回率尚未测试；密集鼓花、切分节奏、低起音素材和速度倍频仍可能产生歧义，不能只凭合成点击测试声称所有音乐识别准确。

复现夹具：python tests/generate_audio_fixtures.py --ffmpeg 本机ffmpeg.exe；不提供FFmpeg时只生成WAV/静音/错误文件，不把缺失格式记为通过。CMake探针build/spatial-audio-probe.exe，分别传入文件路径、--expect-error / --cancel / --cancel-progress。原始批量探针脚本保存在源码包work/v3.2/audio中；其exe路径须改为CMake产物或依照build-probe.cmd构建。
