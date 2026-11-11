# OBS Spatial FX v2.0 真实验收报告

日期：2026-10-30。版本2.0.0。当前工作区 E:/obs spartialFX develop；V1发行包保持原字节。完整命令输出、JSON与真实OBS窗口/GPU截图在 V2 Evidence.zip。

## 环境与构建
- Win10 22H2 x64、Ryzen5800X、AMD RX6900XT、OBS32.2.2，Qt运行时6.11.1。VS2022，Qt6开发SDK6.8，OBS32头文件；UTF-8 /utf-8。
- 实际命令：`cmd.exe /d /c tools\build.cmd 4`，最新日志 logs/v2/build-final.log / build-release.log。真实DLL与x64安装EXE已生成。
- 实际命令：`cmd.exe /d /c tools\test.cmd`，logs/v2/unit-final.log；CTest 2/2，核心40项和模型20项断言。
- 发布字节审计：`python tests/audit_release_v2.py`；打包：`python tools/package_release.py`。解释器使用本机 Codex Python runtime。精确命令亦记录在测试脚本及 logs/v2 的 *-commands.log。

## 最终运行结果
| 验收 | 结果 | 实际证据 |
|---|---|---|
| 原生参数/颜色/导入 | PASS：147项，0失败 | logs/v2/native-private-final/dock-smoke.json |
| 关闭重启保存 | PASS：12项，0失败 | logs/v2/restart-private-final/dock-smoke.json |
| 调参压力 | PASS：19项，0失败 | logs/v2/stress-private-final/dock-smoke.json |
| 跨场景绑定与延迟窗口 | PASS：23项，0失败 | logs/v2/association-private-final/dock-smoke.json |
| 延迟窗口像素锚点 | PASS：1项，0失败 | logs/v2/association-private-final/pending-anchor-tests.json |
| 真实GPU图像对照 | PASS：13项，0失败 | logs/v2/gpu-private-retry/render-tests.json |
| 安装后v1迁移 | PASS：13项，0失败 | logs/v2/installed-migration-first/dock-smoke.json |
| 迁移后再次重启 | PASS：13项，0失败 | logs/v2/installed-migration-restart/dock-smoke.json |
| 普通版安装加载 | PASS：10项，0失败 | logs/v2/phase6-installed-normal/dock-smoke.json |
| 普通版实际运镜输出 | PASS：6项，0失败 | logs/v2/phase6-installed-normal/render-probe.json |
| 便携版安装加载 | PASS：10项，0失败 | logs/v2/release-installed-performance/dock-smoke.json |
| 安装版1080p60 | PASS：3项，0失败 | logs/v2/release-installed-performance/performance.json |

安装后性能测量：69.03秒，4122帧，渲染延迟帧0，平均FPS 60.000，采样报告的最大平均渲染时间 0.488ms。负载为3图层/4光源/软阴影/运镜，见 performance-workload.json。此数据是该机器合成场景，不代表所有直播素材与编码负载。

真实 v1.0.0→v2.0.0 覆盖升级 12项通过；锁定快捷方式后的payload/快捷方式/版本回滚通过。另有11项锁定文件回滚、卸载、保留未知文件/预设和重装检查通过。运行中安装/卸载返回10，DLL字节不变。迁移检查使用隔离的真实 v1 public-settings 文件：原Z=0，升级后分层深度，三秒关键帧持续时间保留，private_settings 在退出与再次启动后仍为schema2。

普通版通过安装于 ProgramData 的产品DLL加载；便携版通过安装器生成的 Start Menu 快捷方式注入系统插件路径。普通版测试只创建独立QA配置/场景集合，结束后恢复 user.ini/global.ini 和原插件配置；哈希见 normal-config-restoration.json。产品安装目录不包含QA模块和Qt DLL。最终测试端口4457关闭。

## 崩溃根因与证据边界
原始 Windows Application 事件：2026-10-30 15:30:27，ntdll、0xc0000374（堆损坏）。WER细节访问受限；不冒充获得原始完整符号栈。本地原生压力复现于 obs_data_array_release；debugger-stack.txt / debugger2-stack.txt 记录真实模块偏移。DbgHelp初始化失败，只有模块地址边界。

