# OBS Spatial FX v3.6.6-b 开发与实测记录

2026-11-18。在 Windows x64、VS2022/MSVC、Qt6 环境编译，使用隔离 OBS 33.0.1 的 D3D11 舞台验收。范围为预设、直接关联的时间轴与安装载荷。

## 装饰与风格

在同一 1920×1080 舞台放置背景、透明角色、同源副本、桌面和标题。旧版少量纸片会横过主体与桌面。新版将每件装饰的轮廓、填色、细部和运动放在同一处构图。少量档最多放主图与一组呼应。丰富档另检查可用边缘。空间不足会缩小或舍弃装饰。检测使用开始时刻的图层投影与导入尺寸，详细控制可指定避开区域。透明窗口的实际人脸位置和播放中改变的构图仍需通过预览检查。

| 风格 | 三组固定种子实际出现的主图 |
|---|---|
| 温柔舒缓 | 双瓣晨光、花簇、展叶 |
| 简洁几何 | 日轮底座、折纸路标、拱门 |
| 活泼波普 | 网点星章、日芒、漫画网点 |
| 纸片拼贴 | 票根、相纸、便签 |
| 科技界面 | 仪表、标尺、示波器 |
| 节奏快剪 | 折面彩带、箭羽、双彩带 |
| 朦胧梦幻 | 极光月轮、流星、行星 |
| 故障实验 | 错版信号、渐层错码、抽样窗 |

## 对象和运动

“参与运动”和“接收转场”按时间轴实例 ID 分别保存。可复制一列选择，可只运动、只转场或只加装饰。单独重抽某一项保留其他项。标准空间缓推按镜头距离调整真实纵深轨。平面模式提供二维运动，环绕对象写入半径与角度。相邻深度、镜头近裁剪和画面边界会限制幅度。详见 `docs/architecture/SPATIAL_COORDINATE_CONTRACT.md`。

实际修改涉及 `src/presets/composer.cpp/.hpp`、`style_art.hpp`、`stage_motion.inc`、`panel.cpp/.hpp`、`src/dock.cpp`、`src/plugin.cpp`、`CMakeLists.txt`、`installer/setup.cpp`、`installer/setup.rc.in`、编译说明、README、坐标约定、专项测试及打包脚本。源码包含此前本地已验证的 v3.6.5 窗口捕捉修复。

## 验收

| 状态 | 项目 | 真实结果 |
|---|---|---|
| PASS | 编译与加载 | `tools\build.cmd 4` 产出 x64 DLL 和 EXE。安装、首次加载、重启的 DLL 哈希一致，控制面板版本 3.6.6-b。 |
| PASS | 关联 CTest | `ctest --test-dir build -R "spatial-(composer|preset-panel|presets|scene-evaluator|editor-timeline)$" --output-on-failure`，5/5。 |
| PASS | 画面对照 | 旧版与新版少量档均取 8 种风格 × 3 个种子 × 3 个时刻，丰富档另取 8 种 × 1 个种子 × 3 个时刻。24 组新版少量档的中段均改变真实舞台像素，入、退场均与中段不同。画面见离线 HTML 和 JPEG。 |
| PASS | 独立运动、转场、选择 | 同源两个角色只选一个时，运动轨只写入所选实例。仅标题擦入的像素差位于 960×540 截图的 (88,127) 至 (268,177) 区域。只换装饰保留动作与蒙版接收者。 |
| PASS | 纵深与写入 | 标准推近三帧角色宽度为 202、219、202 像素。取消恢复原图层，应用写入关键帧，追加接在原有动作后，撤销与重做恢复对应时间轴。重启后保留工程与同帧画面。 |
| PASS | 旧工程 | 读取真实 v3.6.5 纸片、科技、节奏三套已保存的图形与关键帧，中段实际输出像素差均为零。 |
| PASS | 清理 | 正常退出隔离 OBS。恢复测试前的 153 个配置文件、运行时插件目录，移除 QA 驱动并关闭测试监听。 |

初轮取消预览的自动对比曾记录 FAIL。差异只涉及 QA 注入的虚构来源名，OBS 面板会同步为来源真实名称。测试素材改用 OBS 实际名称后重跑，通过完整图层与轨道对比；保留两次证据。中途发现不同种子的主图轮廓过于接近，第二轮增加了各风格独立的轮廓和丰富档边缘图案，最终实拍均来自第二轮载荷。

## 1080p60 渲染

AMD Ryzen 7 5800X、Radeon RX 6900 XT。相同五实例场景，每组预热后采样 30 秒。表中为 OBS 进程统计。三项全开使用纸片风少量装饰、角色运动、标题转场，共 7 个程序图形（含蒙版）。

| 模式 | 平均渲染耗时 | 平均 OBS CPU | 渲染丢帧 / 观察帧数 |
|---|---:|---:|---:|
| 关闭预设 | 0.253 ms | 0.371% | 0 / 1801 |
| 仅运动 | 0.242 ms | 0.387% | 0 / 1801 |
| 运动、转场、装饰 | 0.282 ms | 0.478% | 0 / 1801 |

集成显卡实机、编码推流和功耗测量为 NOT RUN。本次画面对照采用自制场景。

## 复核入口

以下为工作区实际运行的命令。`python` 使用本机 Codex Python 环境，`ctest` 使用项目固定的 CMake 工具链。`work/v3.6.6-b` 保存原始基线、最终载荷证明、截图与异常记录。

```text
tools\build.cmd 4
build\OBS_Spatial_FX_V3.6.6-b_Setup.exe /SILENT
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run_v33_installed_qa.ps1 -Scenario v34-hold -Output work/v3.6.6-b/final-bridge
python tests/v366_styles.py work/v3.6.6-b/final-bridge work/v3.6.6-b/final
python tests/v366_styles.py work/v3.6.6-b/final-bridge work/v3.6.6-b/rich --rich
python tests/v366_acceptance.py work/v3.6.6-b/final-bridge work/v3.6.6-b/qa
python tests/v366_acceptance.py work/v3.6.6-b/final-bridge work/v3.6.6-b/qa2 --skip-perf
python tests/v366_legacy.py work/v3.6.6-b/final-bridge work/v3.6.6-b/legacy work/v3.6.6-b/baseline
python tests/v366_acceptance.py work/v3.6.6-b/restart-bridge work/v3.6.6-b/restart --restart work/v3.6.6-b/qa2
python tests/v366_gallery.py work/v3.6.6-b dist/v3.6.6-b
```

安装器 SHA256 `4410CE4B830A58642F8857E93D350EEB68DEACCC6C17A89FA0AD91F9415D0F6F`。DLL SHA256 `DAD5E978C7768C52CB99B4258A18EDAF96601E0331DBBFFF1960C5E3FFB9F93E`。
