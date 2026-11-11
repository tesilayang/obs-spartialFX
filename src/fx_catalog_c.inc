// Parameter order is the GPU p0..p3 packing contract. Shape help tags drive conditional UI.
add(FxDefinition{"FX11.0", "立体厚度", "在素材矩形内生成圆角厚面板或透明轮廓挤出；这是像素级 2.5D 外观。", 11, 0, {
    FxParameter{"mode","形态","两种厚度外观。",0,0,1,{"圆角厚面板","透明轮廓挤出"}},
    FxParameter{"depth","厚度","挤出长度，像素。",24,0,160,{}},
    FxParameter{"angle","挤出方向","图像平面内方向，度。",45,-180,180,{}},
    FxParameter{"radius","面板圆角","仅圆角厚面板，像素。",28,0,240,{}},
    FxParameter{"shade","侧面明度","侧面相对颜色明度。",0.55,0,1,{}},
    FxParameter{"bevel","边缘亮边","像素宽度。",2,0,12,{}}
},FxAsset::None,true,true,false,false});
add(FxDefinition{"FX11.2", "随机抖动", "平滑确定性随机运动叠加到图层原有坐标与旋转。", 11, 2, {
    FxParameter{"amplitude_x","横向幅度","场景单位。",8,0,300,{}},
    FxParameter{"amplitude_y","纵向幅度","场景单位。",6,0,300,{}},
    FxParameter{"frequency","频率","每秒随机关键点数。",12,0.1,60,{}},
    FxParameter{"rotation","旋转幅度","绕图层 Z 轴，度。",0.4,0,30,{}},
    FxParameter{"seed","随机种子","固定种子便于复现。",1,0,1000,{}}
},FxAsset::None,false,false,false,true});
add(FxDefinition{"FX11.3", "呼吸缩放", "在当前图层缩放上乘以周期性呼吸系数。", 11, 3, {
    FxParameter{"amplitude","缩放幅度","相对基础缩放的比例。",0.04,0,0.9,{}},
    FxParameter{"frequency","频率","每秒周期数。",0.7,0.01,10,{}},
    FxParameter{"phase","相位","周期偏移。",0,0,1,{}},
    FxParameter{"axes","缩放轴","选择同步或单轴呼吸。",0,0,2,{"等比","横向","纵向"}}
},FxAsset::None,false,false,false,true});
add(FxDefinition{"FX11.4", "闪烁", "在当前图层透明度上乘以平滑明暗或开关节奏。", 11, 4, {
    FxParameter{"mode","节奏","平滑呼吸、硬切开关或随机闪烁。",0,0,2,{"平滑明暗","开关节奏","随机闪烁"}},
    FxParameter{"frequency","频率","每秒周期数。",2,0.01,30,{}},
    FxParameter{"minimum","最低透明度","乘数下限。",0.25,0,1,{}},
    FxParameter{"duty","亮起占比","每周期亮起的比例。",0.5,0.01,0.99,{}},
    FxParameter{"phase","相位","周期偏移。",0,0,1,{}},
    FxParameter{"seed","随机种子","随机闪烁使用。",3,0,1000,{}}
},FxAsset::None,false,false,false,true});
add(FxDefinition{"FX12.0", "形状裁切", "按选定形状裁切；四边形顶点应依次围绕轮廓设置，避免自交。", 12, 0, {
    FxParameter{"shape","形状","选择后显示对应形状参数。",2,0,4,{"四边形","多边形","圆角矩形","圆形","椭圆形"}},
    FxParameter{"center_x","中心 X","[shape=1,2,3,4] 归一化坐标。",0.5,0,1,{}},
    FxParameter{"center_y","中心 Y","[shape=1,2,3,4] 归一化坐标。",0.5,0,1,{}},
    FxParameter{"width","宽度／直径","[shape=1,2,3,4] 圆形直径相对画面短边。",0.84,0.01,2,{}},
    FxParameter{"height","高度","[shape=1,2,4] 相对画面高度。",0.84,0.01,2,{}},
    FxParameter{"radius","圆角半径","[shape=2] 像素。",40,0,1000,{}},
    FxParameter{"sides","边数","[shape=1] 取最近整数。",6,3,12,{}},
    FxParameter{"rotation","旋转","[shape=1,2,4] 度。",0,-180,180,{}},
    FxParameter{"tl_x","左上 X","[shape=0] 归一化顶点坐标。",0.08,0,1,{}},
    FxParameter{"tl_y","左上 Y","[shape=0] 归一化顶点坐标。",0.08,0,1,{}},
    FxParameter{"tr_x","右上 X","[shape=0] 归一化顶点坐标。",0.92,0,1,{}},
    FxParameter{"tr_y","右上 Y","[shape=0] 归一化顶点坐标。",0.08,0,1,{}},
    FxParameter{"br_x","右下 X","[shape=0] 归一化顶点坐标。",0.92,0,1,{}},
    FxParameter{"br_y","右下 Y","[shape=0] 归一化顶点坐标。",0.92,0,1,{}},
    FxParameter{"bl_x","左下 X","[shape=0] 归一化顶点坐标。",0.08,0,1,{}},
    FxParameter{"bl_y","左下 Y","[shape=0] 归一化顶点坐标。",0.92,0,1,{}}
},FxAsset::None,false,false,false,false});
add(FxDefinition{"FX12.1", "科幻开窗", "切角窗口、动态边光与轻微轮廓呼吸。", 12, 1, {
    FxParameter{"margin","边距","相对画面短边。",0.04,0,0.45,{}},
    FxParameter{"corner","切角","相对画面短边。",0.12,0,0.45,{}},
    FxParameter{"border","边光宽度","像素。",3,0,30,{}},
    FxParameter{"motion","轮廓呼吸","相对画面短边的变化幅度。",0.008,0,0.1,{}},
    FxParameter{"speed","动画速度","每秒周期数。",0.7,0,10,{}}
},FxAsset::None,true,false,false,false});
add(FxDefinition{"FX12.2", "动态源遮罩", "选择辅助图层，用其透明度或亮度控制当前素材。未选择时保持原图。", 12, 2, {
    FxParameter{"channel","遮罩通道","使用另一幅画面的透明度或颜色。",1,0,4,{"透明度","亮度","红色","绿色","蓝色"}},
    FxParameter{"threshold","阈值","遮罩中点。",0.5,0,1,{}},
    FxParameter{"softness","软化范围","阈值过渡宽度。",0.2,0.001,1,{}},
    FxParameter{"invert","反转","反转遮罩值。",0,0,1,{"关闭","开启"}}
},FxAsset::Source,false,false,false,false});
add(FxDefinition{"FX12.3", "亮度转透明", "按亮度抠除黑底或白底，并可去除对应底色的边缘污染。", 12, 3, {
    FxParameter{"mode","去除底色","去黑或去白。",0,0,1,{"去黑底","去白底"}},
    FxParameter{"threshold","亮度阈值","透明与保留的分界。",0.08,0,1,{}},
    FxParameter{"softness","过渡宽度","亮度过渡范围。",0.15,0.001,1,{}},
    FxParameter{"unmatte","底色清理","按遮罩去除底色混色。",0.7,0,1,{}}
},FxAsset::None,false,false,false,false});
add(FxDefinition{"FX12.4", "透明杂边清理", "压低半透明残留，同时保留可调的边缘过渡。", 12, 4, {
    FxParameter{"threshold","透明度阈值","低于此值的像素被清除。",0.15,0,0.99,{}},
    FxParameter{"softness","过渡宽度","设置低值获得较硬边缘。",0.08,0.001,1,{}},
    FxParameter{"normalize","保留区域实化","将保留的半透明像素推向不透明。",0.3,0,1,{}}
},FxAsset::None,false,false,false,false});
add(FxDefinition{"FX12.5", "透明度／渐变淡出", "整体透明度与任意方向的线性渐变。", 12, 5, {
    FxParameter{"opacity","整体透明度","乘以原素材透明度。",0.8,0,1,{}},
    FxParameter{"gradient","渐变强度","零表示只调整整体透明度。",1,0,1,{}},
    FxParameter{"angle","渐变方向","度。",0,-180,180,{}},
    FxParameter{"start","渐变起点","归一化投影距离。",0,0,1,{}},
    FxParameter{"end","渐变终点","归一化投影距离。",1,0,1,{}},
    FxParameter{"invert","反转方向","反转渐变。",0,0,1,{"关闭","开启"}}
},FxAsset::None,false,false,false,false});
add(FxDefinition{"FX12.9", "渐变叠加", "在素材可见轮廓内覆盖纯色、线性或径向渐变，保留原素材透明度。", 12, 9, {
    FxParameter{"mode","叠加类型","选择纯色、线性渐变或径向渐变。",1,0,2,{"纯色","线性渐变","径向渐变"}},
    FxParameter{"center_x","中心 X","[shape=1,2] 渐变中心，素材归一化横向坐标。",0.5,0,1,{}},
    FxParameter{"center_y","中心 Y","[shape=1,2] 渐变中心，素材归一化纵向坐标。",0.5,0,1,{}},
    FxParameter{"scale","渐变范围","[shape=1,2] 相对素材短边的整体范围。",1,0.01,4,{}},
    FxParameter{"softness","柔度","[shape=1,2] 颜色过渡占范围的比例；较小值形成硬边。",1,0.01,1,{}},
    FxParameter{"reverse","反向渐变","[shape=1,2] 交换渐变方向。",0,0,1,{"关闭","开启"}},
    FxParameter{"angle","渐变角度","[shape=1] 度；零从左到右，九十从上到下。",0,-180,180,{}},
    FxParameter{"scale_x","径向横向比例","[shape=2] 横向半径倍率；与纵向不同时形成椭圆。",1,0.01,4,{}},
    FxParameter{"scale_y","径向纵向比例","[shape=2] 纵向半径倍率；两个轴相同时保持圆形。",1,0.01,4,{}}
},FxAsset::None,true,true,false,false});
add(FxDefinition{"FX12.6", "混合模式", "将辅助图层按选定公式混入当前素材的可见区域。未选辅助源时保持原图。", 12, 6, {
    FxParameter{"mode","混合模式","基础与扩充模式统一选择。",2,0,9,{"正常","相加","相乘","变暗","变亮","滤色","叠加","柔光","差值","排除"}},
    FxParameter{"opacity","混合透明度","辅助源混入比例。",0.65,0,1,{}}
},FxAsset::Source,false,false,false,false});
add(FxDefinition{"FX12.7", "动态纹理覆盖", "平铺并移动选定辅助源纹理，保持素材轮廓。未选辅助源时保持原图。", 12, 7, {
    FxParameter{"scale","平铺次数","每轴重复次数。",2,0.1,20,{}},
    FxParameter{"speed_x","横向流动","每秒 UV 位移。",0.05,-2,2,{}},
    FxParameter{"speed_y","纵向流动","每秒 UV 位移。",0.02,-2,2,{}},
    FxParameter{"opacity","覆盖强度","纹理混入比例。",0.4,0,1,{}},
    FxParameter{"mode","覆盖方式","正常、相乘或滤色。",1,0,2,{"正常","相乘","滤色"}}
},FxAsset::Image,false,false,false,false});
add(FxDefinition{"FX12.8", "燃烧溶解", "程序噪声驱动单素材显隐及高温边缘。", 12, 8, {
    FxParameter{"progress","溶解进度","零完整保留，一完全消失。",0.35,0,1,{}},
    FxParameter{"scale","噪声密度","烧蚀纹理密度。",12,1,100,{}},
    FxParameter{"edge","燃烧边缘","噪声阈值带宽。",0.08,0.001,0.4,{}},
    FxParameter{"softness","边缘柔度","噪声阈值过渡。",0.02,0.001,0.3,{}},
    FxParameter{"speed","噪声流动","每秒纹理运动。",0.15,0,5,{}}
},FxAsset::None,true,true,false,false});
add(FxDefinition{"FX13.0", "LED 演出舞台", "把素材画面采样为发光灯珠与黑色缝隙，模拟真实LED屏幕；圆点或RGB灯珠可选。", 13, 0, {
    {"spacing", "灯珠间距", "每个LED单元的像素尺寸，越大越容易看到独立灯珠与缝隙。", 12, 3, 80, {}},
    {"fill", "灯珠尺寸", "灯珠占单元的比例；越小黑色缝隙越明显。", .72, .1, .95, {}},
    {"gain", "发光亮度", "灯珠颜色来自原素材；控制其发光亮度。", 1.3, 0, 4, {}},
    {"mode", "灯珠排列", "圆点为单色LED单元；RGB将源颜色分解成红绿蓝三颗灯珠。", 0, 0, 1, {"圆形灯珠", "RGB三色灯珠"}},
    {"glow", "灯珠柔光", "以固定九点邻域形成低强度柔光，零关闭柔光采样。", .12, 0, 1, {}},
    {"scan", "扫描亮度", "控制每行灯珠的扫描明暗差；零为稳定屏幕。", 0, 0, 1, {}},
    {"speed", "扫描速度", "仅扫描亮度非零时生效，单位为每秒扫描周期。", .4, 0, 4, {}}}});
