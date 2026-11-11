> 原始v3.2交付记录，适用其当时的二进制。本次同版本覆盖修订的结果见 [PATCH_2026-11-01.md](PATCH_2026-11-01.md)。下方历史结果不作为本次补丁的通过依据。

# v3.2 验证记录

本文件对应OBS Spatial FX3.2.0，证据在work/v3.2。验证跨2026-10-31至2026-11-01完成；下面的PASS仅表示明确执行过的测试范围。

## 环境与命令

Windows x64；AMD Ryzen7 5800X、AMD Radeon RX6900XT，驱动32.0.21045.5002。VS2022 MSVC19.44，CMake4.4.5/Ninja，官方OBS32.0.0头文件与Qt6开发依赖；隔离OBS32.2.2、33.0.1，两套Qt运行时6.11.1。没有使用Qt5、CUDA或打包OBS/Qt DLL。

实际构建命令：

    & ./tools/build.cmd 4
    & ./tools/test.cmd

最终产品构建为shortcut-build.log，shortcut-test.log为5/5 CTest；release-build/release-test为此前阶段记录。QA驱动追加检查编译日志为audio-observation-qa-build.log等。编译器启用/utf-8 /W4 /permissive- /we4996。中间的失败构建/红测试保留，不能把它们计为最后通过。

实际运行命令（都针对独立portable配置，不启动/关闭日常用户OBS）：

    ./tools/launch-v3-qa.ps1 -Major 33 -Scenario camera-remake -Output work/v3.2/ui33-final
    ./tools/launch-v3-qa.ps1 -Major 32 -Scenario camera-remake -Output work/v3.2/ui32-final
    ./tools/launch-v3-qa.ps1 -Major 33 -Scenario camera-remake-audio -Output work/v3.2/audio-native33
    ./tools/launch-v3-qa.ps1 -Major 33 -Scenario camera-remake-persist -Output work/v3.2/persist33
    python tests/v3_render_probe.py work/v3.2/gpu33
    python tests/v31_parameter_probe.py work/v3.2/gpu33
    python tests/camera_remake_probe.py work/v3.2/installed33-perf
    python tests/camera_stability_probe.py work/v3.2/installed33-perf 1800

音频原生测试的SPATIAL_QA_AUDIO指向work/v3.2/audio/fixtures/点击_120_反相.wav。真实OBS已有素材分析测试使用该已注明合成夹具；它不是人工标注商业音乐。

## 已执行结果

| 项目 | 实际结果 | 证据（work/v3.2/下） |
|---|---|---|
| CTest核心/模型/动画/节拍/统一求值器 | PASS，5/5 | release-test.log |
| OBS33原生镜头操作 | 101 项，0 失败 | ui33-final/dock-smoke.json |
| OBS32原生镜头操作 | 101 项，0 失败 | ui32-final/dock-smoke.json |
| OBS33媒体集成 | 42 项，0 失败 | audio-native33/dock-smoke.json |
| 重启加载 | 9 项，0 失败 | persist33/dock-smoke.json |
| 原有光照/阴影/景深/视差/锚定GPU | PASS，17项 | gpu33/render-tests.json、render-regression-r3.log |
| 深度顺序/视差/景深倍率GPU | PASS，11项 | gpu33/parameter-tests.json、parameter-gpu.log |
| schema4相机/图层/合成/长时长/灯光解析GPU | PASS，8项 | installed33-perf/camera-render-tests.json、camera-gpu.log |
| 音频算法 | PASS，37检查 | audio/passages-green.log、audio/final-review.md |
| 系统MF解码支持/失败/取消探针 | PASS，11项 | audio/probe-results.json、audio/report.md |
| 重启保留schema4轨道/图层/媒体/旧范围等 | PASS，10项字段比较 | persistence.json |
| 实际3.1→3.2覆盖安装 | PASS，exit0、卸载条目3.2.0 | upgrade-before.json、upgrade-after.json、upgrade-install.log |
| 安装版唯一ProgramData DLL加载 | PASS，载入1个DLL，SHA256等于build | installed-module-path.json |
| 控制面板同一卸载项实际删除并重装 | PASS，条目/自有DLL删除，再安装exit0 | uninstall-result.json、reinstall-result.json |
| OBS正在运行时拒绝覆盖 | PASS，exit10，DLL哈希不变 | install-guard.json、install-while-running.log |

