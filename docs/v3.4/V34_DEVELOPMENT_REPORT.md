# OBS Spatial FX v3.4 开发交付记录

本次按已确认目录实现13个大类、92个子功能，对应113条原始选择。原“待定”项全部确认；暗角圆度0为矩形，1为跟随画布长宽比的椭圆。原选择JSON与旧版说明书均保留。

## 实际交互

每个场景的FX初始为空，通过“+”选择该舞台已导入时间轴的素材实例。同一个OBS素材的多个实例可以分别添加和设置FX。

素材卡片显示13个类别图标，初始全部关闭。单击图标变色并显示该类子项，再次单击暂停该类。子项用复选框启用；未勾选时参数折叠，勾选后显示紧凑参数与0.01步进数值滑条。类别与子项均提供悬停说明。组关闭后保留参数与子项选择，暂停处理。

卡片支持颜色预览、素材引用、图片/LUT文件、单效果复位与整卡移除；卡片移除保留时间轴对象及关键帧。形状裁切等模式只显示相关参数。全局“启用舞台灯光”仍控制既有灯光，新逐实例FX由自身组/子项开关控制。

完整目录映射与参数范围：`V34_IMPLEMENTATION_MAP.json`。

## 实际修改文件

完整清单：`work/v3.4-development/changed-files.json`。主要文件：

| 文件 | 内容 |
|---|---|
| src/fx_catalog.hpp/.cpp、fx_catalog_a/b/c.inc | 13类、92子项、稳定ID、范围、默认值及悬停说明 |
| src/fx_panel.hpp/.cpp | 空页加号、素材卡片、图标入口、按需展开参数 |
| src/fx_model.cpp、model.hpp/.cpp | schema5、独立实例FX、32槽参数与保存读取 |
| src/fx_renderer.hpp/.cpp、fx_shader.hpp | 子效果按需编译、复用处理表面、历史双缓冲 |
| src/fx_assets.cpp | 图片及PNG/.cube LUT验证、资源缓存 |
| src/fx_dependencies.hpp | 隐藏素材也可作为原始遮罩/纹理引用 |
| src/fx_geometry.inc、fx_motion.hpp、scene_evaluator.cpp、stage.cpp | 统一运动/光照世界坐标、局部UV运动换算、裁切基准 |
| src/dock.cpp、dock.hpp、dock_actions.cpp、dock_v2.cpp | Dock集成、场景同步、预设引用按目标实例映射 |
| data/effects/fx_common.body、fx_footer.body、fx01–fx13.body | 新效果实现、混合与透明度处理 |
| data/effects/spatial.effect | 每灯独立阴影颜色及初始化 |
| src/plugin.cpp、CMakeLists.txt、installer/setup.cpp、setup.rc.in | 3.4.0版本、编译与安装载荷 |
| tests/fx_tests.cpp、fx_asset_tests.cpp、v34_qa.inc、v34_render_probe.py、v34_coordinate_probe.py | 模型/资源、真实Qt界面与GPU像素对照 |

## 编译与真实验证

最终实际编译命令：`& tools/build.cmd 4`，日志 `work/v3.4-development/build14.log`。

最终实际单元测试：`& tools/test.cmd`，日志 `work/v3.4-development/ctest-final.log`，7/7 PASS。

真实运行环境：Windows10 22H2 x64、AMD Ryzen7 5800X、Radeon RX6900XT、D3D11。隔离OBS32.2.2与33.0.1，运行Qt6.11.1；未向插件目录打包Qt DLL。