add(FxDefinition{"FX13.1", "21:9 宽银幕", "在整个Spatial FX舞台完成合成后处理上下区域；不改变OBS全局画布。多个对象启用时使用时间轴第一项有效设置。", 13, 1, {
    {"mode", "外围处理", "黑边：上下覆盖不透明黑色；透明裁切：裁掉上下内容并保留中间21:9区域，外围透明。关闭即恢复。", 0, 0, 1, {"黑边覆盖", "透明裁切"}}}, FxAsset::None, false, false, false, false, true});
add(FxDefinition{"FX13.2", "动画氛围柔光", "共享模糊副本，以滤色、柔光或相乘塑造明暗氛围。", 13, 2, {
    FxParameter{"radius","柔光半径","像素。",12,0,100,{}},
    FxParameter{"amount","混合强度","模糊副本混入量。",0.5,0,1,{}},
    FxParameter{"mode","混合方式","明亮柔光或暗部氛围。",0,0,2,{"滤色","柔光","相乘"}},
    FxParameter{"saturation","饱和度","最终色彩浓度。",1.1,0,2,{}},
    FxParameter{"tint","氛围染色","颜色混入柔光副本的比例。",0.1,0,1,{}}
},FxAsset::None,true,false,false,false});
add(FxDefinition{"FX13.3", "醉酒眩晕", "周期性镜头偏移、脉动、共享模糊与高光组合。", 13, 3, {
    FxParameter{"sway","晃动","UV 偏移幅度。",0.018,0,0.15,{}},
    FxParameter{"speed","节奏","每秒周期数。",0.45,0.01,4,{}},
    FxParameter{"blur","失焦","像素。",7,0,60,{}},
    FxParameter{"glow","辉光","失焦高光混入量。",0.3,0,2,{}},
    FxParameter{"pulse","脉动","周期性缩放幅度。",0.025,0,0.2,{}},
    FxParameter{"double","重影间距","第二视像素偏移。",5,0,60,{}}
},FxAsset::None,false,false,false,false});