原生界面检查包含真实Qt鼠标按下/移动/释放事件的跨对象Ctrl多选和拖帧，手工时间移动到100000秒，独立X不生成Y，基础值隔离，曲线编辑，复位取消零变化/确认可撤销，撤销保留后续灯光修改，场景切换不恢复外来历史。没有把仅找到控件当作其功能通过。

媒体检查包含原源不中断、来源/偏移重新发布、实际媒体主时钟推进、完成分析后源文件更换拒绝旧结果、8万候选批次立即取消不改曲线、真实15秒PCM捕获识别120BPM及具名预设恢复新轨道。详细日志和native checks保留。

GPU静态解析对照：相机/图层1秒线性中点与手工静态坐标像素完全一致；FX关闭时全画面透明度曲线和静态0.6一致；50000秒长轨中点一致；动画图层绑定灯与解析世界位置一致；720°旋转中点保留圈数。截图来自OBS实际D3D11源输出。

## 失败过程与限制

早期ui33/ui33-r2含控件选区语义/测试夹具名称导致的失败；修复后ui33-r3/ui33-final和ui32-final为0失败。保留全部旧报告，最终以指定最后目录为准。首次GPU旧脚本未把迁移后的独立播放范围映射回schema3输入，动画对照失败；修正QA适配后17项全通过。一次QA桥接文件替换撞到Windows读句柄（WinError5），测试端增加有限重试后整组重跑通过。这些过程不冒充产品通过证据。

- 人工标注真实音乐precision/recall、3名无PR/AE经验用户2分钟上手：NOT RUN（缺少标注集/参与者）。
- NVIDIA、Win10 1909、其他Qt运行时、未来OBS二进制、推流/录制多小时直播：NOT RUN。本机AMD+隔离版本结果不替代这些项目。
- 系统解码器支持矩阵只代表所测文件，本地联动仅100%速度媒体源；网络/VLC/变速/crop复杂时间映射不承诺定位同步。实时采集只能识别已观察15秒。
- 实际有资源容量边界：32层/4灯、100万候选点/批、离线音频6小时；时间轴本身没有旧版256帧/3600秒截断。超大批次最终保存序列化仍可能阻塞UI，不能声称任意规模零延迟。
- 原偶发桌面白蒙版根因未可靠复现；本轮不新增根因修复声明。现有内嵌数值滑条继续使用。
- 多个隔离OBS退出日志仍有1个全局内存泄漏计数，未定位归属；不宣称所有会话零泄漏。
- 旧隐藏相机控件作为历史绑定兼容桥仍在源码内，用户可见页面完全使用新编辑器；旧控件的3600秒范围不限制新模型/新时间轴。

最终安装包审计和清理记录见文末。

## 最终安装版与密集轨

| 项目 | 结果 | 证据 |
|---|---|---|
| 安装版OBS33镜头操作 | PASS，101/101 | installed33-final/dock-smoke.json |
| 安装版OBS32镜头操作 | PASS，101/101 | installed32-final/dock-smoke.json |
| 曲线鼠标手柄/锁定/相机与合成复位/环绕/复制粘贴 | PASS，45/45 | advanced33-final/dock-smoke.json |
| 安装版媒体集成、波形、15秒PCM、预打点/取消 | PASS，43/43 | audio-native33-final/dock-smoke.json、waveform-detail.png |
| 安装版保存重启 | PASS，10项字段等价 | persistence-final.json、persist33-final |
| 十万关键帧/1000次定位 | PASS，17项原生检查 | dense33/dock-smoke.json、dense-performance.json |

密集轨实际数值：发布十万帧433.4ms，1000次同步定位字段更新P95=0.8654ms、max=1.2804ms；当前可见窗口绘制16.40ms，缩放显示全部键86.79ms。这些测量不是操作系统输入排队到最终呈现的端到端延迟，不能把它们当作任意大工程零停顿的保证。大项目OBS全量保存/首次载入仍可能有秒级UI停顿；渲染线程继续使用已发布快照。

新增高级检查最初发现OBS主窗口快捷键抢走时间轴CtrlC/V：advanced33保留原44项/1失败红记录。实际修复是在TimelineWidget对ShortcutOverride接受本组件的复制/粘贴/删除事件，然后仍由原按键处理器编辑曲线；advanced33-final真实Qt按键复测通过。没有更改渲染、坐标、插值或音频算法。

密集工程之后，固定800ms/1000ms Qt定时器可能晚回调，旧QA错误假定回调准时而把有效运行时钟4.93秒判为失败；修正为真实墙钟经过时间比较，分析完成改为等待具体结果。另一次等待结果过快，没留够观察原源持续播放的窗口，补回至少2秒观察。保留installed33-clock-qa-first、audio-native33-timing-first、audio-native33-observation-first，未删除这些失败证据。产品未因这几项QA时序修正改变行为。