| 验证 | 状态 | 证据 |
|---|---|---|
| 已安装OBS32真实Dock/92参数页/开关/精度/引用预设 | PASS，55项 | installed-ui32/dock-smoke.json |
| 已安装OBS33相同界面检查 | PASS，55项 | installed-ui33/dock-smoke.json |
| OBS32、33退出保存后重启 | PASS，各12项 | installed-persist32、installed-persist33 |
| 所有子项GPU渲染、非空默认输出与离散模式 | PASS，92子项、每个OBS版本224组默认/模式变体 | gpu32-final、gpu33-verified/render-tests.json |
| 禁用完全旁路、隐藏引用、相同源双实例独立 | PASS | 同上 |
| 低帧率历史保持与0.5强度混合无递归漂移 | PASS | 同上，hold-before/after-source-change.png |
| 暗角端点/材质旋转/裁切/运动图层 | PASS，每版本8项 | gpu32-final、gpu33-final/coordinates |
| 1080p60代表效果链持续30秒 | PASS，纹理分配稳定18次，采样段无新增渲染丢帧 | 各GPU目录performance.json |
| 实际v3.3→v3.4覆盖升级、占用文件回滚、卸载/重装 | PASS，18项 | installer/installer-tests.json |
| OBS运行期间阻止覆盖 | PASS，退出10且全部安装文件哈希未变 | install-running-guard.json |
| 旧版说明书与历史安装包保留 | PASS，哈希核对 | historical-hashes.json、final-package-audit.json |
| NVIDIA硬件运行 | NOT RUN，当前设备无NVIDIA显卡 | 无伪造结果 |
| Win10 1909、Win11及未来OBS34+运行 | NOT RUN，当前测试环境未覆盖 | 仅以上实测版本结论 |
| 数小时持续运行/92效果同时开启 | NOT RUN | 30秒测试只代表指定效果链 |

表内相对证据路径均位于 `work/v3.4-development`。真实已安装加载路径与DLL哈希保存在四个UI目录的 `installed-path.json`，均为ProgramData中的单一产品模块。四次最终安装版UI/保存重启退出日志均记录 `Number of memory leaks: 0`；这不等同于无限时长的无泄漏证明。

## 排查与修正

1. OBS effect parser不保留全局const初值，导致子效果分支错误。改为编译前直接替换函数字面量，避免编译成功而画面错误；88个像素子效果另有独立真实D3D11编译记录。
2. 组着色器中的复杂无用分支使Direct3D编译器栈溢出。改为按子效果专门化编译，仅保留实际使用技术；LED散景积分分支重排后保留原数学效果。
3. OBS结束一个技术时会清空参数。历史捕获后的显示混合重新绑定image、strength、effect_result，解决残影/隔行/低帧率黑画面。
4. 保存预设时引用其他场景的实例ID会失效。改存图层槽位，再映射到当前目标实例；无效槽位清空。
5. 隐藏/禁用的引用素材此前未捕获。仅激活当前FX确实依赖的原始输入，不递归叠加被引用对象的FX。
6. 受光改用实际旋转、缩放、裁切中心和相机世界位置；运动模糊改为投影基向量的雅可比逆换算UV，隐藏/缺失期间打断运动缓存。
7. LUT按资源类型缓存，拒绝非有限域、非法尺寸与不完整数据；六区调色提供各自独立的色相、饱和度和密度。

早期失败日志保留。GPU33第一次最终回归中，两个稀疏粒子效果因“平均Alpha必须大于1”的错误测试条件被判失败；像素证据显示其输出非空。条件已改为Alpha最大值大于0，OBS32与最终OBS33完整复测通过，没有改写旧失败报告。

## 边界与后续记录

- 运动模糊使用已知图层/相机位移，不分析视频内部光流。静止时旁路表现与原图一致。
- 像素描边、厚度、背光、模糊等被限制在源素材矩形内；它们不把图片变为真实三维网格。既有跨层投影继续走舞台灯光通道。
- 原始素材引用只提供原始输入，避免相互引用形成无限效果递归。LUT或图片缺失时旁路并保留错误提示。
- 性能测试使用基础模糊、基础颜色和暗角三效果链；数据不代表任意组合、任意显卡或所有92项同时工作。
- 审查记录P3的CPU临时集合分配仍可优化；本版没有将GPU纹理复用测量包装成“CPU零分配”。
- 安装目标为ProgramData，便携OBS使用生成的专用启动入口。OBS33实测新加载路径；未来OBS版本需实际回归，不能提前保证未发布API兼容性。

交付：`dist/OBS_Spatial_FX_V3.4_Setup.exe`、完整源码、PDB、验证证据及 `V34_RELEASE.json`。详细构建与重现命令见 `BUILD_V34.md`。