仅把解析挪到UI，以及仅绕过deferred source update，均不足：早期压力与混合外部设置访问仍曾崩溃，失败日志保留。最终Dock不再编辑或遍历共享public obs_data对象；UI发布不可变C++快照，分离的private model由OBS保存；proc API复制调用者拥有的数据。渲染线程无UI等待锁。

最终80,008次原生空间/灯光调参无进程终止；同时执行400次公共SetInputSettings和400次GetInputSettings，外部阶段60.55秒，0错误。公共实时更新被产品有意忽略，此项证明原生模型与外部公共数据隔离，不代表任意第三方对同一obs_data并发写是安全的。源生命周期和删除通过弱引用及快照回收处理。

OBS关闭报告仍有1个allocator leak；无产品基线也有1个，未宣称零泄漏。未进行多小时soak，不能宣称排除所有第三方插件、驱动或系统崩溃。

## 本轮实现文件
- src/model.hpp/.cpp：schema2、自动深度、旧版迁移、多灯、默认变换、导入参考、预设持久化。
- src/core.hpp/.cpp：投影/视差、相机/目标/缓动、OBS裁剪/边界/镜像布局与旋转组合。
- src/stage.hpp/.cpp、data/effects/spatial.effect：immutable模型、private保存/owned proc、复用捕获纹理、实际平面照明、alpha阴影、方向轮廓、面光5采样、近似景深。
- src/dock.hpp、dock.cpp、dock_actions.cpp、dock_v2.cpp：五页面、场景/源列表、窗口导入、可调参数、预设槽位关联、原子保存。
- installer/setup.cpp、setup.rc.in、CMakeLists.txt：2.0.0版本、原路径覆盖、回滚与元数据保存。
- tests/*v2*、native_control.py、obs_qa_driver.cpp、window_fixture.cpp、tools/test-*-v2.ps1：真实UI/GPU/窗口/升级回归。QA二进制不进入安装器。

## 失败记录与修复说明
- gpu-private-final/render-tests.json：6项通过后测试文件桥超时。QSaveFile替换遇读句柄后，测试驱动过早记住请求ID，不再重试。仅修复测试桥提交成功后记录ID，gpu-private-retry最终13/13通过；不是把失败改成PASS。
- upgrade-attempt1：升级实际成功，但断言用混合斜杠字符串比较目录误报。改为规范化绝对路径，重新真实安装v1基线再执行完整升级；upgrade-tests.json为最终结果。
- area-direct-red：早期面光尺寸只改变半影，未改变直接照明。加入5点直接照明采样后最终GPU对照通过。
- 早期压力失败与最终回归记录并存；最终验收只引用明确列出的最终报告。

## NOT RUN / 限制
- Win10 1909、Win11、NVIDIA实机、多小时连续直播、全部第三方插件组合：NOT RUN。当前AMD实机无CUDA依赖，不能将跨硬件推测写成PASS。
- 强制注册表写失败后的回滚故障注入、磁盘耗尽/断电中断：NOT RUN；代码只读审核通过不等于这些系统故障已实测。
- 平面2.5D、32层/4灯/256关键帧、512遮罩9采样软阴影。无体积网格/PBR/路径追踪。复杂相交平面按中心深度排序；穿越近裁剪面整片跳过。
- 受保护/最小化窗口捕获受OBS/Windows约束；组/嵌套场景不导入。安装包未签名。

## 运行截图
真实GPU图：logs/v2/gpu-private-retry/v2-final-render.png（明确使用QA合成墙面与透明人物素材，非用户美术资源）。真实原生Dock五页：logs/v2/native-private-final/dock-tab-0.png 至 dock-tab-4.png。普通版与便携版安装后主窗口截图在对应测试目录。窗口延迟就绪实际锚点误差0像素。

## 发布字节审计
最终审计 45 项通过，0失败；见 logs/v2/release-audit.json。安装器嵌入DLL、effect、README、LICENSE与源码工作区逐字节匹配，安装DLL与最终build相同；V1安装包SHA256保持原值。