## 持续性能的二进制边界

30分钟测的是快捷键修正前已安装二进制SHA256 DE49116720F6AE077CA98E9DEE416CAD8C3C5D6B10B454707363EFE46986E94A：1800.516秒、108012帧、8渲染延迟帧，约0.0074%，平均59.9956fps，最高报告平均渲染耗时4.0427ms；工作集净变化−191.8MiB。场景为1920×1080/60、3图层、1点灯/阴影/轮廓光和循环相机运动，约每秒一次完整状态发布。并非32层/4灯上限负载或推流编码测试。证据installed33-perf/stability.json、stability.log与stability-obs.log，最后退出全局泄漏计数0。

最终二进制仅增加上述快捷键事件保留逻辑，已重新构建、5/5 CTest、OBS32/33实际加载与操作；8项schema4 GPU及60秒性能重新执行，结果记录installed33-final-perf。30分钟结果不冒充快捷键修改后最终哈希的完整30分钟跑测。将网络QA桥接往返P95（旧长测219ms）与原生字段更新P95分开记录，不能混称原生UI延迟。

未执行：快捷键修正后最终二进制再跑完整30分钟、最大32层/4灯负载、分析与高频拖动长期并行、实际降级恢复至3.1。已执行的长测、最终短测和功能回归范围在上面分别列明。

## 最终交付二进制与清理（2026-11-01）

最终插件DLL SHA256：`90D14ECD44696C7EB59FC011E334357066D67A7C2869D3C8F7B20DEADBD93246`。
最终安装器SHA256：`4F839D0DC9C59E916810971B978E47C0E46C546381102D3A9956E888D3222DB7`。

最终安装版验证入口为：

    ./tools/run_v32_installed_qa.ps1 -Major 33 -Scenario camera-remake -Output work/v3.2/installed33-final
    ./tools/run_v32_installed_qa.ps1 -Major 32 -Scenario camera-remake -Output work/v3.2/installed32-final
    python tests/camera_remake_probe.py work/v3.2/installed33-final-perf
    python tests/camera_stability_probe.py work/v3.2/installed33-final-perf 60
    python tools/package_v32.py
    python tools/verify_v32.py

最后一次1080p60测试持续60.25秒，实际3600帧、0渲染延迟帧；平均60.0000024fps，最高报告平均渲染耗时0.560932ms，工作集净变化−0.1367MiB。网络QA发布往返P95为219ms，不能与原生字段定位P95混淆。8项最终schema4 GPU对照全部通过。证据为installed33-final-perf/stability.json、camera-render-tests.json、performance-final.log及final-performance-obs.log。

最终二进制的运行中覆盖保护exit10且哈希不变，实际卸载删除卸载项/自有DLL，再次安装exit0、DisplayVersion=3.2.0，所有安装布局的DLL与build相同。证据为install-guard-final.json、uninstall-final-result.json、reinstall-final-result.json及对应日志。OBS32/33实际唯一模块路径见各自installed32-final/installed-path.json和installed33-final/installed-path.json；早期installed-module-path.json属于30分钟测试前的历史哈希，不替代最终路径证明。

测试组件以可恢复移动方式存入work/v3.2/cleanup-backup，不放入安装器。OBS33测试WebSocket配置按原备份逐字节恢复，OBS32/33均关闭测试服务。日常用户OBS的71个JSON/INI文件逐一核对：0修改、0缺失、0新增。清理后0个OBS进程、0个4457监听、runtime目录0个QA DLL。证据为cleanup-and-user-config.json和cleanup-processes.json。最终包校验报告为final-package-audit.json；77项校验、0失败，包含16个历史交付哈希、压缩包CRC、实际安装DLL、安装证据及测试结果。交付前再次执行核心测试5/5通过，日志delivery-test.log。

## 交互实现边界

当前曲线页编辑选中参数的单个区间和贝塞尔手柄；关键帧时间移动使用主时间轴或数值框。尚未提供计划线框中跨全轨的曲线关键点拖动、多个已选关键帧同时输入绝对秒数、编辑视口/展开状态持久化、复位时包含锁定图层的额外选项。锁定图层始终受到保护。结构图与渲染共用坐标求值器，但结构图尺寸输入仍采用导入尺寸，捕捉源运行中改变尺寸时示意图可能与实时渲染存在差异。这些不列为已通过的完整计划细节。
