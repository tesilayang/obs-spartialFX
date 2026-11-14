# v3.6.1 原生面板验收契约

- 保留面板根名 `spatial.presets36`，不创建旧 `spatial.preset-choice` / `spatial.preset-name` / `spatial.save-preset` 控件及“我的整套预设”折叠区。
- 首屏勾选 `spatial.preset-targets361` 的现有未锁定实例。无素材时禁用生成，并显示导入入口提示。
- 风格卡片名 `spatial.preset-style-<styleId>`，八张。详细控制 `spatial.preset-details-toggle361` 默认关闭。
- 首屏强度 `spatial.preset-strength36`，时长 `spatial.preset-period36`，循环 `spatial.preset-loop36`，装饰量 `spatial.preset-amount361`（0无/1少/2丰富）。
- 主操作 `spatial.preset-generate361`（生成一套）、`spatial.preset-reroll361`（换一套）、`spatial.preset-apply36`、`spatial.preset-cancel36`。保留状态 `spatial.preset-status36`。
- 详细控制的零件选择：`spatial.preset-motion-part361`、`spatial.preset-mask-part361`、`spatial.preset-decor-part361`。每个 combo 的 data 是核心库 ID，空值为按风格随机，`none` 为关闭。默认遮罩为关闭。
- 参数开始时间、配色A/B、空间位置、锚点、循环次数、节拍、冲突替换、镜头开关、随机种子放详细控制。
- 配色按钮保留 `spatial.preset-color-a36` / `spatial.preset-color-b36`；预览 QLabel 名 `spatial.preset-color-preview-a361` / `spatial.preset-color-preview-b361`，显示十六进制色值和整条背景色。
- 不开启灯光，不改基础布局与深度，不改未选对象。生成预览可替换和取消；应用一次即可撤销整套。
- 已应用列表/移除/烘焙允许保留在详细控制中，使用现有36后缀控件名。
- 锁定/失效/冲突反馈具体且不写入半套对象；切场景/离开页/取消均恢复权威文档。
- 色彩调整实时更新已生成预览，取消恢复先前颜色与先前预览状态；未生成时选色不应自动开始新的随机结果。
- 原生 Qt 测试覆盖生成、换一套、应用、撤销回调、保存重读、对象重排/删除、配色取消、详细手动选择、关闭详细控制不改变选择结果。
