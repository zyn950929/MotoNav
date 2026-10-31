// v7.8 新增音乐页：导航左滑天气，右滑音乐；音乐左滑返回，上下滑也可查看天气；上一首/播放暂停/下一首，中文歌名和歌手。
// MotoNav 圆屏固件：蓝牙接收导航指令，显示箭头、距离和各种行车信息
// 板子：微雪 ESP32-S3-Touch-LCD-1.85B（360x360，466x466 逻辑画布）
// 配套手机 App：v2.0 及以上
//
// 版本记录
//   v1.0  蓝牙接收指令，显示箭头和距离
//   v1.1  改用内存画布绘图，修复箭头不显示
//   v1.2  修复串口打印卡住导致的约 4 秒延迟
//   v1.3  断线保护：蓝牙断开时不再停留在旧箭头；导航中超过 6 秒收不到数据，
//         屏幕边缘亮红圈并提示 NO DATA；支持心跳指令 "P"
//   v1.3.1 修复编译错误（drawWaiting 的参数类型）
//   v1.4  所有箭头按外框居中显示；NO DATA 提示移到距离数字下方
//   v1.5  三阶段转向提示（远：直行线+灰色预告；中：大箭头+黄色进度环；近：橙色）
//   v1.6  拐弯处改为圆角；远处预告竖条居中；进度环连续平滑、动画追赶
//   v1.7  新增：顶部车速、电子眼限速牌（超速变红）、车道提示、
//         中文路名和底部“剩余里程·到达时间·红绿灯数”（由手机渲染成图片发来）、
//         亮度调节；蓝牙改为二进制消息队列，支持最长 512 字节的数据包
//   v1.7.1 整体排版上移，路名和底部信息离外圈更远；文字图片太宽时自动缩小，
//         保证不碰到圆环；有车道时距离数字稍小一号（配套 App v1.7.1）
//   v1.7.2 车道里的左转、右转小图标改为直角样式（竖着上去，再横着拐出去）
//   v1.7.3 底部“剩余里程·到达时间·红绿灯数”常驻显示（出现车道时也不再隐藏）
//   v1.8  转弯箭头角度跟随路口实际角度（手机算好发来），锐角弯显示锐角箭头
//         （配套 App v1.8；旧 App 不发角度时照旧用标准角度）
//   v1.8.1 大图标和远处预告改用“抗锯齿矢量绘制”：转弯处是真正连续的圆弧，边缘平滑无锯齿、
//         无拼接痕迹；小角度转弯的弯也更圆润。图标画好后缓存，画面刷新时直接复制，不增加耗时
//   v1.8.2 图标自动适应可用区域：不管箭头是什么角度（缓弯的箭头特别高），都整体缩放并居中到
//         “车速下方 ~ 车道/距离上方”之间，不再挡住 km/h
//   v1.8.3 进度环、超速红圈、断线红圈、状态点、限速牌改为抗锯齿绘制，边缘平滑无毛边
//         （开机时预先算好圆环上每个像素的半径和角度，每帧只查表，速度很快）
//   v2.0  小地图（罪恶都市风格）：手机发来附近的道路/绿地/水面（地图块）和导航路线，
//         圆屏画成车头朝上、跟着转的小地图；左下角小圆牌显示转向和距离，上方显示车速。
//         离路口 150 米内自动切回大箭头画面。新增蓝牙通知通道，收齐地图后回确认给手机。
//         （配套 App v2.0）
//   v2.1  小地图一直显示，路口前不再跳回大箭头画面：离路口 150 米内左下角圆牌放大、
//         边框变橙色，车道图标也显示在圆牌里；100 米内箭头和进度环变橙色
//   v2.2  提高小地图帧率：双缓冲 + 双核并行（一个核画下一帧，另一个核同时把上一帧推到屏幕）；
//         取消 60 毫秒的帧间隔限制；道路只在拐弯处补圆角，少画很多圆。
//         每 2 秒把帧率、画图耗时、刷屏耗时发给手机，App 上可以看到（配套 App v2.0.1）
//   v2.3  减轻画面撕裂：屏幕数据线速度可在 App 里切换 40 / 60 / 80 MHz（保存在圆屏里，切换后自动重启）。
//         速度越快，每帧推到屏幕的时间越短，撕裂越轻；如果花屏就切回 40。（配套 App v2.1）
//   v2.4  小地图左下角圆牌：离路口 500 米以外显示直行箭头（和大箭头画面的远阶段一致），
//         500 米以内才显示转向箭头，不再几公里外就提示转弯（配套 App v2.2：测速提醒提前）
//   v2.5  支持天地图“图片地图”（大块数据类型 3）：手机把天地图底图归类成 街区/道路/绿地/水面 四色、
//         压缩后发来，圆屏每帧按车头方向旋转显示；线条地图（OpenStreetMap）照常支持，
//         两种都有时显示最新收到的那种（配套 App v2.3）
//   v2.6  提高帧率：
//         1. 快速刷屏：自己驱动屏幕数据线，一边整理下一段像素一边传上一段（原来是排队一段一段来），
//            可在 App 里关掉（花屏时用）；
//         2. 图片地图按 16×16 小块存放，旋转取像素时内存命中率高很多；
//         3. 整圈的圆环（黑边、底环、红边）改为按行填充，不再逐点查表；
//         4. 新位置到达时，和推算位置之间的偏差在 0.4 秒内平滑修正，地图不再“跳一下”；
//         5. 帧率统计里多报“画地图”“画边框信息”各用多少毫秒（配套 App v2.4）
//   v2.7  小地图信息区重新设计：去掉左下角突兀的小圆牌和车速胶囊，改成和黑色外圈连成一体的
//         上下两块“弧形面板”——上面显示车速/测速，下面显示转向箭头和距离；
//         离路口 150 米内下面板升高、分隔线变橙色，并显示车道
//   v2.8  按要求改回整屏地图：去掉上下面板，车速、转向箭头、距离直接浮在地图上，
//         带黑色描边保证看得清；车道放在一条半透明深色底上（地图仍然透得过来）
//   v2.9  更有 GTA 味：
//         1. 地图配色主题：罪恶都市（默认，暖沙色街区、青绿水面、粉紫路线、青色车速）/
//            GTA5（深色地面、浅色道路、紫色路线）/ 原版，可在 App 里切换（"#t,0/1/2"）；
//         2. 雷达边缘的“N”北向标记，跟着车头方向转；
//         3. 目的地标记：在雷达范围内显示在地图上，范围外贴在雷达边缘指出方向；
//         4. 雷达边缘一圈渐暗阴影，更像游戏里的圆形雷达（配套 App v2.5）
//   v3.0  1. 走过的路线变灰（或直接擦掉，App 里选），只有前方的路线是鲜艳的颜色；
//         2. 夜间配色：三套主题都有夜间版（地面压暗、道路变浅色线条、路线保持鲜艳），
//            手机按当地日出日落自动切换，也可以固定白天/夜间（配套 App v2.8）
//   v3.1  小地图底部加一行行程信息：到达时间 · 剩余里程 · 红绿灯数（离路口 150 米内让位给车道，自动隐藏）；
//         前方 300 米内有红绿灯时，左侧显示红绿灯图标和距离（配套 App v3.1）
//   v3.2  红绿灯直接画在小地图上它实际的位置（只画还没开过的），下一个灯在 300 米内时旁边标距离；
//         去掉 v3.1 左侧的红绿灯面板（配套 App v3.3）
//   v3.3  小地图的车道提示从底部移到车速下面，并放大一点（不再和底部的 N 标记、外圈挤在一起）
//   v3.4  车道图标抗锯齿：边缘平滑，左转/右转/掉头的拐角改成圆弧（大箭头界面的车道也一样）
//   v3.5  电子眼画在小地图上的实际位置：测速点是带限速数字的小限速牌，其他拍照点是相机图标；
//         开过以后自动消失（配套 App v3.4）
//   v3.6  首页加一只像素猫猫：连上手机时睁眼、摇尾巴、偶尔眨眼；没连上手机时闭眼睡觉，头上飘 z
//   v3.7  转弯更顺：小地图转向改成按时间平滑转动（最快每秒 70 度），缩放也按时间平滑，
//         相邻两帧变化更小更均匀，撕裂感更轻（这块板子没接屏幕刷新同步信号，撕裂无法完全消除）
//   v3.8  小地图上的电子眼按类型画不同图标（测速牌、监控、闯红灯、违章拍照、公交道、应急车道、
//         非机动车道、区间起点/终点、流动测速、ETC、礼让行人、违法变道、违停、禁止鸣笛）；
//         图标优先画在实际位置，和别的东西（另一个电子眼、红绿灯、车头、北向标记、车速、底部信息）
//         重叠时自动挪到旁边的空位，并用细线连回实际位置
//   v3.9  电子眼图标挪开时紧贴在实际位置旁边（不再拉长线，实际位置画个小白点）；车头箭头缩小到约 70%
//   v4.0  电子眼图标位置选好后锁住不动，转弯转稳后才重新选一次；停车/暂停后一直保持地图画面
//         （以前 5 秒收不到位置就切回大箭头画面）
//   v4.1  路上只画蓝色小摄像头（代替小白点）和红绿灯；类型图标（测速牌、违章拍照……）一律摆在
//         小摄像头旁边，优先放在道路两侧，不压路线，图标之间留空隙不重叠
//   v4.2  修复两个已锁住位置的类型图标叠在一起：锁住的图标和别的电子眼图标重叠时重新选位置
//   v4.3  同一处有好几个电子眼时，类型图标排成一列（和道路平行）整列放在道路一侧，间距固定不会重叠；
//         整列一起锁住（v4.2 的“重叠就重选”去掉了，它会让图标来回跳）；两侧都放不下时放到远一列，用细线连回
//   v4.4  导入高德分享路线导航时，原路线画成一条橙色细线（在导航路线下面，走错路时才露出来）；
//         新增大块数据类型 4 = 原路线，"#o" = 不显示原路线
//   v4.5  蓝牙允许 2M 模式和最长空中数据包，手机请求时直接用（大块数据传得更快）
//   v4.6  首页的猫猫换成原创像素女骑手和小摩托：连上手机时围巾马尾飘动、眨眼、挥手；没连上时车灯熄灭、打瞌睡
//   v4.7  修复类型图标被推到远处、拉一根长线：小摄像头和图标之间改按圆形判断重叠（以前按方框，斜放时误判）
//   v4.8  巡航模式（"#z,1/0"）：没有目的地时也显示小地图、车速、电子眼和限速；
//         道路安全提示图标（急弯、学校、事故多发、注意行人、陡坡、村庄、窄桥、落石、隧道、注意，编号 16~25），
//         路上画小黄点、图标摆在旁边；电子眼最多 10 个；修复文字消息超过 63 字节被截断（电子眼多了会丢）
//   v4.9  清爽模式（"#w,1/0"）：小地图照常显示，去掉红绿灯、电子眼、安全提示、测速牌、超速红圈和底部行程信息，
//         只留地图、路线、车速、转弯箭头/距离、车道和进度环
//   v5.0  路况颜色（"#u"：缓行黄、拥堵红、严重拥堵深红）；走错路时显示 REROUTING（"#y,1/0"）；
//         发现更快路线时显示 FASTER ROUTE -X min（"#h,分钟"）；GPS 信号弱时左上角亮 GPS 图标（"#x,1/0"）
//   v5.1  来电显示（"#i,1,号码" / "#i,0"，名字是图片栏 2 的文字图片）：来电时整屏显示是谁，接听或挂断后回到原画面
//   v5.2  触屏接电话：来电画面上按住屏幕任意位置 0.8 秒（绿圈外一圈白色进度走满）就接听，
//         通过通知通道告诉手机 "T,A"；按下时也报一次坐标 "T,P,x,y"（以后做左右按钮用）。
//         触摸芯片 CST9217（SensorLib 库），只在来电时读取
//   v5.3  天气页：在圆屏上左右滑（上下滑也行）切换到天气，再滑一下回来。手机用高德天气查当地实况和
//         4 天预报发来（"#W,图标,温度,更新时间,天数,{星期,图标,最高,最低}…"；地名·天气、风·湿度是
//         图片栏 3、4 的文字图片）。导航中看天气 15 秒后、或快到路口（200 米内）自动回到导航画面。
//         触摸改为一直监听（触摸芯片有动静才读），切到天气页时向手机要一次数据 "T,W"
//   v5.4  换页滑动动画：跟着手指方向，旧画面滑出、新画面滑入（约 0.3 秒，先快后慢）；
//         自动回导航时按原路滑回去。触摸方向和画面相反的话，改 TOUCH_FLIP_X / TOUCH_FLIP_Y
//   v5.5  换页改成“跟手”：手指按住拖动时新页面跟着手指一点点滑进来，松手后按速度顺势滑完；
//         拖得不够远、也没甩出去就弹回原页面。新页面盖在上面，旧页面慢慢往后退（视差）、
//         渐渐变暗，交界处有一道阴影；动画用两块画布轮流推屏，更顺滑
//   v5.5.1 触摸左右方向反了：TOUCH_FLIP_X 改为 1
//   v5.6  天气页下面一排改成接下来 5 个小时：几点、图标、温度，会下雨时显示降雨概率（蓝色）。
//         "#W,图标,温度,更新时间,H,个数,{几点,图标,温度,降雨%}…"；手机查不到逐小时时发 D（按天，和以前一样）
//   v5.7  小地图提速：
//         1) 天地图底图（每帧最费时的逐像素计算）改成两个核心一起画：主核心画偶数行，另一个核心画奇数行。
//            另一个核心平时只负责推屏，推屏大部分时间在等数据线传完，正好空着
//         2) 雷达边缘渐暗：开机时把每个像素的暗度算好存表，每帧只查表（原来每个像素都要开平方、做 3 次除法）
//         3) 帧率统计里多报一项“底图用时”，方便看瓶颈在哪
//   v5.8  实测 8.9 帧：底图 42 毫秒、边框和信息 53 毫秒。
//         1) 帮忙画底图的任务优先级提到推屏任务之上（原来推屏一直占着那个核心，帮忙的任务排不上，
//            主核心画完自己那一半只能干等）
//         2) 描边文字（车速、距离）原来要把字重复画 24 遍，改成 16 遍，效果一样
//         3) 每帧各步骤分别计时，每 2 秒发一份 "Q,…" 给手机（单位 0.1 毫秒），找出真正慢的地方
//   v5.9  按实测分步用时继续提速（13 帧时：外圈 22.7 毫秒、边缘渐暗 10.3 毫秒）：
//         1) 外圈（黑边 + 进度环底 + 进度条 + 超速红圈）原来分 4 次画，进度条还要把一圈圈像素在屏幕上
//            来回跳着访问（很费外挂内存）。改成一次从上到下逐行算出最终颜色，两个核心各画一半的行
//         2) 边缘渐暗也改成逐行、两个核心一起画，暗度按“半径平方”查一张放在芯片内部内存里的小表
//         3) 混色改用乘法加移位，不再做除法
//   v6.0  v5.9 的外圈反而更慢（48 毫秒）：这块芯片的浮点开平方、除法是软件算的，很慢，而外圈每个像素都开了平方。
//         1) 外圈改成纯整数：半径用“半径平方 → 半径”的小查表（放芯片内部内存），进度条的角度判断改成
//            两次乘法（叉乘）；圆外角落直接整段清零
//         2) 两个核心分活改成“上半屏 / 下半屏”，不再隔行交错（交错时两个核心抢同一块缓存，互相拖慢）
//         3) 转弯撕裂：屏幕没有接刷新同步信号，帧率低时相邻两帧角度差太大就会看到明显的错位。
//            现在每帧最多转 3.5°（帧率高时不影响，帧率低时画面转得稍慢一点，但不再撕得厉害）
//   v6.1  新增 8 种地图风格（"#t,3"~"#t,10"）：
//           3 赛博霓虹  4 复古纸地图（夜间配色时自动变成霓虹）  5 极简黑白  6 蓝图（跟着地图走的网格）
//           7 掌机像素（底图按 3×3 大像素画，还更快）  8 雷达（转动的扫描线 + 距离圈）
//           9 寻宝罗盘（六边形网格、跟着车头转的罗盘刻度、目的地菱形 + 脉冲圈）
//           10 卫星图（手机把天地图卫星照片压成 16 色发来，地图数据里自带调色板）
//         另加“弯道急缓上色”开关（v6.2 起是 "#Q,1/0"）：前方路线按弯急程度涂成绿 / 黄 / 红，任何风格都能叠加
//   v6.1.1 赛博霓虹的道路青色调暗一点，让品红路线更跳
//   v6.2  圆屏方向：整个画面可以转 0° / 90° / 180° / 270°（"#R,0~3"，存在板子里，开机就按这个方向），
//         装车时线从哪边出来都行。转向在推屏时顺手完成（不多花画图时间）；触摸方向跟着一起转。
//         修复：v6.1 的弯道上色用了 "#q"，和“屏幕数据线速度”撞车，一直开不了 → 改成 "#Q,1/0"
//   v6.4  圆屏方向支持任意角度（"#D角度"，0~359，没有逗号，旧固件会忽略它）：App 上转圆盘，圆屏实时跟着转。
//         90° 的整数倍照旧走原来的快速通道；其他角度推屏时逐像素按角度取样（只算圆里面的像素），
//         帧率会略低一点。手指滑动方向也按角度换算。角度停下 1.5 秒后才存进板子（拖动时不反复写闪存）。
//         （配套安卓 App v6.3；Mac 上 iOS 那边做的 v6.3 固件用的指令可能不同，以这一版为准再对齐）
//   v6.5  蓝牙配对验证：陌生手机连上也发不了任何指令。
//         - 第一次连接时圆屏显示 6 位配对码，在手机弹出的配对框里输入；配好后手机被记住（最多 3 台），以后自动连、全程加密
//         - 只有“开机 2 分钟内”或“还没有配对过任何手机”时才显示配对码；其他时候有陌生手机来配对，
//           圆屏显示 LOCKED，配对必然失败（要加新手机：重启圆屏，2 分钟内配对）
//         - 连上 60 秒还没通过验证的连接自动断开，不会占着圆屏让你自己的手机连不上
//         - 通过验证后通知手机 "U,1"（App 收到后把所有状态重发一遍）；"#UNPAIR" 清除记住的所有手机
//   v6.6  修："#UNPAIR" 把正在用的手机也忘了，可手机那边还记着配对 → 两边对不上，再也连不上。
//         改成只忘掉“别的”手机，正在连着的这台保留（丢手机 / 换手机时用新手机点一下就行）
//   v6.7  小地图跟得更紧（减少“慢半拍”）：
//         1. 转弯预判：用前后两次定位算出正在以多快的速度转弯，两次定位之间地图按这个速度继续转，
//            位置也沿着弯道推算（原来只会沿直线推、朝向要等下一次定位才变）
//         2. 提前量：画面显示“再过一点点时间”的位置和朝向，抵消 GPS 和传输的延迟（"#L毫秒"，0~800，默认 300；
//            App 里可选 关 / 0.3 秒 / 0.6 秒）
//         3. 地图朝向追得更快（时间常数 0.35 → 0.22 秒）
//         4. 不认识的 "#..." 指令一律忽略（以前会被误当成导航方向）
//   v6.8  电量显示（接了锂电池时）：板子上的 AXP2101 电源芯片读电量、是否在充电、是否插着 USB。
//         圆屏顶部（蓝牙小圆点右边 / 小地图车速上方）画一个小电池和百分比：>50% 绿、20~50% 黄、<20% 红，
//         充电时青色带小闪电；低于 10% 没在充电时闪烁。没接电池就不画。
//         电量变化或每分钟发给手机一次："V,百分比,是否充电,是否插USB,电压mV"
//   v6.9  来电按住接听从 0.8 秒缩短到 0.2 秒（骑车时手离开车把的时间越短越安全）；
//         提示文字改成 "TAP & HOLD"，白点进度圈照旧（0.2 秒走满）
//   v7.0  蓝牙名带上自己硬件地址的最后 4 位（例 "MotoNav-3A7F"），几块圆屏在一起不会混；
//         这 4 位和手机蓝牙设置里看到的地址末尾一样。首页和配对页都显示这个名字
//   v7.1  到达特效：到达目的地时约 7 秒庆祝动画（放射光芒、冲击波、烟花、飘动的终点旗、ARRIVED 霓虹字、
//         行程统计逐字打出）；到达途经点 3 秒短版。点一下屏幕提前结束。
//         新指令（不带逗号，旧固件会忽略）："#O米/分钟/最高车速" 行程统计（在 "A,0" 前发），"#J" 到达途经点
//   v7.2  修：首页电量图标开机闪一下就没了（骑手动画每次重画都把它擦掉）→ 骑手画完补画蓝牙点和电量；
//         电量变化时只重画状态栏，不再整屏重画；偶尔读不到电池不会马上隐藏（连续 3 次才算没接电池）；
//         小地图上电量图标加黑色底，浅色地图上也看得清
//   v7.3  路程进度：屏幕左边沿一条细弧（在转弯进度环里面），从左下往左上走满 = 到达；白点是你现在的位置，
//         上端粉点是终点。用 "#e" 里的剩余里程自己算：每次剩余变少就记进“已走”，进度 = 已走 /（已走 + 剩余），
//         偏航重新规划也不会归零。"#M1" / "#M0" 开关（不带逗号，旧固件忽略；默认开）
//   v7.4  路程进度改到最外圈：一开始是一整圈绿环，从正上方顺时针，走过的部分变灰，交界处一个小白点；
//         快到路口出现转弯倒计时环时，倒计时环盖掉行程环，转完弯再回到行程环。去掉 v7.3 左边那条细弧
//   v7.5  行程环改细：8 像素 → 4 像素
//   v7.6  巡航模式不画路线：以前会把上一次导航留下的路线（粉色 / 走过变灰）、弯道上色、路况、红绿灯、终点标记画出来，
//         现在巡航时道路都保持地图原色
//   v7.7  圆屏转到 0/90/180/270 以外的角度时字发糊：推屏时改成双线性插值（取周围 4 个点按距离混合），
//         斜着转的字和线条边缘平滑、粗细均匀
//
// ---------------- 通信协议（手机 → 圆屏，每条消息一个蓝牙数据包） ----------------
//   R,300 / L,120 / S,800 / SL / SR / U / A   导航：方向,距离(米)
//   L,120,-135            v1.8：可选第三项 = 路口实际转弯角度（负左正右，单位度）
//   C                     清屏回到待机
//   P                     心跳
//   #v,42                 车速 km/h
//   #k,60,300 / #k        前方测速：限速,距离(米)；只有 #k 表示没有测速
//   #j,0033F0             车道：每条车道两个十六进制字符（背景方向码、推荐方向码，F=不推荐）；只有 #j 表示隐藏
//   #b,200                屏幕亮度 0~255
//   'B' + 二进制头 + 数据   文字图片（4 位灰度），见 handleBitmap()
//   #p,坐标系,x分米,y分米,朝向,车速   v2.0 当前位置（平面坐标，x 向东 y 向北）
//   #m,1 / #m,0           v2.0 小地图开 / 关
//   #q,40 / #q,60 / #q,80 v2.3 屏幕数据线速度（MHz），保存后重启生效
//   #f,1 / #f,0           v2.6 快速刷屏 开 / 关，保存后重启生效
//   #t,0 / #t,1 / #t,2    v2.9 地图配色：0 原版，1 罪恶都市，2 GTA5（v6.1 增加 3~10，见版本记录）
//   #Q,1 / #Q,0           v6.2 弯道急缓上色 开 / 关（v6.1 误用了 #q）
//   #R,0~3[,角度]         v7.10.1 兼容四档方向，带第二项时按精确角度旋转
//   #D角度                v6.4 圆屏方向任意角度 0~359（例 "#D37"；故意不带逗号，旧固件收到会忽略）
//   #UNPAIR               v6.5 忘掉配对过的手机（v6.6 起保留正在连着的这台）
//   #L毫秒                v6.7 小地图预判提前量 0~800（例 "#L300"；不带逗号，旧固件会忽略）
//   #n,1 / #n,0           v3.0 夜间配色 开 / 关
//   #r,1 / #r,2           v3.0 走过的路线：1 变灰，2 擦除
//   #e,12300,1432,8 / #e  v3.1 行程信息：剩余米,到达时间(时分),红绿灯数；只有 #e 表示隐藏
//   #g,120 / #g           v3.1 前方红绿灯距离（米）；只有 #g 表示隐藏
//   #c,坐标系,x,y,限速,图标,... / #c   v3.5 前方电子眼位置（米），最多 6 个；只有 #c 表示清空
//                         v3.8 最后一项是图标编号 0~15（手机按高德类型换算好，见 CAM_ICONS）
//   'M' + 二进制头 + 数据   v2.0 大块数据（1 = 地图块，2 = 导航路线，3 = 天地图图片地图，4 = 原路线 v4.4），见 handleBulk()
//   #o                    v4.4 不显示原路线
//   #z,1 / #z,0           v4.8 巡航模式开 / 关
//   #w,1 / #w,0           v4.9 清爽模式开 / 关
//   #i,1,号码 / #i,0      v5.1 来电显示开始 / 结束（名字用 'B' 图片栏 2）
//   #u,起,止,状态,...      v5.0 路况（米，离路线起点）；#y,1/0 重新规划中；#h,分钟 / #h 更快路线；#x,1/0 GPS 弱
//   #HM,0/1               v7.9 App 音乐控制 / HID 媒体按键（保存到圆屏；互斥）
//   HID,模式,可发按键        v7.9 HID 就绪状态；CAP,HID,1 能力通知
//   #Z,有会话,播放中,按钮掩码   v7.8 音乐状态（1=上一首，2=播放暂停，4=下一首）；图片栏 5/6 为歌名/歌手
// ---------------- 圆屏 → 手机（通知） ----------------
//   T,M,Q / T,M,P / T,M,T / T,M,N   v7.8 请求音乐状态 / 上一首 / 播放暂停 / 下一首
//   K,类型,编号           v2.0 这一份大块数据已收齐
//   U,1                   v6.5 这台手机通过了配对验证（连上后发 3 次）
//   V,电量,充电,USB,mV    v6.8 圆屏电池（电量 -1 = 没接电池）

#define FW_VERSION "v7.12.3-185B"

// 调试时想在串口监视器里看到每条收到的文字指令，把 0 改成 1
#define DEBUG_SERIAL 0

// 导航中超过这么久没收到任何消息（含心跳），就提示数据中断
#define STALE_MS 6000

// 三阶段的分界距离（米），可按需要调整
#define STAGE_FAR_M  500   // 超过这个距离：远处预告画面
#define STAGE_NEAR_M 100   // 小于等于这个距离：橙色警示

#include <Arduino.h>
#include "Arduino_GFX_Library.h"
#include "pin_config.h"
#include <Wire.h>
#include "TouchDrv.hpp"
#include "GaugeDrv.hpp"
#include "PortScale.h"
#include "Board185BPanel.h"     // v5.2 触摸屏（SensorLib 库，微雪示例里带的）
#include <Preferences.h>
#include <esp_sleep.h>
#include <esp_system.h>
#include <driver/rtc_io.h>
#include "IdlePowerOff.h"
#include <driver/spi_master.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <BLEHIDDevice.h>
#include "HidMediaKeys.h"
#include "AppleMediaClient.h"
#include "ScreenRotationCommand.h"
#include <BLESecurity.h>       // v6.5 配对验证
#include <miniz.h>            // 罪恶都市轮廓：仅接收地图时解压
#include "BootAnimation.h"
#include "DragonRadar.h"
int bootBrand = 0;
String pendingBootAck;
#include <esp_mac.h>           // v7.0 读蓝牙地址，拼蓝牙名
#if defined(CONFIG_NIMBLE_ENABLED)
#include "host/ble_gap.h"     // v4.5 蓝牙 2M 模式
#include "host/ble_store.h"   // v6.5 记住的手机
#endif

// ===== 屏幕（和官方 HelloWorld 示例一样的初始化方式）=====
// v2.6 第 7 个参数 true = 总线可共享（快速刷屏要和它轮流使用数据线）
Arduino_DataBus *bus = new Arduino_ESP32QSPI(
  LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3, true);
Board185BPanel *panel = new Board185BPanel(bus);
PortScale portScale;
struct TextImg;    // Arduino 自动生成的函数声明需要先认识文字图片类型。
void flushLogicalFrame(const uint16_t *fb);
bool backlightPWM=false;
void boardBrightness(uint8_t value) {
  if(backlightPWM)ledcWrite(LCD_BL,value);
  else digitalWrite(LCD_BL,value>0?HIGH:LOW);
}
// 先画在内存里的“画布”上，画完再一次性推到屏幕（需要 PSRAM：OPI PSRAM）
// v2.2：可以切换画布内存的画布，用来做双缓冲
class SwapCanvas : public Arduino_Canvas {
public:
  SwapCanvas(int16_t w, int16_t h, Arduino_G *out) : Arduino_Canvas(w, h, out) {}
  void setFramebuffer(uint16_t *f) { _framebuffer = f; }
  void flush(bool force_flush=false) override { if(_framebuffer)flushLogicalFrame(_framebuffer); }
};
SwapCanvas *gfx = new SwapCanvas(LCD_WIDTH, LCD_HEIGHT, panel);
bool needFlush = false;
bool frameIsMap = false;          // v2.2 这一帧是小地图（整帧重画，可以异步刷屏）
int qspiMHz = 40;                 // v2.3 屏幕数据线速度
bool fastFlushOn = true;          // v2.6 快速刷屏


// ===== 颜色（RGB565）=====
#define C_BLACK  0x0000
#define C_WHITE  0xFFFF
#define C_GREEN  0x07E0
#define C_GREY   0x7BEF
#define C_CYAN   0x07FF
#define C_RED    0xF800
#define C_DIM    0x6B4D   // #6B6B6B 预告、非推荐车道用的灰色
#define C_TRACK  0x2945   // #2A2A2A 进度环底色
#define C_YELLOW 0xF623   // #F5C518
#define C_ORANGE 0xFBC3   // #FF7A1A
#define C_SEP    0x31A6   // #333333 车道分隔线
#define C_SIGNRED 0xE104  // #E02020 限速牌红圈

// ===== 屏幕几何：466x466，圆心 (233, 233) =====
#define CX 233
#define RING_R 222

// ===== 布局（y 坐标）=====
#define SPEED_Y      50    // 车速数字顶部
#define ICON_CY      188   // 没有车道时，图标中心（v1.8.2 起只作初始位置，最终按可用区域自动摆放）
#define ICON_CY_LANE 172   // 有车道时，图标中心
#define ICON_TOP     120   // v1.8.2 图标可用区域上沿（车速 km/h 下方）
#define ICON_MAX_W   250   // 图标最大宽度
#define LANES_Y      238   // 车道行顶部
#define DIST_Y       270   // 没有车道时，距离数字顶部（7 号字，高 56）
#define DIST_Y_LANE  278   // 有车道时，距离数字顶部（6 号字，高 48）
#define ROAD_Y       334   // 没有车道时，路名图片顶部
#define ROAD_Y_LANE  334   // 有车道时，路名图片顶部
#define BOTTOM_Y     372   // 底部信息图片顶部（常驻显示）
#define TEXT_R       200   // 文字必须在这个半径以内（圆环内沿约 215）

// ===== 蓝牙：手机端必须用同样的名字和 UUID =====
#define DEVICE_NAME  "MotoNav"
String bleName = DEVICE_NAME;        // v7.0 实际蓝牙名 "MotoNav-XXXX"（XXXX = 蓝牙地址最后两个字节），开机时算好
#define SERVICE_UUID "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define RX_UUID      "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define TX_UUID      "6e400003-b5a3-f393-e0a9-e50e24dcca9e"   // v2.0 圆屏 → 手机通知
BLECharacteristic *txChar = nullptr;

volatile bool bleConnected = false;

// ===== 接收队列：蓝牙线程收到的每个数据包原样放进队列，主循环再取出处理 =====
#define RXQ_SLOTS 48   // v2.0 加大，接收地图大块数据时不丢包
#define RXQ_MAX   512
struct RxMsg { uint16_t len; uint8_t data[RXQ_MAX]; };
RxMsg rxq[RXQ_SLOTS];
volatile int rxHead = 0, rxTail = 0;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

// ===== 导航与行车状态 =====
bool shownConn = false;       // 屏幕上反映的连接状态
bool inNav = false;           // 是否处于导航中
String navDir = "";           // 当前方向
int navDist = 0;              // 当前距离（米）
float navAngle = NAN;         // v1.8 路口实际转弯角度；NAN = 没有，用标准角度
unsigned long lastRxMs = 0;   // 最近一次收到任何消息的时间
bool staleShown = false;      // 是否正在显示“数据中断”提示
bool navDirty = false;        // 导航画面需要重画

int speedKmh = -1;            // 车速，-1 表示还没有
int camLimit = 0, camDist = 0;
bool hasCam = false;          // 前方是否有测速
int laneN = 0;                // 车道数，0 表示不显示
// v3.1 小地图底部的行程信息（"#e,剩余米,到达时间HHMM,红绿灯数"）和前方红绿灯（"#g,距离"）
bool tripOn = false;
bool cruiseOn = false;        // v4.8 巡航模式（没有目的地，只显示地图、车速、电子眼、安全提示）
IdlePowerOffTimer idlePowerOffTimer = {0, false};
volatile bool powerOffPending = false;
bool cleanOn = false;         // v4.9 清爽模式：地图照常，去掉红绿灯、电子眼、安全提示、测速牌和行程信息（"#w,1/0"）
int tripRemainM = 0, tripEta = 0, tripLights = 0;
// v7.3 路程进度
#include "TripProgress.h"
TripProgress tripProgress;
bool progOn = true;
long progDone = 0;                   // 这趟已经走掉的里程（米）
int progLastRm = -1;                 // 上一次收到的剩余里程
unsigned long progLastAt = 0;
#define progReset() do { progDone = 0; progLastRm = -1; progLastAt = 0; tripProgress.reset(); } while (0)   // 宏：放这么靠前写成函数会打乱 Arduino 自动生成的声明
int lightDist = -1;           // 前方红绿灯距离（米），-1 = 不显示
// v3.5 小地图上的电子眼（手机发 "#c,坐标系,x,y,限速,类型,..."，最多 6 个）
#define MCAM_MAX 10           // v4.8 6 → 10（电子眼 + 道路安全提示）
int mapCamN = 0, mapCamFrame = -1;
int32_t mapCamX[MCAM_MAX], mapCamY[MCAM_MAX];
int mapCamSpd[MCAM_MAX];
int mapCamTyp[MCAM_MAX];      // v3.8 图标编号（手机换算好的，0~15）
int8_t mapCamPref[MCAM_MAX];  // v3.8 图标放好了没有（-1 = 还没放过）
float mapCamPrefA[MCAM_MAX];  // v4.1 类型图标在小摄像头的哪个方向（屏幕角度）
int8_t mapCamPrefK[MCAM_MAX]; // v4.3 放图标时这一处有几个类型图标（个数变了就重新放）
float mapCamLockH[MCAM_MAX];  // v4.0 放图标时的地图朝向：之后固定不动，转弯后（朝向变了且已经转稳）才重新放
uint8_t laneBg[8], laneFr[8]; // 每条车道的背景方向码、推荐方向码（0xF = 不推荐）

// ===== 文字图片（手机渲染好的 4 位灰度图）=====
#define IMG_SLOTS 13           // 0 = 路名，1 = 底部信息，2 = 来电人名字，3/4 = 天气地名和风湿度，5/6 = 音乐歌名和歌手
#define IMG_MAX   8192        // 每张图最多字节数
struct TextImg {
  uint8_t *show;              // 正在显示的图
  uint16_t showW; uint8_t showH;
  uint8_t *recv;              // 正在接收的图
  int recvId; uint16_t recvW; uint8_t recvH; uint8_t recvParts; uint32_t recvMask;
};
TextImg imgs[IMG_SLOTS];
AppleMediaClient appleMedia;
volatile uint32_t mediaConnectionEpoch = 0;
uint32_t lyricImageGeneration = 0; uint16_t lyricImageTitleRevision = 0, lyricImageArtistRevision = 0;
bool musicLyrics = false;                  // #ML,0/1; never persist an enabled default
bool musicDirty = true;
bool musicHave = false, musicPlaying = false;
int musicActions = 0;
int musicPressed = -1, musicAck = 0;
unsigned long musicAckUntil = 0;
// HID adds a second GATT service to the existing authenticated navigation connection.
bool hidMusicMode = false;                  // persisted by #HM,0/1; Android explicitly selects 0
BLEHIDDevice *mediaHid = nullptr;
BLECharacteristic *mediaInput = nullptr;
volatile bool hidSubscribed = false, hidSuspended = false, hidReportMode = true;
volatile uint16_t hidSubscriber = 0xFFFF;
volatile bool hidNotifyAccepted = false;
volatile uint32_t hidSessionEpoch = 0;
uint32_t hidSeenEpoch = 0;
bool hidReadyShown = false, hidReleaseOnReady = true;
String pendingHidStatus, pendingLyricsStatus, pendingMediaStatus;
unsigned long hidInitialReleaseAt = 0;
HidMediaKeys::Pulses hidPulses;
unsigned long hidTapAt = 0;
void hidMediaTick();
bool hidMediaReady();
bool wxDirty = false;                 // v5.3 天气页要重画

// ================= v6.5 蓝牙配对验证 =================
#define PAIR_WINDOW_MS  120000UL      // 开机 2 分钟内可以配对新手机
#define AUTH_TIMEOUT_MS 60000UL       // 连上 60 秒还没通过验证就断开
volatile bool bleAuthed = false;      // 当前连接已加密 + 通过配对码验证
volatile uint16_t bleConnHandle = 0xFFFF;
volatile unsigned long bleConnAt = 0, bleAuthAt = 0;
volatile uint32_t pairCode = 0;       // 要显示的配对码
volatile bool pairReq = false;        // 有新的配对请求（蓝牙线程 → 主循环）
volatile bool pairLocked = false;     // 不在允许配对的时间：不显示配对码
volatile bool pairDone = false;       // 配对结束（成功或失败）
volatile bool pairOk = false;
int authNotifies = 0;                 // "U,1" 已经发了几次

int bondCount() {
  int n = 0;
#if defined(CONFIG_NIMBLE_ENABLED)
  ble_store_util_count(BLE_STORE_OBJ_TYPE_OUR_SEC, &n);
#endif
  return n;
}
bool pairingAllowed() { return millis() < PAIR_WINDOW_MS || bondCount() == 0; }

class SecCB : public BLESecurityCallbacks {
  uint32_t onPassKeyRequest() { return 0; }
  // 圆屏要显示配对码（每次配对随机生成）：只在允许配对的时间里真的显示出来
  void onPassKeyNotify(uint32_t k) { pairCode = k; pairLocked = !pairingAllowed(); pairDone = false; pairReq = true; }
  bool onSecurityRequest() { return true; }
  bool onConfirmPIN(uint32_t pin) { return false; }
  bool onAuthorizationRequest(uint16_t connHandle, uint16_t attrHandle, bool isRead) { return true; }
#if defined(CONFIG_NIMBLE_ENABLED)
  void onAuthenticationComplete(ble_gap_conn_desc *d) {
    bool ok = d && d->sec_state.encrypted && d->sec_state.authenticated;
    if (ok) { bleAuthed = true; bleAuthAt = millis(); authNotifies = 0; }
    pairOk = ok;
    pairDone = true;
  }
#endif
};

class HidInputCB : public BLECharacteristicCallbacks {
public:
#if defined(CONFIG_NIMBLE_ENABLED)
  void onSubscribe(BLECharacteristic *, ble_gap_conn_desc *desc, uint16_t value) override {
    hidSubscriber = desc ? desc->conn_handle : 0xFFFF;
    hidSubscribed = (value & 1) != 0; ++hidSessionEpoch;
  }
#endif
  void onStatus(BLECharacteristic *, Status status, uint32_t) override {
    hidNotifyAccepted = status == SUCCESS_NOTIFY;
  }
};
class HidControlCB : public BLECharacteristicCallbacks {
public:
  void onWrite(BLECharacteristic *c) override {
    if (c->getLength() == 1 && c->getData()[0] <= 1) { hidSuspended = c->getData()[0] == 0; ++hidSessionEpoch; }
  }
};
class HidProtocolCB : public BLECharacteristicCallbacks {
public:
  void onWrite(BLECharacteristic *c) override {
    if (c->getLength() == 1 && c->getData()[0] <= 1) { hidReportMode = c->getData()[0] == 1; ++hidSessionEpoch; }
  }
};
// Gate every report on this connection's pairing, host subscription and report mode.
bool hidMediaLinkReady() {
  return mediaInput && bleConnected && bleAuthed && hidSubscribed &&
         hidSubscriber == bleConnHandle && !hidSuspended && hidReportMode;
}
bool hidMediaReady() { return hidMediaLinkReady() && !hidReleaseOnReady && hidSeenEpoch == hidSessionEpoch; }
bool hidMediaNotify(uint16_t usage) {
  if (!hidMediaLinkReady()) return false;
  uint8_t report[2]; HidMediaKeys::encode(usage, report);
  hidNotifyAccepted = false;
  mediaInput->setValue(report, sizeof(report)); mediaInput->notify();
  return hidNotifyAccepted; // queued by BLE stack; does not confirm player state
}
void hidMediaTick() {
  static bool resetWritten = false;
  uint32_t epoch = hidSessionEpoch;
  if (hidSeenEpoch != epoch) {
    hidPulses.reset(); hidReleaseOnReady = true; resetWritten = false; hidSeenEpoch = epoch;
  }
  bool linkReady = hidMediaLinkReady();
  if (!linkReady) {
    if (!resetWritten) {
      hidPulses.reset(); hidReleaseOnReady = true;
      if (mediaInput) { const uint8_t zero[2] = {0, 0}; mediaInput->setValue(zero, sizeof(zero)); }
      resetWritten = true;
    }
  } else if (hidReleaseOnReady) {
    resetWritten = false;
    if (!hidInitialReleaseAt || millis() - hidInitialReleaseAt >= 50) {
      hidInitialReleaseAt = millis();
      if (hidMediaNotify(0)) hidReleaseOnReady = false;
    }
  } else {
    if (!hidMusicMode) hidPulses.cancelQueued();
    HidMediaKeys::Pulses::Report report;
    if (hidPulses.poll(millis(), report)) {
      bool sent = hidMediaNotify(report.usage);
      hidPulses.submitted(report, millis(), sent);
      if (!report.release || !sent) {
        musicAck = sent ? 6 : 8; musicAckUntil = millis() + 2500; musicDirty = true;
      }
    }
  }
  bool ready = hidMediaReady();
  static int lastMode = -1;
  if (ready != hidReadyShown || lastMode != (int)hidMusicMode) {
    hidReadyShown = ready; lastMode = hidMusicMode;
    pendingHidStatus = String("HID,") + (hidMusicMode ? 1 : 0) + "," + (ready ? 1 : 0);
    Serial.printf("MEDIA HID ready=%d auth=%d sub=%d suspended=%d report=%d release=%d\n", ready, (bool)bleAuthed, (bool)hidSubscribed, (bool)hidSuspended, (bool)hidReportMode, hidReleaseOnReady);
    musicDirty = true;
  }
}
void initMediaHid(BLEServer *server) {
  mediaHid = new BLEHIDDevice(server);
  mediaHid->manufacturer()->setValue("MotoNav");
  // DIY/private device identity, not a borrowed keyboard vendor/product ID.
  const uint8_t identity[7] = {0x01, 0xFF, 0xFF, 0x01, 0x00, 0x00, 0x01};
  mediaHid->deviceInfo()->getCharacteristic(BLEUUID((uint16_t)0x2A50))->setValue(identity, sizeof(identity));
  mediaHid->hidInfo(0, 0x02); // Normally Connectable, no remote wake claim
  mediaHid->reportMap(const_cast<uint8_t *>(HidMediaKeys::REPORT_MAP), sizeof(HidMediaKeys::REPORT_MAP));
  mediaInput = mediaHid->inputReport(HidMediaKeys::REPORT_ID);
  mediaInput->setCallbacks(new HidInputCB());
  const uint8_t zero[2] = {0, 0}; mediaInput->setValue(zero, sizeof(zero));
  mediaHid->hidControl()->setCallbacks(new HidControlCB());
  mediaHid->protocolMode()->setCallbacks(new HidProtocolCB());
  mediaHid->setBatteryLevel(100); // updated from the existing fuel gauge in the loop
  mediaHid->startServices();
}

class ServerCB : public BLEServerCallbacks {
  void onConnect(BLEServer *s) {
    bleConnected = true; ++mediaConnectionEpoch;
    bleAuthed = false;
    hidSubscribed = false; hidSubscriber = 0xFFFF; hidSuspended = false; hidReportMode = true; ++hidSessionEpoch;
    bleConnAt = millis();
    authNotifies = 0;
  }
#if defined(CONFIG_NIMBLE_ENABLED)
  void onConnect(BLEServer *s, ble_gap_conn_desc *desc) { if (desc) bleConnHandle = desc->conn_handle; }
#endif
  void onDisconnect(BLEServer *s) {
    bleConnected = false; ++mediaConnectionEpoch;
    bleAuthed = false;
    bleConnHandle = 0xFFFF;
    hidSubscribed = false; hidSubscriber = 0xFFFF; ++hidSessionEpoch;
    if (!powerOffPending) BLEDevice::startAdvertising();
  }
};

// v2.6 保险：屏幕相关设置在蓝牙线程里直接处理（即使主循环被卡住也能改回来）
void saveAndRestart(const char *key, int val, bool isBool) {
  Preferences pr;
  pr.begin("motonav", false);
  if (isBool) pr.putBool(key, val != 0); else pr.putInt(key, val);
  pr.end();
  delay(100);
  ESP.restart();
}

class RxCB : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *c) {
    if (powerOffPending) return;
    size_t n = c->getLength();
    if (n == 0) return;
    const uint8_t *d0 = c->getData();
    if (n >= 4 && n < 12 && d0[0] == '#' && d0[2] == ',' && (d0[1] == 'f' || d0[1] == 'q')) {
      char num[12];
      size_t ln = n - 3;
      memcpy(num, d0 + 3, ln);
      num[ln] = 0;
      int v = atoi(num);
      if (d0[1] == 'f' && (v != 0) != fastFlushOn) saveAndRestart("fastf", v, true);
      if (d0[1] == 'q' && (v == 40 || v == 60 || v == 80) && v != qspiMHz) saveAndRestart("qspi", v, false);
      return;
    }
    if (n > RXQ_MAX) n = RXQ_MAX;
    portENTER_CRITICAL(&mux);
    int next = (rxHead + 1) % RXQ_SLOTS;
    if (next == rxTail) rxTail = (rxTail + 1) % RXQ_SLOTS;   // 队列满：丢掉最旧的一条
    rxq[rxHead].len = n;
    memcpy(rxq[rxHead].data, c->getData(), n);
    rxHead = next;
    portEXIT_CRITICAL(&mux);
  }
};

// ---- v2.2 双缓冲：核心 0 上的刷屏任务 ----
uint16_t *fbBuf[2] = {nullptr, nullptr};
int fbCur = 0;                    // 画布当前用的是哪一块
SemaphoreHandle_t flushReq = nullptr, flushDone = nullptr;
uint16_t *volatile flushSrc = nullptr;
volatile uint32_t flushMsAcc = 0, flushCnt = 0;

// ---- v2.6 快速刷屏：自己往屏幕数据线上排队发送，两块缓冲轮流用（一块在传、一块在准备）----
#define FAST_PIX 4096                     // 每段像素数（8KB）
spi_device_handle_t fastDev = nullptr;
uint8_t *fastBuf[2] = {nullptr, nullptr};

int screenRot = 0;                        // v6.2 圆屏方向：0 / 1 / 2 / 3 = 0° / 90° / 180° / 270°
int screenDeg = 0;                        // v6.4 圆屏方向（度，0~359）
bool rotFree = false;                     // v6.4 不是 90° 的整数倍：推屏时逐像素按角度取样
int32_t rotCos = 65536, rotSin = 0;       // v6.4 角度的 cos / sin（×65536）
float rotCosF = 1, rotSinF = 0;
unsigned long rotSaveAt = 0;              // v6.4 角度停下后过一会儿再存进板子
int16_t rotSpan[LCD_HEIGHT];              // v6.4 每一行圆里面的半宽（圆外面的像素直接涂黑）

// v6.4 设定圆屏角度。save = true：稍后存进板子
void setScreenDeg(int d, bool save) {
  d = ((d % 360) + 360) % 360;
  screenDeg = d;
  rotFree = (d % 90) != 0;
  screenRot = rotFree ? 0 : d / 90;
  float a = d * (float)M_PI / 180.0f;
  rotCosF = cosf(a); rotSinF = sinf(a);
  rotCos = (int32_t)lroundf(rotCosF * 65536.0f);
  rotSin = (int32_t)lroundf(rotSinF * 65536.0f);
  if (save) rotSaveAt = millis() + 1500;
}

void initRotSpan() {
  const int c = LCD_WIDTH / 2;
  for (int y = 0; y < LCD_HEIGHT; y++) {
    int dy = y - c;
    int r2 = c * c - dy * dy;
    rotSpan[y] = r2 > 0 ? (int16_t)sqrtf((float)r2) : 0;
  }
}

// v6.4 停下 1.5 秒后把角度存起来（loop 里调用）
void saveRotIfDue() {
  if (!rotSaveAt || (long)(millis() - rotSaveAt) < 0) return;
  rotSaveAt = 0;
  Preferences pr;
  pr.begin("motonav", false);
  pr.putInt("rotd", screenDeg);
  pr.putInt("rot", screenRot);
  pr.end();
}

bool fastFlushInit() {
  spi_device_interface_config_t dc;
  memset(&dc, 0, sizeof(dc));
  dc.command_bits = 8;
  dc.address_bits = 24;
  dc.mode = 0;
  dc.clock_source = SPI_CLK_SRC_DEFAULT;
  dc.clock_speed_hz = qspiMHz * 1000000;
  dc.spics_io_num = -1;                   // 片选自己控制（和屏幕库一样）
  dc.flags = SPI_DEVICE_HALFDUPLEX;
  dc.queue_size = 2;
  if (spi_bus_add_device(SPI2_HOST, &dc, &fastDev) != ESP_OK) { fastDev = nullptr; return false; }
  for (int i = 0; i < 2; i++) {
    fastBuf[i] = (uint8_t *)heap_caps_aligned_alloc(16, FAST_PIX * 2, MALLOC_CAP_DMA);
    if (!fastBuf[i]) { fastDev = nullptr; return false; }
  }
  return true;
}

// v7.7 RGB565 拆成 0x0GGRRBB 形式（绿、红、蓝之间留空），一次乘法就能三个颜色一起按权重混合
static inline uint32_t rgbSpread(uint16_t c) { return ((uint32_t)c | ((uint32_t)c << 16)) & 0x07E0F81Fu; }

void fastPush(const uint16_t *fb) {
  panel->startWrite();panel->writeAddrWindow(0,0,PANEL_WIDTH,PANEL_HEIGHT);panel->endWrite();
  spi_device_acquire_bus(fastDev,portMAX_DELAY);digitalWrite(LCD_CS,LOW);
  static spi_transaction_ext_t tr[2];
  uint32_t sent=0,total=PANEL_WIDTH*PANEL_HEIGHT;int bi=0,inflight=0;bool first=true;
  while(sent<total) {
    uint32_t count=min((uint32_t)FAST_PIX,total-sent);
    if(inflight==2){spi_transaction_t* result;spi_device_get_trans_result(fastDev,&result,portMAX_DELAY);inflight--;}
    uint16_t* out=(uint16_t*)fastBuf[bi];
    int y=sent/PANEL_WIDTH,x=sent%PANEL_WIDTH;
    for(uint32_t i=0;i<count;i++) {
      uint16_t pixel=portScale.pixel(fb,x,y,screenRot,rotFree,rotCos,rotSin);
      out[i]=(uint16_t)((pixel>>8)|(pixel<<8));
      if(++x==PANEL_WIDTH){x=0;y++;}
    }
    spi_transaction_ext_t& t=tr[bi];memset(&t,0,sizeof(t));
    if(first){t.base.flags=SPI_TRANS_MODE_QIO;t.base.cmd=0x32;t.base.addr=0x002C00;first=false;}
    else t.base.flags=SPI_TRANS_MODE_QIO|SPI_TRANS_VARIABLE_CMD|SPI_TRANS_VARIABLE_ADDR|SPI_TRANS_VARIABLE_DUMMY;
    t.base.tx_buffer=fastBuf[bi];t.base.length=count*16;
    spi_device_queue_trans(fastDev,(spi_transaction_t*)&t,portMAX_DELAY);
    inflight++;bi^=1;sent+=count;
  }
  while(inflight){spi_transaction_t* result;spi_device_get_trans_result(fastDev,&result,portMAX_DELAY);inflight--;}
  digitalWrite(LCD_CS,HIGH);spi_device_release_bus(fastDev);
}

void flushLogicalFrame(const uint16_t* fb) {
  if(!fb)return;
  if(fastDev){fastPush(fb);return;}
  // 标准刷屏和低内存路径也走相同缩放，不能把 466 像素直接写到 360 像素面板。
  static uint16_t* physical=nullptr;
  if(!physical)physical=(uint16_t*)ps_malloc(PANEL_WIDTH*PANEL_HEIGHT*2);
  if(physical) {
    for(int y=0;y<PANEL_HEIGHT;y++)for(int x=0;x<PANEL_WIDTH;x++)physical[y*PANEL_WIDTH+x]=portScale.pixel(fb,x,y,screenRot,rotFree,rotCos,rotSin);
    panel->draw16bitRGBBitmap(0,0,physical,PANEL_WIDTH,PANEL_HEIGHT);
  }else {
    uint16_t row[PANEL_WIDTH];
    for(int y=0;y<PANEL_HEIGHT;y++) {
      for(int x=0;x<PANEL_WIDTH;x++)row[x]=portScale.pixel(fb,x,y,screenRot,rotFree,rotCos,rotSin);
      panel->draw16bitRGBBitmap(0,y,row,PANEL_WIDTH,1);
    }
  }
}

void flushTask(void *arg) {
  for (;;) {
    xSemaphoreTake(flushReq, portMAX_DELAY);
    uint32_t t0 = millis();
    flushLogicalFrame(flushSrc);
    flushMsAcc += millis() - t0; flushCnt++;
    xSemaphoreGive(flushDone);
  }
}

// v2.6 等刷屏完成；超过 3 秒还没完成，说明快速刷屏在这块板子上不工作：关掉它并重启
void flushWait() {
  if (xSemaphoreTake(flushDone, pdMS_TO_TICKS(3000)) == pdTRUE) return;
  if (fastFlushOn) saveAndRestart("fastf", 0, true);
  xSemaphoreTake(flushDone, portMAX_DELAY);
}

// 异步刷屏：把刚画好的这一块交给核心 0 推到屏幕，画布换到另一块继续画下一帧
void flushAsync() {
  if (!fbBuf[1]) { gfx->flush(); return; }
  flushWait();                                    // 等上一帧推完
  flushSrc = fbBuf[fbCur];
  xSemaphoreGive(flushReq);
  fbCur ^= 1;
  gfx->setFramebuffer(fbBuf[fbCur]);
}

// 同步刷屏（普通画面用，等推完再返回）
void flushSync() {
  if (!fbBuf[1]) { gfx->flush(); return; }
  flushWait();
  flushSrc = fbBuf[fbCur];
  xSemaphoreGive(flushReq);
  flushWait();                                    // 等这一帧推完
  xSemaphoreGive(flushDone);
}

void initDoubleBuffer() {
  fbBuf[0] = gfx->getFramebuffer();
  fbBuf[1] = (uint16_t *)heap_caps_aligned_alloc(16, LCD_WIDTH * LCD_HEIGHT * 2, MALLOC_CAP_SPIRAM);
  if (!fbBuf[1]) return;
  memcpy(fbBuf[1], fbBuf[0], LCD_WIDTH * LCD_HEIGHT * 2);
  if (fastFlushOn) fastFlushInit();                // v2.6 快速刷屏
  flushReq = xSemaphoreCreateBinary();
  flushDone = xSemaphoreCreateBinary();
  xSemaphoreGive(flushDone);
  xTaskCreatePinnedToCore(flushTask, "flush", 4096, nullptr, 2, nullptr, 0);
}

// ================= 基础画图工具 =================

int16_t rnd(float v) { return (int16_t)lroundf(v); }

// ================= v1.8.1 抗锯齿矢量图标 =================
// 画大图标时先把“线段 / 三角形 / 圆”记录下来，再逐像素按到图形边缘的距离算覆盖率，
// 边缘自然过渡（抗锯齿），线段之间的连接也没有缝。结果缓存起来，图标不变就直接复制。
#define PRIM_MAX   192      // v3.4 车道也用这套（多条车道图形较多）
#define ICON_BUF_W 360
#define ICON_BUF_H 360
struct Prim {
  uint8_t type;            // 0 = 线段（起点平头、终点圆头），1 = 三角形，2 = 圆
  uint16_t col;
  float ax, ay, bx, by, cx, cy, r;
  float x0, y0, x1, y1;    // 外框
};
Prim prims[PRIM_MAX];
int primN = 0;
bool recIcon = false;      // true = 正在记录图标，thickLine/arrowHead/iconCircle 只记录不直接画
uint16_t *iconBuf = nullptr;
int iconX = 0, iconY = 0, iconW = 0, iconH = 0;
uint32_t iconHash = 0;

void addPrim(const struct Prim &p) { if (primN < PRIM_MAX) prims[primN++] = p; }

void iconBegin() { primN = 0; recIcon = true; }

static inline float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

// 一个像素中心 (px,py) 被图形覆盖的比例 0~1
float primAlpha(const struct Prim &p, float px, float py) {
  if (p.type == 0) {
    float dx = p.bx - p.ax, dy = p.by - p.ay;
    float len2 = dx * dx + dy * dy;
    float qx = px - p.ax, qy = py - p.ay;
    if (len2 < 1e-4f) return clamp01(p.r - sqrtf(qx * qx + qy * qy) + 0.5f);
    float h = (qx * dx + qy * dy) / len2;
    if (h < 0) {                                   // 起点：平头
      float len = sqrtf(len2);
      float perp = fabsf(qx * dy - qy * dx) / len;
      return clamp01(p.r - perp + 0.5f) * clamp01(0.5f + h * len);
    }
    if (h > 1) h = 1;                              // 终点：圆头
    float ex = qx - dx * h, ey = qy - dy * h;
    return clamp01(p.r - sqrtf(ex * ex + ey * ey) + 0.5f);
  }
  if (p.type == 1) {
    float vx[3] = {p.ax, p.bx, p.cx}, vy[3] = {p.ay, p.by, p.cy};
    float area = (vx[1] - vx[0]) * (vy[2] - vy[0]) - (vy[1] - vy[0]) * (vx[2] - vx[0]);
    float sgn = area > 0 ? 1.0f : -1.0f;
    float m = 1e9f;
    for (int i = 0; i < 3; i++) {
      int j = (i + 1) % 3;
      float ex = vx[j] - vx[i], ey = vy[j] - vy[i];
      float len = sqrtf(ex * ex + ey * ey);
      if (len < 1e-4f) return 0;
      float d = sgn * ((px - vx[i]) * ey - (py - vy[i]) * ex) / len;   // 到这条边的距离（里面为负）
      if (-d < m) m = -d;
    }
    return clamp01(m + 0.5f);
  }
  float qx = px - p.ax, qy = py - p.ay;
  return clamp01(p.r - sqrtf(qx * qx + qy * qy) + 0.5f);
}

bool iconHalo = false;              // v2.8 图标外加黑色描边（浮在地图上时用）
#define HALO_R 3

void iconRender() {
  float bx0 = 1e9f, by0 = 1e9f, bx1 = -1e9f, by1 = -1e9f;
  for (int i = 0; i < primN; i++) {
    bx0 = fminf(bx0, prims[i].x0); by0 = fminf(by0, prims[i].y0);
    bx1 = fmaxf(bx1, prims[i].x1); by1 = fmaxf(by1, prims[i].y1);
  }
  int mg = iconHalo ? HALO_R + 1 : 0;
  iconX = (int)floorf(bx0) - 1 - mg; iconY = (int)floorf(by0) - 1 - mg;
  if (iconX < 0) iconX = 0;
  if (iconY < 0) iconY = 0;
  iconW = (int)ceilf(bx1) + 2 + mg - iconX; iconH = (int)ceilf(by1) + 2 + mg - iconY;
  if (iconX + iconW > LCD_WIDTH)  iconW = LCD_WIDTH - iconX;
  if (iconY + iconH > LCD_HEIGHT) iconH = LCD_HEIGHT - iconY;
  if (iconW > ICON_BUF_W) iconW = ICON_BUF_W;
  if (iconH > ICON_BUF_H) iconH = ICON_BUF_H;
  if (iconW <= 0 || iconH <= 0) { iconW = iconH = 0; return; }
  for (int y = 0; y < iconH; y++) {
    float py = iconY + y + 0.5f;
    for (int x = 0; x < iconW; x++) {
      float px = iconX + x + 0.5f;
      float r = 0, g = 0, b = 0;                  // 背景是黑色，按顺序叠加每个图形
      for (int i = 0; i < primN; i++) {
        const Prim &p = prims[i];
        if (px < p.x0 || px > p.x1 || py < p.y0 || py > p.y1) continue;
        float a = primAlpha(p, px, py);
        if (a <= 0) continue;
        float cr = ((p.col >> 11) & 0x1F) / 31.0f, cg = ((p.col >> 5) & 0x3F) / 63.0f, cb = (p.col & 0x1F) / 31.0f;
        r += (cr - r) * a; g += (cg - g) * a; b += (cb - b) * a;
      }
      iconBuf[y * ICON_BUF_W + x] = ((uint16_t)lroundf(r * 31) << 11) | ((uint16_t)lroundf(g * 63) << 5) | (uint16_t)lroundf(b * 31);
    }
  }
  // v2.8 描边：图形周围 HALO_R 像素以内的空白处涂成接近黑色（纯黑 0 表示透明，所以用 0x0841）
  if (iconHalo) {
    for (int y = 0; y < iconH; y++) {
      for (int x = 0; x < iconW; x++) {
        if (iconBuf[y * ICON_BUF_W + x]) continue;
        bool near = false;
        for (int dy = -HALO_R; dy <= HALO_R && !near; dy++) {
          int yy = y + dy;
          if (yy < 0 || yy >= iconH) continue;
          for (int dx = -HALO_R; dx <= HALO_R; dx++) {
            int xx = x + dx;
            if (xx < 0 || xx >= iconW || dx * dx + dy * dy > HALO_R * HALO_R + 1) continue;
            uint16_t v = iconBuf[yy * ICON_BUF_W + xx];
            if (v && v != 0x0841) { near = true; break; }
          }
        }
        if (near) iconBuf[y * ICON_BUF_W + x] = 0x0841;
      }
    }
  }
}

// v1.8.2：把记录好的图标整体缩放、居中到 [top, bottom] 区域里（只缩小不放大）
void iconFitAt(float cx, float top, float bottom, float maxW) {
  if (primN == 0) return;
  float bx0 = 1e9f, by0 = 1e9f, bx1 = -1e9f, by1 = -1e9f;
  for (int i = 0; i < primN; i++) {
    bx0 = fminf(bx0, prims[i].x0 + 1); by0 = fminf(by0, prims[i].y0 + 1);
    bx1 = fmaxf(bx1, prims[i].x1 - 1); by1 = fmaxf(by1, prims[i].y1 - 1);
  }
  float w = bx1 - bx0, h = by1 - by0;
  if (w <= 0 || h <= 0) return;
  float sc = fminf(1.0f, fminf((bottom - top) / h, maxW / w));
  float ox = (bx0 + bx1) / 2, oy = (by0 + by1) / 2;
  float nx = cx, ny = (top + bottom) / 2;
  for (int i = 0; i < primN; i++) {
    Prim &p = prims[i];
    p.ax = nx + (p.ax - ox) * sc; p.ay = ny + (p.ay - oy) * sc;
    p.bx = nx + (p.bx - ox) * sc; p.by = ny + (p.by - oy) * sc;
    p.cx = nx + (p.cx - ox) * sc; p.cy = ny + (p.cy - oy) * sc;
    p.r *= sc;
    p.x0 = nx + (p.x0 + 1 - ox) * sc - 1; p.y0 = ny + (p.y0 + 1 - oy) * sc - 1;
    p.x1 = nx + (p.x1 - 1 - ox) * sc + 1; p.y1 = ny + (p.y1 - 1 - oy) * sc + 1;
  }
}

void iconFit(float top, float bottom, float maxW) { iconFitAt(CX, top, bottom, maxW); }

// 结束记录：图标和上次一样就直接用缓存，否则重新计算；然后复制到画布
void iconEnd() {
  recIcon = false;
  if (!iconBuf || primN == 0) return;
  uint32_t h = 2166136261u;                      // FNV 哈希，判断图标有没有变化
  const uint8_t *bytes = (const uint8_t *)prims;
  for (size_t i = 0; i < sizeof(Prim) * primN; i++) { h ^= bytes[i]; h *= 16777619u; }
  h ^= primN;
  h ^= iconHalo ? 0x5A5A5A5Au : 0;
  if (h != iconHash) { iconRender(); iconHash = h; }
  uint16_t *fb = gfx->getFramebuffer();
  if (!fb) return;
  for (int y = 0; y < iconH; y++) {
    const uint16_t *src = iconBuf + y * ICON_BUF_W;
    uint16_t *dst = fb + (iconY + y) * LCD_WIDTH + iconX;
    for (int x = 0; x < iconW; x++) if (src[x]) dst[x] = src[x];
  }
}

void iconCircle(float x, float y, float r, uint16_t c) {
  if (recIcon) {
    Prim p; memset(&p, 0, sizeof(p));
    p.type = 2; p.col = c; p.ax = x; p.ay = y; p.r = r;
    p.x0 = x - r - 1; p.y0 = y - r - 1; p.x1 = x + r + 1; p.y1 = y + r + 1;
    addPrim(p);
    return;
  }
  gfx->fillCircle(x, y, r, c);
}

// 有宽度的线（终点带圆头）
void thickLine(float x0, float y0, float x1, float y1, float w, uint16_t c) {
  if (recIcon) {
    Prim p; memset(&p, 0, sizeof(p));
    float r = w / 2;
    p.type = 0; p.col = c; p.ax = x0; p.ay = y0; p.bx = x1; p.by = y1; p.r = r;
    p.x0 = fminf(x0, x1) - r - 1; p.y0 = fminf(y0, y1) - r - 1;
    p.x1 = fmaxf(x0, x1) + r + 1; p.y1 = fmaxf(y0, y1) + r + 1;
    addPrim(p);
    return;
  }
  float dx = x1 - x0, dy = y1 - y0;
  float len = sqrtf(dx * dx + dy * dy);
  if (len < 1) return;
  float nx = -dy / len * w / 2, ny = dx / len * w / 2;
  gfx->fillTriangle(x0 + nx, y0 + ny, x0 - nx, y0 - ny, x1 + nx, y1 + ny, c);
  gfx->fillTriangle(x0 - nx, y0 - ny, x1 - nx, y1 - ny, x1 + nx, y1 + ny, c);
  gfx->fillCircle(x1, y1, w / 2, c);
}

// 箭头头部；deg：0 = 朝上，-90 = 朝左，90 = 朝右，180 = 朝下
void arrowHead(float x, float y, float deg, float s, uint16_t c) {
  float a = deg * PI / 180;
  float dx = sinf(a), dy = -cosf(a);
  float px = cosf(a), py = sinf(a);
  if (recIcon) {
    Prim p; memset(&p, 0, sizeof(p));
    p.type = 1; p.col = c;
    p.ax = x + dx * s;           p.ay = y + dy * s;
    p.bx = x + px * s * 0.8f;    p.by = y + py * s * 0.8f;
    p.cx = x - px * s * 0.8f;    p.cy = y - py * s * 0.8f;
    p.x0 = fminf(fminf(p.ax, p.bx), p.cx) - 1; p.y0 = fminf(fminf(p.ay, p.by), p.cy) - 1;
    p.x1 = fmaxf(fmaxf(p.ax, p.bx), p.cx) + 1; p.y1 = fmaxf(fmaxf(p.ay, p.by), p.cy) + 1;
    addPrim(p);
    return;
  }
  gfx->fillTriangle(x + dx * s, y + dy * s,
                    x + px * s * 0.8f, y + py * s * 0.8f,
                    x - px * s * 0.8f, y - py * s * 0.8f, c);
}

float min4(float a, float b, float c, float d) { return fminf(fminf(a, b), fminf(c, d)); }
float max4(float a, float b, float c, float d) { return fmaxf(fmaxf(a, b), fmaxf(c, d)); }

// 居中打印一行字（自带字体每个字符宽 6*size 像素）
void printCentered(const String &s, int x, int y, int size, uint16_t color) {
  gfx->setTextSize(size);
  gfx->setTextColor(color);
  gfx->setCursor(x - (int)s.length() * 6 * size / 2, y);
  gfx->print(s);
}

String distText(int m) {
  return (m >= 1000) ? String(m / 1000.0, 1) + "km" : String(m) + "m";
}

// 画一条“先竖直向上、再圆滑地拐向 deg 方向”的粗线。
// (ox, oy) 是拐角；stem = 拐角以下竖直段长度；arm = 拐出段长度；fillet = 圆角半径。
void bentStroke(float ox, float oy, float stem, float arm, float deg, float fillet, float w,
                uint16_t stemCol, uint16_t armCol, float &endX, float &endY, float maxT) {
  float a = deg * PI / 180;
  float dx = sinf(a), dy = -cosf(a);
  float t = fillet * tanf(fabsf(a) / 2);
  if (fabsf(deg) > 3) {                     // v1.8.1：小角度转弯也给一段明显的圆弧，不要折角
    float minT = fminf(fillet * 0.55f, maxT);
    if (t < minT) t = minT;
  }
  if (t > maxT)        t = maxT;          // v1.8：锐角弯时圆角不要太大
  if (t > arm * 0.7f)  t = arm * 0.7f;
  if (t > stem * 0.7f) t = stem * 0.7f;
  float p1x = ox,          p1y = oy + t;
  float p2x = ox + dx * t, p2y = oy + dy * t;
  endX = ox + dx * arm;
  endY = oy + dy * arm;
  if (t < 0.5f && fabsf(deg) < 1 && stemCol == armCol) {   // 直行：一整根，不留接缝
    thickLine(ox, oy + stem, endX, endY, w, stemCol);
    return;
  }
  float lx = p1x, ly = p1y;
  const int N = 12;
  for (int i = 1; i <= N; i++) {
    float u = (float)i / N, v = 1 - u;
    float x = v * v * p1x + 2 * v * u * ox + u * u * p2x;
    float y = v * v * p1y + 2 * v * u * oy + u * u * p2y;
    thickLine(lx, ly, x, y, w, armCol);
    lx = x; ly = y;
  }
  thickLine(p2x, p2y, endX, endY, w, armCol);
  thickLine(ox, oy + stem, p1x, p1y, w, stemCol);
}

// ================= 大图标（中、近阶段），k = 缩放比例，cy = 中心 y =================

void drawTurn(float deg, uint16_t col, float cy, float k) {
  // v1.8：转角超过 90 度（锐角弯）时，竖段和拐出段加长、线条稍细、圆角变小，
  //       让箭头和竖线分得开，看起来是清楚的锐角
  float sv = constrain((fabsf(deg) - 90) / 60, 0.0f, 1.0f);
  const float W = (42 - 6 * sv) * k, STEM = (86 + 40 * sv) * k, ARM = (78 + 30 * sv) * k, HEAD = (62 - 6 * sv) * k;
  float a = deg * PI / 180;
  float dx = sinf(a), dy = -cosf(a);
  float px = cosf(a), py = sinf(a);
  float ex = dx * ARM, ey = dy * ARM;
  float tx = ex + dx * HEAD, ty = ey + dy * HEAD;
  float c1x = ex + px * HEAD * 0.8f, c1y = ey + py * HEAD * 0.8f;
  float c2x = ex - px * HEAD * 0.8f, c2y = ey - py * HEAD * 0.8f;
  float minX = fminf(min4(-W / 2, ex - W / 2, tx, c1x), c2x);
  float maxX = fmaxf(max4(W / 2, ex + W / 2, tx, c1x), c2x);
  float minY = fminf(min4(-W / 2, ey - W / 2, ty, c1y), c2y);
  float maxY = fmaxf(max4(STEM, ey + W / 2, ty, c1y), c2y);     // v1.8：锐角弯箭头朝下，也要算进外框
  float ox = CX - (minX + maxX) / 2;
  float oy = cy - (minY + maxY) / 2;
  float endX, endY;
  bentStroke(ox, oy, STEM, ARM, deg, 40 * k, W, col, col, endX, endY, (40 - 28 * sv) * k);
  arrowHead(endX, endY, deg, HEAD, col);
}

void drawUturn(uint16_t col, float cy, float k) {
  const float W = 42 * k, R = 54 * k, LEG = 130 * k, DOWN = 44 * k, HEAD = 50 * k;
  float minX = -R - HEAD * 0.8f, maxX = R + W / 2;
  float minY = -R - W / 2,       maxY = fmaxf(LEG, DOWN + HEAD);
  float ox = CX - (minX + maxX) / 2;
  float oy = cy - (minY + maxY) / 2;
  thickLine(ox + R, oy + LEG, ox + R, oy, W, col);
  float lx = ox + R, ly = oy;
  for (int i = 1; i <= 16; i++) {
    float t = PI * i / 16;
    float x = ox + R * cosf(t), y = oy - R * sinf(t);
    thickLine(lx, ly, x, y, W, col);
    lx = x; ly = y;
  }
  thickLine(ox - R, oy, ox - R, oy + DOWN, W, col);
  arrowHead(ox - R, oy + DOWN, 180, HEAD, col);
}

void drawArrive(uint16_t col, float cy, float k) {
  iconCircle(CX, cy, 88 * k, col);
  iconCircle(CX, cy, 43 * k, C_BLACK);
  iconCircle(CX, cy, 20 * k, col);
}

bool dirAngle(const String &dir, float &deg) {
  // v1.8：有实际角度、并且左右方向一致时，用实际角度
  if (!isnan(navAngle) && (dir == "L" || dir == "R" || dir == "SL" || dir == "SR")) {
    bool left = (dir == "L" || dir == "SL");
    if ((left && navAngle < 0) || (!left && navAngle > 0)) {
      deg = constrain(navAngle, -150.0f, 150.0f);
      return true;
    }
  }
  if (dir == "L")  { deg = -90; return true; }
  if (dir == "R")  { deg = 90;  return true; }
  if (dir == "SL") { deg = -45; return true; }
  if (dir == "SR") { deg = 45;  return true; }
  return false;
}

// 大图标；laneK = 有车道时的额外缩小比例
void drawBigIconAt(const String &dir, uint16_t col, float cy, float laneK,
                   float cx, float top, float bottom, float maxW) {
  float deg;
  iconBegin();
  if (dir == "U")              drawUturn(col, cy, 0.70f * laneK);
  else if (dir == "A")         drawArrive(col == C_WHITE ? C_GREEN : col, cy, 0.78f * laneK);
  else if (dirAngle(dir, deg)) drawTurn(deg, col, cy, 0.78f * laneK);
  else                         drawTurn(0, col, cy, 0.62f * laneK);   // 直行
  iconFitAt(cx, top, bottom, maxW);
  iconEnd();
}

void drawBigIcon(const String &dir, uint16_t col, float cy, float laneK, float top, float bottom) {
  drawBigIconAt(dir, col, cy, laneK, CX, top, bottom, ICON_MAX_W);
}

// ================= 远阶段预告：白色直行竖条（居中）+ 灰色转向预告 =================

void drawPreviewTurn(float deg, float cy, float k) {
  float sv = constrain((fabsf(deg) - 90) / 60, 0.0f, 1.0f);   // v1.8 锐角弯：拐出段加长
  const float W = 30 * k, STEM = 175 * k, ARM = (52 + 25 * sv) * k, HEAD = 42 * k;
  float a = deg * PI / 180;
  float dy = -cosf(a), py = sinf(a);
  float ey = dy * ARM;
  float ty = ey + dy * HEAD;
  float c1y = ey + py * HEAD * 0.8f, c2y = ey - py * HEAD * 0.8f;
  float minY = fminf(min4(-W / 2, ey - W / 2, ty, c1y), c2y);
  float maxY = fmaxf(max4(STEM, ey + W / 2, ty, c1y), c2y);
  float oy = cy - (minY + maxY) / 2;
  float endX, endY;
  bentStroke(CX, oy, STEM, ARM, deg, 26 * k, W, C_WHITE, C_DIM, endX, endY, (26 - 16 * sv) * k);
  arrowHead(endX, endY, deg, HEAD, C_DIM);
}

void drawPreviewUturn(float cy, float k) {
  const float W = 30 * k, STEM = 175 * k, R = 30 * k, DOWN = 26 * k, HEAD = 42 * k;
  float oy = cy - ((-R - W / 2) + STEM) / 2;
  float ox = CX;
  float lx = ox, ly = oy;
  for (int i = 1; i <= 12; i++) {
    float t = PI * i / 12;
    float x = ox - R + R * cosf(t), y = oy - R * sinf(t);
    thickLine(lx, ly, x, y, W, C_DIM);
    lx = x; ly = y;
  }
  thickLine(ox - 2 * R, oy, ox - 2 * R, oy + DOWN, W, C_DIM);
  arrowHead(ox - 2 * R, oy + DOWN, 180, HEAD * 0.8f, C_DIM);
  thickLine(ox, oy + STEM, ox, oy, W, C_WHITE);
}

void drawPreviewArrive(float cy, float k) {
  const float W = 30 * k, STEM = 175 * k, TR = 32 * k;
  float oy = cy - ((-2 * TR - 8 * k) + STEM) / 2;
  thickLine(CX, oy + STEM, CX, oy, W, C_WHITE);
  float ty = oy - TR - 8 * k;
  iconCircle(CX, ty, TR, C_DIM);
  iconCircle(CX, ty, TR / 2, C_BLACK);
  iconCircle(CX, ty, TR / 5, C_DIM);
}

void drawPreview(const String &dir, float cy, float k, float top, float bottom) {
  float deg;
  iconBegin();
  if (dir == "U")              drawPreviewUturn(cy, k);
  else if (dir == "A")         drawPreviewArrive(cy, k);
  else if (dirAngle(dir, deg)) drawPreviewTurn(deg, cy, k);
  iconFit(top, bottom, ICON_MAX_W);
  iconEnd();
}

// ================= 圆环 =================

// 填充一段圆环：p0~p1 表示位置（0 = 正上方，顺时针，1 = 一整圈），每 0.5° 两个三角形
// v1.8.3 抗锯齿圆环：开机时把半径 RLUT_MIN~RLUT_MAX 之间每个像素的“半径、角度”算好存表，
// 按整数半径分桶；画环时只遍历需要的那几圈像素，按到边缘的距离算覆盖率，和底色混合。
#define RLUT_MIN 212
#define RLUT_MAX 236
#define RLUT_N   (RLUT_MAX - RLUT_MIN)
struct RingPix { uint32_t idx; uint16_t ang; uint16_t r8; };   // 像素位置、角度（0~65535 = 一圈）、半径×256
RingPix *ringLut = nullptr;
int ringOff[RLUT_N + 1];

void buildRingLut() {
  int cnt[RLUT_N] = {0};
  for (int pass = 0; pass < 2; pass++) {
    if (pass == 1) {
      int total = 0;
      for (int b = 0; b < RLUT_N; b++) { ringOff[b] = total; total += cnt[b]; cnt[b] = 0; }
      ringOff[RLUT_N] = total;
      ringLut = (RingPix *)ps_malloc(sizeof(RingPix) * (total + 1));
      if (!ringLut) return;
    }
    for (int y = 0; y < LCD_HEIGHT; y++) {
      float dy = y + 0.5f - CX;
      for (int x = 0; x < LCD_WIDTH; x++) {
        float dx = x + 0.5f - CX;
        float r = sqrtf(dx * dx + dy * dy);
        int b = (int)floorf(r) - RLUT_MIN;
        if (b < 0 || b >= RLUT_N) continue;
        if (pass == 0) { cnt[b]++; continue; }
        float a = atan2f(dx, -dy) / (2 * PI);                     // 0 = 正上方，顺时针
        if (a < 0) a += 1;
        RingPix &e = ringLut[ringOff[b] + cnt[b]++];
        e.idx = y * LCD_WIDTH + x;
        e.ang = (uint16_t)fminf(a * 65536.0f, 65535.0f);
        e.r8 = (uint16_t)(r * 256);
      }
    }
  }
}

// 两个 RGB565 颜色按 t（0~255）混合
static inline uint16_t blend565(uint16_t a, uint16_t b, int t) {
  int it = 255 - t;
  int r = (((a >> 11) & 0x1F) * it + ((b >> 11) & 0x1F) * t + 127) / 255;
  int g = (((a >> 5) & 0x3F) * it + ((b >> 5) & 0x3F) * t + 127) / 255;
  int bl = ((a & 0x1F) * it + (b & 0x1F) * t + 127) / 255;
  return (r << 11) | (g << 5) | bl;
}

// 抗锯齿实心圆（和已有内容混合）
void aaDisc(float cx, float cy, float rad, uint16_t c) {
  uint16_t *fb = gfx->getFramebuffer();
  if (!fb) { gfx->fillCircle(cx, cy, rad, c); return; }
  int x0 = max(0, (int)floorf(cx - rad - 1)), x1 = min(LCD_WIDTH - 1, (int)ceilf(cx + rad + 1));
  int y0 = max(0, (int)floorf(cy - rad - 1)), y1 = min(LCD_HEIGHT - 1, (int)ceilf(cy + rad + 1));
  for (int y = y0; y <= y1; y++) {
    float dy = y + 0.5f - cy;
    for (int x = x0; x <= x1; x++) {
      float dx = x + 0.5f - cx;
      float a = rad - sqrtf(dx * dx + dy * dy) + 0.5f;
      if (a <= 0) continue;
      int t = a >= 1 ? 255 : (int)(a * 255);
      uint16_t &px = fb[y * LCD_WIDTH + x];
      px = (t == 255) ? c : blend565(px, c, t);
    }
  }
}

// 填充一段圆环：p0~p1 表示位置（0 = 正上方，顺时针，1 = 一整圈），rIn~rOut 为内外半径
// v2.6 整圈圆环：按行填充（中间实心部分直接写，只有边缘一两个像素算覆盖率）
void fillAnnulus(float rIn, float rOut, uint16_t c) {
  uint16_t *fb = gfx->getFramebuffer();
  if (!fb) return;
  int y0 = max(0, (int)floorf(CX - rOut - 1)), y1 = min(LCD_HEIGHT - 1, (int)ceilf(CX + rOut + 1));
  float ro2 = (rOut + 0.5f) * (rOut + 0.5f), ri2 = (rIn - 0.5f) * (rIn - 0.5f);
  float so2 = (rOut - 0.5f) * (rOut - 0.5f), si2 = (rIn + 0.5f) * (rIn + 0.5f);
  for (int y = y0; y <= y1; y++) {
    float dy = y + 0.5f - CX, dy2 = dy * dy;
    if (dy2 >= ro2) continue;
    float ho = sqrtf(ro2 - dy2);                            // 有覆盖的外沿
    float hi = dy2 < ri2 ? sqrtf(ri2 - dy2) : -1;           // 有覆盖的内沿（空心）
    float so = dy2 < so2 ? sqrtf(so2 - dy2) : -1;           // 实心外沿
    float si = dy2 < si2 ? sqrtf(si2 - dy2) : 0;            // 实心内沿
    uint16_t *row = fb + y * LCD_WIDTH;
    for (int side = 0; side < 2; side++) {
      int xa, xb;
      if (hi < 0) {                                          // 这一行没有空心：一整段
        if (side) break;
        xa = (int)floorf(CX - ho); xb = (int)ceilf(CX + ho);
      } else if (side == 0) {
        xa = (int)floorf(CX - ho); xb = (int)ceilf(CX - hi);
      } else {
        xa = (int)floorf(CX + hi); xb = (int)ceilf(CX + ho);
      }
      xa = max(xa, 0); xb = min(xb, LCD_WIDTH - 1);
      for (int x = xa; x <= xb; x++) {
        float dx = fabsf(x + 0.5f - CX);
        if (so >= 0 && dx >= si && dx <= so) { row[x] = c; continue; }
        float r = sqrtf(dx * dx + dy2);
        float a = fminf(r - rIn + 0.5f, rOut - r + 0.5f);
        if (a <= 0) continue;
        if (a >= 1) { row[x] = c; continue; }
        row[x] = blend565(row[x], c, (int)(a * 255));
      }
    }
  }
}

void fillRingBand(float p0, float p1, float rIn, float rOut, uint16_t c) {
  if (p1 <= p0) return;
  if (p1 - p0 >= 0.9999f) { fillAnnulus(rIn, rOut, c); return; }   // v2.6 整圈走快速路径
  uint16_t *fb = gfx->getFramebuffer();
  if (!ringLut || !fb) return;
  bool full = (p1 - p0) >= 0.9999f;
  int b0 = (int)floorf(rIn - 1) - RLUT_MIN, b1 = (int)ceilf(rOut + 1) - RLUT_MIN;
  if (b0 < 0) b0 = 0;
  if (b1 > RLUT_N) b1 = RLUT_N;
  if (b0 >= b1) return;
  const float TWO_PI_F = 2 * PI;
  for (int i = ringOff[b0]; i < ringOff[b1]; i++) {
    const RingPix &e = ringLut[i];
    float r = e.r8 * (1.0f / 256);
    float ar = fminf(r - rIn + 0.5f, rOut - r + 0.5f);
    if (ar <= 0) continue;
    if (ar > 1) ar = 1;
    if (!full) {
      float a = e.ang * (1.0f / 65536);
      float aa = fminf((a - p0) * TWO_PI_F * r, (p1 - a) * TWO_PI_F * r) + 0.5f;   // 到两端的弧长（像素）
      if (aa <= 0) continue;
      if (aa < 1) ar *= aa;
    }
    int t = (int)(ar * 255);
    if (t <= 0) continue;
    uint16_t &px = fb[e.idx];
    px = (t >= 255) ? c : blend565(px, c, t);
  }
}

void drawRingTrack() { fillRingBand(0, 1, RING_R - 5, RING_R + 5, C_TRACK); }

void drawRingProgress(float p, int thick, uint16_t col) {
  if (p <= 0.002f) return;
  if (p > 1) p = 1;
  float h = thick / 2.0f;
  fillRingBand(0, p, RING_R - h, RING_R + h, col);
  float a = p * 2 * PI;
  // 两端圆头（像素中心在 +0.5 处，所以圆心也取 CX 而不是 CX-0.5）
  aaDisc(CX, CX - RING_R, h, col);
  aaDisc(CX + RING_R * sinf(a), CX - RING_R * cosf(a), h, col);
}

// v7.3 抗锯齿圆弧带：角度 a0 → a1（度，0 = 正上方，顺时针；跨度要小于 180°），半径 rIn~rOut，alpha 0~255
void arcBand(float a0, float a1, float rIn, float rOut, uint16_t c, int alpha = 255) {
  uint16_t *fb = gfx->getFramebuffer();
  if (!fb || a1 <= a0) return;
  float s0 = sinf(a0 * PI / 180), c0 = cosf(a0 * PI / 180);
  float s1 = sinf(a1 * PI / 180), c1 = cosf(a1 * PI / 180);
  float ro2 = (rOut + 1) * (rOut + 1), ri2 = rIn > 1 ? (rIn - 1) * (rIn - 1) : 0;
  int y0 = max(0, (int)(CX - rOut - 1)), y1 = min(LCD_HEIGHT - 1, (int)(CX + rOut + 1));
  for (int y = y0; y <= y1; y++) {
    float dy = y + 0.5f - CX, dy2 = dy * dy;
    if (dy2 >= ro2) continue;
    float ho = sqrtf(ro2 - dy2), hi = dy2 < ri2 ? sqrtf(ri2 - dy2) : 0;
    uint16_t *row = fb + y * LCD_WIDTH;
    for (int side = 0; side < 2; side++) {
      int xa = side ? (int)(CX + hi) : (int)(CX - ho) - 1;
      int xb = side ? (int)(CX + ho) + 1 : (int)(CX - hi) + 1;
      xa = max(xa, 0); xb = min(xb, LCD_WIDTH - 1);
      for (int x = xa; x <= xb; x++) {
        float dx = x + 0.5f - CX;
        float e0 = s0 * dy + c0 * dx;                    // 到起点那条半径线的距离（在弧这边为正）
        float e1 = -(s1 * dy + c1 * dx);                 // 到终点那条半径线的距离（在弧这边为正）
        if (e0 < -0.5f || e1 < -0.5f) continue;
        float r = sqrtf(dx * dx + dy2);
        float a = fminf(fminf(r - rIn, rOut - r), fminf(e0, e1)) + 0.5f;
        if (a <= 0) continue;
        int t = a >= 1 ? alpha : (int)(a * alpha);
        if (t <= 0) continue;
        uint16_t &px = row[x];
        px = t >= 255 ? c : blend565(px, c, t);
      }
    }
  }
}

// v7.4 路程进度：0~1（走过的比例）；不显示时返回 -1
#define TRIP_DONE_C 0x5AEB          // 走过的：灰
#define TRIP_LEFT_C 0x26E8          // 没走的：绿 #22DD44
#define TRIP_H      2               // 行程环半宽（v7.5 整环 8 → 4 像素粗）
float tripFrac() {
  if (!progOn || !tripOn || cruiseOn || progLastRm < 0) return -1;
  long total = progDone + tripRemainM;
  if (total < 300) return -1;                            // 太短的路线不显示
  return tripProgress.value(millis());
}

// 行程环上“现在的位置”小白点
void drawTripDot(float p) {
  if (p <= 0.002f || p >= 0.998f) return;
  float a = p * 2 * PI;
  float x = CX + RING_R * sinf(a), y = CX - RING_R * cosf(a);
  aaDisc(x, y, 5.0f, C_BLACK);                           // v7.5 环变细了，小白点保持原来大小
  aaDisc(x, y, 3.5f, C_WHITE);
}

// 大箭头画面用：整圈行程环
void drawTripRing(float p) {
  fillRingBand(0, 1, RING_R - TRIP_H, RING_R + TRIP_H, TRIP_LEFT_C);
  if (p > 0.002f) fillRingBand(0, p, RING_R - TRIP_H, RING_R + TRIP_H, TRIP_DONE_C);
  drawTripDot(p);
}

// 屏幕边缘的红圈（断线、数据中断用）
void drawRedRing() {
  fillRingBand(0, 1, 222, 232, C_RED);      // v1.8.3 抗锯齿
}

// 顶部小圆点：绿 = 蓝牙已连接，灰 = 未连接
// ================= 1.85B 电量计 BQ27220（只读取，不改写充电或电池参数）=================
GaugeBQ27220 batteryGauge;
bool pmuOK=false;
int batPct=-1;
bool batChg=false,usbIn=false;
int batMv=0;
unsigned long batAt=0,batSentAt=0;
String pendingBat="";
void batInit() {
  pmuOK=batteryGauge.begin(Wire,IIC_SDA,IIC_SCL);
  Serial.println(pmuOK?"BQ27220 ok":"BQ27220 not found (navigation continues)");
}

// 小电池图标：x0,y0 = 左上角（电池身 24x12，右边小凸头，再右边是百分比数字）
void drawBattery(int x0, int y0, bool backing = false) {
  if (batPct < 0) return;                                  // 没接电池不画
  if (backing) {                                           // v7.2 小地图上垫一块黑底
    int tw = String(batPct).length() * 6;
    gfx->fillRoundRect(x0 - 4, y0 - 3, 24 + 2 + 5 + tw + 7, 12 + 6, 6, C_BLACK);
  }
  uint16_t c = batChg ? C_CYAN : batPct > 50 ? C_GREEN : batPct >= 20 ? 0xFE60 : C_RED;
  if (!batChg && batPct < 10 && (millis() / 600) % 2) c = 0x4208;   // 快没电：闪
  const int W = 24, H = 12;
  gfx->drawRoundRect(x0, y0, W, H, 3, c);
  gfx->drawRoundRect(x0 + 1, y0 + 1, W - 2, H - 2, 2, c);
  gfx->fillRect(x0 + W, y0 + 3, 2, H - 6, c);
  int fw = (W - 6) * constrain(batPct, 0, 100) / 100;
  if (fw > 0) gfx->fillRect(x0 + 3, y0 + 3, fw, H - 6, c);
  if (batChg) {                                            // 小闪电
    int bx = x0 + W / 2, by = y0 + H / 2;
    gfx->fillTriangle(bx + 1, by - 5, bx - 3, by + 1, bx, by + 1, C_BLACK);
    gfx->fillTriangle(bx - 1, by + 5, bx + 3, by - 1, bx, by - 1, C_BLACK);
  }
  gfx->setTextSize(1);
  gfx->setTextColor(c);
  gfx->setCursor(x0 + W + 5, y0 + 2);
  gfx->print(String(batPct));
}

void drawStatus() {
  gfx->fillRect(CX - 9, 21, 9 + 14 + 24 + 2 + 5 + 18 + 4, 19, C_BLACK);   // v7.2 先擦掉旧的（只重画状态栏时用）
  aaDisc(CX, 30, 7, bleConnected ? C_GREEN : C_GREY);
  drawBattery(CX + 14, 24);                               // v6.8 电量（蓝牙小圆点右边）
  needFlush = true;
}

// ================= 车速和限速牌 =================

void drawSpeedBlock(bool over) {
  if (speedKmh < 0) return;
  int sx = hasCam ? 200 : CX;
  printCentered(String(speedKmh), sx, SPEED_Y, 5, over ? C_RED : C_WHITE);   // 每个字符 30x40
  printCentered("km/h", sx, SPEED_Y + 46, 2, over ? 0xFC10 : C_GREY);
  if (hasCam) {
    const int bx = 298, by = 76;
    aaDisc(bx, by, 28, C_SIGNRED);
    aaDisc(bx, by, 21, C_WHITE);
    String lim = String(camLimit);
    int sz = lim.length() >= 3 ? 2 : 3;
    printCentered(lim, bx, by - 4 * sz, sz, C_BLACK);
    printCentered(distText(camDist), bx, by + 34, 2, over ? 0xFC10 : C_GREY);
  }
}

// ================= 车道 =================

// 高德车道方向码 → 方向位：1 直行，2 左转，4 右转，8 左掉头，16 右掉头
// 注意：这张对照表来自经验，可能需要实测校准（App 上会显示原始编码）
uint8_t laneFlags(uint8_t code) {
  switch (code) {
    case 0:  return 1;
    case 1:  return 2;
    case 2:  return 1 | 2;
    case 3:  return 4;
    case 4:  return 1 | 4;
    case 5:  return 8;
    case 6:  return 2 | 4;
    case 7:  return 1 | 2 | 4;
    case 8:  return 16;
    case 9:  return 1 | 8;
    case 10: return 1 | 16;
    case 11: return 2 | 8;
    case 12: return 4 | 16;
    default: return 1;
  }
}

// v3.4 车道图标改成抗锯齿：先把线段/三角形“记录”下来（和大箭头同一套），按每个像素的覆盖率
//      算好颜色和透明度存进车道缓存；车道不变就直接用缓存，每帧和地图混合（边缘平滑，拐角是圆弧）
#define LANE_BUF_W 320
#define LANE_BUF_H 44
uint16_t *laneCol = nullptr;
uint8_t *laneA = nullptr;
int laneBX = 0, laneBY = 0, laneBW = 0, laneBH = 0;
uint32_t laneHash = 0;

// 画一段圆弧（用几小段线连起来）；角度：0 = 朝右，90 = 朝下（屏幕坐标）
void arcLine(float ox, float oy, float r, float a0, float a1, float w, uint16_t col) {
  const int N = 4;
  float px = ox + r * cosf(a0 * PI / 180), py = oy + r * sinf(a0 * PI / 180);
  for (int i = 1; i <= N; i++) {
    float a = (a0 + (a1 - a0) * i / N) * PI / 180;
    float nx = ox + r * cosf(a), ny = oy + r * sinf(a);
    thickLine(px, py, nx, ny, w, col);
    px = nx; py = ny;
  }
}

// 在一条车道里画方向符号（只记录图形）；cx = 车道中心 x，y0 = 顶部，h = 高度
void drawLaneGlyph(float cx, float y0, float h, uint8_t flags, uint16_t col) {
  const float k = h / 32.0f;
  const float w = 4.6f * k, hs = 6.8f * k;
  const float base = y0 + h, top = y0 + 3 * k;
  if (flags & 1) {                                         // 直行
    thickLine(cx, base, cx, top + hs, w, col);
    arrowHead(cx, top + hs, 0, hs, col);
  }
  if (flags & (2 | 4)) {                                   // 左转/右转：竖线 → 圆弧 → 横线 → 箭头
    const float rr = 4.5f * k, arm = 9.0f * k;
    float cy = y0 + h * ((flags & 1) ? 0.6f : 0.42f);     // 和直行合在一起时拐点放低
    thickLine(cx, base, cx, cy + rr, w, col);
    if (flags & 4) {
      arcLine(cx + rr, cy + rr, rr, 180, 270, w, col);
      thickLine(cx + rr, cy, cx + arm, cy, w, col);
      arrowHead(cx + arm, cy, 90, hs, col);
    }
    if (flags & 2) {
      arcLine(cx - rr, cy + rr, rr, 0, -90, w, col);
      thickLine(cx - rr, cy, cx - arm, cy, w, col);
      arrowHead(cx - arm, cy, -90, hs, col);
    }
  }
  if (flags & (8 | 16)) {                                  // 掉头：竖线 → 半圆 → 向下 + 箭头
    const float u = 4.2f * k, ut = top + 6 * k + u, down = y0 + h * 0.62f;
    thickLine(cx, base, cx, ut, w, col);
    if (flags & 8) {
      arcLine(cx - u, ut, u, 0, -180, w, col);
      thickLine(cx - 2 * u, ut, cx - 2 * u, down - hs, w, col);
      arrowHead(cx - 2 * u, down - hs, 180, hs, col);
    }
    if (flags & 16) {
      arcLine(cx + u, ut, u, 180, 360, w, col);
      thickLine(cx + 2 * u, ut, cx + 2 * u, down - hs, w, col);
      arrowHead(cx + 2 * u, down - hs, 180, hs, col);
    }
  }
  if (!(flags & (1 | 2 | 4 | 8 | 16))) {                   // 没有方向位时画直行
    thickLine(cx, base, cx, top + hs, w, col);
    arrowHead(cx, top + hs, 0, hs, col);
  }
}

// 把记录好的车道图形算成“颜色 + 透明度”存进缓存
void laneRender() {
  float bx0 = 1e9f, by0 = 1e9f, bx1 = -1e9f, by1 = -1e9f;
  for (int i = 0; i < primN; i++) {
    bx0 = fminf(bx0, prims[i].x0); by0 = fminf(by0, prims[i].y0);
    bx1 = fmaxf(bx1, prims[i].x1); by1 = fmaxf(by1, prims[i].y1);
  }
  laneBX = max(0, (int)floorf(bx0)); laneBY = max(0, (int)floorf(by0));
  laneBW = min(LANE_BUF_W, (int)ceilf(bx1) + 1 - laneBX);
  laneBH = min(LANE_BUF_H, (int)ceilf(by1) + 1 - laneBY);
  if (laneBX + laneBW > LCD_WIDTH) laneBW = LCD_WIDTH - laneBX;
  if (laneBY + laneBH > LCD_HEIGHT) laneBH = LCD_HEIGHT - laneBY;
  if (laneBW <= 0 || laneBH <= 0) { laneBW = laneBH = 0; return; }
  for (int y = 0; y < laneBH; y++) {
    float py = laneBY + y + 0.5f;
    for (int x = 0; x < laneBW; x++) {
      float px = laneBX + x + 0.5f;
      float r = 0, g = 0, b = 0, A = 0;                  // 从透明开始，按顺序叠加每个图形
      for (int i = 0; i < primN; i++) {
        const Prim &p = prims[i];
        if (px < p.x0 || px > p.x1 || py < p.y0 || py > p.y1) continue;
        float a = primAlpha(p, px, py);
        if (a <= 0) continue;
        float cr = ((p.col >> 11) & 0x1F) / 31.0f, cg = ((p.col >> 5) & 0x3F) / 63.0f, cb = (p.col & 0x1F) / 31.0f;
        r += (cr - r) * a; g += (cg - g) * a; b += (cb - b) * a; A += (1 - A) * a;
      }
      int k = y * LANE_BUF_W + x;
      if (A < 0.01f) { laneA[k] = 0; continue; }
      r /= A; g /= A; b /= A;                              // 去掉透明度，得到纯颜色
      laneCol[k] = ((uint16_t)lroundf(fminf(r, 1) * 31) << 11) | ((uint16_t)lroundf(fminf(g, 1) * 63) << 5) | (uint16_t)lroundf(fminf(b, 1) * 31);
      laneA[k] = (uint8_t)lroundf(fminf(A, 1) * 255);
    }
  }
}

void drawLanes(int y0) { drawLanesAt(CX, y0, 300, 32); }

// v2.1 在指定位置画车道：cx = 中心 x，maxW = 最大总宽，h = 高度
void drawLanesAt(float centerX, float y0, float maxW, float h) {
  if (laneN <= 0) return;
  if (!laneCol) {
    laneCol = (uint16_t *)malloc(LANE_BUF_W * LANE_BUF_H * 2);
    laneA = (uint8_t *)malloc(LANE_BUF_W * LANE_BUF_H);
    if (!laneCol || !laneA) return;
  }
  float lw = fminf(36 * h / 32, maxW / laneN);
  float x0 = centerX - lw * laneN / 2;
  // 1. 记录所有车道的图形
  primN = 0; recIcon = true;
  for (int i = 0; i < laneN; i++) {
    float cx = x0 + lw * (i + 0.5f);
    bool rec = laneFr[i] != 0xF;
    drawLaneGlyph(cx, y0, h, laneFlags(laneBg[i]), rec ? C_SEP : C_DIM);   // 底层：这条车道能走的方向
    if (rec) drawLaneGlyph(cx, y0, h, laneFlags(laneFr[i]), C_WHITE);       // 推荐方向：白色
  }
  recIcon = false;
  // 2. 和上次一样就用缓存，否则重新计算
  uint32_t hs = 2166136261u;
  const uint8_t *bytes = (const uint8_t *)prims;
  for (size_t i = 0; i < sizeof(Prim) * primN; i++) { hs ^= bytes[i]; hs *= 16777619u; }
  hs ^= primN * 7919u;
  if (hs != laneHash) { laneRender(); laneHash = hs; }
  primN = 0;
  // 3. 和底下的画面按透明度混合
  uint16_t *fb = gfx->getFramebuffer();
  if (fb) {
    for (int y = 0; y < laneBH; y++) {
      uint16_t *dst = fb + (laneBY + y) * LCD_WIDTH + laneBX;
      const uint16_t *sc = laneCol + y * LANE_BUF_W;
      const uint8_t *sa = laneA + y * LANE_BUF_W;
      for (int x = 0; x < laneBW; x++) {
        uint8_t a = sa[x];
        if (!a) continue;
        dst[x] = a == 255 ? sc[x] : blend565(dst[x], sc[x], a);
      }
    }
  }
  // 4. 车道之间的虚线
  for (int i = 1; i < laneN; i++)
    for (float y = y0; y < y0 + h; y += 8) gfx->drawFastVLine(rnd(x0 + lw * i), rnd(y), 4, C_SEP);
}

// ================= 文字图片 =================

// 把 4 位灰度图画到 (中心 x, 顶部 y)，tint = 最亮处的灰度 0~255
// v1.7.1：如果图片超出半径 TEXT_R 的圆，就整体等比缩小（按面积取平均，缩小后依然平滑）
static inline uint8_t imgNib(const TextImg &im, int bpr, int x, int y) {
  uint8_t b = im.show[y * bpr + (x >> 1)];
  return (x & 1) ? (b & 0x0F) : (b >> 4);
}

static inline uint16_t greyColor(uint8_t a, uint8_t tint) {      // a = 0~15
  uint8_t g = (uint16_t)(a * 17) * tint / 255;
  return ((g >> 3) << 11) | ((g >> 2) << 5) | (g >> 3);
}

void drawTextImg(int slot, int y0, uint8_t tint) {
  TextImg &im = imgs[slot];
  if (!im.show || im.showW == 0 || im.showH == 0) return;
  int bpr = (im.showW + 1) / 2;

  // 算缩放比例：图片四个角都要落在半径 TEXT_R 的圆里
  float s = 1.0f;
  for (int it = 0; it < 4; it++) {
    float h = im.showH * s;
    float dy = fmaxf(fabsf(y0 - CX), fabsf(y0 + h - CX));
    float half = dy < TEXT_R ? sqrtf((float)TEXT_R * TEXT_R - dy * dy) : 0;
    if (im.showW * s / 2 <= half) break;
    s = 2 * half / im.showW;
  }
  if (s < 0.5f) s = 0.5f;

  if (s >= 0.999f) {                       // 不用缩小：原样画
    int x0 = CX - im.showW / 2;
    for (int y = 0; y < im.showH; y++)
      for (int x = 0; x < im.showW; x++) {
        uint8_t a = imgNib(im, bpr, x, y);
        if (a) gfx->drawPixel(x0 + x, y0 + y, greyColor(a, tint));
      }
    return;
  }

  int dw = (int)(im.showW * s), dh = (int)(im.showH * s);
  if (dw < 1 || dh < 1) return;
  int x0 = CX - dw / 2;
  float inv = 1.0f / s;
  for (int dy = 0; dy < dh; dy++) {
    int sy0 = (int)(dy * inv), sy1 = (int)((dy + 1) * inv);
    if (sy1 <= sy0) sy1 = sy0 + 1;
    if (sy1 > im.showH) sy1 = im.showH;
    for (int dx = 0; dx < dw; dx++) {
      int sx0 = (int)(dx * inv), sx1 = (int)((dx + 1) * inv);
      if (sx1 <= sx0) sx1 = sx0 + 1;
      if (sx1 > im.showW) sx1 = im.showW;
      int sum = 0, cnt = 0;
      for (int y = sy0; y < sy1; y++)
        for (int x = sx0; x < sx1; x++) { sum += imgNib(im, bpr, x, y); cnt++; }
      uint8_t a = cnt ? (sum + cnt / 2) / cnt : 0;
      if (a) gfx->drawPixel(x0 + dx, y0 + dy, greyColor(a, tint));
    }
  }
}

// 'B' slot id part parts wLo wHi h offLo offHi + 数据
void handleBitmap(const uint8_t *d, int n) {
  if (n < 10) return;
  int slot = d[1], id = d[2], part = d[3], parts = d[4];
  int w = d[5] | (d[6] << 8), h = d[7];
  int off = d[8] | (d[9] << 8);
  if (slot >= IMG_SLOTS) return;
  TextImg &im = imgs[slot];
  if (w == 0 || h == 0 || parts == 0) {          // 空图：清掉这一栏
    im.showW = 0; im.showH = 0; im.recvId = -1;
    navDirty = true;
    if (slot == 3 || slot == 4) wxDirty = true;
    if (slot == 5 || slot == 6 || slot == 12) musicDirty = true;
    return;
  }
  if (slot == 12 && (!musicLyrics || !hidMusicMode || !appleMedia.available ||
      lyricImageGeneration != appleMedia.generation || lyricImageTitleRevision != appleMedia.values[0].revision || lyricImageArtistRevision != appleMedia.values[1].revision)) return;
  int total = ((w + 1) / 2) * h;
  if (total > IMG_MAX || parts > 32) return;
  if (id != im.recvId) {                          // 新的一张图：重新开始接收
    im.recvId = id; im.recvW = w; im.recvH = h; im.recvParts = parts; im.recvMask = 0;
  }
  int len = n - 10;
  if (off + len > total) len = total - off;
  if (len > 0) memcpy(im.recv + off, d + 10, len);
  im.recvMask |= (1UL << part);
  uint32_t full = (parts >= 32) ? 0xFFFFFFFFUL : ((1UL << parts) - 1);
  if ((im.recvMask & full) == full) {             // 收齐了：换成显示用的图
    uint8_t *t = im.show; im.show = im.recv; im.recv = t;
    im.showW = im.recvW; im.showH = im.recvH;
    im.recvId = -1;
    navDirty = true;
    if (slot == 3 || slot == 4) wxDirty = true;
    if (slot == 5 || slot == 6 || slot == 12) musicDirty = true;                // 音乐文字图片收齐后刷新
  }
}

// ================= 整屏画面 =================

#define WAIT_READY  0
#define WAIT_NOLINK 1
#define WAIT_LOST   2

// ================= v4.6 首页像素骑手（代替 v3.6 的猫猫） =================
// 原创的像素女骑手 + 小摩托：连上手机时围巾和马尾随风飘、偶尔眨眼、每隔几秒挥挥手；
// 没连上时摩托车灯熄灭、她闭眼打瞌睡，头上飘 z
// v4.6 首页像素骑手（原创角色）：40×26，每格放大 RIDER_S 倍；由 rider2.py 生成
#define RIDER_W 40
#define RIDER_H 26
#define RIDER_FRAMES 7
// 帧：stand0、stand1、blink0、blink1、wave0、wave1、sleep
const char *const RIDER[RIDER_FRAMES][RIDER_H] = {
  { // stand0
    "........................................",
    "........KKKKK...........................",
    "......KKHHHHHKK.........................",
    ".....KHhhHHHHHHK........................",
    "....KHhHHHHHHHHHK.......................",
    "....KHVVVVVVVVVHK.......................",
    "....KVvvVVVVVVVVK.......................",
    "....KHKKKKKKKKKHK.......................",
    "....KRRRSSRSSRRRKK......................",
    "...KRRSSEESEESSRRKK.....................",
    "...KRRSSEESEESSRRKRK....................",
    "...KRRSBSSSSSBSRRKRK....................",
    "....KRRSSSKSSSRRK.RK.................KK.",
    ".....KRKKSSSKKRK..K.................KcK.",
    ".....KJJJFFFJFFK........KKKKKKK.....KcK.",
    "....KJJJJJFJJKFFK......KsssssssKKKKKMMK.",
    "...KJJKJJJFJJJKKJK.....KMMMMMMMMMMMMMMKK",
    "...KJjKJJJJJJJKjJK....KMMWMMMMMMMMMMMLLK",
    "...KGGKJJJJJJJKGGK....KMMMMMmmmmMMMMMLLK",
    "....KK.KPPPPPPK.KK....KmmMMmmmmmmmMMMMKK",
    ".......KPPPPPPK........KTTKcCCCCCcKKTTK.",
    ".......KPPKKPPK.......KTttTKcCKKCcKTttTK",
    ".......KPpKKpPK.......KtWWtKKccccKKtWWtK",
    ".......KPPKKPPK.......KtWWtK......KtWWtK",
    "......KbbbKKbbbK......KTttTK......KTttTK",
    "......KKKKKKKKKK.......KTTK........KTTK.",
  },
  { // stand1
    "........................................",
    "........KKKKK...........................",
    "......KKHHHHHKK.........................",
    ".....KHhhHHHHHHK........................",
    "....KHhHHHHHHHHHK.......................",
    "....KHVVVVVVVVVHK.......................",
    "....KVvvVVVVVVVVK.......................",
    "....KHKKKKKKKKKHK.......................",
    "....KRRRSSRSSRRRKKK.....................",
    "...KRRSSEESEESSRRKRK....................",
    "...KRRSSEESEESSRRKRRK...................",
    "...KRRSBSSSSSBSRRKRRK...................",
    "....KRRSSSKSSSRRK..KK................KK.",
    ".....KRKKSSSKKRK....................KcK.",
    ".....KJJJFFFJFFFK.......KKKKKKK.....KcK.",
    "....KJJJJJFJJKFFFK.....KsssssssKKKKKMMK.",
    "...KJJKJJJFJJJKKKK.....KMMMMMMMMMMMMMMKK",
    "...KJjKJJJJJJJKjJK....KMMWMMMMMMMMMMMLLK",
    "...KGGKJJJJJJJKGGK....KMMMMMmmmmMMMMMLLK",
    "....KK.KPPPPPPK.KK....KmmMMmmmmmmmMMMMKK",
    ".......KPPPPPPK........KTTKcCCCCCcKKTTK.",
    ".......KPPKKPPK.......KTttTKcCKKCcKTttTK",
    ".......KPpKKpPK.......KtWWtKKccccKKtWWtK",
    ".......KPPKKPPK.......KtWWtK......KtWWtK",
    "......KbbbKKbbbK......KTttTK......KTttTK",
    "......KKKKKKKKKK.......KTTK........KTTK.",
  },
  { // blink0
    "........................................",
    "........KKKKK...........................",
    "......KKHHHHHKK.........................",
    ".....KHhhHHHHHHK........................",
    "....KHhHHHHHHHHHK.......................",
    "....KHVVVVVVVVVHK.......................",
    "....KVvvVVVVVVVVK.......................",
    "....KHKKKKKKKKKHK.......................",
    "....KRRRSSRSSRRRKK......................",
    "...KRRSSSSSSSSSRRKK.....................",
    "...KRRSSKKSKKSSRRKRK....................",
    "...KRRSBSSSSSBSRRKRK....................",
    "....KRRSSSKSSSRRK.RK.................KK.",
    ".....KRKKSSSKKRK..K.................KcK.",
    ".....KJJJFFFJFFK........KKKKKKK.....KcK.",
    "....KJJJJJFJJKFFK......KsssssssKKKKKMMK.",
    "...KJJKJJJFJJJKKJK.....KMMMMMMMMMMMMMMKK",
    "...KJjKJJJJJJJKjJK....KMMWMMMMMMMMMMMLLK",
    "...KGGKJJJJJJJKGGK....KMMMMMmmmmMMMMMLLK",
    "....KK.KPPPPPPK.KK....KmmMMmmmmmmmMMMMKK",
    ".......KPPPPPPK........KTTKcCCCCCcKKTTK.",
    ".......KPPKKPPK.......KTttTKcCKKCcKTttTK",
    ".......KPpKKpPK.......KtWWtKKccccKKtWWtK",
    ".......KPPKKPPK.......KtWWtK......KtWWtK",
    "......KbbbKKbbbK......KTttTK......KTttTK",
    "......KKKKKKKKKK.......KTTK........KTTK.",
  },
  { // blink1
    "........................................",
    "........KKKKK...........................",
    "......KKHHHHHKK.........................",
    ".....KHhhHHHHHHK........................",
    "....KHhHHHHHHHHHK.......................",
    "....KHVVVVVVVVVHK.......................",
    "....KVvvVVVVVVVVK.......................",
    "....KHKKKKKKKKKHK.......................",
    "....KRRRSSRSSRRRKKK.....................",
    "...KRRSSSSSSSSSRRKRK....................",
    "...KRRSSKKSKKSSRRKRRK...................",
    "...KRRSBSSSSSBSRRKRRK...................",
    "....KRRSSSKSSSRRK..KK................KK.",
    ".....KRKKSSSKKRK....................KcK.",
    ".....KJJJFFFJFFFK.......KKKKKKK.....KcK.",
    "....KJJJJJFJJKFFFK.....KsssssssKKKKKMMK.",
    "...KJJKJJJFJJJKKKK.....KMMMMMMMMMMMMMMKK",
    "...KJjKJJJJJJJKjJK....KMMWMMMMMMMMMMMLLK",
    "...KGGKJJJJJJJKGGK....KMMMMMmmmmMMMMMLLK",
    "....KK.KPPPPPPK.KK....KmmMMmmmmmmmMMMMKK",
    ".......KPPPPPPK........KTTKcCCCCCcKKTTK.",
    ".......KPPKKPPK.......KTttTKcCKKCcKTttTK",
    ".......KPpKKpPK.......KtWWtKKccccKKtWWtK",
    ".......KPPKKPPK.......KtWWtK......KtWWtK",
    "......KbbbKKbbbK......KTttTK......KTttTK",
    "......KKKKKKKKKK.......KTTK........KTTK.",
  },
  { // wave0
    "........................................",
    "........KKKKK...........................",
    "......KKHHHHHKK.........................",
    ".....KHhhHHHHHHK........................",
    "....KHhHHHHHHHHHK.......................",
    "....KHVVVVVVVVVHK.......................",
    "....KVvvVVVVVVVVK.......................",
    "....KHKKKKKKKKKHK.......................",
    "....KRRRSSRSSRRRKK......................",
    "KK.KRRSSEESEESSRRKK.....................",
    "GGKKRRSSEESEESSRRKRK....................",
    "GGKKRRSBSSSSSBSRRKRK....................",
    "KJK.KRRSSSKSSSRRK.RK.................KK.",
    ".KJK.KRKKSSSKKRK..K.................KcK.",
    ".KJJKKJJJFFFJFFK........KKKKKKK.....KcK.",
    "..KJKJJJJJFJJKFFK......KsssssssKKKKKMMK.",
    ".....KKJJJFJJJKKJK.....KMMMMMMMMMMMMMMKK",
    ".....KKJJJJJJJKjJK....KMMWMMMMMMMMMMMLLK",
    ".....KKJJJJJJJKGGK....KMMMMMmmmmMMMMMLLK",
    ".......KPPPPPPK.KK....KmmMMmmmmmmmMMMMKK",
    ".......KPPPPPPK........KTTKcCCCCCcKKTTK.",
    ".......KPPKKPPK.......KTttTKcCKKCcKTttTK",
    ".......KPpKKpPK.......KtWWtKKccccKKtWWtK",
    ".......KPPKKPPK.......KtWWtK......KtWWtK",
    "......KbbbKKbbbK......KTttTK......KTttTK",
    "......KKKKKKKKKK.......KTTK........KTTK.",
  },
  { // wave1
    "........................................",
    "........KKKKK...........................",
    "......KKHHHHHKK.........................",
    ".....KHhhHHHHHHK........................",
    "....KHhHHHHHHHHHK.......................",
    "....KHVVVVVVVVVHK.......................",
    "....KVvvVVVVVVVVK.......................",
    "....KHKKKKKKKKKHK.......................",
    "....KRRRSSRSSRRRKKK.....................",
    ".KKKRRSSEESEESSRRKRK....................",
    "KGGKRRSSEESEESSRRKRRK...................",
    "KGGKRRSBSSSSSBSRRKRRK...................",
    ".KJKKRRSSSKSSSRRK..KK................KK.",
    ".KJK.KRKKSSSKKRK....................KcK.",
    ".KJJKKJJJFFFJFFFK.......KKKKKKK.....KcK.",
    "..KJKJJJJJFJJKFFFK.....KsssssssKKKKKMMK.",
    ".....KKJJJFJJJKKKK.....KMMMMMMMMMMMMMMKK",
    ".....KKJJJJJJJKjJK....KMMWMMMMMMMMMMMLLK",
    ".....KKJJJJJJJKGGK....KMMMMMmmmmMMMMMLLK",
    ".......KPPPPPPK.KK....KmmMMmmmmmmmMMMMKK",
    ".......KPPPPPPK........KTTKcCCCCCcKKTTK.",
    ".......KPPKKPPK.......KTttTKcCKKCcKTttTK",
    ".......KPpKKpPK.......KtWWtKKccccKKtWWtK",
    ".......KPPKKPPK.......KtWWtK......KtWWtK",
    "......KbbbKKbbbK......KTttTK......KTttTK",
    "......KKKKKKKKKK.......KTTK........KTTK.",
  },
  { // sleep
    "........................................",
    "........KKKKK...........................",
    "......KKHHHHHKK.........................",
    ".....KHhhHHHHHHK........................",
    "....KHhHHHHHHHHHK.......................",
    "....KHVVVVVVVVVHK.......................",
    "....KVvvVVVVVVVVK.......................",
    "....KHKKKKKKKKKHK.......................",
    "....KRRRSSRSSRRRKK......................",
    "...KRRSSSSSSSSSRRKK.....................",
    "...KRRSSKKSKKSSRRKRK....................",
    "...KRRSBSSSSSBSRRKRK....................",
    "....KRRSSSSSSSRRK.RK.................KK.",
    ".....KRKKSSSKKRK..K.................KcK.",
    ".....KJJJFFFJJJK........KKKKKKK.....KcK.",
    "....KJJJJJFJJJJJK......KsssssssKKKKKMMK.",
    "...KJJKJJJFJJJKJJK.....KMMMMMMMMMMMMMMKK",
    "...KJjKJJJJJJJKjJK....KMMWMMMMMMMMMMMllK",
    "...KGGKJJJJJJJKGGK....KMMMMMmmmmMMMMMllK",
    "....KK.KPPPPPPK.KK....KmmMMmmmmmmmMMMMKK",
    ".......KPPPPPPK........KTTKcCCCCCcKKTTK.",
    ".......KPPKKPPK.......KTttTKcCKKCcKTttTK",
    ".......KPpKKpPK.......KtWWtKKccccKKtWWtK",
    ".......KPPKKPPK.......KtWWtK......KtWWtK",
    "......KbbbKKbbbK......KTttTK......KTttTK",
    "......KKKKKKKKKK.......KTTK........KTTK.",
  },
};
uint16_t riderColor(char ch) {
  switch (ch) {
    case 'K': return 0x18C6;
    case 'H': return 0x2E57;
    case 'h': return 0xAFBD;
    case 'V': return 0x29ED;
    case 'v': return 0x7D1C;
    case 'S': return 0xFEB5;
    case 'E': return 0x18C6;
    case 'B': return 0xFCB2;
    case 'R': return 0x9225;
    case 'J': return 0xFC65;
    case 'j': return 0xD2E2;
    case 'F': return 0xFF07;
    case 'G': return 0x39E8;
    case 'P': return 0x3A91;
    case 'p': return 0x29AC;
    case 'b': return 0x6A25;
    case 'W': return 0xFFFF;
    case 'M': return 0xE187;
    case 'm': return 0xA0E5;
    case 'C': return 0xCE7A;
    case 'c': return 0x8432;
    case 'T': return 0x2966;
    case 't': return 0x94B4;
    case 'L': return 0xFF8F;
    case 'l': return 0x6B6C;
    case 's': return 0x4185;
    default: return 0;
  }
}

#define RIDER_S  5
#define RIDER_X0 (CX - RIDER_W * RIDER_S / 2)
#define RIDER_Y0 60
bool onWaitScreen = false;
bool sliding = false;                       // v5.5 正在拖动/滑动换页：别的画面先别画
int waitSt = 0;
int riderPrevF = -1, riderPrevZ = -2;
bool catForce = false;                      // 首页刚重画：马上画一次

// f = 第几帧；zph = 睡觉时 z 的动画帧（-1 = 不睡）
void drawRider(int f, int zph) {
  gfx->fillRect(RIDER_X0 - 4, RIDER_Y0 - 40, RIDER_W * RIDER_S + 8, RIDER_H * RIDER_S + 42, C_BLACK);
  drawStatus();                             // v7.2 上面擦黑的那块盖住了蓝牙点和电量：马上补画（z 字母画在它上面）
  for (int y = 0; y < RIDER_H; y++) {
    const char *row = RIDER[f][y];
    for (int x = 0; row[x] && x < RIDER_W; x++) {
      uint16_t c = riderColor(row[x]);
      if (c || row[x] == 'K') gfx->fillRect(RIDER_X0 + x * RIDER_S, RIDER_Y0 + y * RIDER_S, RIDER_S, RIDER_S, c ? c : 0x18C6);
    }
  }
  if (zph >= 0) {                           // 三个 z 从她头顶往右上方飘
    for (int k = 0; k <= zph; k++)
      printCentered("z", CX + 4 + k * 13, RIDER_Y0 + 2 - k * 13, 2 + (k > 0 ? 1 : 0), k == zph ? C_CYAN : C_GREY);
  }
}

// 每 0.1 秒检查一次，样子变了才重画（函数名沿用 v3.6）
void animateCat() {
  if (!onWaitScreen || sliding) return;
  static unsigned long last = 0;
  unsigned long now = millis();
  if (!catForce && now - last < 100) return;
  catForce = false;
  last = now;
  int f, zph = -1;
  int t = (int)((now / 400) % 2);           // 围巾、马尾飘动
  if (waitSt == WAIT_NOLINK) {
    f = 6;                                  // 睡觉
    zph = (int)((now / 700) % 3);
  } else if (waitSt == WAIT_READY && (now % 5000) < 1500) {
    f = 4 + (int)((now / 250) % 2);         // 挥手
  } else if ((now % 3600) < 160) {
    f = 2 + t;                              // 眨眼
  } else {
    f = t;
  }
  if (f == riderPrevF && zph == riderPrevZ) return;
  riderPrevF = f; riderPrevZ = zph;
  drawRider(f, zph);
  needFlush = true;
}

// 页面：导航左滑天气，右滑音乐，上下滑也可查看天气
int page = 0;                               // 0 = 导航 / 首页，1 = 天气，2 = 音乐
unsigned long pageAt = 0;

// 页面顺序对应滑动位置：音乐在左，主页居中，天气在右；导航主页不显示。
void drawPageDots() {
  if (page == 0 && inNav) return;
  const int pageOrder[] = {2, 0, 1};
  for (int i = 0; i < 3; i++) {
    aaDisc(CX + (i - 1) * 20, 444, 4, page == pageOrder[i] ? C_WHITE : 0x4208);
  }
}

void drawWaiting(int st) {
  waitSt = st;
  if (sliding) return;                                                 // v5.5 正在滑动：滑完再画
  if (page == 2) { onWaitScreen = false; musicDirty = true; return; }
  if (page == 1) { onWaitScreen = false; wxDirty = true; return; }   // v5.3 正在看天气：先不画首页，滑回来时再画
  gfx->fillScreen(C_BLACK);
  onWaitScreen = true; waitSt = st;          // v3.6 首页猫猫 → v4.6 像素骑手
  riderPrevF = -1; riderPrevZ = -2;
  catForce = true;
  animateCat();
  printCentered("MotoNav", CX, 200, 6, st == WAIT_READY ? C_CYAN : C_GREY);
  if (st == WAIT_READY) {
    printCentered("ready", CX, 275, 3, C_GREEN);
  } else if (st == WAIT_NOLINK) {
    printCentered("no link", CX, 275, 3, C_GREY);
  } else {
    drawRedRing();
    printCentered("LINK LOST", CX, 275, 4, C_RED);
  }
  printCentered(bleName, CX, 332, 2, C_GREY);                          // v7.0 这块圆屏的蓝牙名
  printCentered(String("fw ") + FW_VERSION + "  " + String(qspiMHz) + "MHz", CX, 360, 2, C_GREY);
  drawStatus();
  drawPageDots();
  needFlush = true;
}

// ================= v6.5 配对画面 =================
bool pairOn = false;
unsigned long pairShowAt = 0;

void drawPairScreen() {
  onWaitScreen = false;
  frameIsMap = false;
  gfx->fillScreen(C_BLACK);
  printCentered("PAIRING", CX, 120, 3, C_CYAN);
  if (pairLocked) {
    printCentered("LOCKED", CX, 205, 5, C_RED);
    printCentered("restart this screen,", CX, 290, 2, C_GREY);
    printCentered("then pair within 2 min", CX, 315, 2, C_GREY);
  } else {
    char b[8];
    snprintf(b, sizeof(b), "%06lu", (unsigned long)pairCode);
    printCentered(b, CX, 200, 7, C_WHITE);
    printCentered("enter this code", CX, 295, 2, C_GREY);
    printCentered("on your phone", CX, 320, 2, C_GREY);
  }
  printCentered(bleName, CX, 372, 2, C_CYAN);                            // v7.0 确认连的是这块

  needFlush = true;
}

// ================= v7.1 到达特效 =================
// 到达目的地（"A,0"）：约 7 秒的庆祝动画——粉色放射光芒慢慢转、冲击波圆环、几轮彩色烟花、
// 中间一面飘动的黑白格终点旗、"ARRIVED" 霓虹字，手机发了行程统计（"#O米/分钟/最高车速"）就逐字打出来。
// 到达途经点（"#J"）：约 3 秒的短版（青色，"WAYPOINT"）。点一下屏幕提前结束，之后回到原来的画面。
#define C_PINK   0xF973   // #FF2E9A 罪恶都市粉
#define FX_DEST_MS 7000
#define FX_WP_MS   3000
#define FX_PN      180
#define CCY 233                // 屏幕中心 y（圆屏 466×466）
bool fxOn = false, fxWp = false;
unsigned long fxAt = 0, fxDur = 0;
bool touchEat = false;                 // 点掉特效的那一下：不当成滑动换页
long tripM = -1; int tripMin = -1, tripMaxKmh = -1; unsigned long tripAt = 0;   // "#O" 行程统计
struct FxP { int16_t x, y, vx, vy; uint16_t t0; uint16_t col; };
FxP fxP[FX_PN];
int fxPN = 0;

static inline float frand(float a, float b) { return a + (b - a) * (esp_random() & 0xFFFF) / 65535.0f; }

void startArriveFx(bool wp) {
  fxOn = true; fxWp = wp;
  fxAt = millis();
  fxDur = wp ? FX_WP_MS : FX_DEST_MS;
  // 烟花：几轮，每轮从一个点炸开
  const uint16_t pal[5] = { C_PINK, C_CYAN, C_YELLOW, 0x07F0, C_WHITE };
  const int burstsD[6] = { 0, 650, 1300, 2200, 3200, 4300 };
  const int burstsW[2] = { 0, 700 };
  int nb = wp ? 2 : 6;
  int per = FX_PN / nb;
  fxPN = 0;
  for (int b = 0; b < nb; b++) {
    float bx = CX + frand(-110, 110), by = (wp ? 200 : 150) + frand(-50, 50);
    uint16_t c1 = wp ? C_CYAN : pal[esp_random() % 5], c2 = wp ? C_WHITE : pal[esp_random() % 5];
    for (int i = 0; i < per && fxPN < FX_PN; i++) {
      float a = frand(0, 2 * PI), sp = frand(70, 230);
      FxP &p = fxP[fxPN++];
      p.x = (int16_t)bx; p.y = (int16_t)by;
      p.vx = (int16_t)(cosf(a) * sp); p.vy = (int16_t)(sinf(a) * sp - 40);
      p.t0 = (uint16_t)((wp ? burstsW[b] : burstsD[b]) + frand(0, 60));
      p.col = (i & 1) ? c1 : c2;
    }
  }
}

void stopArriveFx() { fxOn = false; }

// 一圈 w 像素粗的圆环
void fxRing(float r, int w, uint16_t c) {
  for (int k = 0; k < w; k++) if (r + k > 0) gfx->drawCircle(CX, CCY, (int)(r + k), c);
}

// 飘动的黑白格旗：(px, py) 是旗杆顶，s = 缩放，t = 毫秒
void fxFlag(float px, float py, float s, unsigned long t, uint16_t light) {
  const int COLS = 8, ROWS = 5;
  float cell = 13 * s;
  thickLine(px, py - 6 * s, px, py + 150 * s, 5 * s, 0xC618);            // 旗杆
  aaDisc(px, py - 8 * s, 5 * s, C_YELLOW);                               // 杆头
  for (int c = 0; c < COLS; c++) {
    float k = (c + 1) / (float)COLS;                                     // 越往外飘得越厉害
    float yo = sinf(t * 0.008f - c * 0.75f) * 7 * s * k;
    float x = px + 3 * s + c * cell;
    float y = py + yo;
    gfx->fillRect((int)x, (int)(y - 2), (int)ceilf(cell) + (c == COLS - 1 ? 2 : 0), (int)(ROWS * cell + 4), light);
    for (int r = 0; r < ROWS; r++)
      if ((r + c) & 1) gfx->fillRect((int)x, (int)(y + r * cell), (int)ceilf(cell), (int)ceilf(cell), C_BLACK);
  }
}

// 霓虹字：外面一圈暗色描边当光晕
void fxNeonText(const String &s, int y, int size, uint16_t col, uint16_t glow) {
  for (int dx = -2; dx <= 2; dx += 2)
    for (int dy = -2; dy <= 2; dy += 2)
      if (dx || dy) printCentered(s, CX + dx, y + dy, size, glow);
  printCentered(s, CX, y, size, col);
}

void drawArriveFx(unsigned long t) {
  onWaitScreen = false;
  frameIsMap = false;
  gfx->fillScreen(C_BLACK);
  uint16_t mc = fxWp ? C_CYAN : C_PINK;
  float fade = (t + 600 > fxDur) ? (fxDur - t) / 600.0f : 1.0f;          // 最后 0.6 秒淡出
  if (fade < 0) fade = 0;
  int fadeA = 255 - (int)(fade * 255);                                   // 和黑色混合的比例

  // 1. 放射光芒（慢慢转）
  {
    float rot = t * 0.00025f;
    uint16_t rc = blend565(blend565(mc, C_BLACK, 205), C_BLACK, fadeA);
    for (int i = 0; i < 16; i++) {
      float a0 = rot + i * (2 * PI / 16), a1 = a0 + 0.11f;
      gfx->fillTriangle(CX, CCY, CX + cosf(a0) * 260, CCY + sinf(a0) * 260,
                        CX + cosf(a1) * 260, CCY + sinf(a1) * 260, rc);
    }
  }
  // 2. 冲击波圆环
  for (int k = 0; k < 3; k++) {
    float ph = ((long)t - k * 260) / 900.0f;
    if (ph < 0 || ph > 1) continue;
    uint16_t c = blend565((k & 1) ? C_CYAN : mc, C_BLACK, (int)(ph * 255));
    fxRing(30 + ph * 210, 4, c);
  }
  // 3. 外圈霓虹跑马灯
  {
    float base = t * 0.0016f;
    for (int i = 0; i < 24; i++) {
      float a0 = base + i * (2 * PI / 24), a1 = a0 + 0.14f;
      uint16_t c = blend565((i & 1) ? C_CYAN : mc, C_BLACK, fadeA);
      thickLine(CX + cosf(a0) * 222, CCY + sinf(a0) * 222, CX + cosf(a1) * 222, CCY + sinf(a1) * 222, 5, c);
    }
  }
  // 4. 旗子：弹出来（先放大一点再回到正常大小）
  {
    float pt = t / 450.0f;
    float s = pt >= 1 ? 1.0f : (pt < 0.7f ? pt / 0.7f * 1.15f : 1.15f - (pt - 0.7f) / 0.3f * 0.15f);
    if (fxWp) s *= 0.75f;
    if (s > 0.05f) {
      float fw = (3 + 8 * 13) * s;
      fxFlag(CX - fw / 2, (fxWp ? 120 : 92) + (1 - s) * 60, s, t, blend565(C_WHITE, C_BLACK, fadeA));
    }
  }
  // 5. 烟花粒子（带重力，越到后面越暗越小）
  {
    const float LIFE = 1.7f, G = 170;
    for (int i = 0; i < fxPN; i++) {
      float dt = ((long)t - fxP[i].t0) / 1000.0f;
      if (dt < 0 || dt > LIFE) continue;
      float x = fxP[i].x + fxP[i].vx * dt;
      float y = fxP[i].y + fxP[i].vy * dt + 0.5f * G * dt * dt;
      float lf = dt / LIFE;
      int r = lf < 0.4f ? 4 : (lf < 0.75f ? 3 : 2);
      uint16_t c = blend565(fxP[i].col, C_BLACK, (int)(lf * lf * 230));
      gfx->fillRect((int)x - r / 2, (int)y - r / 2, r + 1, r + 1, c);
      if (dt < 0.35f) {                                                  // 刚炸开时带一小段拖尾
        float tx = x - fxP[i].vx * 0.04f, ty = y - (fxP[i].vy + G * dt) * 0.04f;
        gfx->drawLine((int)tx, (int)ty, (int)x, (int)y, blend565(c, C_BLACK, 120));
      }
    }
  }
  // 6. 文字
  if (t > 250) {
    uint16_t tc = (t / 180) % 2 ? C_WHITE : mc;                        // 前 1.5 秒闪烁，之后定住
    if (t > 1500) tc = C_WHITE;
    tc = blend565(tc, C_BLACK, fadeA);
    uint16_t gc = blend565(blend565(mc, C_BLACK, 110), C_BLACK, fadeA);
    if (fxWp) fxNeonText("WAYPOINT", 268, 4, tc, gc);
    else      fxNeonText("ARRIVED", 262, 5, tc, gc);
  }
  if (!fxWp && t > 900) {
    String l1 = "", l2 = "";
    bool haveTrip = tripM >= 0 && millis() - tripAt < 30000;
    if (haveTrip) {
      l1 = (tripM >= 1000 ? String(tripM / 1000.0, 1) + " km" : String(tripM) + " m") + "  " + String(tripMin) + " min";
      if (tripMaxKmh > 0) l2 = "MAX " + String(tripMaxKmh) + " km/h";
    } else {
      l1 = "RIDE SAFE";
    }
    int n = (t - 900) / 45;                                              // 逐字打出来
    String s1 = l1.substring(0, min(n, (int)l1.length()));
    printCentered(s1, CX, 328, 3, blend565(C_CYAN, C_BLACK, fadeA));
    int n2 = n - (int)l1.length() - 4;
    if (l2.length() && n2 > 0) printCentered(l2.substring(0, min(n2, (int)l2.length())), CX, 366, 2, blend565(C_YELLOW, C_BLACK, fadeA));
  }
  needFlush = true;
}

// ================= v5.1 来电显示 =================
// 手机来电时整屏显示：上面 “INCOMING CALL”，中间绿色电话图标带往外扩散的波纹，下面是名字（手机渲染好的
// 文字图片，图片栏 2）和号码；接听或挂断后回到原来的画面（地图 / 大箭头 / 首页）
bool callOn = false;
String callNum = "";
unsigned long callAt = 0, callSeenAt = 0;

// v5.2 触屏接电话：按住任意位置 HOLD_MS 毫秒 → 发 "T,A" 给手机（v6.9 800 → 200）
#define HOLD_MS 200
TouchDrvCST816 touch;
bool touchOK = false;
unsigned long touchHoldAt = 0, touchLastSeen = 0;
float touchHold = 0;                 // 0~1 按住的进度
bool callAnswerSent = false;
unsigned long callAnswerAt = 0;
String pendingProf = "";             // v5.8 分步用时
String pendingEvt = "";              // 待发给手机的触摸事件

// v5.3 触摸芯片一有动静就拉低中断脚；平时只看这个标记，不用一直读 I2C
volatile bool touchIrq = false;
void IRAM_ATTR onTouchIrq() { touchIrq = true; }

void touchInit() {
  touch.setPins(TP_RESET, TP_INT);
  touchOK = touch.begin(Wire, CST816_SLAVE_ADDRESS, IIC_SDA, IIC_SCL);
  Serial.println(touchOK ? "touch ok" : "touch not found");
  if (touchOK) { touch.disableAutoSleep();attachInterrupt(TP_INT, onTouchIrq, FALLING); }   // v5.3
}

bool touchDown = false;
int16_t touchX0 = 0, touchY0 = 0, touchX1 = 0, touchY1 = 0;
void dragMove(int dx, int dy, unsigned long now);   // v5.5 跟手换页
int musicButtonAt(int x, int y);
void musicTap(int x, int y);
void dragEnd();
void dragAbort();

// 读触摸：按下 → 报坐标；来电时按住够久 → 接听；拖动 → 跟手换页（v5.5）
// 手指离开超过 0.15 秒才算松开（防止读数偶尔丢一下）
void touchPoll() {
  static unsigned long lastPoll = 0;
  unsigned long now = millis();
  if (!touchOK) return;
  if (!touchIrq && !touchDown && !callOn) return;
  if (now - lastPoll < 12) return;
  lastPoll = now;
  touchIrq = false;
  if (callAnswerSent && now - callAnswerAt > 4000) callAnswerSent = false;   // 4 秒还在响：允许再按一次
  int16_t x[5] = {0}, y[5] = {0};
  uint8_t n = touch.getPoint(x, y, 1);
  if(n>0) { x[0]=PortScale::touch(x[0]);y[0]=PortScale::touch(y[0]); }
  if (n > 0) {
    if (!touchDown) {
      touchDown = true;
      touchHoldAt = now;
      touchX0 = touchX1 = x[0]; touchY0 = touchY1 = y[0];
      pendingEvt = "T,P," + String(x[0]) + "," + String(y[0]);
      if (page == 2 && !callOn && !pairOn) { musicPressed = musicButtonAt(x[0], y[0]); musicDirty = true; }
      if (fxOn && !callOn) { stopArriveFx(); touchEat = true; }          // v7.1 点一下结束到达特效
    }
    touchX1 = x[0]; touchY1 = y[0];
    touchLastSeen = now;
    if (callOn || pairOn || touchEat || fxOn) dragAbort();
    else dragMove(touchX1 - touchX0, touchY1 - touchY0, now);
  } else if (touchDown && now - touchLastSeen > 80) {
    touchDown = false;
    touchHoldAt = 0;
    if (callOn || pairOn || touchEat || fxOn) dragAbort();
    else {
      if (page == 2 && abs(touchX1 - touchX0) < 36 && abs(touchY1 - touchY0) < 36) musicTap(touchX0, touchY0);
      if (page == 1 && abs(touchX1 - touchX0) < 36 && abs(touchY1 - touchY0) < 36) weatherTap();
      dragEnd();
    }
    touchEat = false;
    if (musicPressed >= 0) { musicPressed = -1; musicDirty = true; }
  }
  // 来电：按住不动才算（滑动不算）
  bool still = abs(touchX1 - touchX0) < 60 && abs(touchY1 - touchY0) < 60;
  touchHold = (callOn && touchDown && still) ? min(1.0f, (now - touchHoldAt) / (float)HOLD_MS) : 0;
  if (touchHold >= 1 && !callAnswerSent) {
    callAnswerSent = true;
    callAnswerAt = now;
    touchHold = 0;
    pendingEvt = "T,A";
  }
}

// 电话听筒：一段拱形的圆点串当手柄，两头各一个大点当听筒和话筒，整体斜过来（常见的电话图标）
void drawHandset(float cx, float cy, float r, uint16_t c) {
  const float ROT = -35 * PI / 180, cr = cosf(ROT), sr = sinf(ROT);
  auto dot = [&](float x, float y, float rr) {
    aaDisc(cx + x * cr - y * sr, cy + x * sr + y * cr, rr, c);
  };
  for (int a = 25; a <= 155; a += 5) {
    float rad = a * PI / 180;
    dot(cosf(rad) * r, -sinf(rad) * r * 0.55f + r * 0.25f, r * 0.22f);
  }
  for (int k = 0; k < 2; k++) {
    float rad = (k ? 155 : 25) * PI / 180;
    dot(cosf(rad) * r + (k ? -1 : 1) * r * 0.04f, -sinf(rad) * r * 0.55f + r * 0.25f + r * 0.21f, r * 0.32f);
  }
}

void drawCallScreen() {
  onWaitScreen = false;
  frameIsMap = false;
  gfx->fillScreen(C_BLACK);
  unsigned long t = millis() - callAt;
  const float icy = 168;
  // 往外扩散的两圈波纹
  for (int k = 0; k < 2; k++) {
    float ph = fmodf(t / 1400.0f + k * 0.5f, 1.0f);
    float r = 52 + ph * 110;
    uint16_t c = blend565(C_GREEN, C_BLACK, (int)(ph * 255));
    for (int w = 0; w < 3; w++) gfx->drawCircle(CX, (int)icy, (int)r + w, c);
  }
  aaDisc(CX, icy, 50, 0x2DC6);
  aaDisc(CX, icy, 46, C_GREEN);
  // 听筒轻轻左右晃
  float wob = ((t / 120) % 4 < 2) ? -2.5f : 2.5f;
  drawHandset(CX + wob, icy, 26, C_WHITE);
  printCentered("INCOMING CALL", CX, 58, 2, C_GREEN);
  bool hasName = imgs[2].show && imgs[2].showW > 0;
  if (hasName) {
    drawTextImg(2, 252, 0xFF);
    if (callNum.length()) printCentered(callNum, CX, 318, 2, C_GREY);
  } else if (callNum.length()) {
    printCentered(callNum, CX, 262, callNum.length() > 12 ? 3 : 4, C_WHITE);
  } else {
    printCentered("UNKNOWN", CX, 262, 4, C_WHITE);
  }
  // v5.2 触屏接听提示；按住时绿圈外面一圈白点从正上方顺时针走满
  if (touchOK) {
    if (callAnswerSent) printCentered("ANSWERING...", CX, 392, 2, C_GREEN);
    else printCentered("TAP & HOLD", CX, 392, 2, C_GREY);      // v6.9
    if (touchHold > 0 && !callAnswerSent) {
      int steps = (int)(touchHold * 72);
      for (int k = 0; k <= steps; k++) {
        float a = k * 5 * PI / 180;
        aaDisc(CX + sinf(a) * 64, icy - cosf(a) * 64, 4, C_WHITE);
      }
    }
  }
  needFlush = true;
}

// ================= v5.3 天气页 =================
// 图标编号（手机按高德的天气文字归类）：0 晴 1 多云 2 阴 3 雨 4 大雨 5 雷雨 6 雪 7 雾霾 8 雨夹雪 9 沙尘 10 夜晴 11 夜多云
bool wxHave = false;
int wxIcon = 0, wxTemp = 0;
String wxTime = "";
int wxN = 0;
bool wxHourly = false;                 // v5.6 下面一排是逐小时（H）还是按天（D）
int wxWk[6], wxFi[6], wxFh[6], wxFl[6];   // 按天：星期/图标/最高/最低；逐小时：几点/图标/温度/降雨%
unsigned long wxAt = 0, wxAskAt = 0;

#define WX_SUN   0xFE60
#define WX_CLOUD 0xDEFB
#define WX_DARK  0x8C71
#define WX_RAIN  0x4D7F
#define WX_DUST  0xC4A8

// 粗线：沿线段画一串抗锯齿圆点
void wxLine(float x0, float y0, float x1, float y1, float w, uint16_t c) {
  float len = sqrtf((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));
  int n = max(1, (int)(len / 1.5f));
  for (int i = 0; i <= n; i++) aaDisc(x0 + (x1 - x0) * i / n, y0 + (y1 - y0) * i / n, w / 2, c);
}

void wxSun(float cx, float cy, float r) {
  for (int k = 0; k < 8; k++) {
    float a = k * PI / 4;
    wxLine(cx + cosf(a) * r * 1.35f, cy + sinf(a) * r * 1.35f, cx + cosf(a) * r * 1.75f, cy + sinf(a) * r * 1.75f, max(2.0f, r * 0.18f), WX_SUN);
  }
  aaDisc(cx, cy, r, WX_SUN);
}

void wxMoon(float cx, float cy, float r) {
  aaDisc(cx, cy, r, WX_SUN);
  aaDisc(cx + r * 0.45f, cy - r * 0.3f, r * 0.82f, C_BLACK);
}

// 云：三个圆 + 圆角底座
void wxCloud(float cx, float cy, float s, uint16_t c) {
  aaDisc(cx - s * 0.5f, cy + s * 0.08f, s * 0.42f, c);
  aaDisc(cx + s * 0.05f, cy - s * 0.2f, s * 0.58f, c);
  aaDisc(cx + s * 0.55f, cy + s * 0.12f, s * 0.38f, c);
  int r = (int)(s * 0.25f);
  gfx->fillRoundRect((int)(cx - s * 0.92f), (int)(cy + s * 0.02f), (int)(s * 1.84f), (int)(s * 0.5f), r, c);
}

// 画一个天气图标；s ≈ 图标半宽
void drawWxIcon(int id, float cx, float cy, float s) {
  switch (id) {
    case 0: wxSun(cx, cy, s * 0.55f); break;
    case 10: wxMoon(cx, cy, s * 0.7f); break;
    case 1: case 11:
      if (id == 1) wxSun(cx + s * 0.3f, cy - s * 0.3f, s * 0.42f); else wxMoon(cx + s * 0.3f, cy - s * 0.35f, s * 0.5f);
      wxCloud(cx - s * 0.12f, cy + s * 0.2f, s * 0.78f, WX_CLOUD);
      break;
    case 2: wxCloud(cx, cy, s * 0.95f, WX_CLOUD); break;
    case 3: case 4: case 8: {
      int drops = id == 4 ? 4 : 3;
      for (int k = 0; k < drops; k++) {
        float x = cx + (k - (drops - 1) / 2.0f) * s * 0.42f;
        if (id == 8 && k % 2) aaDisc(x - s * 0.08f, cy + s * 0.72f, s * 0.11f, C_WHITE);
        else wxLine(x, cy + s * 0.5f, x - s * 0.16f, cy + s * 0.92f, max(2.0f, s * 0.1f), WX_RAIN);
      }
      wxCloud(cx, cy - s * 0.2f, s * 0.9f, id == 4 ? WX_DARK : WX_CLOUD);
      break;
    }
    case 5: {
      wxCloud(cx, cy - s * 0.25f, s * 0.9f, WX_DARK);
      float x = cx, y = cy + s * 0.2f;           // 闪电
      gfx->fillTriangle((int)(x + s * 0.12f), (int)y, (int)(x - s * 0.22f), (int)(y + s * 0.42f), (int)(x + s * 0.02f), (int)(y + s * 0.42f), WX_SUN);
      gfx->fillTriangle((int)(x + s * 0.1f), (int)(y + s * 0.3f), (int)(x - s * 0.12f), (int)(y + s * 0.85f), (int)(x + s * 0.2f), (int)(y + s * 0.3f), WX_SUN);
      break;
    }
    case 6:
      wxCloud(cx, cy - s * 0.2f, s * 0.9f, WX_CLOUD);
      for (int k = 0; k < 3; k++) aaDisc(cx + (k - 1) * s * 0.42f, cy + s * 0.68f + (k % 2) * s * 0.18f, s * 0.11f, C_WHITE);
      break;
    case 7: case 9:
      for (int k = 0; k < 4; k++) {
        float y = cy - s * 0.45f + k * s * 0.3f, off = (k % 2) * s * 0.18f;
        wxLine(cx - s * 0.75f + off, y, cx + s * 0.6f + off, y, max(3.0f, s * 0.16f), id == 9 ? WX_DUST : WX_DARK);
      }
      break;
    default: wxCloud(cx, cy, s * 0.95f, WX_CLOUD);
  }
}

// 温度大字 + 度数小圆圈
void drawTemp(int t, int cx, int y, int size, uint16_t c) {
  String v = String(t);
  int w = v.length() * 6 * size;
  printCentered(v, cx - size * 2, y, size, c);
  float ox = cx - size * 2 + w / 2.0f + size * 1.6f, oy = y + size * 2.5f;
  for (int a = 0; a < 360; a += 12) aaDisc(ox + cosf(a * PI / 180) * size * 1.3f, oy + sinf(a * PI / 180) * size * 1.3f, max(1.5f, size * 0.32f), c);
}

const char *WX_WEEK[] = {"", "MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN"};

// QWeather extension: no strings in BLE packets; Chinese summaries use image slots 7-11.
int wxDetail = 0, wxAQI = -1, wxAlerts = -1, wxLevel = 0, wxRainN = 0;
int wxRain[24];
unsigned long wxAlertTag = 0;
bool wxExtra = false;
void weatherTap() { if (wxHave && wxExtra) { wxDetail = (wxDetail + 1) % 3; wxDirty = true; pageAt = millis(); } }
void parseWeatherExtra(const String &m) {
  char buffer[240]; m.toCharArray(buffer, sizeof(buffer));
  char *save = nullptr; strtok_r(buffer, ",", &save);
  int values[29], n = 0; char *token;
  while ((token = strtok_r(nullptr, ",", &save)) && n < 29) values[n++] = atoi(token);
  if (n < 5 || values[3] < 0 || values[3] > 24 || n < 5 + values[3]) return;
  wxAQI = values[0]; wxAlerts = values[1]; wxLevel = values[2]; wxRainN = values[3];
  unsigned long tag = (unsigned long) values[4];
  if (wxAlerts > 0 && tag != wxAlertTag && page == 1) wxDetail = 2;
  wxAlertTag = tag;
  for (int i = 0; i < wxRainN; ++i) wxRain[i] = values[5+i];
  wxExtra = true; wxDirty = true;
}
bool drawWeatherExtra() {
  if (!wxExtra || wxDetail == 0) return false;
  gfx->fillScreen(C_BLACK);
  if (wxDetail == 1) {
    printCentered("RAIN / AIR", CX, 55, 3, C_CYAN);
    drawTextImg(7, 105, 0xFF);
    int peak = 0;
    for (int i=0; i<wxRainN; ++i) peak = max(peak, wxRain[i]);
    if (wxRainN > 0) {
      for (int i=0; i<wxRainN; ++i) {
        int x = 95 + i * 276 / wxRainN;
        int height = wxRain[i] < 0 ? 2 : wxRain[i] * 72 / max(1, peak);
        gfx->fillRect(x, 225-height, max(2, 260/wxRainN), max(2,height), wxRain[i] < 0 ? C_GREY : C_CYAN);
      }
      printCentered("NEXT 2H  MAX " + String(peak / 100.0f, 2) + " mm/5m", CX, 240, 2, C_GREY);
    } else printCentered("RAIN DATA UNAVAILABLE", CX, 190, 2, C_GREY);
    uint16_t airColor = wxAQI < 0 ? C_GREY : C_CYAN;
    drawTextImg(8, 285, 0xFF);
    printCentered(wxAQI < 0 ? "AQI --" : "AQI " + String(wxAQI), CX, 335, 3, airColor);
  } else {
    printCentered(wxAlerts > 0 ? "WEATHER ALERT" : "ALERT STATUS", CX, 65, 3, wxLevel >= 3 ? C_ORANGE : C_CYAN);
    drawTextImg(9, 125, 0xFF);
    drawTextImg(10, 200, 0xDC);
    drawTextImg(11, 270, 0xDC);
    if (wxAlerts > 1) printCentered(String(wxAlerts) + " ALERTS - DETAILS ON PHONE", CX, 330, 1, C_GREY);
    printCentered("MAY BE DELAYED - CHECK OFFICIAL", CX, 365, 1, C_GREY);
  }
  printCentered("QWEATHER - TAP TO SWITCH", CX, 400, 1, C_GREY);
  drawPageDots(); needFlush = true;
  return true;
}

void drawWeather() {
  if (wxHave && drawWeatherExtra()) return;
  onWaitScreen = false;
  frameIsMap = false;
  gfx->fillScreen(C_BLACK);
  if (!wxHave) {
    drawWxIcon(1, CX, 190, 60);
    printCentered(bleConnected ? "LOADING..." : "NO PHONE", CX, 290, 3, C_GREY);
    printCentered("WEATHER", CX, 70, 2, C_GREY);
    drawPageDots();
    needFlush = true;
    return;
  }
  // 地名 · 天气
  drawTextImg(3, 56, 0xFF);
  // 大图标 + 温度
  drawWxIcon(wxIcon, CX - 78, 165, 58);
  drawTemp(wxTemp, CX + 82, 135, 8, C_WHITE);
  // 风 · 湿度
  drawTextImg(4, 232, 0xC0);
  if (wxHourly) {
    if (wxN <= 0) {
      printCentered("HOURLY UNAVAILABLE", CX, 322, 2, C_GREY);
      printCentered("CHECK PHONE WEATHER DETAILS", CX, 354, 1, C_GREY);
    }
    // v5.6 接下来几个小时：几点 / 图标 / 温度 / 降雨概率（≥10% 才显示）
    for (int i = 0; i < wxN && i < 6; i++) {
      int x = CX + (int)((i - (wxN - 1) / 2.0f) * 76);
      char hh[8]; snprintf(hh, sizeof(hh), "%02d:00", wxWk[i]);
      printCentered(hh, x, 280, 2, C_GREY);
      drawWxIcon(wxFi[i], x, 322, 19);
      String t = String(wxFh[i]);
      printCentered(t, x - 4, 348, 2, C_WHITE);
      float ox = x - 4 + t.length() * 6 + 3, oy = 351;      // 小度数圈
      for (int a = 0; a < 360; a += 30) aaDisc(ox + cosf(a * PI / 180) * 2.6f, oy + sinf(a * PI / 180) * 2.6f, 0.9f, C_WHITE);
      if (wxFl[i] >= 10) printCentered(String(wxFl[i]) + "%", x, 372, 2, WX_RAIN);
    }
  } else {
    // 按天（手机查不到逐小时时）
    for (int i = 0; i < wxN && i < 4; i++) {
      int x = CX + (int)((i - (wxN - 1) / 2.0f) * 92);
      String lb = i == 0 ? String("TODAY") : String(WX_WEEK[wxWk[i] >= 1 && wxWk[i] <= 7 ? wxWk[i] : 0]);
      printCentered(lb, x, 282, 2, i == 0 ? C_CYAN : C_GREY);
      drawWxIcon(wxFi[i], x, 328, 20);
      printCentered(String(wxFh[i]) + "/" + String(wxFl[i]), x, 360, 2, C_WHITE);
    }
  }
  if (wxTime.length()) printCentered("QWEATHER " + wxTime + (wxAlerts > 0 ? " ! TAP" : " TAP"), CX, 406, 1, 0x5AEB);
  drawPageDots();
  needFlush = true;
}

// "#W,图标,温度,更新时间,H/D,个数,{…4 个数…}..."
void parseWeather(const String &m) {
  char buf[220];
  m.toCharArray(buf, sizeof(buf));
  char *save = nullptr;
  char *tok = strtok_r(buf, ",", &save);           // "#W"
  String f[32]; int n = 0;
  while ((tok = strtok_r(nullptr, ",", &save)) && n < 32) f[n++] = tok;
  if (n < 5) return;
  wxIcon = f[0].toInt(); wxTemp = f[1].toInt(); wxTime = f[2];
  wxHourly = f[3] == "H";
  wxN = min(wxHourly ? 6 : 4, (int)f[4].toInt());
  for (int i = 0; i < wxN; i++) {
    int b = 5 + i * 4;
    if (b + 3 >= n) { wxN = i; break; }
    wxWk[i] = f[b].toInt(); wxFi[i] = f[b + 1].toInt(); wxFh[i] = f[b + 2].toInt(); wxFl[i] = f[b + 3].toInt();
  }
  wxHave = true;
  wxAt = millis();
  wxDirty = true;
}

// ---- v5.5 跟手换页 ----
// 触摸坐标和画面方向相反时改成 1（拖动时画面往手指反方向走就是反了）
#define TOUCH_FLIP_X 1      // v5.5.1 实测左右是反的
#define TOUCH_FLIP_Y 1      // 实机坐标确认：上下也反向；点按和手势使用同一校正
#define DRAG_START  16        // 手指移动超过这么多像素才开始拖
uint16_t *slideOld = nullptr, *slideNew = nullptr;
int16_t chordHalf[LCD_HEIGHT];   // 每一行在圆里的半宽（圆外不用算）
bool chordReady = false;
int lastSlideSx = -1, lastSlideSy = 0;      // 上次进天气页的方向（自动回去时反着滑）
void drawNavFrame();

// v7.8 音乐页：歌名/歌手图片栏 5/6；#Z,有会话,播放中,按钮位掩码。
void drawMusic() {
  gfx->fillScreen(C_BLACK);
  onWaitScreen = false;
  frameIsMap = false;
  printCentered("MUSIC", CX, 65, 4, C_CYAN);
  bool linked = bleConnected && bleAuthed;
  if (hidMusicMode && linked && !appleMedia.available) {
    printCentered("PHONE MEDIA KEYS", CX, 158, 2, C_WHITE);
    printCentered("OPEN MUSIC APP ON PHONE", CX, 210, 2, C_GREY);
  } else if (linked) {
    drawTextImg(5, 158, 0xFF);
    drawTextImg(6, 210, 0xBB);
  } else printCentered("PHONE OFFLINE", CX, 170, 3, C_GREY);
  bool showLyrics = linked && hidMusicMode && musicLyrics && appleMedia.available && imgs[12].showW > 0;
  if (showLyrics) drawTextImg(12, 242, 0xDD);
  const char *feedback[] = {"", "SENT", "PHONE RECEIVED", "DONE", "NO RESPONSE", "NO PLAYER", "HID SENT", "HID NOT READY", "HID SEND FAILED", "AMS SENT", "AMS SEND FAILED", "AMS QUEUED"};
  String state = linked && musicHave ? (musicPlaying ? "PLAYING" : "PAUSED") : "NO PLAYER";
  if (hidMusicMode) {
    state = !hidMediaReady() ? (appleMedia.controlMask() ? "AMS MEDIA READY" : "PAIR HID IN PHONE SETTINGS") : !appleMedia.available ? "HID READY / AMS UNAVAILABLE"
      : !appleMedia.playback.known ? "STATE UNKNOWN" : appleMedia.playback.state == 1 ? "PLAYING" : appleMedia.playback.state == 0 ? "PAUSED" : "SEEKING";
  }
  if (musicAck > 0 && musicAck <= 11) state = feedback[musicAck];
  printCentered(state, CX, showLyrics ? 272 : 257, 2, musicAck == 4 ? C_ORANGE : C_GREY);
  for (int i = 0; i < 3; i++) {
    int x = CX + (i - 1) * 100, y = 335;
    bool enabled = hidMusicMode ? (hidMediaReady() || appleMedia.controlReady(i)) : linked && musicHave;
    uint16_t c = enabled ? C_WHITE : C_GREY;
    aaDisc(x, y, 43, enabled && musicPressed == i ? 0x0470 : enabled ? 0x19A7 : 0x1082);
    if (i == 1) {
      if (hidMusicMode && !appleMedia.playback.known) {
        gfx->fillTriangle(x - 19, y - 15, x - 19, y + 15, x + 1, y, c);
        gfx->fillRect(x + 7, y - 15, 5, 30, c); gfx->fillRect(x + 16, y - 15, 5, 30, c);
      } else if (musicPlaying) {
        gfx->fillRect(x - 13, y - 15, 8, 30, c);
        gfx->fillRect(x + 5, y - 15, 8, 30, c);
      } else gfx->fillTriangle(x - 10, y - 17, x - 10, y + 17, x + 17, y, c);
    } else {
      int sign = i == 0 ? -1 : 1;
      gfx->fillTriangle(x - sign * 12, y - 15, x - sign * 12, y + 15, x + sign * 10, y, c);
      gfx->fillRect(x + sign * 15 - 3, y - 15, 6, 30, c);
    }
  }
  if (hidMusicMode && appleMedia.available) {
    double duration = AppleMediaData::duration(appleMedia.values[3].text.c_str());
    if (duration > 0 && appleMedia.playback.known) {
      double elapsed = appleMedia.playback.elapsed + appleMedia.playback.rate * (millis() - appleMedia.playbackAt) / 1000.0;
      int sec = (int)constrain(elapsed, 0.0, duration), total = (int)duration;
      char time[32]; snprintf(time, sizeof(time), "%d:%02d / %d:%02d", sec/60, sec%60, total/60, total%60);
      printCentered(time, CX, 392, 2, C_GREY);
    }
  } else printCentered("SWIPE LEFT TO RETURN", CX, 402, 2, C_GREY);
  drawPageDots();
  needFlush = true;
}

// 拖动状态
bool dragging = false;
int dragTarget = 0, dragSign = 1;
bool dragHoriz = true;
float dragP = 0, dragV = 0;                 // 新页面已经进来多少像素；速度（像素/毫秒）
unsigned long dragT = 0;

uint16_t *shownFrame() {
  if (fbBuf[1] && flushSrc) return flushSrc;
  return gfx->getFramebuffer();
}

static inline uint16_t dim565(uint16_t c, uint32_t k) {   // k: 0~32，32 = 原样
  uint32_t r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
  return (uint16_t)(((r * k >> 5) << 11) | ((g * k >> 5) << 5) | (b * k >> 5));
}

// 画一个页面到画布（不改当前页）
void renderPage(int p) {
  int keep = page;
  bool ks = sliding;
  page = p; sliding = false;
  if (p == 2) drawMusic();
  else if (p == 1) drawWeather();
  else if (inNav) drawNavFrame();
  else drawWaiting(bleConnected ? WAIT_READY : WAIT_NOLINK);
  page = keep; sliding = ks;
}

// 准备滑动：存下屏幕上的旧画面，在画布上画好目标页面存起来
bool slidePrepare(int target) {
  const size_t BYTES = (size_t)LCD_WIDTH * LCD_HEIGHT * 2;
  if (!slideOld) slideOld = (uint16_t *)heap_caps_malloc(BYTES, MALLOC_CAP_SPIRAM);
  if (!slideNew) slideNew = (uint16_t *)heap_caps_malloc(BYTES, MALLOC_CAP_SPIRAM);
  if (!slideOld || !slideNew) return false;
  if (!chordReady) {
    for (int y = 0; y < LCD_HEIGHT; y++) {
      float d = y + 0.5f - LCD_HEIGHT / 2.0f, r = LCD_WIDTH / 2.0f + 1;
      chordHalf[y] = d * d < r * r ? (int16_t)ceilf(sqrtf(r * r - d * d)) : 0;
    }
    chordReady = true;
  }
  if (fbBuf[1]) { flushWait(); xSemaphoreGive(flushDone); }   // 等正在推的那帧推完，旧画面才完整
  memcpy(slideOld, shownFrame(), BYTES);
  renderPage(target);
  memcpy(slideNew, gfx->getFramebuffer(), BYTES);
  sliding = true;
  needFlush = false; frameIsMap = false;
  return true;
}

// 合成一帧：新页面已经进来 p 像素（盖在上面），旧页面往同方向退 30%、变暗，交界处一道阴影
void slideCompose(int p) {
  const int W = LCD_WIDTH, H = LCD_HEIGHT;
  if (p < 0) p = 0;
  if (p > (dragHoriz ? W : H)) p = dragHoriz ? W : H;
  uint16_t *fb = gfx->getFramebuffer();
  int full = dragHoriz ? W : H;
  int q = p * 3 / 10;                                   // 旧页面退后的距离
  uint32_t k0 = 32 - (uint32_t)(p * 13 / full);         // 旧页面整体亮度：100% → 约 60%
  const int SH = 22;                                    // 阴影宽度
  if (dragHoriz) {
    static uint8_t colK[LCD_WIDTH];
    for (int x = 0; x < W; x++) {
      int dist = dragSign > 0 ? x - p : (W - p - 1) - x;   // 离交界多远（旧页面这一侧）
      uint32_t k = k0;
      if (p > 0 && dist < SH) k = k * (14 + dist * 18 / SH) / 32;
      colK[x] = (uint8_t)k;
    }
    for (int y = 0; y < H; y++) {
      int h = chordHalf[y];
      if (!h) continue;
      int xa = max(0, W / 2 - h), xb = min(W, W / 2 + h);
      uint16_t *row = fb + (size_t)y * W;
      const uint16_t *o = slideOld + (size_t)y * W, *n = slideNew + (size_t)y * W;
      if (dragSign > 0) {                               // 新页面从左边进来
        int a = xa, b = min(p, xb);
        if (b > a) memcpy(row + a, n + (W - p) + a, (b - a) * 2);
        for (int x = max(p, xa); x < xb; x++) row[x] = colK[x] >= 32 ? o[x - q] : dim565(o[x - q], colK[x]);
      } else {                                          // 新页面从右边进来
        int a = max(W - p, xa), b = xb;
        if (b > a) memcpy(row + a, n + (a - (W - p)), (b - a) * 2);
        for (int x = xa; x < min(W - p, xb); x++) row[x] = colK[x] >= 32 ? o[x + q] : dim565(o[x + q], colK[x]);
      }
      // 新页面前沿一道细亮边，看得出是一张“卡片”盖过来
      if (p > 1 && p < W - 1) {
        int ex = dragSign > 0 ? p - 1 : W - p;
        if (ex >= xa && ex < xb) row[ex] = 0x4A69;
        int ex2 = ex + (dragSign > 0 ? -1 : 1);
        if (ex2 >= xa && ex2 < xb) row[ex2] = 0x2945;
      }
    }
  } else {
    for (int y = 0; y < H; y++) {
      int h = chordHalf[y];
      if (!h) continue;
      int xa = max(0, W / 2 - h), xb = min(W, W / 2 + h);
      uint16_t *row = fb + (size_t)y * W;
      bool isNew = dragSign > 0 ? y < p : y >= H - p;
      if (isNew) {
        int sy = dragSign > 0 ? y + (H - p) : y - (H - p);
        memcpy(row + xa, slideNew + (size_t)sy * W + xa, (xb - xa) * 2);
        int edge = dragSign > 0 ? p - 1 : H - p;                // 前沿细亮边
        if (p > 1 && p < H - 1 && (y == edge || y == edge + (dragSign > 0 ? -1 : 1))) {
          uint16_t ec = y == edge ? 0x4A69 : 0x2945;
          for (int x = xa; x < xb; x++) row[x] = ec;
        }
      } else {
        int sy = dragSign > 0 ? y - q : y + q;
        int dist = dragSign > 0 ? y - p : (H - p - 1) - y;
        uint32_t k = k0;
        if (p > 0 && dist < SH) k = k * (14 + dist * 18 / SH) / 32;
        const uint16_t *o = slideOld + (size_t)sy * W;
        for (int x = xa; x < xb; x++) row[x] = k >= 32 ? o[x] : dim565(o[x], k);
      }
    }
  }
  flushAsync();                                         // 两块画布轮流：推这一帧的同时画下一帧
  needFlush = false; frameIsMap = false;
}

// 换页完成
void slideCommit() {
  const size_t BYTES = (size_t)LCD_WIDTH * LCD_HEIGHT * 2;
  sliding = false; dragging = false;
  page = dragTarget;
  pageAt = millis();
  memcpy(gfx->getFramebuffer(), slideNew, BYTES);
  needFlush = true; frameIsMap = false;
  if (page == 2) {
    onWaitScreen = false; musicDirty = true; pendingEvt = "T,M,Q";
  } else if (page == 1) {
    lastSlideSx = dragHoriz ? dragSign : 0; lastSlideSy = dragHoriz ? 0 : dragSign;
    onWaitScreen = false;
    wxDirty = false;
    if (!wxHave || millis() - wxAt > 15UL * 60 * 1000) { pendingEvt = "T,W"; wxAskAt = millis(); }
  } else {
    if (inNav) navDirty = true;
    else { riderPrevF = -1; riderPrevZ = -2; catForce = true; }
  }
}

// 没换成：回到原页面
void slideCancel() {
  sliding = false; dragging = false;
  renderPage(page);
  if (page == 2) musicDirty = true;
  else if (page == 1) wxDirty = false;
  else if (inNav) navDirty = true;
  else { onWaitScreen = true; catForce = true; }
  needFlush = true;
}

// 从 p0 顺势滑到 p1（先快后慢）；ms = 时长
void slideAnimate(float p0, float p1, int ms) {
  unsigned long t0 = millis();
  for (;;) {
    float t = (millis() - t0) / (float)ms;
    if (t >= 1) break;
    float e = 1 - (1 - t) * (1 - t) * (1 - t);         // ease-out
    slideCompose((int)(p0 + (p1 - p0) * e));
  }
  slideCompose((int)p1);
}

// 触摸向量统一换算：先修正芯片坐标的镜像，再按刷屏的逆旋转映射到画布。
// 位移和相对屏幕中心的点都使用这一个函数，避免旋转后点按与手势方向不一致。
void touchVectorToCanvas(float &dx, float &dy) {
  if (TOUCH_FLIP_X) dx = -dx;
  if (TOUCH_FLIP_Y) dy = -dy;
  float ox = dx, oy = dy;
  if (rotFree) { dx = ox * rotCosF + oy * rotSinF; dy = -ox * rotSinF + oy * rotCosF; }
  else if (screenRot == 1) { dx = oy; dy = -ox; }
  else if (screenRot == 2) { dx = -ox; dy = -oy; }
  else if (screenRot == 3) { dx = -oy; dy = ox; }
}

// 手指在拖：(dx, dy) = 从按下到现在的位移
void dragMove(int dx, int dy, unsigned long now) {
  float vx = dx, vy = dy;
  touchVectorToCanvas(vx, vy);
  dx = (int)lroundf(vx); dy = (int)lroundf(vy);
  if (!dragging) {
    int threshold = page == 2 ? 36 : DRAG_START;
    if (abs(dx) < threshold && abs(dy) < threshold) return;
    dragHoriz = abs(dx) >= abs(dy);
    int d = dragHoriz ? dx : dy;
    dragSign = d > 0 ? 1 : -1;
    if (page == 2) { if (!dragHoriz || dragSign > 0) return; dragTarget = 0; }
    else if (page == 1) dragTarget = 0;
    else if (dragHoriz) dragTarget = dragSign < 0 ? 1 : 2;
    else dragTarget = 1;
    if (!slidePrepare(dragTarget)) return;
    dragging = true;
    dragP = 0; dragV = 0; dragT = now;
  }
  float p = (dragHoriz ? dx : dy) * dragSign;
  if (p < 0) p = 0;
  float dt = now - dragT;
  if (dt > 0) dragV = dragV * 0.5f + ((p - dragP) / dt) * 0.5f;
  dragP = p; dragT = now;
  slideCompose((int)p);
}

// 松手：拖过 30% 或者往前甩出去 → 顺势滑完；否则弹回
void dragEnd() {
  if (!dragging) return;
  int full = dragHoriz ? LCD_WIDTH : LCD_HEIGHT;
  bool go = dragP > full * 0.3f || (dragV > 0.35f && dragP > 24);
  if (dragV < -0.35f) go = false;                       // 往回甩：取消
  float target = go ? full : 0;
  float left = fabsf(target - dragP);
  int ms = 140 + (int)(260 * left / full);
  if (go && dragV > 0.5f) ms = max(140, (int)(ms * 0.75f));   // 甩得快就滑得快
  slideAnimate(dragP, target, ms);
  if (go) slideCommit(); else slideCancel();
}

// 来电等情况：直接放弃拖动
void dragAbort() {
  if (!dragging) return;
  sliding = false; dragging = false;
  if (page == 2) musicDirty = true; else if (page == 1) wxDirty = true; else if (inNav) navDirty = true;
}

// 将触摸点换算到画布坐标，按钮使用较大的点按范围。
int musicButtonAt(int x, int y) {
  float dx = x - CX, dy = y - CX;
  touchVectorToCanvas(dx, dy);
  x = lroundf(CX + dx); y = lroundf(CX + dy);
  for (int i = 0; i < 3; i++) {
    int bx = CX + (i - 1) * 100;
    if ((x - bx) * (x - bx) + (y - 335) * (y - 335) <= 49 * 49) return i;
  }
  return -1;
}

void musicTap(int x, int y) {
  if (dragging || page != 2 || callOn || pairOn || !bleConnected || !bleAuthed) return;
  int button = musicButtonAt(x, y);
  if (hidMusicMode) {
    if (button < 0) return;
    bool hidReady = hidMediaReady();
    if (!hidReady && !appleMedia.controlReady(button)) { musicAck = 7; musicAckUntil = millis() + 2500; musicDirty = true; return; }
    if (hidTapAt && millis() - hidTapAt < 120) return;
    hidTapAt = millis();
    if (!hidReady) {
      musicAck = appleMedia.control(button) ? 11 : 10; musicAckUntil = millis() + 2500; musicDirty = true; return;
    }
    if (!hidPulses.enqueue(button)) { musicAck = 8; musicAckUntil = millis() + 2500; musicDirty = true; }
    return; // no T,M,P/T/N: do not also control Apple Music through the App
  }
  if (button < 0 || !musicHave) {
    pendingEvt = String("T,M,H,") + x + "," + y + "," + button;
    return;
  }
  pendingEvt = String("T,M,") + (button == 0 ? "P" : button == 1 ? "T" : "N") + "," + x + "," + y + "," + button;
  musicAck = 1; musicAckUntil = millis() + 2500; musicDirty = true;
}

// 自动换页（导航中看天气超时 / 快到路口）：整段动画
void showPageAnimated(int target, int sx, int sy) {
  if (page == target) return;
  dragHoriz = sx != 0 || sy == 0;
  dragSign = dragHoriz ? (sx ? sx : 1) : sy;
  dragTarget = target;
  if (!slidePrepare(target)) { page = target; renderPage(target); needFlush = true; return; }
  dragging = true;
  slideAnimate(0, dragHoriz ? LCD_WIDTH : LCD_HEIGHT, 420);
  slideCommit();
}

// 音乐页在路口前回导航，其余时候保持显示，便于连续切歌。
void musicTick() {
  if (musicAckUntil && (long)(millis() - musicAckUntil) >= 0) {
    if (musicAck == 1) { musicAck = 4; musicAckUntil = millis() + 2500; }
    else { musicAck = 0; musicAckUntil = 0; }
    musicDirty = true;
  }
  if (page != 2 || callOn || pairOn || sliding) return;
  if (inNav && !cruiseOn && navDir != "S" && navDir.length() && navDist > 0 && navDist < 200 && millis() - pageAt > 3000) {
    showPageAnimated(0, -1, 0);
    return;
  }
  if (musicDirty) { musicDirty = false; drawMusic(); }
}

// 每圈调用：天气页要不要重画、要不要回导航
void weatherTick() {
  if (page != 1 || callOn || sliding) return;
  unsigned long now = millis();
  // 导航中：看 15 秒自动回去；快到路口也立刻回去
  if (inNav && (now - pageAt > 15000 || (navDir != "S" && navDir.length() && navDist > 0 && navDist < 200))) {
    showPageAnimated(0, -lastSlideSx, -lastSlideSy);   // 按进来的方向反着滑回去
    return;
  }
  // 还没数据或数据旧了：30 秒再要一次
  if (bleConnected && (!wxHave || now - wxAt > 15UL * 60 * 1000) && now - wxAskAt > 30000) { pendingEvt = "T,W"; wxAskAt = now; }
  if (wxDirty) { wxDirty = false; drawWeather(); }
}

void drawStaleOverlay() {
  drawRedRing();
  printCentered("NO DATA", CX, 412, 3, C_RED);
  needFlush = true;
}

// ---- 进度环动画 ----
float ringShown = 0, ringTarget = 0;
bool ringActive = false;
String ringDir = "";

// ================= v2.0 小地图 =================
#define MAP_MAX      (200 * 1024)   // 地图块最大字节数
#define ROUTE_MAX    (96 * 1024)    // 路线最大字节数
#define FEAT_MAX     7200           // 地图块最多要素数
#define ROUTE_PTS    24000          // 路线最多点数
#define MAP_NEAR_M   150            // v2.1 离路口小于这个距离：圆牌放大、显示车道
#define MAP_PY       262            // 车头在屏幕上的 y 位置（偏下，多看前方）
#define POLY_PTS     4096           // 单个要素最多点数（画图用的临时数组）
#define BADGE_X      130            // 左下角转向小圆牌
#define BADGE_Y      334
#define BADGE_R      60

// v2.9 地图配色主题；v3.0 加上“走过的路线”颜色（done 内芯、done2 描边）和夜间配色
struct MapTheme { uint16_t block, road, park, water, route, route2, hud, blip, done, done2; };
#define THEME_N 12
const MapTheme THEMES[THEME_N] = {
  {0xDEFB, 0x3186, 0x5DEB, 0x4C9B, 0x249F, 0xFFFF, 0xFFFF, 0xFE80, 0x94B3, 0x5AEC},   // 0 原版
  {0xE6D9, 0x3187, 0x760D, 0x4E3B, 0xF258, 0x78EC, 0x6F5F, 0xF258, 0xA4B4, 0x62CC},   // 1 罪恶都市：暖沙色街区、青绿水面、粉紫路线
  {0x3A08, 0x9D15, 0x4327, 0x2A8F, 0xA2DF, 0x38EC, 0xFFFF, 0xFE87, 0x738F, 0x18E4},   // 2 GTA5：深色地面、浅色道路、紫色路线
  {0x0021, 0x04B6, 0x0103, 0x1847, 0xF815, 0x5007, 0xB7FF, 0xFF20, 0x39EA, 0x10A3},   // 3 赛博霓虹
  {0xEF18, 0xF5CD, 0xC6B3, 0x9E3A, 0xB0A3, 0x5841, 0xFFFF, 0x298F, 0xA490, 0x6B0A},   // 4 复古纸地图
  {0x0000, 0x8410, 0x0000, 0x0000, 0xFFFF, 0x39E7, 0xFFFF, 0xFFFF, 0x528A, 0x2104},   // 5 极简黑白
  {0x094B, 0xDF5F, 0x1A50, 0x2B35, 0xFE87, 0x08E8, 0xFFFF, 0xFE87, 0x6C76, 0x116B},   // 6 蓝图
  {0x8E0E, 0xE7DA, 0x334A, 0x08C4, 0x08C4, 0xE7DA, 0xE7DA, 0x08C4, 0x334A, 0x8E0E},   // 7 掌机像素
  {0x00C1, 0x0B46, 0x0183, 0x0084, 0x57F0, 0x01C2, 0x97F5, 0xCFF9, 0x1AC6, 0x0101},   // 8 雷达
  {0x0863, 0x29AA, 0x08C4, 0x08A6, 0xFD80, 0x3940, 0xFF35, 0xFD80, 0x6AC7, 0x18A1},   // 9 寻宝罗盘
  {0x5B08, 0x8C50, 0x3285, 0x29E8, 0x073F, 0x0188, 0xFFFF, 0xF9E7, 0xA514, 0x3186},   // 10 卫星图（照片没到时的替代色）
  {DragonRadar::DayBackground, 0, 0, 0, 0xFFF0, 0x0941, 0xFFFF, 0xFDC4, 0x7D69, 0x1283}, // 11 龙珠雷达（无底图）
};
// v3.0 夜间配色：地面压暗、道路变成浅色线条，路线保持鲜艳（晚上一眼就能看到）
const MapTheme THEMES_NIGHT[THEME_N] = {
  {0xDEFB, 0x3186, 0x5DEB, 0x4C9B, 0x249F, 0xFFFF, 0xFFFF, 0xFE80, 0x4A8B, 0x0862},   // 0 原版·夜：深蓝灰
  {0x20C7, 0x6271, 0x1207, 0x0A2B, 0xF258, 0x5047, 0x6F5F, 0xF258, 0x7370, 0x1063},   // 1 罪恶都市·夜：深紫街区、霓虹粉路线
  {0x10C3, 0x5AED, 0x1163, 0x1107, 0xAB3F, 0x30AB, 0xEF5D, 0xFE87, 0x4229, 0x0841},   // 2 GTA5·夜：更深的地面
  {0x0021, 0x04B6, 0x0103, 0x1847, 0xF815, 0x5007, 0xB7FF, 0xFF20, 0x39EA, 0x10A3},   // 3 赛博霓虹·夜
  {0x0021, 0x04B6, 0x0103, 0x1847, 0xF815, 0x5007, 0xB7FF, 0xFF20, 0x39EA, 0x10A3},   // 4 复古纸地图·夜 = 霓虹
  {0x0000, 0x8410, 0x0000, 0x0000, 0xFFFF, 0x39E7, 0xFFFF, 0xFFFF, 0x528A, 0x2104},   // 5 极简黑白·夜
  {0x00C7, 0xAE3D, 0x118B, 0x1A50, 0xFE87, 0x00A5, 0xFFFF, 0xFE87, 0x4B52, 0x08E7},   // 6 蓝图·夜
  {0x334A, 0x8E0E, 0x08C4, 0x08C4, 0xE7DA, 0x08C4, 0xE7DA, 0xE7DA, 0x334A, 0x08C4},   // 7 掌机像素·夜
  {0x00C1, 0x0B46, 0x0183, 0x0084, 0x57F0, 0x01C2, 0x97F5, 0xCFF9, 0x1AC6, 0x0101},   // 8 雷达·夜
  {0x0863, 0x29AA, 0x08C4, 0x08A6, 0xFD80, 0x3940, 0xFF35, 0xFD80, 0x6AC7, 0x18A1},   // 9 寻宝罗盘·夜
  {0x5B08, 0x8C50, 0x3285, 0x29E8, 0x073F, 0x0188, 0xFFFF, 0xF9E7, 0xA514, 0x3186},   // 10 卫星图·夜（照片压暗）
  {DragonRadar::NightBackground, 0, 0, 0, 0xFFF0, 0x0941, 0xFFFF, 0xFDC4, 0x7D69, 0x1283}, // 11 龙珠雷达（无底图）
};
int themeIdx = 1;                                  // 默认罪恶都市
bool curveOn = false;                              // v6.1 弯道急缓上色
#define ST_NEON 3
#define ST_PAPER 4
#define ST_MINIMAL 5
#define ST_BLUEPRINT 6
#define ST_PIXEL 7
#define ST_RADAR 8
#define ST_COMPASS 9
#define ST_SAT 10
#define ST_DRAGON 11
bool nightOn = false;                              // v3.0 夜间配色（手机按日出日落发 "#n,1/0"）
int trailMode = 1;                                 // v3.0 走过的路线：1 变灰，2 擦除（"#r,1/2"）
// v6.1 实际用的风格：复古纸地图到了夜间配色时换成霓虹
static inline int styleIdx() { return (nightOn && themeIdx == ST_PAPER) ? ST_NEON : themeIdx; }
#define TH       ((nightOn ? THEMES_NIGHT : THEMES)[themeIdx])
#define C_BLOCK  (TH.block)
#define C_ROADM  (TH.road)
#define C_PARK   (TH.park)
#define C_WATER  (TH.water)
#define C_ROUTE  (TH.route)
#define C_ROUTE2 (TH.route2)

static inline int16_t rd16(const uint8_t *p) { return (int16_t)(p[0] | (p[1] << 8)); }
static inline uint16_t rdu16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static inline int32_t rd32(const uint8_t *p) { return (int32_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24)); }

// ---- 大块数据接收（'M'）----
struct Bulk {
  uint8_t *buf; uint32_t cap;
  int id; uint32_t total; uint16_t parts; uint16_t got; bool done;
  uint8_t mask[512];        // 每片一位，最多 4096 片
};
Bulk bulks[5];              // 下标 1 = 地图块，2 = 路线，3 = 图片地图（v2.5），4 = 原路线（v4.4）

// ---- v4.4 原路线（导入的高德分享路线）：导航时画成一条淡色细线，走错路时能看到原来该走哪 ----
#define GHOST_PTS 6000
#define GHOST_MAX (12 + 4 * GHOST_PTS + 64)
int32_t *ghostX = nullptr, *ghostY = nullptr, *gbBox = nullptr;
int ghostN = 0, ghostFrame = -1;

// ---- v2.5 图片地图（天地图）----
#include "RasterContinuity.h"
#define RASTER_MAX_BYTES (500 * 1024)   // 压缩后最大字节数
#define RASTER_MAX_PX    (1300 * 1300)  // 最多像素数
#define RASTER_BUF_BYTES (((1300 + 15) / 16) * ((1300 + 15) / 16) * 128)   // v2.6 按 16×16 小块存放
int rasBW = 0;                          // 每行多少个小块
uint8_t *rasterBuf = nullptr;           // 每像素 4 位颜色编号
bool rasterValid = false;
uint8_t *rasterBackBuf = nullptr;
RasterHeader rasterBackHeader = {};
bool rasterBackValid = false;

int rasterFrame = -1;
int32_t rasX0 = 0, rasY0 = 0;           // 左上角（米）
int rasW = 0, rasH = 0;
float rasMpp = 2.0f;                    // 每像素多少米
unsigned long rasterAt = 0, vectorAt = 0;
bool pendingBuildingCapability = false;
String pendingAck = "";     // 待发给手机的确认

// ---- 地图块（正在显示的）----
uint8_t *mapShow = nullptr;
bool mapValid = false;
int mapFrame = -1;
int32_t mapOx = 0, mapOy = 0;     // 地图块原点（米，手机坐标系）
int mapFeatN = 0;
uint32_t *featOff = nullptr;      // 每个要素的点数据在 mapShow 里的位置
uint16_t *featNp = nullptr;       // 点数
uint8_t *featCls = nullptr;       // 类别（最高位 = 面）
int16_t *featBox = nullptr;       // 外框 minx,miny,maxx,maxy（相对原点，米）

// ---- 路线 ----
int32_t *routeX = nullptr, *routeY = nullptr;
int routeN = 0, routeFrame = -1;
int32_t *rbBox = nullptr;         // 每 32 个点一组的外框
#define RB_STEP 32
#define LIGHT_MAX 600
int32_t *lightX = nullptr, *lightY = nullptr;   // v3.2 路线上的红绿灯（米）
float *lightU = nullptr;                        // v3.2 红绿灯在路线上的位置（段号 + 段内比例）
int lightN = 0;
float routeProg = -1;             // v3.0 车在路线上走到哪了：段号 + 段内比例（-1 = 还没对上）
float *routeCum = nullptr;        // v5.0 每个路线点离起点的路程（米），给路况颜色用
uint8_t *routeCurv = nullptr;     // v6.1 每个路线点的弯急程度：0 直/缓，1 中弯，2 急弯
// v5.0 路况：缓行/拥堵的路段（离起点的米数），"#u,起,止,状态,..."；状态 2 缓行 3 拥堵 4 严重拥堵
#define TRAF_MAX 40
int trafN = 0;
int32_t trafA[TRAF_MAX], trafB[TRAF_MAX];
uint8_t trafS[TRAF_MAX];
// v5.0 提示条：重新规划中（"#y,1/0"）、更快路线（"#h,省几分钟" / "#h"）、GPS 信号弱（"#x,1/0"）
unsigned long rerouteAt = 0, fasterAt = 0;
int fasterMin = 0;
bool gpsWeak = false;
unsigned long routeSearchAt = 0;  // v3.0 上次全路线搜索的时间

// ---- 当前位置 ----
bool mapOn = true;
int posFrame = -1;
float posX = 0, posY = 0, posHdg = 0, posKmh = 0;
unsigned long posAt = 0;
float viewHdg = 0, viewScale = 0.7f;
float corrX = 0, corrY = 0;          // v2.6 位置偏差（米），逐渐减小到 0
unsigned long corrAt = 0;
float hdgRate = 0;                   // v6.7 正在转弯的速度（度/秒，顺时针为正）
int leadMs = 300;                    // v6.7 预判提前量（毫秒）
float viewTargetHdg = 0;             // v6.7 地图朝向要追的目标（预判后的朝向）

// v6.7 推算 t 时刻（再加 lead 秒）的位置和朝向：按当前车速和转弯速度沿弧线往前推（最多推 1.5 秒）
void predictPose(unsigned long t, float lead, float &x, float &y, float &h) {
  float tau = fminf((t - posAt) / 1000.0f + lead, 1.5f);
  if (tau < 0) tau = 0;
  float v = posKmh / 3.6f;
  float rate = (t - posAt) > 2500 ? 0 : hdgRate;        // 很久没新定位（停车等）：不再按转弯推
  float hEnd = posHdg + rate * tau;
  float hMid = (posHdg + hEnd) * 0.5f * PI / 180;      // 弧线用中间的朝向近似
  x = posX + sinf(hMid) * v * tau;
  y = posY + cosf(hMid) * v * tau;
  h = hEnd;
}
bool viewInit = false;
unsigned long lastMapDraw = 0;

// 画图用的临时数组（内部内存，速度快）
float *tmpX = nullptr, *tmpY = nullptr;

void mapInit() {
  bulks[1].cap = MAP_MAX;   bulks[1].buf = (uint8_t *)ps_malloc(MAP_MAX);
  bulks[2].cap = ROUTE_MAX; bulks[2].buf = (uint8_t *)ps_malloc(ROUTE_MAX);
  bulks[3].cap = RASTER_MAX_BYTES; bulks[3].buf = (uint8_t *)ps_malloc(RASTER_MAX_BYTES);
  bulks[4].cap = GHOST_MAX; bulks[4].buf = (uint8_t *)ps_malloc(GHOST_MAX);          // v4.4
  ghostX = (int32_t *)ps_malloc(sizeof(int32_t) * GHOST_PTS);
  ghostY = (int32_t *)ps_malloc(sizeof(int32_t) * GHOST_PTS);
  gbBox  = (int32_t *)ps_malloc(sizeof(int32_t) * 4 * (GHOST_PTS / RB_STEP + 2));
  rasterBuf = (uint8_t *)ps_malloc(RASTER_BUF_BYTES);
  for (int t = 1; t <= 4; t++) { bulks[t].id = -1; bulks[t].done = false; }
  mapShow = (uint8_t *)ps_malloc(MAP_MAX);
  featOff = (uint32_t *)ps_malloc(sizeof(uint32_t) * FEAT_MAX);
  featNp  = (uint16_t *)ps_malloc(sizeof(uint16_t) * FEAT_MAX);
  featCls = (uint8_t *)ps_malloc(FEAT_MAX);
  featBox = (int16_t *)ps_malloc(sizeof(int16_t) * 4 * FEAT_MAX);
  routeX = (int32_t *)ps_malloc(sizeof(int32_t) * ROUTE_PTS);
  routeY = (int32_t *)ps_malloc(sizeof(int32_t) * ROUTE_PTS);
  rbBox  = (int32_t *)ps_malloc(sizeof(int32_t) * 4 * (ROUTE_PTS / RB_STEP + 2));
  routeCum = (float *)ps_malloc(sizeof(float) * ROUTE_PTS);                         // v5.0
  routeCurv = (uint8_t *)ps_malloc(ROUTE_PTS);                                        // v6.1
  lightX = (int32_t *)ps_malloc(sizeof(int32_t) * LIGHT_MAX);
  lightY = (int32_t *)ps_malloc(sizeof(int32_t) * LIGHT_MAX);
  lightU = (float *)ps_malloc(sizeof(float) * LIGHT_MAX);
  tmpX = (float *)malloc(sizeof(float) * POLY_PTS);
  tmpY = (float *)malloc(sizeof(float) * POLY_PTS);
}

// 解析刚收齐的地图块，建立索引
void parseMap(const uint8_t *p, uint32_t len) {
  mapValid = false;
  if (len < 12) return;
  int frame = rdu16(p);
  int32_t ox = rd32(p + 2), oy = rd32(p + 6);
  int n = rdu16(p + 10);
  uint32_t pos = 12;
  int cnt = 0;
  for (int i = 0; i < n && cnt < FEAT_MAX; i++) {
    if (pos + 3 > len) break;
    uint8_t cls = p[pos];
    uint16_t np = rdu16(p + pos + 1);
    pos += 3;
    if (pos + (uint32_t)np * 4 > len) break;
    int16_t x0 = 32767, y0 = 32767, x1 = -32768, y1 = -32768;
    for (int k = 0; k < np; k++) {
      int16_t x = rd16(p + pos + 4 * k), y = rd16(p + pos + 4 * k + 2);
      if (x < x0) x0 = x;
      if (x > x1) x1 = x;
      if (y < y0) y0 = y;
      if (y > y1) y1 = y;
    }
    featCls[cnt] = cls; featNp[cnt] = np; featOff[cnt] = pos;
    featBox[4 * cnt] = x0; featBox[4 * cnt + 1] = y0; featBox[4 * cnt + 2] = x1; featBox[4 * cnt + 3] = y1;
    cnt++;
    pos += (uint32_t)np * 4;
  }
  mapFeatN = cnt; mapOx = ox; mapOy = oy; mapFrame = frame;
  mapValid = cnt > 0;
  vectorAt = millis();
}

// v2.5 解析图片地图：u16 坐标系, i32 左上角 x, i32 左上角 y, u16 宽, u16 高, u16 每像素毫米, 然后是压缩的颜色编号
static inline uint16_t blendF(uint16_t a, uint16_t b, uint32_t t);   // v6.1 后面定义
bool rasHasPal = false;                    // v6.1 卫星图自带调色板
uint16_t rasPal[16];

// 罪恶都市局部边界几何；其它风格仍读取原来的 4 位材质图。
uint16_t *rasterVice = nullptr;
size_t rasterAllocatedBytes = RASTER_BUF_BYTES;
bool rasterViceValid = false;
tinfl_decompressor *viceInflater = nullptr;
uint16_t viceEdgePal[7][257];
uint16_t vicePalKey[4] = {0xffff, 0xffff, 0xffff, 0xffff};
// 覆盖率按像素方形与半平面的相交面积计算。利用镜像对称，只需保存九个角度的表。
int8_t viceNX[64], viceNY[64];
int32_t viceOffsets[2][32];
uint16_t viceCoverage[9][257];
bool rasterViceFine = false;
uint8_t viceBaseShift = 13;
uint8_t rasterViceVersion = 0;
const uint16_t *vicePatchData = nullptr;
uint16_t vicePatchCount = 0;

void initViceSampling() {
  static bool ready = false;
  if (ready) return;
  for (int i = 0; i < 64; i++) {
    float a = i * (2.0f * PI / 64.0f);
    viceNX[i] = (int8_t)lroundf(cosf(a) * 127);
    viceNY[i] = (int8_t)lroundf(sinf(a) * 127);
  }
  const float offsets[16] = {0.0f, 0.0625f, 0.125f, 0.1875f,
    0.25f, 0.3125f, 0.375f, 0.4375f, 0.5f, 0.5625f,
    0.625f, 0.6875f, 0.8125f, 1.0f, 1.25f, 1.625f};
  for (int i = 0; i < 32; i++) {
    viceOffsets[0][i] = (i < 16 ? i : i - 32) * 4064;
    viceOffsets[1][i] = (int32_t)lroundf(offsets[i & 15] * (i & 16 ? -32512.0f : 32512.0f));
  }
  for (int angle = 0; angle <= 8; angle++) {
    float a = angle * (PI / 32.0f);
    float major = cosf(a), minor = sinf(a);
    for (int i = 0; i <= 256; i++) {
      float distance = (i - 128) * (1.5f / 256.0f);
      float t = distance + (major + minor) * 0.5f;
      float coverage;
      if (t <= 0) coverage = 0;
      else if (t >= major + minor) coverage = 1;
      else if (minor < 0.00001f) coverage = t / major;
      else if (t < minor) coverage = t * t / (2 * major * minor);
      else if (t <= major) coverage = (t - minor * 0.5f) / major;
      else { float tail = major + minor - t; coverage = 1 - tail * tail / (2 * major * minor); }
      viceCoverage[angle][i] = (uint16_t)max(0, min(256, (int)lroundf(coverage * 256.0f)));
    }
  }
  ready = true;
}

static inline uint8_t viceCoverageAngle(float nx, float ny) {
  int angle = (int)lroundf(atan2f(fabsf(ny), fabsf(nx)) * (32.0f / PI));
  if (angle > 8) angle = 16 - angle;
  return (uint8_t)max(0, min(8, angle));
}


void parseViceGeometry(const uint8_t *p, uint32_t len, uint32_t pos, int bw, int bh) {
  rasterViceValid = false;
  vicePatchCount = 0; vicePatchData = nullptr;
  if (pos <= len && len - pos >= 8 && p[pos] == 'V' && p[pos + 1] == 'C' && p[pos + 2] == 'E' &&
      p[pos + 3] >= 1 && p[pos + 3] <= 3) {
    uint8_t version = p[pos + 3];
    uint32_t packed = (uint32_t)rd32(p + pos + 4);
    uint32_t header = version == 3 ? 12 : 8;
    if (len - pos < header) return;
    uint16_t patchCount = version == 3 ? rdu16(p + pos + 8) : 0;
    if (patchCount > 2047 || (version == 3 && rdu16(p + pos + 10) != 0)) return;
    size_t cellsBytes = (size_t)bw * bh * 512;
    size_t needed = cellsBytes + (size_t)patchCount * 8;
    if (packed == len - pos - header && needed <= RASTER_BUF_BYTES * 4 + 2047 * 8) {
      if (rasterAllocatedBytes != needed) {
        free(rasterBuf);
        rasterBuf = (uint8_t *)ps_malloc(needed);
        rasterAllocatedBytes = rasterBuf ? needed : 0;
      }
      rasterVice = (uint16_t *)rasterBuf;
      if (!viceInflater) viceInflater = (tinfl_decompressor *)malloc(sizeof(tinfl_decompressor));
      if (rasterVice && rasterAllocatedBytes == needed && viceInflater) {
        tinfl_init(viceInflater);
        size_t inBytes = packed, outBytes = needed;
        tinfl_status result = tinfl_decompress(viceInflater, p + pos + header, &inBytes,
            (uint8_t *)rasterVice, (uint8_t *)rasterVice, &outBytes,
            TINFL_FLAG_PARSE_ZLIB_HEADER | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
        rasterViceValid = result == TINFL_STATUS_DONE && inBytes == packed && outBytes == needed;
        const uint16_t *vertices = (const uint16_t *)(rasterBuf + cellsBytes);
        if (rasterViceValid) {
          for (uint32_t i = 0; i < (uint32_t)patchCount * 4; i++) {
            uint16_t v = vertices[i];
            if ((v & 15) + ((v >> 4) & 15) + ((v >> 8) & 15) + (v >> 12) != 15) {
              rasterViceValid = false; break;
            }
          }
        }
        if (rasterViceValid) {
          rasterViceVersion = version;
          rasterViceFine = version >= 2;
          viceBaseShift = rasterViceFine ? 14 : 13;
          vicePatchCount = patchCount; vicePatchData = patchCount ? vertices : nullptr;
        }
      }
    }
  }
  if (!rasterViceValid) rasterVice = nullptr;
}

void prepareVicePalette(const uint16_t *pal) {
  static bool initialized = false;
  initViceSampling();
  bool changed = !initialized;
  for (int i = 0; i < 4; i++) if (vicePalKey[i] != pal[i]) changed = true;
  if (!changed) return;
  const uint8_t pairA[7] = {0, 0, 0, 0, 1, 1, 2};
  const uint8_t pairB[7] = {0, 1, 2, 3, 2, 3, 3};
  for (int p = 1; p <= 6; p++) for (int a = 0; a <= 256; a++)
    viceEdgePal[p][a] = blendF(pal[pairA[p]], pal[pairB[p]], a);
  memcpy(vicePalKey, pal, sizeof(vicePalKey));
  initialized = true;
}

void parseRasterLegacy(const uint8_t *p, uint32_t len) {
  rasterValid = false;
  rasterViceValid = false;
  if (len < 16) return;
  int frame = rdu16(p);
  int32_t x0 = rd32(p + 2), y0 = rd32(p + 6);
  int w = rdu16(p + 10), h = rdu16(p + 12), mm = rdu16(p + 14);
  bool hasPal = (mm & 0x8000) != 0;
  mm &= 0x7FFF;
  uint16_t palNew[16];
  if (hasPal) {
    if (len < 48) return;
    for (int i = 0; i < 16; i++) palNew[i] = rdu16(p + 16 + 2 * i);
  }
  uint32_t n = (uint32_t)w * h;
  if (w == 0 || h == 0 || n > RASTER_MAX_PX || mm == 0) return;
  int bw = (w + 15) >> 4, bh = (h + 15) >> 4;
  size_t legacyBytes = (size_t)bw * bh * 128;
  if (legacyBytes > RASTER_BUF_BYTES) return;
  uint32_t dataStart = hasPal ? 48 : 16;
  // 先检查普通图的游程长度，找到扩展的位置；不另存一份解压后的普通图。
  uint32_t pos = dataStart, i = 0;
  while (pos < len && i < n) {
    uint8_t b = p[pos++], r = b >> 4;
    uint32_t run = r + 1;
    if (r == 15) { if (pos >= len) return; run = 16 + p[pos++]; }
    if (run > n - i) return;
    i += run;
  }
  if (i != n) return;
  if (!hasPal) parseViceGeometry(p, len, pos, bw, bh);
  if (!rasterViceValid) {
    // 不带扩展的地图、卫星图或扩展解码失败，回到原来的 4 位格式。
    if (!rasterBuf || rasterAllocatedBytes != RASTER_BUF_BYTES) {
      free(rasterBuf);
      rasterBuf = (uint8_t *)ps_malloc(RASTER_BUF_BYTES);
      rasterAllocatedBytes = rasterBuf ? RASTER_BUF_BYTES : 0;
    }
    rasterVice = nullptr;
    if (!rasterBuf) return;
    memset(rasterBuf, 0, legacyBytes);
    pos = dataStart; i = 0;
    int x = 0, y = 0;
    while (i < n) {
      uint8_t b = p[pos++], c = b & 15, r = b >> 4;
      uint32_t run = r + 1;
      if (r == 15) run = 16 + p[pos++];
      if (c) {
        for (uint32_t k = 0; k < run; k++) {
          uint32_t a = ((uint32_t)((y >> 4) * bw + (x >> 4)) << 8) | ((y & 15) << 4) | (x & 15);
          rasterBuf[a >> 1] |= (a & 1) ? (c << 4) : c;
          if (++x == w) { x = 0; y++; }
        }
      } else {
        x += run;
        while (x >= w) { x -= w; y++; }
      }
      i += run;
    }
  }
  rasBW = bw;
  rasX0 = x0; rasY0 = y0; rasW = w; rasH = h; rasMpp = mm / 1000.0f; rasterFrame = frame;
  rasHasPal = hasPal;
  if (hasPal) memcpy(rasPal, palNew, sizeof(rasPal));
  rasterValid = true;
  rasterAt = millis();
}

// 普通地图先在备用缓冲解码，成功后交换；上一块保留，用来补齐新块的覆盖边缘。
bool parseRaster(const uint8_t *p, uint32_t len) {
  RasterHeader next;
  if (!rasterCheck(p,len,next,RASTER_MAX_PX,RASTER_BUF_BYTES)) return false;
  if (next.dataEnd != len) {
    // 保留既有轮廓扩展的兼容路径；新 App 只发送无扩展的普通底图。
    if (len-next.dataEnd<8 || p[next.dataEnd]!='V' || p[next.dataEnd+1]!='C' || p[next.dataEnd+2]!='E') return false;
    rasterBackValid=false;
    parseRasterLegacy(p,len);
    return rasterValid;
  }
  if (!rasterBackBuf) rasterBackBuf=(uint8_t *)ps_malloc(RASTER_BUF_BYTES);
  if (!rasterBackBuf) return false;
  rasterDecode(p,next,rasterBackBuf);
  RasterHeader old = {};
  old.frame=rasterFrame; old.x0=rasX0; old.y0=rasY0; old.width=rasW; old.height=rasH;
  old.mm=(uint16_t)lroundf(rasMpp*1000); old.blocksWide=rasBW;
  bool keep=rasterValid && !rasterViceValid && !rasHasPal && !next.palette &&
      rasterFrame==next.frame && old.mm==next.mm && rasterAllocatedBytes==RASTER_BUF_BYTES;
  uint8_t *oldPixels=rasterBuf;
  rasterBuf=rasterBackBuf;
  rasterBackBuf=oldPixels;
  if (rasterAllocatedBytes!=RASTER_BUF_BYTES) { free(rasterBackBuf); rasterBackBuf=nullptr; keep=false; }
  rasterAllocatedBytes=RASTER_BUF_BYTES;
  rasterBackHeader=old; rasterBackValid=keep;
  rasterVice=nullptr; rasterViceValid=false; vicePatchData=nullptr; vicePatchCount=0;
  rasBW=next.blocksWide; rasX0=next.x0; rasY0=next.y0; rasW=next.width; rasH=next.height;
  rasMpp=next.mm/1000.0f; rasterFrame=next.frame; rasHasPal=next.palette;
  if (next.palette) for (int i=0;i<16;i++) rasPal[i]=rasterU16(p+16+2*i);
  rasterValid=true; rasterAt=millis();
  return true;
}

// ---- v5.7 第二个核心帮忙画：把逐行的计算分给两个核心（各画一半的行）----
TaskHandle_t helperTask = nullptr;
SemaphoreHandle_t helpGo = nullptr, helpDone = nullptr;
void (*volatile helpFn)(int y0, int y1, int step) = nullptr;
volatile int helpY0 = 0, helpY1 = 0;

void helperLoop(void *) {
  for (;;) {
    xSemaphoreTake(helpGo, portMAX_DELAY);
    if (helpFn) helpFn(helpY0, helpY1, 1);
    xSemaphoreGive(helpDone);
  }
}

void helperInit() {
  helpGo = xSemaphoreCreateBinary();
  helpDone = xSemaphoreCreateBinary();
  // v5.8 优先级比推屏任务高：画底图只要几毫秒，推屏晚一点开始没关系（它不卡画面）
  if (xTaskCreatePinnedToCore(helperLoop, "help", 4096, nullptr, 3, &helperTask, 0) != pdPASS) helperTask = nullptr;   // v5.8 优先级 1 → 3
}

// 把 [y0, y1) 这些行分给两个核心：v6.0 主核心画上半，另一个核心画下半（圆形的东西上下对称，活一样多）
void parallelRows(void (*fn)(int, int, int), int y0, int y1) {
  if (!helperTask) { fn(y0, y1, 1); return; }
  int mid = (y0 + y1) / 2;
  helpFn = fn; helpY0 = mid; helpY1 = y1;
  xSemaphoreGive(helpGo);
  fn(y0, mid, 1);
  xSemaphoreTake(helpDone, portMAX_DELAY);
}

// v2.5 画图片地图：屏幕上每个像素反算到地图图片上的位置，取颜色（只画圆环以内）
// v5.7 改成按行分给两个核心画（参数放在全局变量里）
struct { float cx, cy, sc, cs, sn; uint16_t pal[16]; uint16_t *fb; bool pix2; int32_t viceGain; int viceRotation; } RM;
#define PIXB 3                                      // v6.1 掌机像素：3×3 个屏幕像素当一个大像素

void rasterRows(int y0, int y1, int step) {
  uint16_t *fb = RM.fb;
  const uint16_t *pal = RM.pal;
  float cx = RM.cx, cy = RM.cy, sc = RM.sc, cs = RM.cs, sn = RM.sn;
  float inv = 1.0f / sc, kpx = inv / rasMpp;
  int32_t DU = (int32_t)(cs * kpx * 65536.0f), DV = (int32_t)(sn * kpx * 65536.0f);
  const bool useBack=rasterBackValid && rasterBackHeader.frame==rasterFrame && !rasHasPal && !rasterViceValid;
  const int backU=useBack ? (int)lroundf((rasX0-rasterBackHeader.x0)/rasMpp) : 0;
  const int backV=useBack ? (int)lroundf((rasterBackHeader.y0-rasY0)/rasMpp) : 0;
  const float R = 212;
  for (int y = y0; y < y1; y += step) {
    float dyc = y + 0.5f - CX;
    if (RM.pix2 && (y % PIXB) && y > y0 && fabsf(dyc) < R - 2) {  // v6.1 掌机像素：每 3 行只算第一行，其余照抄
      float half = sqrtf(R * R - dyc * dyc);
      int xa = max(0, (int)(CX - half)), xb = min(LCD_WIDTH - 1, (int)(CX + half));
      memcpy(fb + y * LCD_WIDTH + xa, fb + (y - 1) * LCD_WIDTH + xa, (xb - xa + 1) * 2);
      continue;
    }
    if (fabsf(dyc) >= R) continue;
    float half = sqrtf(R * R - dyc * dyc);
    int xa = max(0, (int)(CX - half)), xb = min(LCD_WIDTH - 1, (int)(CX + half));
    if (RM.pix2) xa -= xa % PIXB;                       // 掌机像素：列也按 3 个一组对齐
    float rx = (xa + 0.5f - CX) * inv, ry = (MAP_PY - (y + 0.5f)) * inv;
    float dx = rx * cs + ry * sn, dy = -rx * sn + ry * cs;
    float u = (cx + dx - rasX0) / rasMpp, v = (rasY0 - (cy + dy)) / rasMpp;
    int32_t U = (int32_t)(u * 65536.0f), V = (int32_t)(v * 65536.0f);
    uint16_t *row = fb + y * LCD_WIDTH;
    int xstep = RM.pix2 ? PIXB : 1;
    if (RM.pix2) { DU *= PIXB; DV *= PIXB; }
    for (int x = xa; x <= xb; x += xstep) {
      int32_t ui = U >> 16, vi = V >> 16;
      uint8_t idx = 0;
      if ((uint32_t)ui < (uint32_t)rasW && (uint32_t)vi < (uint32_t)rasH) {
        uint32_t k = ((uint32_t)((vi >> 4) * rasBW + (ui >> 4)) << 8) | ((vi & 15) << 4) | (ui & 15);
        if (rasterViceValid) {
          idx = (rasterVice[k] >> viceBaseShift) & 3;
        } else {
          uint8_t b = rasterBuf[k >> 1];
          idx = (k & 1) ? (b >> 4) : (b & 15);
        }
      } else if (useBack) {
        rasterRead(rasterBackBuf,rasterBackHeader,ui+backU,vi+backV,idx);
      }
      row[x] = pal[idx];
      for (int k = 1; k < xstep && x + k <= xb; k++) row[x + k] = pal[idx];
      U += DU; V += DV;
    }
    if (RM.pix2) { DU /= PIXB; DV /= PIXB; }
  }
}

static inline uint16_t vicePatchColor(uint16_t cell, int fx, int fy) {
  uint16_t base = RM.pal[(cell >> 14) & 3];
  uint32_t index = cell & 2047;
  if (index >= vicePatchCount || !vicePatchData) return base;
  const uint16_t *q = vicePatchData + index * 4;
  int tx = max(0, min(256, fx + 128)), ty = max(0, min(256, fy + 128));
  uint32_t r = 0, g = 0, b = 0;
  for (int c = 0; c < 4; c++) {
    int shift = c * 4;
    int a = (q[0] >> shift) & 15, e = (q[1] >> shift) & 15;
    int f = (q[2] >> shift) & 15, d = (q[3] >> shift) & 15;
    int top = a * 256 + (e - a) * tx, bottom = f * 256 + (d - f) * tx;
    uint32_t mass = top * (256 - ty) + bottom * ty;
    uint16_t color = RM.pal[c];
    r += (color >> 11) * mass;
    g += ((color >> 5) & 63) * mass;
    b += (color & 31) * mass;
  }
  // 每个顶点的四材质权重之和固定为 15；双线性权重之和固定为 65536。
  return (uint16_t)((((r + 491520) / 983040) << 11) |
      (((g + 491520) / 983040) << 5) | ((b + 491520) / 983040));
}

static inline bool viceReadColor(int x, int y, int fx, int fy, uint16_t &color) {
  if ((uint32_t)x >= (uint32_t)rasW || (uint32_t)y >= (uint32_t)rasH) return false;
  uint32_t k = ((uint32_t)((y >> 4) * rasBW + (x >> 4)) << 8) | ((y & 15) << 4) | (x & 15);
  uint16_t cell = rasterVice[k];
  int pair = (cell >> 11) & 7;
  color = RM.pal[(cell >> 14) & 3];
  if (pair == 7) { color = vicePatchColor(cell, fx, fy); return true; }
  if (pair == 0) return true;
  int direction = cell & 63, offset = (cell >> 6) & 31;
  int distance = viceNX[direction] * fx + viceNY[direction] * fy + viceOffsets[1][offset];
  int index = 128 + (int)((distance * RM.viceGain) >> 16);
  index = max(0, min(256, index));
  int angle = (direction - RM.viceRotation) & 15;
  if (angle > 8) angle = 16 - angle;
  color = viceEdgePal[pair][viceCoverage[angle][index]];
  return true;
}

static inline uint16_t viceJoinColor(uint16_t color, int x, int y, int qx, int qy) {
  // 仅在格子交界的 1/8 源像素带内过渡；内部轮廓保持原来的清晰度。
  int sx = qx < 32 ? -1 : (qx > 224 ? 1 : 0);
  int sy = qy < 32 ? -1 : (qy > 224 ? 1 : 0);
  int wx = sx < 0 ? (32 - qx) * 4 : (sx > 0 ? (qx - 224) * 4 : 0);
  int wy = sy < 0 ? (32 - qy) * 4 : (sy > 0 ? (qy - 224) * 4 : 0);
  int fx = qx - 128, fy = qy - 128;
  uint16_t bx = color, by = color, diagonal = color;
  if (wx && !viceReadColor(x + sx, y, fx - sx * 256, fy, bx)) wx = 0;
  if (wy && !viceReadColor(x, y + sy, fx, fy - sy * 256, by)) wy = 0;
  if (!wx) return wy ? blendF(color, by, wy) : color;
  if (!wy) return blendF(color, bx, wx);
  if (!viceReadColor(x + sx, y + sy, fx - sx * 256, fy - sy * 256, diagonal)) diagonal = by;
  uint32_t weights[4] = {(uint32_t)((256 - wx) * (256 - wy)),
    (uint32_t)(wx * (256 - wy)), (uint32_t)((256 - wx) * wy), (uint32_t)(wx * wy)};
  uint16_t colors[4] = {color, bx, by, diagonal};
  uint32_t r = 0, g = 0, b = 0;
  for (int i = 0; i < 4; i++) {
    r += (colors[i] >> 11) * weights[i];
    g += ((colors[i] >> 5) & 63) * weights[i];
    b += (colors[i] & 31) * weights[i];
  }
  return (uint16_t)((((r + 32768) >> 16) << 11) |
      (((g + 32768) >> 16) << 5) | ((b + 32768) >> 16));
}

void rasterViceRows(int y0, int y1, int step) {
  uint16_t *fb = RM.fb;
  const uint16_t *pal = RM.pal;
  const uint16_t *cells = rasterVice;
  float inv = 1.0f / RM.sc, kpx = inv / rasMpp;
  int32_t du = (int32_t)(RM.cs * kpx * 65536.0f), dv = (int32_t)(RM.sn * kpx * 65536.0f);
  const int32_t gain = RM.viceGain;
  const int pairShift = rasterViceFine ? 11 : 10;
  const int offsetShift = rasterViceFine ? 6 : 5;
  const int baseShift = viceBaseShift;
  const int32_t *offsets = viceOffsets[rasterViceFine ? 1 : 0];
  const bool fine = rasterViceFine;
  const bool joins = rasterViceVersion == 3;
  const int rotation = RM.viceRotation;
  for (int y = y0; y < y1; y += step) {
    float dyc = y + 0.5f - CX;
    if (fabsf(dyc) >= 212) continue;
    float half = sqrtf(212.0f * 212.0f - dyc * dyc);
    int xa = max(0, (int)(CX - half)), xb = min(LCD_WIDTH - 1, (int)(CX + half));
    float rx = (xa + 0.5f - CX) * inv, ry = (MAP_PY - (y + 0.5f)) * inv;
    float dx = rx * RM.cs + ry * RM.sn, dy = -rx * RM.sn + ry * RM.cs;
    int32_t u = (int32_t)(((RM.cx + dx - rasX0) / rasMpp) * 65536.0f);
    int32_t v = (int32_t)(((rasY0 - (RM.cy + dy)) / rasMpp) * 65536.0f);
    uint16_t *out = fb + y * LCD_WIDTH;
    for (int x = xa; x <= xb; x++, u += du, v += dv) {
      int ui = u >> 16, vi = v >> 16;
      uint16_t color = pal[0];
      if ((uint32_t)ui < (uint32_t)rasW && (uint32_t)vi < (uint32_t)rasH) {
        uint32_t k = ((uint32_t)((vi >> 4) * rasBW + (ui >> 4)) << 8) | ((vi & 15) << 4) | (ui & 15);
        uint16_t cell = cells[k];
        uint32_t pair = (cell >> pairShift) & 7;
        color = pal[(cell >> baseShift) & 3];
        if (pair > 0 && (pair <= 6 || joins)) {
          int qx = (u >> 8) & 255, qy = (v >> 8) & 255;
          int fx = qx - 128, fy = qy - 128;
          if (pair <= 6) {
            int direction = fine ? (cell & 63) : ((cell & 31) << 1);
            int offset = (cell >> offsetShift) & 31;
            int distance = viceNX[direction] * fx + viceNY[direction] * fy + offsets[offset];
            int index = 128 + (int)((distance * gain) >> 16);
            index = max(0, min(256, index));
            int angle = (direction - rotation) & 15;
            if (angle > 8) angle = 16 - angle;
            color = viceEdgePal[pair][viceCoverage[angle][index]];
          } else {
            color = vicePatchColor(cell, fx, fy);
          }
          if (joins && (qx < 32 || qx > 224 || qy < 32 || qy > 224))
            color = viceJoinColor(color, ui, vi, qx, qy);
        }
      }
      out[x] = color;
    }
  }
}

uint32_t rasMsAcc = 0;                     // v5.7 统计：底图用时
void drawRasterMap(float cx, float cy, float sc, float cs, float sn) {
  unsigned long t0 = millis();
  RM.fb = gfx->getFramebuffer();
  if (rasHasPal) {                                     // v6.1 卫星图：用照片自带的颜色（夜间压暗）
    for (int i = 0; i < 16; i++) RM.pal[i] = (themeIdx != 0 && nightOn) ? blendF(rasPal[i], C_BLACK, 110) : rasPal[i];
  } else {
    for (int i = 0; i < 16; i++) RM.pal[i] = C_BLOCK;
    RM.pal[1] = C_ROADM; RM.pal[2] = C_PARK; RM.pal[3] = C_WATER;
    RM.pal[4] = blendF(C_BLOCK,C_ROADM,64);   // 俯视屋顶
    RM.pal[5] = blendF(C_BLOCK,C_ROADM,112);  // 屋顶轮廓
    RM.pal[6] = RM.pal[4]; RM.pal[7] = RM.pal[5]; // 保留原绿地类别的屋顶
  }
  if (styleIdx() == ST_MINIMAL) {
    for (int i=0;i<16;i++) RM.pal[i]=0x0000;
    RM.pal[1]=0x8410; // independent monochrome road mask; no building/park/water layer
  }
  RM.pix2 = styleIdx() == ST_PIXEL;
  RM.cx = cx; RM.cy = cy; RM.sc = sc; RM.cs = cs; RM.sn = sn;
  if (styleIdx() == 1 && !rasHasPal && rasterViceValid) {
    prepareVicePalette(RM.pal);
    // 按方形像素的面积覆盖率过渡，斜边的支撑范围自然扩展到约 1.414 像素。
    RM.viceGain = (int32_t)(sc * rasMpp * (65536.0f / (127.0f * 1.5f)));
    RM.viceRotation = (int)lroundf(atan2f(sn, cs) * (32.0f / PI));
    parallelRows(rasterViceRows, (int)(CX - 213), (int)(CX + 213));
  } else {
    parallelRows(rasterRows, (int)(CX - 213), (int)(CX + 213));
  }
  rasMsAcc += millis() - t0;
}

void parseRoute(const uint8_t *p, uint32_t len) {
  routeN = 0;
  if (len < 12) return;
  int frame = rdu16(p);
  int32_t x = rd32(p + 2), y = rd32(p + 6);
  int n = rdu16(p + 10);
  if (n > ROUTE_PTS) n = ROUTE_PTS;
  if (n < 1 || 12 + (uint32_t)(n - 1) * 4 > len) return;
  routeX[0] = x; routeY[0] = y;
  for (int i = 1; i < n; i++) {
    x += rd16(p + 12 + 4 * (i - 1)); y += rd16(p + 14 + 4 * (i - 1));
    routeX[i] = x; routeY[i] = y;
  }
  // 每 32 个点一组算外框（相邻组共用端点，保证线段完整）
  for (int b = 0; b * RB_STEP < n - 1; b++) {
    int s = b * RB_STEP, e = min(n - 1, s + RB_STEP);
    int32_t x0 = INT32_MAX, y0 = INT32_MAX, x1 = INT32_MIN, y1 = INT32_MIN;
    for (int i = s; i <= e; i++) {
      x0 = min(x0, routeX[i]); x1 = max(x1, routeX[i]);
      y0 = min(y0, routeY[i]); y1 = max(y1, routeY[i]);
    }
    rbBox[4 * b] = x0; rbBox[4 * b + 1] = y0; rbBox[4 * b + 2] = x1; rbBox[4 * b + 3] = y1;
  }
  routeN = n; routeFrame = frame;
  routeProg = -1; routeSearchAt = 0;                  // v3.0 新路线：重新找车的位置
  if (routeCum) {                                     // v5.0 路程表（路况颜色用）
    routeCum[0] = 0;
    for (int i = 1; i < n; i++) {
      float dx = routeX[i] - routeX[i - 1], dy = routeY[i] - routeY[i - 1];
      routeCum[i] = routeCum[i - 1] + sqrtf(dx * dx + dy * dy);
    }
  }
  if (routeCum && routeCurv) {                        // v6.1 弯急程度：前后各 20 米之间转了多少度
    const float WIN = 20;
    int j0 = 0, j1 = 0;
    for (int i = 0; i < n; i++) {
      while (j0 < i && routeCum[i] - routeCum[j0 + 1] >= WIN) j0++;
      if (j1 < i) j1 = i;
      while (j1 < n - 1 && routeCum[j1] - routeCum[i] < WIN) j1++;
      uint8_t cls = 0;
      if (j0 < i && j1 > i) {
        float h0 = atan2f(routeY[i] - routeY[j0], routeX[i] - routeX[j0]);
        float h1 = atan2f(routeY[j1] - routeY[i], routeX[j1] - routeX[i]);
        float d = fabsf(h1 - h0);
        if (d > PI) d = 2 * PI - d;
        float deg = d * 180 / PI;
        cls = deg < 20 ? 0 : (deg < 50 ? 1 : 2);
      }
      routeCurv[i] = cls;
    }
  }
  // v3.2 路线点后面可选：红绿灯个数(2) + 每个灯 x(4) y(4) 所在段号(2) 段内比例(1，0~255)
  lightN = 0;
  uint32_t pos = 12 + (uint32_t)(n - 1) * 4;
  if (pos + 2 <= len) {
    int ln = rdu16(p + pos); pos += 2;
    for (int i = 0; i < ln && lightN < LIGHT_MAX && pos + 11 <= len; i++, pos += 11) {
      lightX[lightN] = rd32(p + pos); lightY[lightN] = rd32(p + pos + 4);
      lightU[lightN] = rdu16(p + pos + 8) + p[pos + 10] / 255.0f;
      lightN++;
    }
  }
}

// ---- v3.0 走过的路线 ----
static inline float segLen(int k) {
  float dx = routeX[k + 1] - routeX[k], dy = routeY[k + 1] - routeY[k];
  return sqrtf(dx * dx + dy * dy);
}

// 路线上的点：u = 段号 + 段内比例
void routePt(float u, float &x, float &y) {
  int k = (int)u;
  if (k >= routeN - 1) { x = routeX[routeN - 1]; y = routeY[routeN - 1]; return; }
  float t = u - k;
  x = routeX[k] + (routeX[k + 1] - routeX[k]) * t;
  y = routeY[k] + (routeY[k + 1] - routeY[k]) * t;
}

// 找车在路线上的位置：平时只在当前位置附近（往回 30 米、往前 300 米）找，
// 这样掉头、绕回同一条路时不会跳到路线后面去；还没对上时才搜整条路线（最多每秒一次）
void updateRouteProgress(float cx, float cy) {
  if (routeN < 2) return;
  int k0, k1;
  if (routeProg < 0) {
    if (millis() - routeSearchAt < 1000 && routeSearchAt) return;
    routeSearchAt = millis();
    k0 = 0; k1 = routeN - 2;
  } else {
    int base = min((int)routeProg, routeN - 2);
    float acc = 0;
    k0 = base; while (k0 > 0 && acc < 30) { acc += segLen(k0 - 1); k0--; }
    acc = 0;
    k1 = base; while (k1 < routeN - 2 && acc < 300) { acc += segLen(k1); k1++; }
  }
  float best = 1e18f, bestU = -1;
  for (int k = k0; k <= k1; k++) {
    float ax = routeX[k], ay = routeY[k];
    float vx = routeX[k + 1] - ax, vy = routeY[k + 1] - ay;
    float L2 = vx * vx + vy * vy;
    float t = L2 > 0 ? ((cx - ax) * vx + (cy - ay) * vy) / L2 : 0;
    t = constrain(t, 0.0f, 1.0f);
    float ex = ax + vx * t - cx, ey = ay + vy * t - cy;
    float d2 = ex * ex + ey * ey;
    if (d2 < best) { best = d2; bestU = k + t; }
  }
  if (best > 50.0f * 50.0f) return;                   // 离路线超过 50 米（偏航中）：先不动
  routeProg = bestU;
}

void mapPolyline(const float *xs, const float *ys, int n, float w, uint16_t c, bool caps);

// 画路线的一段（u 从 a 到 b），先画整段描边再画内芯
// v6.1 弯道上色：内芯按每段的弯急程度分成几截，各用绿 / 黄 / 红画
const uint16_t CURVE_COL[3] = {0x3EEB, 0xFE45, 0xF9E6};
void drawRouteCurved(float a, float b, float cx, float cy, float sc, float cs, float sn, float viewR, float w1) {
  for (int bi = 0; bi * RB_STEP < routeN - 1; bi++) {
    int s = bi * RB_STEP, e = min(routeN - 1, s + RB_STEP);
    if (e <= a || s >= b) continue;
    const int32_t *bb = rbBox + 4 * bi;
    if (bb[2] < cx - viewR || bb[0] > cx + viewR || bb[3] < cy - viewR || bb[1] > cy + viewR) continue;
    float ua = fmaxf(a, (float)s), ub = fminf(b, (float)e);
    int m = 0, cls = -1;
    auto proj = [&](float x, float y) {
      tmpX[m] = CX + sc * ((x - cx) * cs - (y - cy) * sn); tmpY[m] = MAP_PY - sc * ((x - cx) * sn + (y - cy) * cs); m++;
    };
    float x, y;
    routePt(ua, x, y); proj(x, y);
    cls = routeCurv[(int)floorf(ua)];
    for (int k = (int)floorf(ua) + 1; k < ub; k++) {
      proj((float)routeX[k], (float)routeY[k]);
      int c2 = routeCurv[k];
      if (c2 != cls) {                                   // 换颜色：先把这一截画掉，从这个点接着画
        if (m >= 2) mapPolyline(tmpX, tmpY, m, w1, CURVE_COL[cls], true);
        tmpX[0] = tmpX[m - 1]; tmpY[0] = tmpY[m - 1]; m = 1;
        cls = c2;
      }
    }
    routePt(ub, x, y); proj(x, y);
    if (m >= 2) mapPolyline(tmpX, tmpY, m, w1, CURVE_COL[cls], true);
  }
}

void drawRouteRange(float a, float b, float cx, float cy, float sc, float cs, float sn, float viewR,
                    float w0, float w1, uint16_t c0, uint16_t c1) {
  if (b - a < 0.001f) return;
  for (int pass = 0; pass < 2; pass++) {
    for (int bi = 0; bi * RB_STEP < routeN - 1; bi++) {
      int s = bi * RB_STEP, e = min(routeN - 1, s + RB_STEP);
      if (e <= a || s >= b) continue;
      const int32_t *bb = rbBox + 4 * bi;
      if (bb[2] < cx - viewR || bb[0] > cx + viewR || bb[3] < cy - viewR || bb[1] > cy + viewR) continue;
      float ua = fmaxf(a, (float)s), ub = fminf(b, (float)e);
      int m = 0;
      float x, y;
      routePt(ua, x, y);
      tmpX[m] = CX + sc * ((x - cx) * cs - (y - cy) * sn); tmpY[m] = MAP_PY - sc * ((x - cx) * sn + (y - cy) * cs); m++;
      for (int k = (int)floorf(ua) + 1; k < ub; k++) {
        float dx = routeX[k] - cx, dy = routeY[k] - cy;
        tmpX[m] = CX + sc * (dx * cs - dy * sn); tmpY[m] = MAP_PY - sc * (dx * sn + dy * cs); m++;
      }
      routePt(ub, x, y);
      tmpX[m] = CX + sc * ((x - cx) * cs - (y - cy) * sn); tmpY[m] = MAP_PY - sc * ((x - cx) * sn + (y - cy) * cs); m++;
      if (m >= 2) mapPolyline(tmpX, tmpY, m, pass == 0 ? w0 : w1, pass == 0 ? c0 : c1, true);
    }
  }
}

// v4.4 原路线：格式和导航路线一样（坐标系、起点、点数、差值），后面的红绿灯不管
void parseGhost(const uint8_t *p, uint32_t len) {
  ghostN = 0;
  if (!ghostX || len < 12) return;
  int frame = rdu16(p);
  int32_t x = rd32(p + 2), y = rd32(p + 6);
  int n = rdu16(p + 10);
  if (n > GHOST_PTS) n = GHOST_PTS;
  if (n < 2 || 12 + (uint32_t)(n - 1) * 4 > len) return;
  ghostX[0] = x; ghostY[0] = y;
  for (int i = 1; i < n; i++) {
    x += rd16(p + 12 + 4 * (i - 1)); y += rd16(p + 14 + 4 * (i - 1));
    ghostX[i] = x; ghostY[i] = y;
  }
  for (int b = 0; b * RB_STEP < n - 1; b++) {
    int s = b * RB_STEP, e = min(n - 1, s + RB_STEP);
    int32_t x0 = INT32_MAX, y0 = INT32_MAX, x1 = INT32_MIN, y1 = INT32_MIN;
    for (int i = s; i <= e; i++) {
      x0 = min(x0, ghostX[i]); x1 = max(x1, ghostX[i]);
      y0 = min(y0, ghostY[i]); y1 = max(y1, ghostY[i]);
    }
    gbBox[4 * b] = x0; gbBox[4 * b + 1] = y0; gbBox[4 * b + 2] = x1; gbBox[4 * b + 3] = y1;
  }
  ghostN = n; ghostFrame = frame;
}

// v4.4 画原路线：橙色细线，画在导航路线下面（两条重合的地方只看到导航路线，分开的地方才露出来）
void drawGhost(float cx, float cy, float sc, float cs, float sn, float viewR, float w) {
  if (ghostN < 2 || ghostFrame != posFrame) return;
  for (int bi = 0; bi * RB_STEP < ghostN - 1; bi++) {
    const int32_t *bb = gbBox + 4 * bi;
    if (bb[2] < cx - viewR || bb[0] > cx + viewR || bb[3] < cy - viewR || bb[1] > cy + viewR) continue;
    int s = bi * RB_STEP, e = min(ghostN - 1, s + RB_STEP), m = 0;
    for (int k = s; k <= e; k++) {
      float dx = ghostX[k] - cx, dy = ghostY[k] - cy;
      tmpX[m] = CX + sc * (dx * cs - dy * sn); tmpY[m] = MAP_PY - sc * (dx * sn + dy * cs); m++;
    }
    if (m >= 2) mapPolyline(tmpX, tmpY, m, w, styleIdx()==ST_MINIMAL ? 0x8410 : 0xFD20, true);
  }
}

// 'M' 类型 编号 总片数(2) 片序号(2) 总字节(4) 偏移(4) + 数据
void handleBulk(const uint8_t *d, int n) {
  if (n < 15) return;
  int type = d[1], id = d[2];
  int parts = d[3] | (d[4] << 8), seq = d[5] | (d[6] << 8);
  uint32_t total = (uint32_t)rd32(d + 7), off = (uint32_t)rd32(d + 11);
  if (type < 1 || type > 4) return;
  Bulk &b = bulks[type];
  if (!b.buf || total == 0 || total > b.cap || parts == 0 || parts > 4096 || seq >= parts) return;
  if (id != b.id || total != b.total || parts != b.parts) {     // 新的一份：从头开始收
    b.id = id; b.total = total; b.parts = parts; b.got = 0; b.done = false;
    memset(b.mask, 0, sizeof(b.mask));
  }
  if (b.done) { pendingAck = "K," + String(type) + "," + String(id); return; }   // 重复的：再确认一次
  uint32_t len = n - 15;
  if (off >= total) return;
  if (off + len > total) len = total - off;
  if (!(b.mask[seq >> 3] & (1 << (seq & 7)))) {
    memcpy(b.buf + off, d + 15, len);
    b.mask[seq >> 3] |= (1 << (seq & 7));
    b.got++;
  }
  if (b.got >= b.parts) {
    b.done = true;
    if (type == 1) {
      uint8_t *t = mapShow; mapShow = b.buf; b.buf = t;   // 交换：新图显示，旧缓冲留着收下一块
      parseMap(mapShow, total);
    } else if (type == 2) {
      parseRoute(b.buf, total);
    } else if (type == 4) {
      parseGhost(b.buf, total);                        // v4.4 原路线
    } else {
      if (!parseRaster(b.buf,total)) {
        // 没有成功解码就不回“已确认”，并允许相同编号完整重发。
        b.done=false; b.got=0; memset(b.mask,0,sizeof(b.mask));
        return;
      }
    }
    pendingAck = "K," + String(type) + "," + String(id);
    navDirty = true;
  }
}

String pendingStat2 = "";
// v6.8 每 5 秒读一次；有变化就重画、通知手机
void batPoll() {
  if (!pmuOK) return;
  unsigned long now = millis();
  if (batAt && now - batAt < 5000) return;
  batAt = now;
  if(!batteryGauge.refresh())return;
  int voltage=batteryGauge.getVoltage();
  bool present=batteryGauge.getBatteryStatus().isBatteryPresent() && voltage>=2000 && voltage<=5000;
  int pct=present?(int)batteryGauge.getStateOfCharge():-1;
  if(pct>100)pct=-1;
  if(present)batMv=voltage;else batMv=0;
  static int missCnt=0;
  if(pct<0){if(batPct>=0 && ++missCnt<3){pct=batPct;present=true;}}
  else missCnt=0;
  bool chg=present && batteryGauge.isCharging();
  // BQ27220 没有独立 VBUS 状态；只能在确认充电时报告外部供电。
  bool usb=chg;
  bool changed = pct != batPct || chg != batChg || usb != usbIn;
  batPct = pct; batChg = chg; usbIn = usb;
  if (mediaHid && changed && pct >= 0) mediaHid->setBatteryLevel((uint8_t)constrain(pct, 0, 100));
  if (changed) {
    if (onWaitScreen && page == 0 && !sliding) drawStatus();        // v7.2 只重画状态栏
    else if (inNav) navDirty = true;
  }
  if (changed || now - batSentAt > 60000) {
    batSentAt = now;
    pendingBat = "V," + String(batPct) + "," + String(batChg ? 1 : 0) + "," + String(usbIn ? 1 : 0) + "," + String(batMv);
  }
}

void sendPendingAck() {
  if (!txChar || !bleConnected || !bleAuthed) return;    // v6.5 没通过验证的手机什么都不发
  // v6.5 告诉手机“验证通过了”：连上后 0.3 / 1.5 / 4 秒各发一次（手机订阅通知可能比加密晚一点）
  static const uint16_t uAt[3] = {300, 1500, 4000};
  if (authNotifies < 3 && millis() - bleAuthAt > uAt[authNotifies]) {
    authNotifies++;
    txChar->setValue((uint8_t *)"U,1", 3);
    txChar->notify();
    txChar->setValue((uint8_t *)"CAP,HID,1", 9); txChar->notify();
    txChar->setValue((uint8_t *)"CAP,LYR,1", 9); txChar->notify();
    pendingHidStatus = String("HID,") + (hidMusicMode ? 1 : 0) + "," + (hidMediaReady() ? 1 : 0);
    appleMedia.resend();
    pendingLyricsStatus = String("ML,") + (musicLyrics ? 1 : 0);
    pendingMediaStatus = String("AMC,") + appleMedia.controlMask();
  }
  if (pendingLyricsStatus.length()) { txChar->setValue((uint8_t *)pendingLyricsStatus.c_str(), pendingLyricsStatus.length()); txChar->notify(); pendingLyricsStatus = ""; }
  if (pendingMediaStatus.length()) { txChar->setValue((uint8_t *)pendingMediaStatus.c_str(), pendingMediaStatus.length()); txChar->notify(); pendingMediaStatus = ""; }
  uint8_t amsFrame[139]; size_t amsSize = 0;
  if (appleMedia.nextFrame(amsFrame, amsSize, millis())) { txChar->setValue(amsFrame, amsSize); txChar->notify(); }
  if (pendingHidStatus.length()) {
    txChar->setValue((uint8_t *)pendingHidStatus.c_str(), pendingHidStatus.length());
    txChar->notify(); pendingHidStatus = "";
  }
  if (pendingBootAck.length()) {
    txChar->setValue((uint8_t *)pendingBootAck.c_str(), pendingBootAck.length());
    txChar->notify(); pendingBootAck = "";
  }
  if (pendingBuildingCapability) {
    txChar->setValue((uint8_t *)"CAP,BLD,1", 9); txChar->notify(); pendingBuildingCapability=false;
  }
  if (pendingAck.length()) {
    txChar->setValue((uint8_t *)pendingAck.c_str(), pendingAck.length());
    txChar->notify();
    pendingAck = "";
  }
  if (pendingStat2.length()) {
    txChar->setValue((uint8_t *)pendingStat2.c_str(), pendingStat2.length());
    txChar->notify();
    pendingStat2 = "";
  }
  if (pendingProf.length()) {                         // v5.8 分步用时
    txChar->setValue((uint8_t *)pendingProf.c_str(), pendingProf.length());
    txChar->notify();
    pendingProf = "";
  }
  if (pendingBat.length()) {                          // v6.8 电量
    txChar->setValue((uint8_t *)pendingBat.c_str(), pendingBat.length());
    txChar->notify();
    pendingBat = "";
  }
  if (pendingEvt.length()) {                          // v5.2 触摸事件
    txChar->setValue((uint8_t *)pendingEvt.c_str(), pendingEvt.length());
    txChar->notify();
    pendingEvt = "";
  }
}

// 现在是否显示小地图
bool mapModeActive() {
  if (!mapOn || !inNav) return false;
  bool vecOK = mapValid && mapFrame == posFrame;
  bool rasOK = rasterValid && rasterFrame == posFrame;      // v2.5
  // v4.0 停车或暂停时手机可能不再发位置：不再 5 秒后切回箭头画面，保持地图（位置停在最后一次）
  if (!(styleIdx() == ST_DRAGON || vecOK || rasOK) || posAt == 0) return false;
  return true;                                      // v2.1 一直显示地图，路口信息放大显示在圆牌里
}

// ---- 画图工具 ----

// 粗折线（圆角连接）；屏幕外的线段直接跳过
// v2.2：只在方向变化明显的拐点补圆（直线上的点不画圆，省很多时间）；caps = 两端也画圆头
struct VicePlane { float nx, ny, c, support; uint8_t angle; };

static inline void viceBlendPixel(uint16_t *row, int x, uint16_t color, float coverage) {
  int alpha = (int)(coverage * 256.0f + 0.5f);
  if (alpha <= 0) return;
  row[x] = alpha >= 256 ? color : blendF(row[x], color, alpha);
}

void viceAAQuad(const float *px, const float *py, uint16_t color) {
  VicePlane plane[4];
  float cx = 0, cy = 0, ymin = py[0], ymax = py[0];
  for (int i = 0; i < 4; i++) { cx += px[i]; cy += py[i]; ymin = fminf(ymin, py[i]); ymax = fmaxf(ymax, py[i]); }
  cx *= 0.25f; cy *= 0.25f;
  for (int i = 0; i < 4; i++) {
    int j = (i + 1) & 3;
    float dx = px[j] - px[i], dy = py[j] - py[i], length = sqrtf(dx * dx + dy * dy);
    if (length < 0.001f) return;
    float nx = -dy / length, ny = dx / length;
    if ((cx - px[i]) * nx + (cy - py[i]) * ny < 0) { nx = -nx; ny = -ny; }
    plane[i] = {nx, ny, -nx * px[i] - ny * py[i], (fabsf(nx) + fabsf(ny)) * 0.5f, viceCoverageAngle(nx, ny)};
  }
  uint16_t *fb = gfx->getFramebuffer();
  int ya = max(0, (int)floorf(ymin - 1)), yb = min(LCD_HEIGHT - 1, (int)ceilf(ymax + 1));
  for (int y = ya; y <= yb; y++) {
    float sy = y + 0.5f, left = 0.5f, right = LCD_WIDTH - 0.5f;
    float solidLeft = left, solidRight = right;
    bool outside = false, noSolid = false;
    float q[4];
    for (int i = 0; i < 4; i++) {
      q[i] = plane[i].ny * sy + plane[i].c;
      float nx = plane[i].nx, support = plane[i].support;
      if (fabsf(nx) < 0.00001f) {
        if (q[i] <= -support) outside = true;
        if (q[i] < support) noSolid = true;
      } else if (nx > 0) {
        left = fmaxf(left, (-support - q[i]) / nx);
        solidLeft = fmaxf(solidLeft, (support - q[i]) / nx);
      } else {
        right = fminf(right, (-support - q[i]) / nx);
        solidRight = fminf(solidRight, (support - q[i]) / nx);
      }
    }
    if (outside || left > right) continue;
    int xa = max(0, (int)ceilf(left - 0.5f)), xb = min(LCD_WIDTH - 1, (int)floorf(right - 0.5f));
    int ia = max(xa, (int)ceilf(solidLeft - 0.5f)), ib = min(xb, (int)floorf(solidRight - 0.5f));
    if (noSolid || ia > ib) { ia = xb + 1; ib = xb; }
    uint16_t *row = fb + y * LCD_WIDTH;
    for (int x = xa; x <= xb;) {
      if (x == ia && ia <= ib) {
        gfx->drawFastHLine(x, y, ib - ia + 1, color);
        x = ib + 1; continue;
      }
      float sx = x + 0.5f;
      int alpha = 256;
      for (int i = 0; i < 4; i++) {
        float distance = plane[i].nx * sx + q[i];
        if (distance <= -plane[i].support) { alpha = 0; break; }
        if (distance < plane[i].support) {
          int index = max(0, min(256, (int)floorf(128.5f + distance * (256.0f / 1.5f))));
          alpha = min(alpha, (int)viceCoverage[plane[i].angle][index]);
        }
      }
      if (alpha) row[x] = alpha >= 256 ? color : blendF(row[x], color, alpha);
      x++;
    }
  }
}

void viceAACircle(float cx, float cy, float radius, uint16_t color) {
  uint16_t *fb = gfx->getFramebuffer();
  int ya = max(0, (int)floorf(cy - radius - 1)), yb = min(LCD_HEIGHT - 1, (int)ceilf(cy + radius + 1));
  float outer = radius + 0.5f, inner = fmaxf(0, radius - 0.5f);
  for (int y = ya; y <= yb; y++) {
    float dy = y + 0.5f - cy;
    if (fabsf(dy) >= outer) continue;
    float extent = sqrtf(outer * outer - dy * dy);
    int xa = max(0, (int)ceilf(cx - extent - 0.5f)), xb = min(LCD_WIDTH - 1, (int)floorf(cx + extent - 0.5f));
    int ia = xb + 1, ib = xb;
    if (fabsf(dy) < inner) {
      float core = sqrtf(inner * inner - dy * dy);
      ia = max(xa, (int)ceilf(cx - core - 0.5f)); ib = min(xb, (int)floorf(cx + core - 0.5f));
    }
    uint16_t *row = fb + y * LCD_WIDTH;
    for (int x = xa; x <= xb;) {
      if (x == ia && ia <= ib) {
        gfx->drawFastHLine(x, y, ib - ia + 1, color);
        x = ib + 1; continue;
      }
      float dx = x + 0.5f - cx;
      viceBlendPixel(row, x, color, outer - sqrtf(dx * dx + dy * dy));
      x++;
    }
  }
}

void mapPolyline(const float *xs, const float *ys, int n, float w, uint16_t c, bool caps) {
  bool vice = styleIdx() == 1;
  if (vice) initViceSampling();
  float r = w / 2;
  float pdx = 0, pdy = 0;
  bool havePrev = false;
  for (int i = 0; i + 1 < n; i++) {
    float x0 = xs[i], y0 = ys[i], x1 = xs[i + 1], y1 = ys[i + 1];
    if ((x0 < -w && x1 < -w) || (x0 > LCD_WIDTH + w && x1 > LCD_WIDTH + w) ||
        (y0 < -w && y1 < -w) || (y0 > LCD_HEIGHT + w && y1 > LCD_HEIGHT + w)) continue;
    float dx = x1 - x0, dy = y1 - y0;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 0.5f) continue;
    float ux = dx / len, uy = dy / len;
    float nx = -uy * r, ny = ux * r;
    if (vice) {
      float px[4] = {x0 + nx, x0 - nx, x1 - nx, x1 + nx};
      float py[4] = {y0 + ny, y0 - ny, y1 - ny, y1 + ny};
      viceAAQuad(px, py, c);
    } else {
      gfx->fillTriangle(x0 + nx, y0 + ny, x0 - nx, y0 - ny, x1 + nx, y1 + ny, c);
      gfx->fillTriangle(x0 - nx, y0 - ny, x1 - nx, y1 - ny, x1 + nx, y1 + ny, c);
    }
    if (r >= 1.5f) {
      if (havePrev ? (ux * pdx + uy * pdy < 0.97f) : caps) {
        if (vice) viceAACircle(x0, y0, r, c); else gfx->fillCircle(x0, y0, r, c);
      }
      if (caps && i + 2 == n) {
        if (vice) viceAACircle(x1, y1, r, c); else gfx->fillCircle(x1, y1, r, c);
      }
    }
    pdx = ux; pdy = uy; havePrev = true;
  }
}

// 实心多边形（扫描线，奇偶规则）
void mapFillPoly(const float *xs, const float *ys, int n, uint16_t c) {
  if (n < 3) return;
  float ymin = 1e9f, ymax = -1e9f, xmin = 1e9f, xmax = -1e9f;
  for (int i = 0; i < n; i++) {
    ymin = fminf(ymin, ys[i]); ymax = fmaxf(ymax, ys[i]);
    xmin = fminf(xmin, xs[i]); xmax = fmaxf(xmax, xs[i]);
  }
  if (ymax < 0 || ymin >= LCD_HEIGHT || xmax < 0 || xmin >= LCD_WIDTH) return;
  int y0 = max(0, (int)ceilf(ymin - 0.5f)), y1 = min(LCD_HEIGHT - 1, (int)floorf(ymax - 0.5f));
  static float xs_hit[256];
  for (int y = y0; y <= y1; y++) {
    float sy = y + 0.5f;
    int k = 0;
    for (int i = 0, j = n - 1; i < n; j = i++) {
      float ya = ys[j], yb = ys[i];
      if ((ya <= sy && yb > sy) || (yb <= sy && ya > sy)) {
        if (k < 256) xs_hit[k++] = xs[j] + (sy - ya) * (xs[i] - xs[j]) / (yb - ya);
      }
    }
    for (int a = 1; a < k; a++) {               // 插入排序（交点通常很少）
      float v = xs_hit[a]; int b = a - 1;
      while (b >= 0 && xs_hit[b] > v) { xs_hit[b + 1] = xs_hit[b]; b--; }
      xs_hit[b + 1] = v;
    }
    for (int a = 0; a + 1 < k; a += 2) {
      int xa = max(0, (int)ceilf(xs_hit[a] - 0.5f)), xb = min(LCD_WIDTH - 1, (int)floorf(xs_hit[a + 1] - 0.5f));
      if (xb >= xa) gfx->drawFastHLine(xa, y, xb - xa + 1, c);
    }
  }
}

// 车头箭头（白色，深色描边）
void drawPlayer(float x, float y) {
  // v3.9 车头箭头缩小到原来的约 70%
  const float T[4][2] = {{0, -13}, {-10, 10.5f}, {0, 5}, {10, 10.5f}};
  for (int pass = 0; pass < 2; pass++) {
    float k = pass == 0 ? 1.35f : 1.0f;
    uint16_t c = pass == 0 ? C_BLACK : C_WHITE;
    float yo = pass == 0 ? 1.0f : 0;
    gfx->fillTriangle(x + T[0][0] * k, y + T[0][1] * k + yo, x + T[1][0] * k, y + T[1][1] * k + yo, x + T[2][0] * k, y + T[2][1] * k + yo, c);
    gfx->fillTriangle(x + T[0][0] * k, y + T[0][1] * k + yo, x + T[2][0] * k, y + T[2][1] * k + yo, x + T[3][0] * k, y + T[3][1] * k + yo, c);
  }
}

// ---- v2.7 上下弧形面板：和黑色外圈连成一体 ----
#define CAP_R      213      // 面板填到这个半径（和外圈黑边接上）
#define TOPCAP_Y   98       // 上面板下沿（有测速时 112）
#define BOTCAP_Y   368      // 下面板上沿（离路口 150 米内 334）
#define BOTCAP_Y2  334

// 填充圆内 y0~y1 行（圆的一个弓形）
void fillCapRows(int y0, int y1, uint16_t c) {
  for (int y = max(0, y0); y <= min(LCD_HEIGHT - 1, y1); y++) {
    float dy = y + 0.5f - CX;
    if (fabsf(dy) >= CAP_R) continue;
    float h = sqrtf((float)CAP_R * CAP_R - dy * dy);
    int xa = max(0, (int)(CX - h)), xb = min(LCD_WIDTH - 1, (int)(CX + h));
    gfx->drawFastHLine(xa, y, xb - xa + 1, c);
  }
}

// 面板和地图之间的分隔线（只画到外圈黑边以内）
void capLine(int y, int thick, uint16_t c) {
  for (int k = 0; k < thick; k++) {
    float dy = y + k + 0.5f - CX;
    if (fabsf(dy) >= 211) continue;
    float h = sqrtf(211.0f * 211.0f - dy * dy);
    gfx->drawFastHLine((int)(CX - h), y + k, (int)(2 * h), c);
  }
}

// v2.8 带黑色描边的文字（直接浮在地图上）
void printOutlined(const String &t, int x, int y, int size, uint16_t col) {
  int o = size >= 3 ? 2 : 1;
  // v5.8 只画最外一圈偏移（字的笔画至少 o 像素粗，里面那圈会被盖住，画了也白画）
  for (int dy = -o; dy <= o; dy++)
    for (int dx = -o; dx <= o; dx++)
      if ((dx || dy) && (o < 2 || abs(dx) == o || abs(dy) == o)) printCentered(t, x + dx, y + dy, size, C_BLACK);
  printCentered(t, x, y, size, col);
}

// 把一块区域调暗（半透明深色底），a = 保留多少原来的亮度（0~255）
void dimRect(int x, int y, int w, int h, int r, int keep) {
  uint16_t *fb = gfx->getFramebuffer();
  if (!fb) return;
  for (int j = max(0, y); j < min(LCD_HEIGHT, y + h); j++) {
    for (int i = max(0, x); i < min(LCD_WIDTH, x + w); i++) {
      int dx = i < x + r ? x + r - i : (i >= x + w - r ? i - (x + w - r - 1) : 0);
      int dy = j < y + r ? y + r - j : (j >= y + h - r ? j - (y + h - r - 1) : 0);
      if (dx * dx + dy * dy > r * r) continue;
      uint16_t &p = fb[j * LCD_WIDTH + i];
      p = blend565(C_BLACK, p, keep);
    }
  }
}

// 车速（浮在顶部）+ 测速牌
void drawMapSpeed(bool over) {
  if (speedKmh < 0) return;
  int sx = hasCam ? CX - 40 : CX;
  printOutlined(String(speedKmh), sx, 44, 4, over ? C_RED : TH.hud);
  printOutlined("km/h", sx, 80, 1, over ? 0xFC10 : TH.hud);
  if (hasCam) {
    const int bx = CX + 52, by = 60;
    aaDisc(bx, by, 27, C_BLACK);
    aaDisc(bx, by, 25, C_SIGNRED);
    aaDisc(bx, by, 19, C_WHITE);
    String lim = String(camLimit);
    int sz = lim.length() >= 3 ? 2 : 3;
    printCentered(lim, bx, by - 4 * sz, sz, C_BLACK);
    printOutlined(distText(camDist), bx, by + 31, 2, over ? 0xFC10 : C_WHITE);
  }
}

// ---- v3.1 红绿灯小图标：竖着的黑色灯箱 + 红黄绿三个灯 ----
void drawTrafficLight(float cx, float cy, float h) {
  float w = h * 0.46f, r = h * 0.13f;
  gfx->fillRoundRect((int)(cx - w / 2 - 1), (int)(cy - h / 2 - 1), (int)w + 2, (int)h + 2, (int)(w / 2.6f), C_BLACK);
  gfx->fillRoundRect((int)(cx - w / 2), (int)(cy - h / 2), (int)w, (int)h, (int)(w / 3), 0x2104);
  aaDisc(cx, cy - h * 0.29f, r, 0xF8A2);     // 红
  aaDisc(cx, cy, r, 0xFDA0);                // 黄
  aaDisc(cx, cy + h * 0.29f, r, 0x3F09);    // 绿
}

// v3.1 底部一行行程信息：到达时间 · 剩余里程 · 红绿灯数（半透明深色底）
void drawTripLine(int y) {
  char eta[8];
  snprintf(eta, sizeof(eta), "%02d:%02d", (tripEta / 100) % 24, tripEta % 100);
  String a = String(eta);
  String b = tripRemainM >= 100000 ? String(tripRemainM / 1000) + "km" : distText(tripRemainM);
  String c = String(tripLights);
  const int CW = 12, GAP = 14, ICON = 10;
  bool lights = tripLights > 0;
  int w = (a.length() + b.length()) * CW + GAP + (lights ? GAP + ICON + 3 + c.length() * CW : 0);
  int x = CX - w / 2;
  dimRect(x - 10, y - 5, w + 20, 26, 12, 75);
  printCentered(a, x + a.length() * CW / 2, y, 2, TH.hud);
  x += a.length() * CW + GAP;
  printCentered(b, x + b.length() * CW / 2, y, 2, C_WHITE);
  x += b.length() * CW + GAP;
  if (lights) {
    drawTrafficLight(x + ICON / 2, y + 7, 18);
    x += ICON + 3;
    printCentered(c, x + c.length() * CW / 2, y, 2, C_WHITE);
  }
}

// v3.3 车道：放在车速下面（离路口 150 米内才显示），比原来大一点；有测速牌时再往下让一点
void drawMapLanes() {
  if (laneN <= 0 || navDir == "S" || navDist > MAP_NEAR_M) return;
  const int H = 30, MAXW = 210;
  int lw = (int)(fminf(36.0f * H / 32, (float)MAXW / laneN) * laneN);
  int y = hasCam ? 114 : 100;
  dimRect(CX - lw / 2 - 9, y - 5, lw + 18, H + 10, 11, 70);
  drawLanesAt(CX, y, MAXW, H);
}

// ---- v3.8 小地图上的“占位框”：先登记已经占用的地方，电子眼图标只放在空位上 ----
#define OBS_MAX 40
int16_t obsX0[OBS_MAX], obsY0[OBS_MAX], obsX1[OBS_MAX], obsY1[OBS_MAX];
int obsN = 0;
void obsAdd(float x0, float y0, float x1, float y1) {
  if (obsN >= OBS_MAX) return;
  obsX0[obsN] = (int16_t)x0; obsY0[obsN] = (int16_t)y0; obsX1[obsN] = (int16_t)x1; obsY1[obsN] = (int16_t)y1;
  obsN++;
}
// 和已占用的地方重叠了多少面积（像素）；from = 从第几个占位框开始算（v4.2）
long obsOverlap(int x0, int y0, int x1, int y1, int from = 0) {
  long sum = 0;
  for (int i = from; i < obsN; i++) {
    int w = min(x1, (int)obsX1[i]) - max(x0, (int)obsX0[i]);
    int h = min(y1, (int)obsY1[i]) - max(y0, (int)obsY0[i]);
    if (w > 0 && h > 0) sum += (long)w * h;
  }
  return sum;
}
bool obsHit(int x0, int y0, int x1, int y1) {
  for (int i = 0; i < obsN; i++)
    if (x0 < obsX1[i] && x1 > obsX0[i] && y0 < obsY1[i] && y1 > obsY0[i]) return true;
  return false;
}

// 本帧在雷达里的电子眼，在屏幕上的实际位置
float camPx[MCAM_MAX], camPy[MCAM_MAX];
bool camShown[MCAM_MAX];

// v3.5 算出电子眼在小地图上的位置（已经开过的 = 在车后面的不画）；v3.8 真正的绘制放到 drawCamIcons()
void drawMapCams(float cx, float cy, float sc, float cs, float sn) {
  for (int i = 0; i < MCAM_MAX; i++) camShown[i] = false;
  if (mapCamN == 0 || mapCamFrame != posFrame) return;
  for (int i = 0; i < mapCamN; i++) {
    float dx = mapCamX[i] - cx, dy = mapCamY[i] - cy;
    float px = CX + sc * (dx * cs - dy * sn), py = MAP_PY - sc * (dx * sn + dy * cs);
    if (py > MAP_PY + 12) continue;                                     // 在车后面：已经过了
    float ex = px - CX, ex2 = py - CX;
    if (ex * ex + ex2 * ex2 > 194.0f * 194.0f) continue;
    camPx[i] = px; camPy[i] = py; camShown[i] = true;
  }
}

// v3.8 电子眼类型图标（32x32，每像素 3 字节：RGB565 低、高、透明度），由 gen.py 生成
#define CAMI_N 26
#define CAMI_S 32
const uint8_t CAM_ICONS[CAMI_N][CAMI_S * CAMI_S * 3] = {
  { // 0 电子眼
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,227,24,18,
    227,24,18,73,74,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,134,49,10,0,0,71,0,0,141,0,0,190,0,0,217,0,0,233,
    0,0,233,0,0,220,0,0,195,0,0,149,0,0,80,227,24,16,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    99,8,16,0,0,128,0,0,223,0,0,255,163,16,255,102,41,255,232,57,255,73,66,255,
    73,66,255,232,57,255,102,41,255,163,16,255,0,0,255,0,0,230,0,0,142,66,8,27,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,85,
    0,0,224,65,8,255,167,49,255,13,91,253,176,115,251,240,115,252,241,115,253,240,115,254,
    240,115,254,241,115,253,240,115,252,176,115,251,13,91,252,167,49,255,65,8,255,0,0,235,
    0,0,105,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,136,0,0,255,
    102,41,255,78,99,251,241,115,253,208,115,255,175,107,255,143,107,255,143,107,255,143,107,255,
    143,107,255,143,107,255,143,107,255,175,107,255,208,115,255,240,115,254,78,99,251,102,41,255,
    0,0,255,0,0,159,0,0,6,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,166,0,0,255,73,66,251,
    241,115,253,176,115,255,143,107,255,143,107,255,175,107,255,175,107,255,175,107,255,175,107,255,
    175,107,255,175,107,255,175,107,255,175,107,255,143,107,255,143,107,255,176,115,255,240,115,254,
    73,66,251,0,0,255,0,0,189,3,0,8,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,137,0,0,255,106,74,250,241,123,255,
    143,107,255,143,107,255,175,107,255,175,107,255,175,107,255,175,107,255,175,107,255,175,107,255,
    175,107,255,175,107,255,175,107,255,175,107,255,175,107,255,175,107,255,143,107,255,143,107,255,
    241,123,255,106,74,250,0,0,255,0,0,166,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,32,0,81,0,0,255,41,66,250,241,123,254,143,107,255,
    175,107,255,175,107,255,175,107,255,175,107,255,143,107,255,143,107,255,143,107,255,143,107,255,
    175,107,255,175,107,255,175,107,255,175,107,255,175,107,255,175,107,255,175,107,255,175,107,255,
    143,107,255,241,123,255,41,66,250,0,0,255,32,0,111,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,36,17,14,0,0,230,69,41,254,241,115,253,143,107,255,175,107,255,
    175,107,255,175,107,255,175,107,255,143,107,255,208,115,255,240,123,255,240,123,255,240,115,255,
    143,107,255,175,107,255,175,107,255,175,107,255,175,107,255,175,107,255,175,107,255,175,107,255,
    175,107,255,143,107,255,240,115,254,69,41,252,0,0,247,130,8,31,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,127,65,0,255,78,99,251,176,115,255,143,107,255,175,107,255,
    175,107,255,176,115,255,143,107,255,212,148,255,158,247,255,190,247,255,190,247,255,190,247,255,
    85,165,255,111,107,255,208,115,255,176,115,255,176,115,255,176,115,255,175,107,255,175,107,255,
    175,107,255,143,107,255,176,115,255,78,99,251,65,0,255,0,0,160,0,0,0,0,0,0,
    0,0,0,199,57,9,0,0,228,167,49,254,241,115,253,143,107,255,175,107,255,175,107,255,
    175,107,255,143,107,255,237,82,255,215,181,255,255,255,255,255,255,255,255,255,255,255,255,255,
    89,206,255,237,90,255,110,99,255,78,99,255,78,99,255,78,99,255,111,107,255,175,107,255,
    175,107,255,175,107,255,143,107,255,241,115,254,167,49,254,0,0,246,162,16,24,0,0,0,
    0,0,0,0,0,70,0,0,255,13,91,253,208,115,255,143,107,255,175,107,255,175,107,255,
    111,107,255,82,132,255,150,173,255,219,222,255,255,255,255,255,255,255,255,255,255,255,255,255,
    93,231,255,183,181,255,215,181,255,215,181,255,248,189,255,248,189,255,114,140,255,111,107,255,
    175,107,255,175,107,255,143,107,255,208,115,255,13,91,251,0,0,255,0,0,100,0,0,0,
    0,0,0,0,0,143,162,16,254,176,115,251,175,107,255,175,107,255,175,107,255,143,107,255,
    49,132,255,190,247,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,125,239,255,
    158,247,255,255,255,255,255,255,255,255,255,255,93,239,255,93,239,255,255,255,255,147,140,255,
    111,107,255,175,107,255,175,107,255,175,107,255,176,115,251,162,16,254,0,0,173,0,0,0,
    0,0,0,0,0,192,102,41,255,240,115,252,143,107,255,175,107,255,176,115,255,78,99,255,
    183,181,255,255,255,255,223,255,255,255,255,255,255,255,255,93,239,255,147,140,255,143,107,255,
    111,107,255,147,140,255,93,231,255,255,255,255,143,107,255,111,107,255,255,255,255,89,206,255,
    78,99,255,176,115,255,175,107,255,143,107,255,240,115,253,102,41,255,0,0,217,0,0,0,
    0,0,0,0,0,219,232,57,255,241,115,253,143,107,255,175,107,255,176,115,255,78,99,255,
    215,181,255,255,255,255,255,255,255,255,255,255,158,247,255,17,124,255,45,91,255,114,140,255,
    114,140,255,45,91,255,17,124,255,190,247,255,24,190,255,248,189,255,255,255,255,89,206,255,
    78,99,255,176,115,255,175,107,255,143,107,255,240,115,254,232,57,255,0,0,239,162,16,25,
    227,24,18,0,0,234,73,66,255,240,115,254,143,107,255,175,107,255,176,115,255,78,99,255,
    215,181,255,255,255,255,223,255,255,255,255,255,248,189,255,13,91,255,118,173,255,255,255,255,
    255,255,255,118,173,255,13,91,255,248,189,255,255,255,255,255,255,255,255,255,255,89,206,255,
    78,99,255,176,115,255,175,107,255,143,107,255,240,115,254,73,66,255,0,0,250,65,8,43,
    195,24,19,0,0,235,73,66,255,240,115,254,143,107,255,175,107,255,176,115,255,78,99,255,
    215,181,255,255,255,255,223,255,255,255,255,255,53,165,255,46,91,255,60,231,255,255,255,255,
    255,255,255,60,231,255,46,91,255,53,165,255,255,255,255,223,255,255,255,255,255,89,206,255,
    78,99,255,176,115,255,175,107,255,143,107,255,240,115,254,73,66,255,0,0,251,65,8,43,
    73,74,7,0,0,221,232,57,255,241,115,253,143,107,255,175,107,255,176,115,255,78,99,255,
    215,181,255,255,255,255,223,255,255,255,255,255,248,189,255,13,91,255,118,173,255,255,255,255,
    255,255,255,118,173,255,13,91,255,248,189,255,255,255,255,223,255,255,255,255,255,89,206,255,
    78,99,255,176,115,255,175,107,255,143,107,255,240,115,254,232,57,255,0,0,241,130,16,28,
    0,0,0,0,0,196,102,41,255,240,115,252,143,107,255,175,107,255,176,115,255,78,99,255,
    215,181,255,255,255,255,255,255,255,255,255,255,158,247,255,17,124,255,45,91,255,114,140,255,
    114,140,255,45,91,255,17,124,255,158,247,255,255,255,255,255,255,255,255,255,255,89,206,255,
    78,99,255,176,115,255,175,107,255,143,107,255,240,115,253,102,41,255,0,0,220,0,0,0,
    0,0,0,0,0,151,162,16,254,176,115,251,175,107,255,175,107,255,176,115,255,78,99,255,
    215,181,255,255,255,255,223,255,255,255,255,255,255,255,255,61,231,255,147,140,255,111,107,255,
    111,107,255,147,140,255,61,231,255,255,255,255,255,255,255,223,255,255,255,255,255,122,206,255,
    78,99,255,176,115,255,175,107,255,175,107,255,176,115,252,162,16,255,0,0,180,0,0,0,
    0,0,0,0,0,80,0,0,255,13,91,252,208,115,255,143,107,255,175,107,255,111,107,255,
    114,140,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,191,247,255,
    191,247,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,212,148,255,
    111,99,255,175,107,255,143,107,255,208,115,255,13,91,251,0,0,255,0,0,111,0,0,0,
    0,0,0,36,33,14,0,0,236,167,49,254,241,115,253,143,107,255,175,107,255,175,107,255,
    111,107,255,147,140,255,57,198,255,89,206,255,89,206,255,89,206,255,89,206,255,122,206,255,
    122,206,255,89,206,255,89,206,255,89,206,255,89,206,255,89,198,255,212,148,255,110,99,255,
    175,107,255,175,107,255,143,107,255,241,115,254,167,49,254,0,0,250,97,8,32,0,0,0,
    0,0,0,0,0,0,0,0,143,65,0,255,78,99,251,176,115,255,143,107,255,175,107,255,
    175,107,255,111,107,255,78,99,255,78,99,255,78,99,255,78,99,255,78,99,255,78,99,255,
    78,99,255,78,99,255,78,99,255,78,99,255,78,99,255,78,99,255,111,99,255,175,107,255,
    175,107,255,143,107,255,176,115,255,78,99,251,65,0,255,0,0,176,0,0,0,0,0,0,
    0,0,0,0,0,0,162,8,23,0,0,239,69,41,253,241,115,253,143,107,255,175,107,255,
    175,107,255,175,107,255,176,115,255,176,115,255,176,115,255,176,115,255,176,115,255,176,115,255,
    176,115,255,176,115,255,176,115,255,176,115,255,176,115,255,176,115,255,175,115,255,175,107,255,
    175,107,255,143,107,255,240,115,254,69,41,252,0,0,253,65,0,43,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,32,0,102,0,0,255,41,66,250,241,123,255,143,107,255,
    175,107,255,175,107,255,175,107,255,175,107,255,175,107,255,175,107,255,175,107,255,175,107,255,
    175,107,255,175,107,255,175,107,255,175,107,255,175,107,255,175,107,255,175,107,255,175,107,255,
    143,107,255,241,123,255,41,66,250,0,0,255,0,0,133,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,161,0,0,255,106,74,250,241,123,255,
    143,107,255,143,107,255,175,107,255,175,107,255,175,107,255,175,107,255,175,107,255,175,107,255,
    175,107,255,175,107,255,175,107,255,175,107,255,175,107,255,175,107,255,143,107,255,143,107,255,
    241,123,255,106,74,250,0,0,255,0,0,188,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,5,0,6,0,0,190,0,0,255,73,66,250,
    240,115,254,176,115,255,143,107,255,143,107,255,175,107,255,175,107,255,175,107,255,175,107,255,
    175,107,255,175,107,255,175,107,255,175,107,255,143,107,255,143,107,255,176,115,255,240,115,254,
    73,66,250,0,0,255,0,0,212,1,0,18,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,9,0,0,164,0,0,255,
    102,41,255,78,99,251,240,115,254,208,115,255,175,107,255,143,107,255,143,107,255,143,107,255,
    143,107,255,143,107,255,143,107,255,175,107,255,208,115,255,240,115,254,78,99,252,102,41,254,
    0,0,255,0,0,185,0,0,20,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,113,
    0,0,242,65,8,255,167,49,255,13,91,252,176,115,252,240,115,253,240,115,254,240,115,254,
    240,115,254,240,115,254,240,115,253,176,115,252,13,91,251,167,49,255,65,8,255,0,0,250,
    0,0,134,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    33,0,35,0,0,159,0,0,242,0,0,255,163,16,255,102,41,255,232,57,255,73,66,255,
    73,66,255,232,57,255,102,41,255,163,16,255,0,0,255,0,0,248,0,0,172,33,0,49,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,130,16,28,0,0,100,0,0,170,0,0,215,0,0,237,0,0,250,
    0,0,250,0,0,239,0,0,220,0,0,178,0,0,110,97,8,36,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,162,16,24,97,8,42,
    97,8,42,130,16,27,73,74,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  },
  { // 1 监控
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,231,8,18,
    231,8,18,82,34,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,137,25,10,0,0,71,0,0,141,0,0,190,0,0,217,0,0,233,
    0,0,233,0,0,220,0,0,195,0,0,149,0,0,80,229,8,16,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    101,0,16,0,0,128,0,0,223,0,0,255,165,8,255,76,17,255,208,17,255,18,26,255,
    18,26,255,208,17,255,76,17,255,165,8,255,0,0,255,0,0,230,0,0,142,67,0,27,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,85,
    0,0,224,34,0,255,142,17,255,185,34,253,94,43,251,159,43,252,127,43,253,127,43,254,
    127,43,254,127,43,253,159,43,252,94,43,251,217,34,252,142,17,255,34,0,255,0,0,235,
    0,0,105,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,136,0,0,255,
    75,9,255,27,35,251,159,43,253,127,43,255,94,43,255,61,35,255,61,35,255,61,35,255,
    61,35,255,61,35,255,61,35,255,94,43,255,127,43,255,159,43,254,27,35,251,75,9,255,
    0,0,255,0,0,159,5,0,6,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,166,0,0,255,18,26,251,
    127,43,253,94,43,255,61,35,255,61,43,255,94,43,255,94,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,94,43,255,61,43,255,61,35,255,94,43,255,127,43,254,
    18,26,251,0,0,255,0,0,189,3,0,8,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,137,0,0,255,52,26,250,159,43,255,
    61,35,255,61,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,61,43,255,61,35,255,
    159,43,255,52,26,250,0,0,255,0,0,166,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,33,0,81,0,0,255,18,26,250,159,43,254,61,35,255,
    94,43,255,94,43,255,94,43,255,94,43,255,62,35,255,62,35,255,62,35,255,62,35,255,
    94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,
    61,35,255,159,43,255,18,26,250,0,0,255,32,0,111,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,134,16,14,0,0,230,42,9,254,159,43,253,61,35,255,94,43,255,
    94,43,255,94,43,255,94,43,255,62,35,255,126,51,255,158,51,255,158,51,255,158,51,255,
    62,35,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,
    94,43,255,61,35,255,159,43,254,43,9,252,0,0,247,67,8,31,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,127,34,0,255,27,35,251,94,43,255,61,43,255,94,43,255,
    94,43,255,94,43,255,62,35,255,158,100,255,159,239,255,159,239,255,159,239,255,159,239,255,
    30,125,255,29,35,255,126,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,
    94,43,255,61,43,255,94,43,255,27,35,251,34,0,255,0,0,160,0,0,0,0,0,0,
    0,0,0,206,25,9,0,0,228,141,17,254,159,43,253,61,35,255,94,43,255,94,43,255,
    94,43,255,62,35,255,157,2,255,158,149,255,255,255,255,255,255,255,255,255,255,255,255,255,
    63,174,255,157,10,255,253,26,255,253,26,255,221,18,255,221,18,255,29,35,255,94,43,255,
    94,43,255,94,43,255,61,35,255,159,43,254,141,17,254,0,0,246,165,8,24,0,0,0,
    0,0,0,0,0,70,0,0,255,185,34,253,127,43,255,61,43,255,94,43,255,94,43,255,
    29,35,255,254,67,255,94,133,255,191,198,255,255,255,255,255,255,255,255,255,255,255,255,255,
    63,223,255,126,141,255,190,149,255,159,149,255,223,157,255,223,157,255,62,84,255,29,27,255,
    94,43,255,94,43,255,61,43,255,127,43,255,217,34,251,0,0,255,0,0,100,0,0,0,
    0,0,0,0,0,143,133,0,254,94,43,251,94,43,255,94,43,255,94,43,255,61,35,255,
    254,67,255,191,247,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,127,231,255,
    159,239,255,255,255,255,255,255,255,255,255,255,95,223,255,95,223,255,255,255,255,94,84,255,
    29,27,255,94,43,255,94,43,255,94,43,255,94,43,251,133,0,254,0,0,173,0,0,0,
    0,0,0,0,0,192,75,17,255,159,43,252,61,35,255,94,43,255,94,43,255,253,18,255,
    158,149,255,255,255,255,223,255,255,255,255,255,255,255,255,63,223,255,94,92,255,62,35,255,
    29,35,255,62,84,255,63,223,255,255,255,255,62,35,255,30,35,255,255,255,255,63,174,255,
    253,26,255,94,43,255,94,43,255,61,35,255,127,43,253,75,17,255,0,0,217,0,0,0,
    0,0,0,0,0,219,208,17,255,159,43,253,61,35,255,94,43,255,94,43,255,253,26,255,
    158,149,255,255,255,255,255,255,255,255,255,255,159,239,255,190,59,255,189,18,255,62,84,255,
    62,84,255,189,10,255,190,59,255,159,239,255,255,165,255,223,157,255,255,255,255,63,174,255,
    253,26,255,94,43,255,94,43,255,61,35,255,159,43,254,208,17,255,0,0,239,165,8,25,
    231,8,18,0,0,234,18,26,255,127,43,254,61,35,255,94,43,255,94,43,255,253,26,255,
    158,149,255,255,255,255,223,255,255,255,255,255,223,157,255,157,10,255,62,125,255,255,255,255,
    255,255,255,62,125,255,157,10,255,223,157,255,255,255,255,255,255,255,255,255,255,63,174,255,
    253,26,255,94,43,255,94,43,255,61,35,255,127,43,254,18,26,255,0,0,250,66,0,43,
    198,8,19,0,0,235,18,26,255,127,43,254,61,35,255,94,43,255,94,43,255,253,26,255,
    158,149,255,255,255,255,223,255,255,255,255,255,30,125,255,189,18,255,31,215,255,255,255,255,
    255,255,255,31,215,255,189,18,255,30,125,255,255,255,255,223,247,255,255,255,255,63,174,255,
    253,26,255,94,43,255,94,43,255,61,35,255,127,43,254,18,26,255,0,0,251,66,0,43,
    82,34,7,0,0,221,208,17,255,159,43,253,61,35,255,94,43,255,94,43,255,253,26,255,
    158,149,255,255,255,255,223,255,255,255,255,255,223,157,255,157,10,255,62,125,255,255,255,255,
    255,255,255,62,125,255,157,10,255,223,157,255,255,255,255,223,255,255,255,255,255,63,174,255,
    253,26,255,94,43,255,94,43,255,61,35,255,159,43,254,208,17,255,0,0,241,132,8,28,
    0,0,0,0,0,196,75,17,255,159,43,252,61,35,255,94,43,255,94,43,255,253,26,255,
    158,149,255,255,255,255,255,255,255,255,255,255,159,239,255,190,59,255,189,18,255,62,84,255,
    62,84,255,189,18,255,190,59,255,159,239,255,255,255,255,255,255,255,255,255,255,63,174,255,
    253,26,255,94,43,255,94,43,255,61,35,255,127,43,253,75,17,255,0,0,220,0,0,0,
    0,0,0,0,0,151,133,0,254,94,43,251,94,43,255,94,43,255,94,43,255,253,18,255,
    190,149,255,255,255,255,223,255,255,255,255,255,255,255,255,63,223,255,62,84,255,29,27,255,
    29,27,255,62,84,255,63,223,255,255,255,255,255,255,255,223,255,255,255,255,255,95,182,255,
    253,26,255,94,43,255,94,43,255,94,43,255,94,43,252,133,0,255,0,0,180,0,0,0,
    0,0,0,0,0,80,0,0,255,217,34,252,127,43,255,61,43,255,94,43,255,29,35,255,
    62,84,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,191,247,255,
    191,247,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,158,100,255,
    29,27,255,94,43,255,61,43,255,127,43,255,217,34,251,0,0,255,0,0,111,0,0,0,
    0,0,0,41,17,14,0,0,236,141,17,254,159,43,253,61,35,255,94,43,255,94,43,255,
    29,27,255,94,92,255,31,174,255,63,174,255,63,174,255,63,174,255,63,174,255,95,182,255,
    95,182,255,63,174,255,63,174,255,63,174,255,63,174,255,63,174,255,158,100,255,29,27,255,
    94,43,255,94,43,255,61,35,255,159,43,254,141,17,254,0,0,250,99,0,32,0,0,0,
    0,0,0,0,0,0,0,0,143,34,0,255,27,35,251,94,43,255,61,43,255,94,43,255,
    94,43,255,29,27,255,253,26,255,253,26,255,253,26,255,253,26,255,253,26,255,253,26,255,
    253,26,255,253,26,255,253,26,255,253,26,255,253,26,255,253,26,255,29,27,255,94,43,255,
    94,43,255,61,43,255,94,43,255,27,35,251,34,0,255,0,0,176,0,0,0,0,0,0,
    0,0,0,0,0,0,68,8,23,0,0,239,42,9,253,159,43,253,61,35,255,94,43,255,
    94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,
    94,43,255,61,35,255,159,43,254,43,9,252,0,0,253,34,0,43,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,32,0,102,0,0,255,18,26,250,159,43,255,61,35,255,
    94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,
    61,35,255,159,43,255,18,26,250,0,0,255,0,0,133,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,161,0,0,255,52,26,250,159,43,255,
    61,35,255,61,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,61,43,255,61,35,255,
    159,43,255,52,26,250,0,0,255,0,0,188,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,5,0,6,0,0,190,0,0,255,18,26,250,
    127,43,254,94,43,255,61,35,255,61,43,255,94,43,255,94,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,94,43,255,61,43,255,61,35,255,94,43,255,127,43,254,
    18,26,250,0,0,255,0,0,212,1,0,18,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,0,9,0,0,164,0,0,255,
    75,9,255,27,35,251,159,43,254,127,43,255,94,43,255,61,35,255,61,35,255,61,35,255,
    61,35,255,61,35,255,61,35,255,94,43,255,127,43,255,159,43,254,27,35,252,75,9,254,
    0,0,255,0,0,185,1,0,20,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,113,
    0,0,242,34,0,255,142,17,255,217,34,252,94,43,252,127,43,253,127,43,254,127,43,254,
    127,43,254,127,43,254,127,43,253,94,43,252,217,34,251,142,17,255,34,0,255,0,0,250,
    0,0,134,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    34,0,35,0,0,159,0,0,242,0,0,255,165,8,255,76,17,255,208,17,255,18,26,255,
    18,26,255,208,17,255,76,17,255,165,8,255,0,0,255,0,0,248,0,0,172,33,0,49,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,131,8,28,0,0,100,0,0,170,0,0,215,0,0,237,0,0,250,
    0,0,250,0,0,239,0,0,220,0,0,178,0,0,110,98,0,36,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,165,8,24,99,0,42,
    99,0,42,132,8,27,82,34,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  },
  { // 2 测速
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,97,56,18,
    97,56,18,36,145,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,72,10,0,0,71,0,0,141,0,0,190,0,0,217,0,0,233,
    0,0,233,0,0,220,0,0,195,0,0,149,0,0,80,0,40,16,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,40,16,0,0,128,0,0,223,0,0,255,32,40,255,97,88,255,130,120,255,162,136,255,
    162,136,255,130,120,255,97,88,255,32,40,255,0,0,255,0,0,230,0,0,142,0,24,27,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,85,
    0,0,224,0,16,255,97,104,255,195,184,253,4,225,251,36,241,252,36,241,253,36,241,254,
    36,241,254,36,241,253,36,241,252,4,225,251,195,184,252,97,104,255,0,16,255,0,0,235,
    0,0,105,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,136,0,0,255,
    97,80,255,227,208,251,36,241,253,36,233,255,227,224,255,162,216,255,130,216,255,97,216,255,
    97,216,255,130,216,255,162,216,255,227,224,255,36,233,255,36,241,254,227,208,251,97,80,255,
    0,0,255,0,0,159,0,40,6,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,166,0,0,255,162,136,251,
    4,241,253,36,225,255,227,216,255,130,216,255,4,225,255,105,226,255,174,235,255,113,244,255,
    113,244,255,174,235,255,105,226,255,4,225,255,130,216,255,227,216,255,36,225,255,4,241,254,
    162,136,251,0,0,255,0,0,189,0,24,8,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,137,0,0,255,162,144,250,4,241,255,
    4,217,255,162,216,255,69,225,255,81,236,255,251,254,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,251,254,255,81,236,255,69,225,255,162,216,255,4,217,255,
    4,241,255,162,144,250,0,0,255,0,0,166,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,8,81,0,0,255,130,136,250,4,241,254,4,217,255,
    130,216,255,170,234,255,28,255,255,255,255,255,255,255,255,223,255,255,223,255,255,223,255,255,
    223,255,255,223,255,255,223,255,255,255,255,255,255,255,255,28,255,255,170,234,255,130,216,255,
    4,217,255,4,241,255,130,136,250,0,0,255,0,0,111,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,48,14,0,0,230,97,80,254,4,241,253,4,217,255,130,216,255,
    77,235,255,255,255,255,255,255,255,223,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,223,255,255,255,255,255,255,255,255,77,235,255,
    130,216,255,4,217,255,4,241,254,97,80,252,0,0,247,0,24,31,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,127,0,8,255,227,208,251,36,225,255,162,216,255,170,234,255,
    255,255,255,255,255,255,223,255,255,255,255,255,190,247,255,182,181,255,77,107,255,105,74,255,
    73,74,255,77,107,255,182,181,255,190,247,255,255,255,255,223,255,255,255,255,255,255,255,255,
    170,234,255,162,216,255,36,225,255,227,208,251,0,8,255,0,0,160,0,0,0,0,0,0,
    0,0,0,0,80,9,0,0,228,97,96,254,4,241,253,227,216,255,69,225,255,60,255,255,
    255,255,255,223,255,255,255,255,255,117,173,255,69,41,255,0,0,255,0,0,255,0,0,255,
    0,0,255,0,0,255,0,0,255,36,33,255,117,173,255,255,255,255,223,255,255,255,255,255,
    60,255,255,69,225,255,227,216,255,4,241,254,97,96,254,0,0,246,0,24,24,0,0,0,
    0,0,0,0,0,70,0,0,255,195,184,253,36,233,255,97,216,255,113,244,255,255,255,255,
    223,255,255,255,255,255,207,123,255,0,0,255,0,0,255,36,33,255,142,115,255,20,165,255,
    20,165,255,142,115,255,69,41,255,0,0,255,0,0,255,207,123,255,255,255,255,223,255,255,
    255,255,255,113,244,255,97,216,255,36,233,255,195,184,251,0,0,255,0,0,100,0,0,0,
    0,0,0,0,0,143,32,40,254,4,225,251,227,224,255,4,225,255,28,255,255,255,255,255,
    255,255,255,81,140,255,0,0,255,32,0,255,48,132,255,158,247,255,255,255,255,255,255,255,
    255,255,255,255,255,255,24,198,255,65,8,255,0,0,255,0,0,255,81,140,255,255,255,255,
    255,255,255,28,255,255,4,225,255,227,224,255,4,225,251,32,40,254,0,0,173,0,0,0,
    0,0,0,0,0,192,97,88,255,36,241,252,162,216,255,105,234,255,255,255,255,255,255,255,
    121,214,255,0,0,255,0,0,255,52,165,255,255,255,255,255,255,255,223,255,255,190,247,255,
    223,255,255,125,239,255,101,41,255,0,0,255,231,57,255,65,8,255,0,0,255,121,214,255,
    255,255,255,255,255,255,105,234,255,162,216,255,36,241,253,97,88,255,0,0,217,0,0,0,
    0,0,0,0,0,219,130,120,255,36,241,253,130,216,255,207,235,255,255,255,255,255,255,255,
    170,82,255,0,0,255,203,90,255,255,255,255,223,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,170,82,255,0,0,255,105,74,255,223,255,255,235,90,255,0,0,255,170,82,255,
    255,255,255,255,255,255,207,235,255,130,216,255,36,241,254,130,120,255,0,0,239,65,40,25,
    97,56,18,0,0,234,162,136,255,36,241,254,97,216,255,113,236,255,255,255,255,93,239,255,
    97,8,255,0,0,255,121,206,255,255,255,255,223,255,255,255,255,255,223,255,255,243,156,255,
    203,90,255,0,0,255,101,41,255,158,247,255,255,255,255,89,206,255,0,0,255,97,8,255,
    93,239,255,255,255,255,113,236,255,97,216,255,36,241,254,162,136,255,0,0,250,32,16,43,
    97,48,19,0,0,235,162,136,255,36,241,254,97,216,255,113,236,255,255,255,255,24,198,255,
    0,0,255,4,33,255,223,255,255,255,255,255,255,255,255,255,255,255,235,90,255,0,0,255,
    0,0,255,65,8,255,154,214,255,255,255,255,255,255,255,223,255,255,4,33,255,0,0,255,
    24,198,255,255,255,255,113,236,255,97,216,255,36,241,254,162,136,255,0,0,251,32,16,43,
    36,145,7,0,0,221,130,120,255,36,241,253,130,216,255,174,235,255,255,255,255,215,189,255,
    0,0,255,166,49,255,255,255,255,255,255,255,255,255,255,158,247,255,65,8,255,32,0,255,
    65,8,255,0,0,255,28,231,255,255,255,255,255,255,255,255,255,255,166,49,255,0,0,255,
    215,189,255,255,255,255,174,235,255,130,216,255,36,241,254,130,120,255,0,0,241,65,32,28,
    0,0,0,0,0,196,97,88,255,36,241,252,162,216,255,105,226,255,255,255,255,24,198,255,
    0,0,255,4,33,255,223,255,255,255,255,255,255,255,255,255,255,255,73,74,255,0,0,255,
    0,0,255,101,41,255,190,247,255,255,255,255,255,255,255,223,255,255,4,33,255,0,0,255,
    24,198,255,255,255,255,105,226,255,162,216,255,36,241,253,97,88,255,0,0,220,0,0,0,
    0,0,0,0,0,151,32,40,254,4,225,251,227,224,255,4,225,255,60,255,255,125,239,255,
    97,8,255,0,0,255,121,206,255,255,255,255,223,255,255,255,255,255,125,239,255,207,123,255,
    142,115,255,28,231,255,255,255,255,223,255,255,255,255,255,121,206,255,0,0,255,97,8,255,
    125,239,255,60,255,255,4,225,255,227,224,255,4,225,252,32,40,255,0,0,180,0,0,0,
    0,0,0,0,0,80,0,0,255,195,184,252,36,233,255,130,216,255,81,236,255,255,255,255,
    138,82,255,0,0,255,170,82,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,170,82,255,0,0,255,138,82,255,
    255,255,255,81,236,255,130,216,255,36,233,255,195,184,251,0,0,255,0,0,111,0,0,0,
    0,0,0,0,48,14,0,0,236,97,96,254,4,241,253,227,216,255,36,225,255,93,255,255,
    186,214,255,97,8,255,105,74,255,255,255,255,255,255,255,255,255,255,255,255,255,223,255,255,
    223,255,255,255,255,255,255,255,255,255,255,255,255,255,255,105,74,255,97,8,255,186,214,255,
    93,255,255,36,225,255,227,216,255,4,241,254,97,96,254,0,0,250,0,16,32,0,0,0,
    0,0,0,0,0,0,0,0,143,0,8,255,227,208,251,36,225,255,162,216,255,170,234,255,
    255,255,255,60,231,255,190,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,190,255,255,60,231,255,255,255,255,
    170,234,255,162,216,255,36,225,255,227,208,251,0,8,255,0,0,176,0,0,0,0,0,0,
    0,0,0,0,0,0,0,32,23,0,0,239,97,80,253,4,241,253,4,217,255,130,216,255,
    77,235,255,255,255,255,255,255,255,223,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,223,255,255,255,255,255,255,255,255,77,235,255,
    130,216,255,4,217,255,4,241,254,97,80,252,0,0,253,0,16,43,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,102,0,0,255,130,136,250,4,241,255,4,217,255,
    130,216,255,170,226,255,251,254,255,255,255,255,255,255,255,255,255,255,255,255,255,223,255,255,
    223,255,255,255,255,255,255,255,255,255,255,255,255,255,255,251,254,255,170,226,255,130,216,255,
    4,217,255,4,241,255,130,136,250,0,0,255,0,0,133,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,161,0,0,255,162,144,250,4,241,255,
    4,217,255,162,216,255,69,225,255,81,236,255,251,254,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,251,254,255,81,236,255,69,225,255,162,216,255,4,217,255,
    4,241,255,162,144,250,0,0,255,0,0,188,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,40,6,0,0,190,0,0,255,162,136,250,
    4,241,254,36,225,255,227,216,255,130,216,255,4,225,255,105,226,255,174,235,255,113,244,255,
    113,244,255,174,235,255,105,226,255,4,225,255,130,216,255,227,216,255,36,225,255,4,241,254,
    162,136,250,0,0,255,0,0,212,0,8,18,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,24,9,0,0,164,0,0,255,
    97,80,255,227,208,251,36,241,254,36,233,255,227,224,255,162,216,255,130,216,255,97,216,255,
    97,216,255,130,216,255,162,216,255,227,224,255,36,233,255,36,241,254,227,208,252,97,80,254,
    0,0,255,0,0,185,0,8,20,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,113,
    0,0,242,0,16,255,97,104,255,195,184,252,4,225,252,36,241,253,36,241,254,36,241,254,
    36,241,254,36,241,254,36,241,253,4,225,252,195,192,251,97,104,255,0,16,255,0,0,250,
    0,0,134,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,16,35,0,0,159,0,0,242,0,0,255,32,40,255,97,88,255,130,120,255,162,136,255,
    162,136,255,130,120,255,97,88,255,32,40,255,0,0,255,0,0,248,0,0,172,0,8,49,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,24,28,0,0,100,0,0,170,0,0,215,0,0,237,0,0,250,
    0,0,250,0,0,239,0,0,220,0,0,178,0,0,110,0,16,36,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,65,40,24,32,24,42,
    32,24,42,65,32,27,36,145,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  },
  { // 3 闯红灯
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,97,56,18,
    97,56,18,36,145,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,72,10,0,0,71,0,0,141,0,0,190,0,0,217,0,0,233,
    0,0,233,0,0,220,0,0,195,0,0,149,0,0,80,0,40,16,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,40,16,0,0,128,0,0,223,0,0,255,32,40,255,97,88,255,130,120,255,162,136,255,
    162,136,255,130,120,255,97,88,255,32,40,255,0,0,255,0,0,230,0,0,142,0,24,27,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,85,
    0,0,224,0,16,255,97,104,255,195,184,253,4,225,251,4,241,252,4,241,253,4,241,254,
    4,241,254,4,241,253,4,241,252,4,225,251,195,184,252,97,104,255,0,16,255,0,0,235,
    0,0,105,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,136,0,0,255,
    97,80,255,227,208,251,4,241,253,4,233,255,4,225,255,4,217,255,4,217,255,4,217,255,
    4,217,255,4,217,255,4,217,255,4,225,255,4,233,255,4,241,254,227,208,251,97,80,255,
    0,0,255,0,0,159,0,40,6,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,166,0,0,255,162,136,251,
    4,241,253,4,225,255,227,216,255,227,216,255,4,225,255,195,224,255,130,216,255,97,216,255,
    97,216,255,130,216,255,195,224,255,4,225,255,227,216,255,227,216,255,4,225,255,4,241,254,
    162,136,251,0,0,255,0,0,189,0,24,8,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,137,0,0,255,162,144,250,4,241,255,
    4,217,255,4,217,255,4,225,255,4,225,255,162,224,255,166,225,255,239,235,255,146,244,255,
    146,244,255,16,236,255,8,226,255,162,224,255,4,225,255,4,225,255,4,217,255,4,217,255,
    4,241,255,162,144,250,0,0,255,0,0,166,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,8,81,0,0,255,130,136,250,4,241,254,4,217,255,
    4,225,255,4,225,255,4,225,255,162,224,255,105,226,255,125,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,190,255,255,12,235,255,162,216,255,4,225,255,4,225,255,4,225,255,
    4,217,255,4,241,255,130,136,250,0,0,255,0,0,111,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,48,14,0,0,230,97,80,254,4,241,253,4,217,255,4,225,255,
    4,225,255,4,225,255,4,225,255,195,224,255,121,254,255,255,255,255,243,244,255,134,225,255,
    101,225,255,81,236,255,255,255,255,60,255,255,36,225,255,227,224,255,4,225,255,4,225,255,
    4,225,255,4,217,255,4,241,254,97,80,252,0,0,247,0,24,31,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,127,0,8,255,227,208,251,4,225,255,4,217,255,4,225,255,
    4,225,255,4,225,255,227,224,255,101,225,255,223,255,255,89,254,255,130,216,255,195,224,255,
    227,224,255,65,216,255,117,245,255,255,255,255,8,226,255,195,224,255,4,225,255,4,225,255,
    4,225,255,4,217,255,4,225,255,227,208,251,0,8,255,0,0,160,0,0,0,0,0,0,
    0,0,0,0,80,9,0,0,228,97,96,254,4,241,253,4,217,255,4,225,255,4,225,255,
    4,225,255,4,225,255,227,224,255,69,225,255,255,255,255,113,244,255,130,216,255,36,225,255,
    36,225,255,130,216,255,109,235,255,255,255,255,231,225,255,195,224,255,4,225,255,4,225,255,
    4,225,255,4,225,255,4,217,255,4,241,254,97,96,254,0,0,246,0,24,24,0,0,0,
    0,0,0,0,0,70,0,0,255,195,184,253,4,233,255,4,217,255,4,225,255,4,225,255,
    4,225,255,4,225,255,227,224,255,101,225,255,255,255,255,24,246,255,97,216,255,227,216,255,
    4,225,255,32,216,255,20,245,255,255,255,255,231,225,255,195,224,255,4,225,255,4,225,255,
    4,225,255,4,225,255,4,217,255,4,233,255,195,184,251,0,0,255,0,0,100,0,0,0,
    0,0,0,0,0,143,32,40,254,4,225,251,4,225,255,4,225,255,4,225,255,4,225,255,
    4,225,255,4,225,255,227,224,255,101,225,255,125,255,255,255,255,255,81,244,255,69,233,255,
    36,225,255,174,235,255,223,255,255,255,255,255,8,226,255,195,224,255,4,225,255,4,225,255,
    4,225,255,4,225,255,4,225,255,4,225,255,4,225,251,32,40,254,0,0,173,0,0,0,
    0,0,0,0,0,192,97,88,255,4,241,252,4,217,255,4,225,255,4,225,255,4,225,255,
    4,225,255,4,225,255,227,224,255,101,225,255,125,255,255,255,255,255,255,255,255,56,222,255,
    24,222,255,255,255,255,255,255,255,255,255,255,8,226,255,195,224,255,4,225,255,4,225,255,
    4,225,255,4,225,255,4,225,255,4,217,255,4,241,253,97,88,255,0,0,217,0,0,0,
    0,0,0,0,0,219,130,120,255,4,241,253,227,216,255,4,225,255,4,225,255,4,225,255,
    4,225,255,4,225,255,227,224,255,101,225,255,158,255,255,255,255,255,150,181,255,146,140,255,
    178,140,255,150,181,255,255,255,255,255,255,255,8,226,255,195,224,255,4,225,255,4,225,255,
    4,225,255,4,225,255,4,225,255,227,216,255,4,241,254,130,120,255,0,0,239,65,40,25,
    97,56,18,0,0,234,162,136,255,4,241,254,4,217,255,4,225,255,4,225,255,4,225,255,
    4,225,255,4,225,255,227,224,255,101,225,255,190,255,255,28,223,255,113,140,255,178,156,255,
    178,156,255,113,140,255,251,222,255,255,255,255,8,226,255,195,224,255,4,225,255,4,225,255,
    4,225,255,4,225,255,4,225,255,4,217,255,4,241,254,162,136,255,0,0,250,32,16,43,
    97,48,19,0,0,235,162,136,255,4,241,254,4,217,255,4,225,255,4,225,255,4,225,255,
    4,225,255,4,225,255,227,224,255,101,225,255,190,255,255,28,223,255,113,140,255,211,156,255,
    211,156,255,113,140,255,251,222,255,255,255,255,8,226,255,195,224,255,4,225,255,4,225,255,
    4,225,255,4,225,255,4,225,255,4,217,255,4,241,254,162,136,255,0,0,251,32,16,43,
    36,145,7,0,0,221,130,120,255,4,241,253,227,216,255,4,225,255,4,225,255,4,225,255,
    4,225,255,4,225,255,227,224,255,101,225,255,158,255,255,255,255,255,150,181,255,113,140,255,
    113,140,255,150,181,255,255,255,255,255,255,255,8,226,255,195,224,255,4,225,255,4,225,255,
    4,225,255,4,225,255,4,225,255,227,216,255,4,241,254,130,120,255,0,0,241,65,32,28,
    0,0,0,0,0,196,97,88,255,4,241,252,4,217,255,4,225,255,4,225,255,4,225,255,
    4,225,255,4,225,255,227,224,255,101,225,255,125,255,255,255,255,255,255,255,255,219,222,255,
    219,222,255,255,255,255,255,255,255,255,255,255,8,226,255,195,224,255,4,225,255,4,225,255,
    4,225,255,4,225,255,4,225,255,4,217,255,4,241,253,97,88,255,0,0,220,0,0,0,
    0,0,0,0,0,151,32,40,254,4,225,251,4,225,255,4,225,255,4,225,255,4,225,255,
    4,225,255,4,225,255,227,224,255,101,225,255,125,255,255,255,255,255,186,214,255,52,165,255,
    52,165,255,186,214,255,255,255,255,255,255,255,8,226,255,195,224,255,4,225,255,4,225,255,
    4,225,255,4,225,255,4,225,255,4,225,255,4,225,252,32,40,255,0,0,180,0,0,0,
    0,0,0,0,0,80,0,0,255,195,184,252,4,233,255,4,217,255,4,225,255,4,225,255,
    4,225,255,4,225,255,227,224,255,101,225,255,190,255,255,125,239,255,146,148,255,146,148,255,
    146,148,255,146,148,255,93,239,255,255,255,255,8,226,255,195,224,255,4,225,255,4,225,255,
    4,225,255,4,225,255,4,217,255,4,233,255,195,184,251,0,0,255,0,0,111,0,0,0,
    0,0,0,0,48,14,0,0,236,97,96,254,4,241,253,4,217,255,4,225,255,4,225,255,
    4,225,255,4,225,255,227,224,255,101,225,255,190,255,255,219,214,255,113,140,255,211,156,255,
    211,156,255,113,140,255,186,214,255,255,255,255,8,226,255,195,224,255,4,225,255,4,225,255,
    4,225,255,4,225,255,4,217,255,4,241,254,97,96,254,0,0,250,0,16,32,0,0,0,
    0,0,0,0,0,0,0,0,143,0,8,255,227,208,251,4,225,255,4,217,255,4,225,255,
    4,225,255,4,225,255,227,224,255,134,225,255,190,255,255,190,247,255,211,156,255,113,140,255,
    113,140,255,211,156,255,158,247,255,255,255,255,8,226,255,195,224,255,4,225,255,4,225,255,
    4,225,255,4,217,255,4,225,255,227,208,251,0,8,255,0,0,176,0,0,0,0,0,0,
    0,0,0,0,0,0,0,32,23,0,0,239,97,80,253,4,241,253,4,217,255,4,225,255,
    4,225,255,4,225,255,4,225,255,227,224,255,219,254,255,255,255,255,60,239,255,150,181,255,
    150,181,255,60,239,255,255,255,255,125,255,255,69,225,255,227,224,255,4,225,255,4,225,255,
    4,225,255,4,217,255,4,241,254,97,80,252,0,0,253,0,16,43,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,102,0,0,255,130,136,250,4,241,255,4,217,255,
    4,225,255,4,225,255,4,225,255,162,216,255,235,234,255,223,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,174,235,255,130,216,255,4,225,255,4,225,255,4,225,255,
    4,217,255,4,241,255,130,136,250,0,0,255,0,0,133,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,161,0,0,255,162,144,250,4,241,255,
    4,217,255,4,217,255,4,225,255,4,225,255,162,224,255,73,226,255,211,244,255,85,245,255,
    85,245,255,20,245,255,170,234,255,162,224,255,4,225,255,4,225,255,4,217,255,4,217,255,
    4,241,255,162,144,250,0,0,255,0,0,188,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,40,6,0,0,190,0,0,255,162,136,250,
    4,241,254,4,225,255,227,216,255,227,216,255,4,225,255,162,224,255,130,216,255,130,216,255,
    130,216,255,130,216,255,162,224,255,4,225,255,4,217,255,227,216,255,4,225,255,4,241,254,
    162,136,250,0,0,255,0,0,212,0,8,18,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,24,9,0,0,164,0,0,255,
    97,80,255,227,208,251,4,241,254,4,233,255,4,225,255,4,217,255,4,217,255,4,217,255,
    4,217,255,4,217,255,4,217,255,4,225,255,4,233,255,4,241,254,227,208,252,97,80,254,
    0,0,255,0,0,185,0,8,20,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,113,
    0,0,242,0,16,255,97,104,255,195,184,252,4,225,252,4,241,253,4,241,254,4,241,254,
    4,241,254,4,241,254,4,241,253,4,225,252,195,192,251,97,104,255,0,16,255,0,0,250,
    0,0,134,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,16,35,0,0,159,0,0,242,0,0,255,32,40,255,97,88,255,130,120,255,162,136,255,
    162,136,255,130,120,255,97,88,255,32,40,255,0,0,255,0,0,248,0,0,172,0,8,49,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,24,28,0,0,100,0,0,170,0,0,215,0,0,237,0,0,250,
    0,0,250,0,0,239,0,0,220,0,0,178,0,0,110,0,16,36,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,65,40,24,32,24,42,
    32,24,42,65,32,27,36,145,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  },
  { // 4 违章拍照
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,224,56,18,
    224,56,18,64,146,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,128,73,10,0,0,71,0,0,141,0,0,190,0,0,217,0,0,233,
    0,0,233,0,0,220,0,0,195,0,0,149,0,0,80,224,40,16,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    96,40,16,0,0,128,0,0,223,0,0,255,160,40,255,129,97,255,1,130,255,97,146,255,
    97,146,255,1,130,255,129,97,255,160,40,255,0,0,255,0,0,230,0,0,142,64,24,27,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,8,85,
    0,0,224,64,16,255,193,113,255,34,203,253,194,243,251,2,252,252,2,252,253,2,252,254,
    2,252,254,2,252,253,2,252,252,194,243,251,34,203,252,193,113,255,64,16,255,0,0,235,
    32,0,105,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,136,0,0,255,
    97,89,255,98,219,251,2,252,253,226,251,255,194,243,255,162,235,255,162,235,255,162,235,255,
    162,235,255,162,235,255,162,235,255,194,243,255,226,251,255,2,252,254,65,219,251,32,89,255,
    0,0,255,0,0,159,0,40,6,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,166,0,0,255,65,146,251,
    2,252,253,194,243,255,162,235,255,162,235,255,194,243,255,194,243,255,194,243,255,194,243,255,
    194,243,255,194,243,255,194,243,255,194,243,255,194,235,255,129,235,255,102,252,255,42,253,254,
    197,146,251,0,0,255,0,0,189,0,24,8,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,137,0,0,255,129,162,250,2,252,255,
    162,235,255,162,235,255,194,243,255,194,243,255,194,243,255,194,243,255,194,243,255,194,243,255,
    194,243,255,194,243,255,194,243,255,194,243,255,162,243,255,195,243,255,124,255,255,255,255,255,
    167,252,255,97,162,250,0,0,255,0,0,166,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,32,8,81,0,0,255,65,146,250,2,252,254,162,235,255,
    194,243,255,194,243,255,194,243,255,194,243,255,194,243,255,194,243,255,194,243,255,194,243,255,
    194,243,255,194,243,255,194,243,255,195,243,255,96,235,255,18,254,255,255,255,255,143,245,255,
    64,235,255,35,252,255,65,146,250,0,0,255,32,0,111,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,32,49,14,0,0,230,64,81,254,2,252,253,162,235,255,194,243,255,
    194,243,255,194,243,255,194,243,255,194,243,255,194,243,255,194,243,255,194,243,255,194,243,255,
    194,243,255,194,243,255,194,243,255,129,235,255,136,244,255,255,255,255,216,254,255,96,235,255,
    161,235,255,162,235,255,2,252,254,64,89,252,0,0,247,128,24,31,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,127,64,16,255,98,219,251,194,243,255,162,235,255,194,243,255,
    194,243,255,194,243,255,194,243,255,194,243,255,194,243,255,194,243,255,194,243,255,194,243,255,
    194,243,255,194,243,255,194,243,255,161,243,255,25,255,255,255,255,255,143,245,255,169,244,255,
    135,244,255,161,235,255,194,243,255,98,219,251,64,16,255,0,0,160,0,0,0,0,0,0,
    0,0,0,192,113,9,0,0,228,161,105,254,2,252,253,162,235,255,194,243,255,194,243,255,
    194,243,255,194,243,255,162,243,255,129,235,255,129,235,255,128,235,255,161,243,255,194,243,255,
    194,243,255,195,243,255,96,235,255,143,245,255,255,255,255,255,255,255,255,255,255,255,255,255,
    168,244,255,129,243,255,162,235,255,2,252,254,161,105,254,0,0,246,160,40,24,0,0,0,
    0,0,0,0,0,70,0,0,255,34,203,253,226,251,255,162,235,255,194,243,255,194,243,255,
    194,243,255,194,243,255,227,243,255,201,244,255,202,244,255,202,244,255,70,244,255,162,243,255,
    195,243,255,161,243,255,103,244,255,255,255,255,255,255,255,255,255,255,255,255,255,77,245,255,
    96,235,255,195,243,255,162,235,255,226,251,255,34,203,251,0,0,255,0,0,100,0,0,0,
    0,0,0,0,0,143,160,40,254,194,243,251,194,243,255,194,243,255,194,243,255,194,243,255,
    195,243,255,64,235,255,110,245,255,255,255,255,255,255,255,255,255,255,156,255,255,194,243,255,
    129,235,255,128,235,255,37,244,255,169,244,255,11,245,255,255,255,255,117,254,255,96,235,255,
    195,243,255,194,243,255,194,243,255,194,243,255,194,243,251,160,40,254,0,0,173,0,0,0,
    0,0,0,0,0,192,97,89,255,2,252,252,162,235,255,194,243,255,194,243,255,194,243,255,
    161,235,255,227,243,255,51,254,255,255,255,255,222,255,255,255,255,255,222,255,255,168,244,255,
    70,244,255,103,244,255,103,244,255,161,243,255,44,245,255,157,255,255,194,243,255,162,243,255,
    194,243,255,194,243,255,194,243,255,162,235,255,2,252,253,97,89,255,0,0,217,0,0,0,
    0,0,0,0,0,219,1,130,255,2,252,253,162,235,255,194,243,255,194,243,255,129,235,255,
    142,245,255,222,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,189,255,255,189,255,255,156,255,255,168,244,255,129,235,255,194,243,255,
    194,243,255,194,243,255,194,243,255,162,235,255,2,252,254,1,130,255,0,0,239,160,40,25,
    224,56,18,0,0,234,65,146,255,2,252,254,162,235,255,194,243,255,162,243,255,227,243,255,
    190,255,255,255,255,255,255,255,255,255,255,255,248,254,255,201,244,255,37,244,255,44,245,255,
    157,255,255,190,255,255,128,235,255,84,254,255,223,255,255,194,243,255,194,243,255,194,243,255,
    194,243,255,194,243,255,194,243,255,162,235,255,2,252,254,65,146,255,0,0,250,64,16,43,
    192,48,19,0,0,235,65,146,255,2,252,254,162,235,255,194,243,255,162,243,255,4,244,255,
    189,255,255,255,255,255,255,255,255,25,255,255,129,235,255,162,243,255,136,244,255,64,235,255,
    70,244,255,189,255,255,143,245,255,25,255,255,223,255,255,4,244,255,161,243,255,194,243,255,
    194,243,255,194,243,255,194,243,255,162,235,255,2,252,254,65,146,255,0,0,251,64,16,43,
    64,146,7,0,0,221,1,130,255,2,252,253,162,235,255,194,243,255,162,243,255,4,244,255,
    189,255,255,255,255,255,255,255,255,44,245,255,129,243,255,58,255,255,255,255,255,116,254,255,
    32,235,255,84,254,255,255,255,255,255,255,255,189,255,255,4,244,255,162,243,255,194,243,255,
    194,243,255,194,243,255,194,243,255,162,235,255,2,252,254,1,130,255,0,0,241,128,32,28,
    0,0,0,0,0,196,97,89,255,2,252,252,162,235,255,194,243,255,162,243,255,4,244,255,
    189,255,255,255,255,255,255,255,255,168,244,255,4,244,255,255,255,255,255,255,255,124,255,255,
    96,235,255,209,245,255,255,255,255,255,255,255,189,255,255,4,244,255,162,243,255,194,243,255,
    194,243,255,194,243,255,194,243,255,162,235,255,2,252,253,97,89,255,0,0,220,0,0,0,
    0,0,0,0,0,151,160,40,254,194,243,251,194,243,255,194,243,255,162,243,255,4,244,255,
    189,255,255,255,255,255,255,255,255,18,254,255,32,235,255,142,245,255,215,254,255,169,244,255,
    64,235,255,25,255,255,255,255,255,255,255,255,189,255,255,4,244,255,162,243,255,194,243,255,
    194,243,255,194,243,255,194,243,255,194,243,255,194,243,252,160,40,255,0,0,180,0,0,0,
    0,0,0,0,0,80,0,0,255,34,203,252,226,251,255,194,235,255,162,243,255,4,244,255,
    222,255,255,255,255,255,255,255,255,222,255,255,11,245,255,32,235,255,32,235,255,96,235,255,
    241,253,255,255,255,255,223,255,255,255,255,255,222,255,255,4,244,255,162,243,255,194,243,255,
    194,243,255,194,243,255,162,235,255,226,251,255,34,203,251,0,0,255,0,0,111,0,0,0,
    0,0,0,32,73,14,0,0,236,161,105,254,2,252,253,162,235,255,194,243,255,161,243,255,
    249,254,255,255,255,255,255,255,255,255,255,255,255,255,255,90,255,255,182,254,255,157,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,248,254,255,161,243,255,194,243,255,194,243,255,
    194,243,255,194,243,255,162,235,255,2,252,254,161,105,254,0,0,250,96,24,32,0,0,0,
    0,0,0,0,0,0,0,0,143,64,16,255,98,219,251,194,243,255,194,235,255,161,243,255,
    37,244,255,18,254,255,116,254,255,116,254,255,116,254,255,150,254,255,182,254,255,149,254,255,
    84,254,255,116,254,255,116,254,255,18,254,255,4,244,255,161,243,255,194,243,255,194,243,255,
    194,243,255,162,235,255,194,243,255,98,219,251,64,16,255,0,0,176,0,0,0,0,0,0,
    0,0,0,0,0,0,160,32,23,0,0,239,64,81,253,2,252,253,162,235,255,194,243,255,
    161,243,255,96,235,255,96,235,255,96,235,255,96,235,255,96,235,255,96,235,255,96,235,255,
    96,235,255,96,235,255,96,235,255,96,235,255,162,243,255,194,243,255,194,243,255,194,243,255,
    194,243,255,162,235,255,2,252,254,64,89,252,0,0,253,64,16,43,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,32,0,102,0,0,255,65,146,250,2,252,255,162,235,255,
    194,243,255,195,243,255,194,243,255,195,243,255,195,243,255,195,243,255,195,243,255,195,243,255,
    195,243,255,195,243,255,194,243,255,195,243,255,194,243,255,194,243,255,194,243,255,194,243,255,
    162,235,255,2,252,255,65,146,250,0,0,255,0,0,133,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,161,0,0,255,129,162,250,2,252,255,
    162,235,255,162,235,255,194,243,255,194,243,255,194,243,255,194,243,255,194,243,255,194,243,255,
    194,243,255,194,243,255,194,243,255,194,243,255,194,243,255,194,243,255,162,235,255,162,235,255,
    2,252,255,129,162,250,0,0,255,0,0,188,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,40,6,0,0,190,0,0,255,65,146,250,
    2,252,254,194,243,255,162,235,255,162,235,255,194,243,255,194,243,255,194,243,255,194,243,255,
    194,243,255,194,243,255,194,243,255,194,243,255,162,235,255,162,235,255,194,243,255,2,252,254,
    65,146,250,0,0,255,0,0,212,0,8,18,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,24,9,0,0,164,0,0,255,
    97,89,255,98,219,251,2,252,254,226,251,255,194,243,255,162,235,255,162,235,255,162,235,255,
    162,235,255,162,235,255,162,235,255,194,243,255,226,251,255,2,252,254,98,219,252,97,89,254,
    0,0,255,0,0,185,0,8,20,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,113,
    0,0,242,64,16,255,193,113,255,34,203,252,194,243,252,2,252,253,2,252,254,2,252,254,
    2,252,254,2,252,254,2,252,253,194,243,252,34,203,251,193,113,255,64,16,255,0,0,250,
    0,0,134,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    32,16,35,0,0,159,0,0,242,0,0,255,160,40,255,129,97,255,1,130,255,97,146,255,
    97,146,255,1,130,255,129,97,255,160,40,255,0,0,255,0,0,248,0,0,172,32,8,49,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,128,24,28,0,0,100,0,0,170,0,0,215,0,0,237,0,0,250,
    0,0,250,0,0,239,0,0,220,0,0,178,0,0,110,96,16,36,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,160,40,24,96,24,42,
    96,24,42,128,32,27,64,146,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  },
  { // 5 公交道
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,231,8,18,
    231,8,18,82,34,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,137,25,10,0,0,71,0,0,141,0,0,190,0,0,217,0,0,233,
    0,0,233,0,0,220,0,0,195,0,0,149,0,0,80,229,8,16,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    101,0,16,0,0,128,0,0,223,0,0,255,165,8,255,76,17,255,208,17,255,18,26,255,
    18,26,255,208,17,255,76,17,255,165,8,255,0,0,255,0,0,230,0,0,142,67,0,27,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,85,
    0,0,224,34,0,255,142,17,255,185,34,253,94,43,251,159,43,252,127,43,253,127,43,254,
    127,43,254,127,43,253,159,43,252,94,43,251,217,34,252,142,17,255,34,0,255,0,0,235,
    0,0,105,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,136,0,0,255,
    75,9,255,27,35,251,159,43,253,127,43,255,94,43,255,61,35,255,61,35,255,61,35,255,
    61,35,255,61,35,255,61,35,255,94,43,255,127,43,255,159,43,254,27,35,251,75,9,255,
    0,0,255,0,0,159,5,0,6,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,166,0,0,255,18,26,251,
    127,43,253,94,43,255,61,35,255,93,43,255,94,43,255,94,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,94,43,255,93,43,255,61,43,255,94,43,255,127,43,254,
    18,26,251,0,0,255,0,0,189,3,0,8,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,137,0,0,255,52,26,250,159,43,255,
    61,35,255,93,43,255,94,43,255,29,27,255,253,26,255,253,26,255,253,26,255,253,26,255,
    253,26,255,253,26,255,253,26,255,253,26,255,253,26,255,94,43,255,93,43,255,61,35,255,
    159,43,255,52,26,250,0,0,255,0,0,166,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,33,0,81,0,0,255,18,26,250,159,43,254,61,35,255,
    94,43,255,62,35,255,29,35,255,190,100,255,158,141,255,158,149,255,158,149,255,158,149,255,
    158,149,255,158,149,255,158,149,255,158,149,255,222,108,255,62,35,255,62,35,255,94,43,255,
    61,35,255,159,43,255,18,26,250,0,0,255,32,0,111,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,134,16,14,0,0,230,42,9,254,159,43,253,61,35,255,94,43,255,
    94,43,255,29,35,255,63,174,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,191,198,255,94,43,255,62,35,255,
    94,43,255,61,35,255,159,43,254,43,9,252,0,0,247,67,8,31,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,127,34,0,255,27,35,251,94,43,255,61,43,255,94,43,255,
    29,27,255,190,100,255,255,255,255,191,247,255,191,198,255,191,198,255,191,198,255,191,198,255,
    191,198,255,191,198,255,191,198,255,191,198,255,159,239,255,255,255,255,62,133,255,253,26,255,
    94,43,255,61,43,255,94,43,255,27,35,251,34,0,255,0,0,160,0,0,0,0,0,0,
    0,0,0,206,25,9,0,0,228,141,17,254,159,43,253,61,35,255,94,43,255,94,43,255,
    253,26,255,158,149,255,255,255,255,254,116,255,221,18,255,29,35,255,29,27,255,29,27,255,
    29,27,255,29,27,255,29,35,255,221,18,255,126,92,255,255,255,255,63,174,255,253,26,255,
    94,43,255,94,43,255,61,35,255,159,43,254,141,17,254,0,0,246,165,8,24,0,0,0,
    0,0,0,0,0,70,0,0,255,185,34,253,127,43,255,61,43,255,94,43,255,94,43,255,
    253,26,255,158,149,255,255,255,255,94,84,255,29,27,255,94,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,61,35,255,222,67,255,255,255,255,63,174,255,253,26,255,
    94,43,255,94,43,255,61,43,255,127,43,255,217,34,251,0,0,255,0,0,100,0,0,0,
    0,0,0,0,0,143,133,0,254,94,43,251,94,43,255,94,43,255,94,43,255,94,43,255,
    253,26,255,158,149,255,255,255,255,126,92,255,29,27,255,94,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,29,35,255,254,75,255,255,255,255,63,174,255,253,26,255,
    94,43,255,94,43,255,94,43,255,94,43,255,94,43,251,133,0,254,0,0,173,0,0,0,
    0,0,0,0,0,192,75,17,255,159,43,252,61,35,255,94,43,255,94,43,255,94,43,255,
    253,26,255,158,149,255,255,255,255,94,92,255,29,35,255,126,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,61,35,255,254,67,255,255,255,255,63,174,255,253,26,255,
    94,43,255,94,43,255,94,43,255,61,35,255,127,43,253,75,17,255,0,0,217,0,0,0,
    0,0,0,0,0,219,208,17,255,159,43,253,61,35,255,94,43,255,94,43,255,94,43,255,
    253,26,255,158,149,255,255,255,255,190,108,255,189,10,255,29,27,255,253,26,255,253,26,255,
    253,26,255,253,26,255,253,26,255,189,10,255,62,84,255,255,255,255,63,174,255,253,26,255,
    94,43,255,94,43,255,94,43,255,61,35,255,159,43,254,208,17,255,0,0,239,165,8,25,
    231,8,18,0,0,234,18,26,255,127,43,254,61,35,255,94,43,255,94,43,255,94,43,255,
    253,26,255,158,149,255,255,255,255,127,231,255,63,174,255,63,174,255,63,174,255,63,174,255,
    63,174,255,63,174,255,63,174,255,31,174,255,95,223,255,255,255,255,63,174,255,253,26,255,
    94,43,255,94,43,255,94,43,255,61,35,255,127,43,254,18,26,255,0,0,250,66,0,43,
    198,8,19,0,0,235,18,26,255,127,43,254,61,35,255,94,43,255,94,43,255,94,43,255,
    253,26,255,158,149,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,63,174,255,253,26,255,
    94,43,255,94,43,255,94,43,255,61,35,255,127,43,254,18,26,255,0,0,251,66,0,43,
    82,34,7,0,0,221,208,17,255,159,43,253,61,35,255,94,43,255,94,43,255,94,43,255,
    253,26,255,158,149,255,255,255,255,255,255,255,95,231,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,95,231,255,255,255,255,255,255,255,63,174,255,253,26,255,
    94,43,255,94,43,255,94,43,255,61,35,255,159,43,254,208,17,255,0,0,241,132,8,28,
    0,0,0,0,0,196,75,17,255,159,43,252,61,35,255,94,43,255,94,43,255,94,43,255,
    253,26,255,158,149,255,255,255,255,223,157,255,29,27,255,222,116,255,255,255,255,255,255,255,
    255,255,255,255,255,255,222,116,255,29,27,255,223,157,255,255,255,255,63,174,255,253,26,255,
    94,43,255,94,43,255,94,43,255,61,35,255,127,43,253,75,17,255,0,0,220,0,0,0,
    0,0,0,0,0,151,133,0,254,94,43,251,94,43,255,94,43,255,94,43,255,94,43,255,
    253,26,255,158,149,255,255,255,255,126,92,255,125,2,255,158,51,255,223,247,255,255,255,255,
    255,255,255,223,247,255,158,51,255,125,2,255,126,100,255,255,255,255,63,174,255,253,26,255,
    94,43,255,94,43,255,94,43,255,94,43,255,94,43,252,133,0,255,0,0,180,0,0,0,
    0,0,0,0,0,80,0,0,255,217,34,252,127,43,255,61,43,255,94,43,255,94,43,255,
    253,26,255,222,116,255,255,255,255,95,231,255,30,125,255,191,198,255,255,255,255,255,255,255,
    255,255,255,255,255,255,191,198,255,30,125,255,95,231,255,255,255,255,126,141,255,253,26,255,
    94,43,255,94,43,255,61,43,255,127,43,255,217,34,251,0,0,255,0,0,111,0,0,0,
    0,0,0,41,17,14,0,0,236,141,17,254,159,43,253,61,35,255,94,43,255,94,43,255,
    94,43,255,62,35,255,223,206,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,63,223,255,126,51,255,62,35,255,
    94,43,255,94,43,255,61,35,255,159,43,254,141,17,254,0,0,250,99,0,32,0,0,0,
    0,0,0,0,0,0,0,0,143,34,0,255,27,35,251,94,43,255,61,43,255,94,43,255,
    94,43,255,253,18,255,126,141,255,255,255,255,223,255,255,223,206,255,31,174,255,63,174,255,
    63,174,255,31,174,255,159,198,255,223,255,255,255,255,255,31,174,255,253,18,255,94,43,255,
    94,43,255,61,43,255,94,43,255,27,35,251,34,0,255,0,0,176,0,0,0,0,0,0,
    0,0,0,0,0,0,68,8,23,0,0,239,42,9,253,159,43,253,61,35,255,94,43,255,
    94,43,255,253,26,255,222,157,255,255,255,255,255,255,255,254,116,255,157,10,255,29,27,255,
    29,27,255,189,10,255,94,92,255,255,255,255,255,255,255,95,182,255,253,26,255,94,43,255,
    94,43,255,61,35,255,159,43,254,43,9,252,0,0,253,34,0,43,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,32,0,102,0,0,255,18,26,250,159,43,255,61,35,255,
    94,43,255,29,27,255,222,108,255,255,255,255,223,247,255,94,92,255,61,35,255,126,43,255,
    94,43,255,62,35,255,254,67,255,159,239,255,255,255,255,94,133,255,253,26,255,94,43,255,
    61,35,255,159,43,255,18,26,250,0,0,255,0,0,133,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,161,0,0,255,52,26,250,159,43,255,
    61,35,255,61,43,255,62,35,255,254,67,255,222,67,255,62,35,255,94,43,255,94,43,255,
    94,43,255,94,43,255,62,35,255,190,59,255,254,75,255,62,35,255,61,35,255,61,35,255,
    159,43,255,52,26,250,0,0,255,0,0,188,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,5,0,6,0,0,190,0,0,255,18,26,250,
    127,43,254,94,43,255,61,43,255,29,35,255,61,35,255,94,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,61,35,255,29,35,255,61,43,255,94,43,255,127,43,254,
    18,26,250,0,0,255,0,0,212,1,0,18,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,0,9,0,0,164,0,0,255,
    75,9,255,27,35,251,159,43,254,127,43,255,94,43,255,61,35,255,61,35,255,61,35,255,
    61,35,255,61,35,255,61,35,255,94,43,255,127,43,255,159,43,254,27,35,252,75,9,254,
    0,0,255,0,0,185,1,0,20,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,113,
    0,0,242,34,0,255,142,17,255,217,34,252,94,43,252,127,43,253,127,43,254,127,43,254,
    127,43,254,127,43,254,127,43,253,94,43,252,217,34,251,142,17,255,34,0,255,0,0,250,
    0,0,134,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    34,0,35,0,0,159,0,0,242,0,0,255,165,8,255,76,17,255,208,17,255,18,26,255,
    18,26,255,208,17,255,76,17,255,165,8,255,0,0,255,0,0,248,0,0,172,33,0,49,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,131,8,28,0,0,100,0,0,170,0,0,215,0,0,237,0,0,250,
    0,0,250,0,0,239,0,0,220,0,0,178,0,0,110,98,0,36,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,165,8,24,99,0,42,
    99,0,42,132,8,27,82,34,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  },
  { // 6 应急车道
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,64,57,18,
    64,57,18,96,147,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,96,98,10,0,0,71,0,0,141,0,0,190,0,0,217,0,0,233,
    0,0,233,0,0,220,0,0,195,0,0,149,0,0,80,96,57,16,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    224,40,16,0,0,128,0,0,223,0,0,255,32,41,255,96,98,255,32,131,255,160,155,255,
    160,155,255,32,131,255,96,98,255,32,41,255,0,0,255,0,0,230,0,0,142,128,24,27,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,8,85,
    0,0,224,96,16,255,192,114,255,0,213,253,0,254,251,96,254,252,96,254,253,64,254,254,
    64,254,254,96,254,253,96,254,252,0,254,251,0,213,252,192,114,255,96,16,255,0,0,235,
    32,0,105,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,136,0,0,255,
    64,90,255,128,229,251,96,254,253,32,254,255,224,253,255,192,245,255,192,245,255,192,245,255,
    192,245,255,192,245,255,192,245,255,224,253,255,32,254,255,96,254,254,128,229,251,64,90,255,
    0,0,255,0,0,159,64,41,6,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,166,0,0,255,160,155,251,
    96,254,253,0,254,255,192,245,255,224,253,255,224,253,255,224,253,255,224,253,255,32,254,255,
    64,254,255,224,253,255,224,253,255,224,253,255,224,253,255,192,245,255,0,254,255,64,254,254,
    160,155,251,0,0,255,0,0,189,224,24,8,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,137,0,0,255,224,163,250,96,254,255,
    192,245,255,224,253,255,224,253,255,224,253,255,224,253,255,224,253,255,32,254,255,32,180,255,
    192,163,255,32,254,255,224,253,255,224,253,255,224,253,255,224,253,255,224,253,255,192,245,255,
    96,254,255,224,163,250,0,0,255,0,0,166,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,64,8,81,0,0,255,160,155,250,96,254,254,192,245,255,
    224,253,255,224,253,255,224,253,255,224,253,255,224,253,255,0,254,255,96,229,255,0,0,255,
    0,0,255,192,204,255,32,254,255,224,245,255,224,253,255,224,253,255,224,253,255,224,253,255,
    192,245,255,96,254,255,160,155,250,0,0,255,32,0,111,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,32,49,14,0,0,230,32,90,254,96,254,253,192,245,255,224,253,255,
    224,253,255,224,253,255,224,253,255,224,253,255,192,245,255,96,254,255,96,98,255,0,0,255,
    0,0,255,192,73,255,64,254,255,224,253,255,224,253,255,224,253,255,224,253,255,224,253,255,
    224,253,255,192,245,255,96,254,254,32,90,252,0,0,247,128,24,31,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,127,96,16,255,128,229,251,0,254,255,224,253,255,224,253,255,
    224,253,255,224,253,255,224,253,255,224,253,255,32,254,255,224,212,255,32,0,255,64,8,255,
    96,16,255,0,0,255,96,188,255,64,254,255,192,245,255,224,253,255,224,253,255,224,253,255,
    224,253,255,224,253,255,0,254,255,128,229,251,96,16,255,0,0,160,0,0,0,0,0,0,
    0,0,0,160,114,9,0,0,228,160,114,254,96,254,253,192,245,255,224,253,255,224,253,255,
    224,253,255,224,253,255,224,253,255,192,245,255,64,254,255,224,81,255,0,0,255,128,147,255,
    32,172,255,0,0,255,64,57,255,0,254,255,224,253,255,224,253,255,224,253,255,224,253,255,
    224,253,255,224,253,255,192,245,255,96,254,254,160,114,254,0,0,246,224,40,24,0,0,0,
    0,0,0,0,0,70,0,0,255,0,213,253,32,254,255,224,253,255,224,253,255,224,253,255,
    224,253,255,224,253,255,192,245,255,64,254,255,96,188,255,0,0,255,160,24,255,128,147,255,
    224,163,255,0,41,255,0,0,255,0,172,255,64,254,255,192,245,255,224,253,255,224,253,255,
    224,253,255,224,253,255,224,253,255,32,254,255,0,213,251,0,0,255,0,0,100,0,0,0,
    0,0,0,0,0,143,0,41,254,32,254,251,224,253,255,224,253,255,224,253,255,224,253,255,
    224,253,255,224,253,255,224,253,255,32,254,255,64,49,255,0,0,255,160,114,255,32,0,255,
    0,0,255,64,139,255,0,0,255,224,32,255,224,245,255,224,253,255,224,253,255,224,253,255,
    224,253,255,224,253,255,224,253,255,224,253,255,32,254,251,0,41,254,0,0,173,0,0,0,
    0,0,0,0,0,192,64,98,255,96,254,252,192,245,255,224,253,255,224,253,255,224,253,255,
    224,253,255,192,245,255,64,254,255,224,171,255,0,0,255,0,41,255,192,237,255,192,32,255,
    0,0,255,32,221,255,128,57,255,0,0,255,96,147,255,96,254,255,192,245,255,224,253,255,
    224,253,255,224,253,255,224,253,255,192,245,255,96,254,253,64,98,255,0,0,217,0,0,0,
    0,0,0,0,0,219,32,139,255,96,254,253,192,245,255,224,253,255,224,253,255,224,253,255,
    224,253,255,224,253,255,224,245,255,224,32,255,0,0,255,0,172,255,64,254,255,192,32,255,
    0,0,255,96,229,255,160,196,255,0,0,255,160,24,255,160,237,255,0,254,255,224,253,255,
    224,253,255,224,253,255,224,253,255,192,245,255,96,254,254,32,139,255,0,0,239,224,40,25,
    64,57,18,0,0,234,160,155,255,64,254,254,192,245,255,224,253,255,224,253,255,224,253,255,
    192,245,255,96,254,255,96,139,255,0,0,255,96,57,255,32,254,255,224,253,255,224,32,255,
    0,0,255,0,213,255,96,254,255,192,73,255,0,0,255,0,131,255,96,254,255,192,245,255,
    224,253,255,224,253,255,224,253,255,192,245,255,64,254,254,160,155,255,0,0,250,128,16,43,
    64,49,19,0,0,235,160,155,255,64,254,254,192,245,255,224,253,255,224,253,255,224,253,255,
    0,254,255,128,237,255,128,16,255,0,0,255,128,196,255,64,254,255,160,245,255,192,32,255,
    0,0,255,224,212,255,96,254,255,192,204,255,0,0,255,96,16,255,96,229,255,0,254,255,
    224,253,255,224,253,255,224,253,255,192,245,255,64,254,254,160,155,255,0,0,251,128,16,43,
    96,147,7,0,0,221,32,139,255,96,254,253,192,245,255,224,253,255,224,253,255,192,245,255,
    96,254,255,160,114,255,0,0,255,224,81,255,64,254,255,0,254,255,32,221,255,0,0,255,
    0,0,255,32,221,255,0,254,255,64,254,255,32,90,255,0,0,255,128,106,255,64,254,255,
    192,245,255,224,253,255,224,253,255,192,245,255,96,254,254,32,139,255,0,0,241,192,32,28,
    0,0,0,0,0,196,64,98,255,96,254,252,192,245,255,224,253,255,224,253,255,32,254,255,
    32,221,255,64,8,255,32,0,255,0,213,255,32,254,255,192,245,255,64,254,255,160,155,255,
    160,155,255,64,254,255,192,245,255,32,254,255,32,221,255,64,8,255,32,0,255,0,213,255,
    32,254,255,224,253,255,224,253,255,192,245,255,96,254,253,64,98,255,0,0,220,0,0,0,
    0,0,0,0,0,151,0,41,254,32,254,251,224,253,255,224,253,255,192,245,255,64,254,255,
    32,90,255,0,0,255,96,106,255,64,254,255,160,245,255,224,253,255,64,221,255,64,49,255,
    64,49,255,64,221,255,224,253,255,160,245,255,64,254,255,128,106,255,0,0,255,0,82,255,
    64,254,255,192,245,255,224,253,255,224,253,255,0,254,252,0,41,255,0,0,180,0,0,0,
    0,0,0,0,0,80,0,0,255,0,213,252,32,254,255,192,245,255,64,254,255,192,204,255,
    0,0,255,160,24,255,224,245,255,128,254,255,64,254,255,160,254,255,64,221,255,0,0,255,
    0,0,255,64,221,255,160,254,255,64,254,255,96,254,255,0,254,255,192,32,255,0,0,255,
    160,196,255,64,254,255,192,245,255,32,254,255,0,213,251,0,0,255,0,0,111,0,0,0,
    0,0,0,160,73,14,0,0,236,160,114,254,96,254,253,192,245,255,0,254,255,160,65,255,
    0,0,255,64,57,255,64,131,255,0,123,255,0,131,255,0,131,255,64,131,255,128,65,255,
    128,65,255,64,131,255,0,131,255,0,131,255,0,123,255,64,131,255,64,57,255,0,0,255,
    128,65,255,0,254,255,192,245,255,96,254,254,160,114,254,0,0,250,160,24,32,0,0,0,
    0,0,0,0,0,0,0,0,143,96,16,255,96,229,251,64,254,255,128,196,255,0,0,255,
    0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,
    0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,
    0,0,255,128,196,255,64,254,255,96,229,251,96,16,255,0,0,176,0,0,0,0,0,0,
    0,0,0,0,0,0,160,32,23,0,0,239,32,90,253,96,254,253,160,237,255,224,122,255,
    192,114,255,0,131,255,0,131,255,0,131,255,0,131,255,0,131,255,0,131,255,0,123,255,
    0,123,255,0,131,255,0,131,255,0,131,255,0,131,255,0,131,255,0,131,255,192,114,255,
    224,122,255,160,237,255,96,254,254,32,90,252,0,0,253,64,16,43,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,32,0,102,0,0,255,160,155,250,128,254,255,32,254,255,
    96,254,255,96,254,255,96,254,255,96,254,255,96,254,255,96,254,255,96,254,255,96,254,255,
    96,254,255,96,254,255,96,254,255,96,254,255,96,254,255,96,254,255,96,254,255,96,254,255,
    32,254,255,128,254,255,160,155,250,0,0,255,32,0,133,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,161,0,0,255,224,163,250,96,254,255,
    192,245,255,192,245,255,192,245,255,192,245,255,192,245,255,192,245,255,192,245,255,192,245,255,
    192,245,255,192,245,255,192,245,255,192,245,255,192,245,255,192,245,255,192,245,255,192,245,255,
    96,254,255,224,163,250,0,0,255,0,0,188,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,64,41,6,0,0,190,0,0,255,160,155,250,
    64,254,254,0,254,255,192,245,255,224,253,255,224,253,255,224,253,255,224,253,255,224,253,255,
    224,253,255,224,253,255,224,253,255,224,253,255,224,253,255,192,245,255,0,254,255,64,254,254,
    160,155,250,0,0,255,0,0,212,96,8,18,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,224,24,9,0,0,164,0,0,255,
    64,90,255,128,229,251,96,254,254,32,254,255,224,253,255,192,245,255,192,245,255,192,245,255,
    192,245,255,192,245,255,192,245,255,224,253,255,32,254,255,96,254,254,128,229,252,64,90,254,
    0,0,255,0,0,185,96,8,20,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,0,113,
    0,0,242,96,16,255,192,114,255,0,213,252,0,254,252,96,254,253,96,254,254,64,254,254,
    64,254,254,96,254,254,96,254,253,0,254,252,0,213,251,192,114,255,96,16,255,0,0,250,
    0,0,134,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    96,16,35,0,0,159,0,0,242,0,0,255,32,41,255,96,98,255,32,131,255,160,155,255,
    160,155,255,32,131,255,96,98,255,32,41,255,0,0,255,0,0,248,0,0,172,64,8,49,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,192,32,28,0,0,100,0,0,170,0,0,215,0,0,237,0,0,250,
    0,0,250,0,0,239,0,0,220,0,0,178,0,0,110,160,24,36,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,224,40,24,128,24,42,
    128,24,42,224,32,27,96,147,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  },
  { // 7 非机动车道
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,231,8,18,
    231,8,18,82,34,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,137,25,10,0,0,71,0,0,141,0,0,190,0,0,217,0,0,233,
    0,0,233,0,0,220,0,0,195,0,0,149,0,0,80,229,8,16,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    101,0,16,0,0,128,0,0,223,0,0,255,165,8,255,76,17,255,208,17,255,18,26,255,
    18,26,255,208,17,255,76,17,255,165,8,255,0,0,255,0,0,230,0,0,142,67,0,27,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,85,
    0,0,224,34,0,255,142,17,255,185,34,253,94,43,251,159,43,252,127,43,253,127,43,254,
    127,43,254,127,43,253,159,43,252,94,43,251,217,34,252,142,17,255,34,0,255,0,0,235,
    0,0,105,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,136,0,0,255,
    75,9,255,27,35,251,159,43,253,127,43,255,94,43,255,61,35,255,61,35,255,61,35,255,
    61,35,255,61,35,255,61,35,255,94,43,255,127,43,255,159,43,254,27,35,251,75,9,255,
    0,0,255,0,0,159,5,0,6,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,166,0,0,255,18,26,251,
    127,43,253,94,43,255,61,35,255,61,43,255,94,43,255,94,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,94,43,255,61,43,255,61,35,255,94,43,255,127,43,254,
    18,26,251,0,0,255,0,0,189,3,0,8,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,137,0,0,255,52,26,250,159,43,255,
    61,35,255,61,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,61,43,255,61,35,255,
    159,43,255,52,26,250,0,0,255,0,0,166,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,33,0,81,0,0,255,18,26,250,159,43,254,61,35,255,
    94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,
    61,35,255,159,43,255,18,26,250,0,0,255,32,0,111,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,134,16,14,0,0,230,42,9,254,159,43,253,61,35,255,94,43,255,
    94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,62,35,255,253,26,255,29,27,255,253,26,255,62,35,255,94,43,255,
    94,43,255,61,35,255,159,43,254,43,9,252,0,0,247,67,8,31,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,127,34,0,255,27,35,251,94,43,255,61,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,62,35,255,29,35,255,29,27,255,253,18,255,29,35,255,
    61,35,255,62,35,255,126,51,255,222,108,255,126,92,255,190,108,255,158,51,255,62,35,255,
    94,43,255,61,43,255,94,43,255,27,35,251,34,0,255,0,0,160,0,0,0,0,0,0,
    0,0,0,206,25,9,0,0,228,141,17,254,159,43,253,61,35,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,29,35,255,63,174,255,255,206,255,95,182,255,63,223,255,
    222,116,255,221,18,255,254,116,255,255,255,255,255,255,255,255,255,255,190,108,255,253,26,255,
    94,43,255,94,43,255,61,35,255,159,43,254,141,17,254,0,0,246,165,8,24,0,0,0,
    0,0,0,0,0,70,0,0,255,185,34,253,127,43,255,61,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,29,35,255,63,174,255,95,231,255,63,223,255,31,215,255,
    94,92,255,125,2,255,190,59,255,255,255,255,95,182,255,30,76,255,126,43,255,62,35,255,
    94,43,255,94,43,255,61,43,255,127,43,255,217,34,251,0,0,255,0,0,100,0,0,0,
    0,0,0,0,0,143,133,0,254,94,43,251,94,43,255,94,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,94,43,255,221,18,255,94,92,255,223,157,255,158,149,255,
    158,149,255,191,157,255,191,149,255,223,255,255,223,206,255,221,18,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,94,43,255,94,43,251,133,0,254,0,0,173,0,0,0,
    0,0,0,0,0,192,75,17,255,159,43,252,61,35,255,94,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,127,231,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,254,75,255,62,35,255,94,43,255,
    94,43,255,94,43,255,94,43,255,61,35,255,127,43,253,75,17,255,0,0,217,0,0,0,
    0,0,0,0,0,219,208,17,255,159,43,253,61,35,255,94,43,255,94,43,255,94,43,255,
    62,35,255,253,26,255,253,26,255,189,10,255,158,149,255,255,255,255,255,255,255,126,141,255,
    94,43,255,62,35,255,31,166,255,255,255,255,255,255,255,158,100,255,189,10,255,94,43,255,
    94,43,255,94,43,255,94,43,255,61,35,255,159,43,254,208,17,255,0,0,239,165,8,25,
    231,8,18,0,0,234,18,26,255,127,43,254,61,35,255,94,43,255,94,43,255,29,27,255,
    158,51,255,30,125,255,191,149,255,62,125,255,223,255,255,31,166,255,63,223,255,31,215,255,
    189,18,255,62,84,255,255,255,255,95,182,255,191,247,255,223,206,255,222,116,255,158,51,255,
    29,27,255,94,43,255,94,43,255,61,35,255,127,43,254,18,26,255,0,0,250,66,0,43,
    198,8,19,0,0,235,18,26,255,127,43,254,61,35,255,94,43,255,29,27,255,94,92,255,
    191,239,255,255,255,255,255,255,255,255,255,255,255,255,255,254,75,255,159,149,255,255,255,255,
    190,59,255,255,206,255,255,255,255,127,231,255,255,255,255,255,255,255,255,255,255,191,247,255,
    94,92,255,29,27,255,94,43,255,61,35,255,127,43,254,18,26,255,0,0,251,66,0,43,
    82,34,7,0,0,221,208,17,255,159,43,253,61,43,255,61,35,255,190,59,255,191,247,255,
    63,223,255,222,67,255,190,108,255,223,255,255,159,239,255,31,215,255,126,92,255,223,255,255,
    255,214,255,255,255,255,255,255,255,95,223,255,223,206,255,159,239,255,94,84,255,31,223,255,
    191,247,255,190,59,255,61,35,255,61,43,255,159,43,254,208,17,255,0,0,241,132,8,28,
    0,0,0,0,0,196,75,17,255,159,43,252,93,43,255,253,26,255,159,149,255,255,255,255,
    254,67,255,221,18,255,63,223,255,127,231,255,191,149,255,255,255,255,191,149,255,95,231,255,
    255,255,255,191,247,255,255,255,255,62,84,255,190,108,255,255,255,255,190,59,255,190,59,255,
    255,255,255,159,149,255,253,26,255,93,43,255,127,43,253,75,17,255,0,0,220,0,0,0,
    0,0,0,0,0,151,133,0,254,94,43,251,94,43,255,253,26,255,127,190,255,191,239,255,
    221,18,255,222,116,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    159,239,255,95,182,255,255,255,255,62,35,255,94,92,255,255,255,255,254,116,255,221,18,255,
    191,239,255,127,190,255,253,26,255,94,43,255,94,43,252,133,0,255,0,0,180,0,0,0,
    0,0,0,0,0,80,0,0,255,217,34,252,127,43,255,221,18,255,191,157,255,255,255,255,
    158,59,255,62,35,255,222,116,255,30,76,255,94,133,255,255,255,255,159,149,255,126,92,255,
    30,76,255,254,116,255,255,255,255,62,84,255,253,26,255,222,108,255,62,35,255,158,59,255,
    255,255,255,191,157,255,221,18,255,127,43,255,217,34,251,0,0,255,0,0,111,0,0,0,
    0,0,0,41,17,14,0,0,236,141,17,254,159,43,253,29,35,255,254,67,255,255,255,255,
    191,198,255,158,51,255,221,18,255,190,59,255,255,206,255,191,247,255,126,43,255,253,26,255,
    29,27,255,158,51,255,191,247,255,63,223,255,222,67,255,253,18,255,158,51,255,191,198,255,
    255,255,255,254,67,255,29,35,255,159,43,254,141,17,254,0,0,250,99,0,32,0,0,0,
    0,0,0,0,0,0,0,0,143,34,0,255,27,35,251,126,43,255,253,26,255,222,108,255,
    255,255,255,255,255,255,159,239,255,255,255,255,223,247,255,94,92,255,29,27,255,126,43,255,
    94,43,255,29,27,255,94,92,255,223,247,255,255,255,255,159,239,255,255,255,255,255,255,255,
    222,108,255,253,26,255,126,43,255,27,35,251,34,0,255,0,0,176,0,0,0,0,0,0,
    0,0,0,0,0,0,68,8,23,0,0,239,42,9,253,127,43,253,93,43,255,29,27,255,
    254,67,255,191,149,255,95,182,255,126,141,255,190,59,255,29,27,255,94,43,255,94,43,255,
    94,43,255,94,43,255,29,27,255,190,59,255,126,141,255,95,182,255,191,149,255,254,67,255,
    29,27,255,93,43,255,127,43,254,43,9,252,0,0,253,34,0,43,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,32,0,102,0,0,255,18,26,250,159,43,255,61,43,255,
    29,35,255,253,26,255,253,26,255,253,26,255,62,35,255,94,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,62,35,255,253,26,255,253,26,255,253,26,255,29,35,255,
    61,43,255,159,43,255,18,26,250,0,0,255,0,0,133,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,161,0,0,255,52,26,250,159,43,255,
    61,43,255,93,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,93,43,255,61,43,255,
    159,43,255,52,26,250,0,0,255,0,0,188,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,5,0,6,0,0,190,0,0,255,18,26,250,
    127,43,254,94,43,255,61,35,255,61,43,255,94,43,255,94,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,94,43,255,61,43,255,61,35,255,94,43,255,127,43,254,
    18,26,250,0,0,255,0,0,212,1,0,18,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,0,9,0,0,164,0,0,255,
    75,9,255,27,35,251,159,43,254,127,43,255,94,43,255,61,35,255,61,35,255,61,35,255,
    61,35,255,61,35,255,61,35,255,94,43,255,127,43,255,159,43,254,27,35,252,75,9,254,
    0,0,255,0,0,185,1,0,20,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,113,
    0,0,242,34,0,255,142,17,255,217,34,252,94,43,252,127,43,253,127,43,254,127,43,254,
    127,43,254,127,43,254,127,43,253,94,43,252,217,34,251,142,17,255,34,0,255,0,0,250,
    0,0,134,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    34,0,35,0,0,159,0,0,242,0,0,255,165,8,255,76,17,255,208,17,255,18,26,255,
    18,26,255,208,17,255,76,17,255,165,8,255,0,0,255,0,0,248,0,0,172,33,0,49,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,131,8,28,0,0,100,0,0,170,0,0,215,0,0,237,0,0,250,
    0,0,250,0,0,239,0,0,220,0,0,178,0,0,110,98,0,36,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,165,8,24,99,0,42,
    99,0,42,132,8,27,82,34,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  },
  { // 8 区间起点
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,103,24,18,
    103,24,18,50,73,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,201,48,10,0,0,71,0,0,141,0,0,190,0,0,217,0,0,233,
    0,0,233,0,0,220,0,0,195,0,0,149,0,0,80,101,24,16,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    101,24,16,0,0,128,0,0,223,0,0,255,101,16,255,203,48,255,15,65,255,82,73,255,
    82,73,255,15,65,255,203,48,255,101,16,255,0,0,255,0,0,230,0,0,142,67,16,27,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,85,
    0,0,224,34,8,255,237,56,255,184,105,253,29,130,251,62,130,252,62,130,253,62,130,254,
    62,130,254,62,130,253,62,130,252,29,130,251,184,105,252,237,56,255,34,8,255,0,0,235,
    0,0,105,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,136,0,0,255,
    202,40,255,218,113,251,62,130,253,29,130,255,28,122,255,252,121,255,252,121,255,252,121,255,
    252,121,255,252,121,255,252,121,255,28,122,255,29,130,255,62,130,254,218,113,251,202,40,255,
    0,0,255,0,0,159,5,0,6,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,166,0,0,255,49,73,251,
    62,130,253,29,122,255,252,121,255,252,121,255,28,122,255,28,122,255,28,122,255,28,122,255,
    28,122,255,28,122,255,28,122,255,28,122,255,252,121,255,252,121,255,29,122,255,62,130,254,
    49,73,251,0,0,255,0,0,189,3,0,8,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,137,0,0,255,83,81,250,63,130,255,
    252,121,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,
    28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,252,121,255,
    63,130,255,83,81,250,0,0,255,0,0,166,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,1,0,81,0,0,255,49,73,250,63,130,254,251,121,255,
    252,121,255,220,121,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,
    28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,
    251,121,255,63,130,255,49,73,250,0,0,255,0,0,111,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,134,32,14,0,0,230,170,40,254,62,130,253,28,122,255,252,121,255,
    60,130,255,188,138,255,220,121,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,
    28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,
    28,122,255,252,121,255,62,130,254,170,40,252,0,0,247,67,16,31,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,127,33,8,255,218,113,251,29,122,255,252,121,255,28,122,255,
    191,230,255,191,255,255,93,155,255,188,113,255,28,122,255,28,122,255,28,122,255,28,122,255,
    28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,
    28,122,255,252,121,255,29,122,255,218,113,251,33,8,255,0,0,160,0,0,0,0,0,0,
    0,0,0,234,56,9,0,0,228,237,56,254,62,130,253,252,121,255,252,121,255,60,130,255,
    95,247,255,255,255,255,253,163,255,188,113,255,28,122,255,28,122,255,28,122,255,28,122,255,
    28,122,255,28,122,255,252,121,255,252,121,255,28,122,255,28,122,255,28,122,255,28,122,255,
    28,122,255,28,122,255,252,121,255,62,130,254,237,56,254,0,0,246,67,16,24,0,0,0,
    0,0,0,0,0,70,0,0,255,183,105,253,29,130,255,28,122,255,28,122,255,252,121,255,
    255,238,255,255,255,255,125,155,255,188,113,255,28,122,255,28,122,255,28,122,255,28,122,255,
    28,122,255,220,121,255,253,146,255,220,138,255,188,113,255,60,122,255,28,122,255,28,122,255,
    28,122,255,28,122,255,252,121,255,29,130,255,184,105,251,0,0,255,0,0,100,0,0,0,
    0,0,0,0,0,143,101,16,254,29,130,251,28,122,255,28,122,255,28,122,255,28,122,255,
    31,239,255,255,255,255,157,155,255,188,113,255,28,122,255,28,122,255,28,122,255,28,122,255,
    28,122,255,188,113,255,221,163,255,95,247,255,221,138,255,188,113,255,60,122,255,28,122,255,
    28,122,255,28,122,255,28,122,255,28,122,255,29,130,251,101,16,254,0,0,173,0,0,0,
    0,0,0,0,0,192,203,48,255,62,130,252,252,121,255,28,122,255,28,122,255,28,122,255,
    31,239,255,255,255,255,157,155,255,188,113,255,252,121,255,252,121,255,252,121,255,252,121,255,
    252,121,255,156,113,255,125,155,255,255,255,255,127,247,255,29,147,255,188,113,255,60,122,255,
    28,122,255,28,122,255,28,122,255,252,121,255,62,130,253,203,48,255,0,0,217,0,0,0,
    0,0,0,0,0,219,15,65,255,62,130,253,252,121,255,28,122,255,28,122,255,28,122,255,
    31,239,255,255,255,255,125,155,255,220,121,255,188,138,255,92,130,255,124,130,255,124,130,255,
    124,130,255,28,122,255,221,163,255,255,255,255,255,255,255,223,255,255,125,155,255,124,113,255,
    28,122,255,28,122,255,28,122,255,252,121,255,62,130,254,15,65,255,0,0,239,69,16,25,
    103,24,18,0,0,234,81,73,255,62,130,254,252,121,255,28,122,255,28,122,255,28,122,255,
    31,239,255,255,255,255,157,155,255,127,222,255,223,255,255,159,247,255,159,247,255,159,247,255,
    159,247,255,159,247,255,191,255,255,255,255,255,255,255,255,255,255,255,223,255,255,61,172,255,
    220,113,255,28,122,255,28,122,255,252,121,255,62,130,254,81,73,255,0,0,250,34,8,43,
    102,24,19,0,0,235,81,73,255,62,130,254,252,121,255,28,122,255,28,122,255,28,122,255,
    31,239,255,255,255,255,157,155,255,127,222,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,190,180,255,
    220,113,255,28,122,255,28,122,255,252,121,255,62,130,254,81,73,255,0,0,251,34,8,43,
    50,73,7,0,0,221,15,65,255,62,130,253,252,121,255,28,122,255,28,122,255,28,122,255,
    31,239,255,255,255,255,125,155,255,220,121,255,253,146,255,253,138,255,253,146,255,253,146,255,
    253,146,255,188,138,255,61,172,255,255,255,255,255,255,255,223,255,255,253,163,255,124,113,255,
    28,122,255,28,122,255,28,122,255,252,121,255,62,130,254,15,65,255,0,0,241,68,16,28,
    0,0,0,0,0,196,203,48,255,62,130,252,252,121,255,28,122,255,28,122,255,28,122,255,
    31,239,255,255,255,255,157,155,255,188,113,255,220,121,255,220,113,255,220,113,255,220,113,255,
    220,121,255,124,113,255,93,155,255,255,255,255,159,247,255,125,155,255,156,113,255,60,122,255,
    28,122,255,28,122,255,28,122,255,252,121,255,62,130,253,203,48,255,0,0,220,0,0,0,
    0,0,0,0,0,151,101,16,254,29,130,251,28,122,255,28,122,255,28,122,255,28,122,255,
    31,239,255,255,255,255,157,155,255,188,113,255,60,122,255,28,122,255,28,122,255,28,122,255,
    60,122,255,188,113,255,221,163,255,127,247,255,253,138,255,188,113,255,60,122,255,28,122,255,
    28,122,255,28,122,255,28,122,255,28,122,255,29,122,252,101,16,255,0,0,180,0,0,0,
    0,0,0,0,0,80,0,0,255,184,105,252,29,130,255,28,122,255,28,122,255,252,121,255,
    255,238,255,255,255,255,125,155,255,188,113,255,28,122,255,28,122,255,28,122,255,28,122,255,
    28,122,255,220,121,255,253,146,255,221,138,255,188,113,255,28,122,255,28,122,255,28,122,255,
    28,122,255,28,122,255,252,121,255,29,130,255,184,105,251,0,0,255,0,0,111,0,0,0,
    0,0,0,134,32,14,0,0,236,237,56,254,62,130,253,252,121,255,252,121,255,60,130,255,
    95,247,255,255,255,255,253,163,255,188,113,255,28,122,255,28,122,255,28,122,255,28,122,255,
    28,122,255,28,122,255,252,121,255,220,121,255,28,122,255,28,122,255,28,122,255,28,122,255,
    28,122,255,28,122,255,252,121,255,62,130,254,237,56,254,0,0,250,34,8,32,0,0,0,
    0,0,0,0,0,0,0,0,143,33,8,255,218,113,251,29,122,255,252,121,255,28,122,255,
    191,230,255,191,255,255,93,155,255,188,113,255,28,122,255,28,122,255,28,122,255,28,122,255,
    28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,
    28,122,255,252,121,255,29,122,255,218,113,251,33,8,255,0,0,176,0,0,0,0,0,0,
    0,0,0,0,0,0,68,16,23,0,0,239,170,40,253,62,130,253,28,122,255,252,121,255,
    60,130,255,188,138,255,220,121,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,
    28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,
    28,122,255,252,121,255,62,130,254,170,40,252,0,0,253,34,8,43,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,102,0,0,255,49,73,250,63,130,255,251,121,255,
    252,121,255,220,121,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,
    28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,
    251,121,255,63,130,255,49,73,250,0,0,255,0,0,133,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,161,0,0,255,83,81,250,63,130,255,
    252,121,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,
    28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,252,121,255,
    63,130,255,83,81,250,0,0,255,0,0,188,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,5,0,6,0,0,190,0,0,255,49,73,250,
    62,130,254,29,122,255,252,121,255,252,121,255,28,122,255,28,122,255,28,122,255,28,122,255,
    28,122,255,28,122,255,28,122,255,28,122,255,252,121,255,252,121,255,29,122,255,62,130,254,
    49,73,250,0,0,255,0,0,212,1,0,18,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,0,9,0,0,164,0,0,255,
    202,40,255,218,113,251,62,130,254,29,130,255,28,122,255,252,121,255,252,121,255,252,121,255,
    252,121,255,252,121,255,252,121,255,28,122,255,29,130,255,62,130,254,218,113,252,202,40,254,
    0,0,255,0,0,185,1,0,20,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,113,
    0,0,242,34,8,255,237,56,255,184,105,252,29,122,252,62,130,253,62,130,254,62,130,254,
    62,130,254,62,130,254,62,130,253,29,122,252,184,105,251,237,56,255,34,8,255,0,0,250,
    0,0,134,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    34,8,35,0,0,159,0,0,242,0,0,255,101,16,255,203,48,255,15,65,255,82,73,255,
    82,73,255,15,65,255,203,48,255,101,16,255,0,0,255,0,0,248,0,0,172,33,8,49,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,67,16,28,0,0,100,0,0,170,0,0,215,0,0,237,0,0,250,
    0,0,250,0,0,239,0,0,220,0,0,178,0,0,110,34,8,36,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,69,16,24,35,8,42,
    35,8,42,68,16,27,50,73,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  },
  { // 9 区间终点
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,103,24,18,
    103,24,18,50,73,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,201,48,10,0,0,71,0,0,141,0,0,190,0,0,217,0,0,233,
    0,0,233,0,0,220,0,0,195,0,0,149,0,0,80,101,24,16,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    101,24,16,0,0,128,0,0,223,0,0,255,101,16,255,203,48,255,15,65,255,82,73,255,
    82,73,255,15,65,255,203,48,255,101,16,255,0,0,255,0,0,230,0,0,142,67,16,27,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,85,
    0,0,224,34,8,255,237,56,255,184,105,253,29,130,251,62,130,252,62,130,253,62,130,254,
    62,130,254,62,130,253,62,130,252,29,130,251,184,105,252,237,56,255,34,8,255,0,0,235,
    0,0,105,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,136,0,0,255,
    202,40,255,218,113,251,62,130,253,29,130,255,28,122,255,252,121,255,252,121,255,252,121,255,
    252,121,255,252,121,255,252,121,255,28,122,255,29,130,255,62,130,254,218,113,251,202,40,255,
    0,0,255,0,0,159,5,0,6,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,166,0,0,255,49,73,251,
    62,130,253,29,122,255,252,121,255,252,121,255,28,122,255,28,122,255,28,122,255,28,122,255,
    28,122,255,28,122,255,28,122,255,28,122,255,252,121,255,252,121,255,29,122,255,62,130,254,
    49,73,251,0,0,255,0,0,189,3,0,8,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,137,0,0,255,83,81,250,63,130,255,
    252,121,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,
    28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,252,121,255,
    63,130,255,83,81,250,0,0,255,0,0,166,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,1,0,81,0,0,255,49,73,250,63,130,254,251,121,255,
    28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,
    28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,220,121,255,252,121,255,
    251,121,255,63,130,255,49,73,250,0,0,255,0,0,111,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,134,32,14,0,0,230,170,40,254,62,130,253,252,121,255,28,122,255,
    28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,
    28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,220,121,255,188,138,255,60,130,255,
    252,121,255,28,122,255,62,130,254,170,40,252,0,0,247,67,16,31,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,127,33,8,255,218,113,251,29,122,255,252,121,255,28,122,255,
    28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,
    28,122,255,28,122,255,28,122,255,28,122,255,188,113,255,93,155,255,191,255,255,191,230,255,
    28,122,255,252,121,255,29,122,255,218,113,251,33,8,255,0,0,160,0,0,0,0,0,0,
    0,0,0,234,56,9,0,0,228,237,56,254,62,130,253,252,121,255,28,122,255,28,122,255,
    28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,252,121,255,252,121,255,
    28,122,255,28,122,255,28,122,255,28,122,255,188,113,255,253,163,255,255,255,255,95,247,255,
    60,130,255,252,121,255,252,121,255,62,130,254,237,56,254,0,0,246,67,16,24,0,0,0,
    0,0,0,0,0,70,0,0,255,183,105,253,29,130,255,252,121,255,28,122,255,28,122,255,
    28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,220,121,255,253,146,255,220,138,255,
    188,113,255,60,122,255,28,122,255,28,122,255,188,113,255,125,155,255,255,255,255,255,238,255,
    252,121,255,28,122,255,28,122,255,29,130,255,184,105,251,0,0,255,0,0,100,0,0,0,
    0,0,0,0,0,143,101,16,254,29,130,251,28,122,255,28,122,255,28,122,255,28,122,255,
    28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,188,113,255,221,163,255,95,247,255,
    221,138,255,188,113,255,60,122,255,28,122,255,188,113,255,157,155,255,255,255,255,31,239,255,
    28,122,255,28,122,255,28,122,255,28,122,255,29,130,251,101,16,254,0,0,173,0,0,0,
    0,0,0,0,0,192,203,48,255,62,130,252,252,121,255,28,122,255,28,122,255,252,121,255,
    220,121,255,252,121,255,252,121,255,252,121,255,252,121,255,156,113,255,125,155,255,255,255,255,
    127,247,255,29,147,255,188,113,255,60,130,255,188,113,255,157,155,255,255,255,255,31,239,255,
    28,122,255,28,122,255,28,122,255,252,121,255,62,130,253,203,48,255,0,0,217,0,0,0,
    0,0,0,0,0,219,15,65,255,62,130,253,252,121,255,28,122,255,252,121,255,60,130,255,
    188,138,255,92,130,255,124,130,255,124,130,255,124,130,255,28,122,255,221,163,255,255,255,255,
    255,255,255,223,255,255,125,155,255,156,113,255,220,113,255,157,155,255,255,255,255,31,239,255,
    28,122,255,28,122,255,28,122,255,252,121,255,62,130,254,15,65,255,0,0,239,69,16,25,
    103,24,18,0,0,234,81,73,255,62,130,254,252,121,255,252,121,255,28,122,255,223,230,255,
    223,255,255,159,247,255,159,247,255,159,247,255,159,247,255,159,247,255,191,255,255,255,255,255,
    255,255,255,255,255,255,223,255,255,93,172,255,124,113,255,157,155,255,255,255,255,31,239,255,
    28,122,255,28,122,255,28,122,255,252,121,255,62,130,254,81,73,255,0,0,250,34,8,43,
    102,24,19,0,0,235,81,73,255,62,130,254,252,121,255,252,121,255,28,122,255,223,230,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,190,180,255,124,105,255,157,155,255,255,255,255,31,239,255,
    28,122,255,28,122,255,28,122,255,252,121,255,62,130,254,81,73,255,0,0,251,34,8,43,
    50,73,7,0,0,221,15,65,255,62,130,253,252,121,255,28,122,255,252,121,255,60,122,255,
    253,146,255,253,138,255,253,146,255,253,146,255,253,146,255,188,138,255,61,172,255,255,255,255,
    255,255,255,223,255,255,253,163,255,156,113,255,220,113,255,157,155,255,255,255,255,31,239,255,
    28,122,255,28,122,255,28,122,255,252,121,255,62,130,254,15,65,255,0,0,241,68,16,28,
    0,0,0,0,0,196,203,48,255,62,130,252,252,121,255,28,122,255,28,122,255,252,121,255,
    220,113,255,220,113,255,220,113,255,220,113,255,220,121,255,124,113,255,93,155,255,255,255,255,
    159,247,255,125,155,255,156,113,255,60,130,255,188,113,255,157,155,255,255,255,255,31,239,255,
    28,122,255,28,122,255,28,122,255,252,121,255,62,130,253,203,48,255,0,0,220,0,0,0,
    0,0,0,0,0,151,101,16,254,29,130,251,28,122,255,28,122,255,28,122,255,28,122,255,
    28,122,255,28,122,255,28,122,255,28,122,255,60,122,255,188,113,255,221,163,255,127,247,255,
    253,138,255,188,113,255,60,122,255,28,122,255,188,113,255,157,155,255,255,255,255,31,239,255,
    28,122,255,28,122,255,28,122,255,28,122,255,29,122,252,101,16,255,0,0,180,0,0,0,
    0,0,0,0,0,80,0,0,255,184,105,252,29,130,255,252,121,255,28,122,255,28,122,255,
    28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,220,121,255,253,146,255,221,138,255,
    188,113,255,28,122,255,28,122,255,28,122,255,188,113,255,125,155,255,255,255,255,255,238,255,
    252,121,255,28,122,255,28,122,255,29,130,255,184,105,251,0,0,255,0,0,111,0,0,0,
    0,0,0,134,32,14,0,0,236,237,56,254,62,130,253,252,121,255,28,122,255,28,122,255,
    28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,252,121,255,220,121,255,
    28,122,255,28,122,255,28,122,255,28,122,255,188,113,255,253,163,255,255,255,255,95,247,255,
    60,130,255,252,121,255,252,121,255,62,130,254,237,56,254,0,0,250,34,8,32,0,0,0,
    0,0,0,0,0,0,0,0,143,33,8,255,218,113,251,29,122,255,252,121,255,28,122,255,
    28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,
    28,122,255,28,122,255,28,122,255,28,122,255,188,113,255,93,155,255,191,255,255,191,230,255,
    28,122,255,252,121,255,29,122,255,218,113,251,33,8,255,0,0,176,0,0,0,0,0,0,
    0,0,0,0,0,0,68,16,23,0,0,239,170,40,253,62,130,253,252,121,255,28,122,255,
    28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,
    28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,220,121,255,188,138,255,60,130,255,
    252,121,255,28,122,255,62,130,254,170,40,252,0,0,253,34,8,43,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,102,0,0,255,49,73,250,63,130,255,251,121,255,
    28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,
    28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,220,121,255,252,121,255,
    251,121,255,63,130,255,49,73,250,0,0,255,0,0,133,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,161,0,0,255,83,81,250,63,130,255,
    252,121,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,
    28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,28,122,255,252,121,255,
    63,130,255,83,81,250,0,0,255,0,0,188,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,5,0,6,0,0,190,0,0,255,49,73,250,
    62,130,254,29,122,255,252,121,255,252,121,255,28,122,255,28,122,255,28,122,255,28,122,255,
    28,122,255,28,122,255,28,122,255,28,122,255,252,121,255,252,121,255,29,122,255,62,130,254,
    49,73,250,0,0,255,0,0,212,1,0,18,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,0,9,0,0,164,0,0,255,
    202,40,255,218,113,251,62,130,254,29,130,255,28,122,255,252,121,255,252,121,255,252,121,255,
    252,121,255,252,121,255,252,121,255,28,122,255,29,130,255,62,130,254,218,113,252,202,40,254,
    0,0,255,0,0,185,1,0,20,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,113,
    0,0,242,34,8,255,237,56,255,184,105,252,29,122,252,62,130,253,62,130,254,62,130,254,
    62,130,254,62,130,254,62,130,253,29,122,252,184,105,251,237,56,255,34,8,255,0,0,250,
    0,0,134,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    34,8,35,0,0,159,0,0,242,0,0,255,101,16,255,203,48,255,15,65,255,82,73,255,
    82,73,255,15,65,255,203,48,255,101,16,255,0,0,255,0,0,248,0,0,172,33,8,49,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,67,16,28,0,0,100,0,0,170,0,0,215,0,0,237,0,0,250,
    0,0,250,0,0,239,0,0,220,0,0,178,0,0,110,34,8,36,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,69,16,24,35,8,42,
    35,8,42,68,16,27,50,73,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  },
  { // 10 流动测速
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,224,56,18,
    224,56,18,64,146,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,128,73,10,0,0,71,0,0,141,0,0,190,0,0,217,0,0,233,
    0,0,233,0,0,220,0,0,195,0,0,149,0,0,80,224,40,16,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    96,40,16,0,0,128,0,0,223,0,0,255,160,40,255,129,97,255,1,130,255,97,146,255,
    97,146,255,1,130,255,129,97,255,160,40,255,0,0,255,0,0,230,0,0,142,64,24,27,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,8,85,
    0,0,224,64,16,255,193,113,255,34,203,253,194,243,251,2,252,252,2,252,253,2,252,254,
    2,252,254,2,252,253,2,252,252,194,243,251,34,203,252,193,113,255,64,16,255,0,0,235,
    0,0,105,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,136,0,0,255,
    97,89,255,98,219,251,2,252,253,226,251,255,194,243,255,162,235,255,162,235,255,162,235,255,
    162,235,255,162,235,255,162,235,255,194,243,255,226,251,255,2,252,254,98,219,251,97,89,255,
    0,0,255,0,0,159,0,40,6,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,166,0,0,255,65,146,251,
    2,252,253,194,243,255,162,235,255,162,235,255,194,243,255,194,243,255,194,243,255,194,243,255,
    194,243,255,194,243,255,194,243,255,194,243,255,162,235,255,162,235,255,194,243,255,2,252,254,
    65,146,251,0,0,255,0,0,189,0,24,8,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,137,0,0,255,129,162,250,2,252,255,
    162,235,255,162,235,255,194,243,255,194,243,255,194,243,255,194,243,255,194,243,255,194,243,255,
    194,243,255,194,243,255,194,243,255,194,243,255,194,243,255,194,243,255,162,235,255,162,235,255,
    2,252,255,129,162,250,0,0,255,0,0,166,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,32,8,81,0,0,255,65,146,250,2,252,254,162,235,255,
    194,243,255,194,243,255,194,243,255,194,243,255,194,243,255,194,243,255,194,243,255,194,243,255,
    194,243,255,194,243,255,194,243,255,194,243,255,194,243,255,194,243,255,194,243,255,194,243,255,
    162,235,255,2,252,255,65,146,250,0,0,255,32,0,111,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,32,49,14,0,0,230,64,81,254,2,252,253,162,235,255,194,243,255,
    194,243,255,194,243,255,194,243,255,194,243,255,162,243,255,128,235,255,96,235,255,129,235,255,
    129,235,255,96,235,255,128,235,255,162,243,255,194,243,255,194,243,255,194,243,255,194,243,255,
    194,243,255,162,235,255,2,252,254,64,89,252,0,0,247,128,24,31,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,127,64,16,255,98,219,251,194,243,255,162,235,255,194,243,255,
    194,243,255,194,243,255,194,243,255,128,235,255,195,243,255,234,244,255,51,254,255,182,254,255,
    183,254,255,51,254,255,234,244,255,195,243,255,128,235,255,194,243,255,194,243,255,194,243,255,
    194,243,255,162,235,255,194,243,255,98,219,251,64,16,255,0,0,160,0,0,0,0,0,0,
    0,0,0,192,113,9,0,0,228,161,105,254,2,252,253,162,235,255,194,243,255,194,243,255,
    194,243,255,194,243,255,128,235,255,11,245,255,91,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,91,255,255,11,245,255,128,235,255,194,243,255,194,243,255,
    194,243,255,194,243,255,162,235,255,2,252,254,161,105,254,0,0,246,160,40,24,0,0,0,
    0,0,0,0,0,70,0,0,255,34,203,253,226,251,255,162,235,255,194,243,255,194,243,255,
    194,243,255,129,235,255,241,253,255,255,255,255,255,255,255,123,255,255,18,254,255,77,245,255,
    76,245,255,18,254,255,91,255,255,255,255,255,255,255,255,241,253,255,129,235,255,194,243,255,
    194,243,255,194,243,255,162,235,255,226,251,255,34,203,251,0,0,255,0,0,100,0,0,0,
    0,0,0,0,0,143,160,40,254,194,243,251,194,243,255,194,243,255,194,243,255,195,243,255,
    96,235,255,175,245,255,255,255,255,255,255,255,208,245,255,195,243,255,96,235,255,96,235,255,
    129,235,255,32,235,255,201,244,255,223,255,255,255,255,255,255,255,255,176,245,255,96,235,255,
    195,243,255,194,243,255,194,243,255,194,243,255,194,243,251,160,40,254,0,0,173,0,0,0,
    0,0,0,0,0,192,97,89,255,2,252,252,162,235,255,194,243,255,194,243,255,129,235,255,
    135,244,255,255,255,255,255,255,255,77,245,255,64,235,255,194,243,255,195,243,255,227,243,255,
    194,243,255,227,243,255,58,255,255,255,255,255,248,254,255,222,255,255,255,255,255,135,244,255,
    129,235,255,194,243,255,194,243,255,162,235,255,2,252,253,97,89,255,0,0,217,0,0,0,
    0,0,0,0,0,219,1,130,255,2,252,253,162,235,255,194,243,255,194,243,255,128,235,255,
    149,254,255,255,255,255,117,254,255,96,235,255,195,243,255,194,243,255,194,243,255,96,235,255,
    96,235,255,150,254,255,255,255,255,182,254,255,97,235,255,117,254,255,255,255,255,149,254,255,
    128,235,255,194,243,255,194,243,255,162,235,255,2,252,254,1,130,255,0,0,239,160,40,25,
    224,56,18,0,0,234,65,146,255,2,252,254,162,235,255,194,243,255,162,243,255,4,244,255,
    190,255,255,255,255,255,136,244,255,129,235,255,195,243,255,162,243,255,195,243,255,77,245,255,
    149,254,255,255,255,255,58,255,255,227,243,255,96,235,255,168,244,255,255,255,255,190,255,255,
    4,244,255,162,243,255,194,243,255,162,235,255,2,252,254,65,146,255,0,0,250,64,16,43,
    192,48,19,0,0,235,65,146,255,2,252,254,162,235,255,194,243,255,129,235,255,169,244,255,
    255,255,255,123,255,255,195,243,255,194,243,255,194,243,255,129,235,255,116,254,255,255,255,255,
    255,255,255,222,255,255,135,244,255,129,235,255,194,243,255,195,243,255,123,255,255,255,255,255,
    169,244,255,129,235,255,194,243,255,162,235,255,2,252,254,65,146,255,0,0,251,64,16,43,
    64,146,7,0,0,221,1,130,255,2,252,253,162,235,255,194,243,255,128,235,255,202,244,255,
    255,255,255,25,255,255,162,243,255,194,243,255,162,243,255,4,244,255,222,255,255,223,255,255,
    223,255,255,255,255,255,70,244,255,161,235,255,194,243,255,162,243,255,25,255,255,255,255,255,
    202,244,255,128,235,255,194,243,255,162,235,255,2,252,254,1,130,255,0,0,241,128,32,28,
    0,0,0,0,0,196,97,89,255,2,252,252,162,235,255,194,243,255,129,235,255,169,244,255,
    255,255,255,124,255,255,195,243,255,194,243,255,194,243,255,161,235,255,183,254,255,255,255,255,
    255,255,255,58,255,255,194,243,255,194,243,255,194,243,255,195,243,255,124,255,255,255,255,255,
    169,244,255,129,235,255,194,243,255,162,235,255,2,252,253,97,89,255,0,0,220,0,0,0,
    0,0,0,0,0,151,160,40,254,194,243,251,194,243,255,194,243,255,162,243,255,4,244,255,
    190,255,255,255,255,255,136,244,255,128,235,255,195,243,255,161,243,255,228,243,255,241,253,255,
    18,254,255,37,244,255,161,235,255,195,243,255,128,235,255,136,244,255,255,255,255,190,255,255,
    4,244,255,162,243,255,194,243,255,194,243,255,194,243,252,160,40,255,0,0,180,0,0,0,
    0,0,0,0,0,80,0,0,255,34,203,252,226,251,255,162,235,255,194,243,255,128,235,255,
    149,254,255,255,255,255,150,254,255,129,235,255,194,243,255,194,243,255,162,243,255,96,235,255,
    96,235,255,161,243,255,194,243,255,194,243,255,129,235,255,150,254,255,255,255,255,149,254,255,
    128,235,255,194,243,255,162,235,255,226,251,255,34,203,251,0,0,255,0,0,111,0,0,0,
    0,0,0,32,73,14,0,0,236,161,105,254,2,252,253,162,235,255,194,243,255,129,235,255,
    103,244,255,190,255,255,183,254,255,129,235,255,194,243,255,194,243,255,194,243,255,195,243,255,
    195,243,255,194,243,255,194,243,255,194,243,255,129,235,255,183,254,255,190,255,255,103,244,255,
    129,235,255,194,243,255,162,235,255,2,252,254,161,105,254,0,0,250,96,24,32,0,0,0,
    0,0,0,0,0,0,0,0,143,64,16,255,98,219,251,194,243,255,162,235,255,194,243,255,
    161,243,255,37,244,255,195,243,255,162,243,255,194,243,255,194,243,255,194,243,255,194,243,255,
    194,243,255,194,243,255,194,243,255,194,243,255,162,243,255,195,243,255,37,244,255,161,243,255,
    194,243,255,162,235,255,194,243,255,98,219,251,64,16,255,0,0,176,0,0,0,0,0,0,
    0,0,0,0,0,0,160,32,23,0,0,239,64,81,253,2,252,253,162,235,255,194,243,255,
    194,243,255,161,243,255,162,243,255,194,243,255,194,243,255,194,243,255,194,243,255,194,243,255,
    194,243,255,194,243,255,194,243,255,194,243,255,194,243,255,162,243,255,161,243,255,194,243,255,
    194,243,255,162,235,255,2,252,254,64,89,252,0,0,253,64,16,43,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,32,0,102,0,0,255,65,146,250,2,252,255,162,235,255,
    194,243,255,194,243,255,194,243,255,194,243,255,194,243,255,194,243,255,194,243,255,194,243,255,
    194,243,255,194,243,255,194,243,255,194,243,255,194,243,255,194,243,255,194,243,255,194,243,255,
    162,235,255,2,252,255,65,146,250,0,0,255,0,0,133,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,161,0,0,255,129,162,250,2,252,255,
    162,235,255,162,235,255,194,243,255,194,243,255,194,243,255,194,243,255,194,243,255,194,243,255,
    194,243,255,194,243,255,194,243,255,194,243,255,194,243,255,194,243,255,162,235,255,162,235,255,
    2,252,255,129,162,250,0,0,255,0,0,188,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,40,6,0,0,190,0,0,255,65,146,250,
    2,252,254,194,243,255,162,235,255,162,235,255,194,243,255,194,243,255,194,243,255,194,243,255,
    194,243,255,194,243,255,194,243,255,194,243,255,162,235,255,162,235,255,194,243,255,2,252,254,
    65,146,250,0,0,255,0,0,212,0,8,18,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,24,9,0,0,164,0,0,255,
    97,89,255,98,219,251,2,252,254,226,251,255,194,243,255,162,235,255,162,235,255,162,235,255,
    162,235,255,162,235,255,162,235,255,194,243,255,226,251,255,2,252,254,98,219,252,97,89,254,
    0,0,255,0,0,185,0,8,20,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,113,
    0,0,242,64,16,255,193,113,255,34,203,252,194,243,252,2,252,253,2,252,254,2,252,254,
    2,252,254,2,252,254,2,252,253,194,243,252,34,203,251,193,113,255,64,16,255,0,0,250,
    0,0,134,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    32,16,35,0,0,159,0,0,242,0,0,255,160,40,255,129,97,255,1,130,255,97,146,255,
    97,146,255,1,130,255,129,97,255,160,40,255,0,0,255,0,0,248,0,0,172,32,8,49,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,128,24,28,0,0,100,0,0,170,0,0,215,0,0,237,0,0,250,
    0,0,250,0,0,239,0,0,220,0,0,178,0,0,110,96,16,36,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,160,40,24,96,24,42,
    96,24,42,128,32,27,64,146,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  },
  { // 11 ETC
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,65,1,18,
    65,1,18,100,3,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,131,1,10,0,0,71,0,0,141,0,0,190,0,0,217,0,0,233,
    0,0,233,0,0,220,0,0,195,0,0,149,0,0,80,225,0,16,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    225,0,16,0,0,128,0,0,223,0,0,255,225,0,255,4,10,255,165,10,255,38,11,255,
    38,11,255,165,10,255,4,10,255,225,0,255,0,0,255,0,0,230,0,0,142,129,0,27,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,0,85,
    0,0,224,64,0,255,100,10,255,40,20,253,42,21,251,106,21,252,106,21,253,74,21,254,
    74,21,254,106,21,253,106,21,252,42,21,251,40,20,252,100,10,255,64,0,255,0,0,235,
    32,0,105,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,136,0,0,255,
    227,9,255,169,20,251,106,21,253,42,21,255,10,21,255,233,20,255,233,20,255,233,20,255,
    233,20,255,233,20,255,233,20,255,10,21,255,42,21,255,106,21,254,169,20,251,227,9,255,
    0,0,255,0,0,159,64,1,6,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,166,0,0,255,6,11,251,
    106,21,253,42,21,255,233,20,255,233,20,255,10,21,255,10,21,255,10,21,255,10,21,255,
    10,21,255,10,21,255,10,21,255,10,21,255,233,20,255,233,20,255,42,21,255,74,21,254,
    6,11,251,0,0,255,0,0,189,224,0,8,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,137,0,0,255,70,11,250,106,21,255,
    233,20,255,233,20,255,10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,
    10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,233,20,255,233,20,255,
    106,21,255,70,11,250,0,0,255,0,0,166,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,32,0,81,0,0,255,6,11,250,106,21,254,233,20,255,
    10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,
    10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,
    233,20,255,106,21,255,6,11,250,0,0,255,32,0,111,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,34,1,14,0,0,230,195,1,254,106,21,253,233,20,255,10,21,255,
    10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,
    10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,
    10,21,255,233,20,255,106,21,254,195,1,252,0,0,247,129,0,31,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,127,64,0,255,169,20,251,42,21,255,233,20,255,10,21,255,
    10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,
    10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,
    10,21,255,233,20,255,42,21,255,169,20,251,64,0,255,0,0,160,0,0,0,0,0,0,
    0,0,0,195,1,9,0,0,228,68,10,254,106,21,253,233,20,255,10,21,255,10,21,255,
    10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,
    10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,
    10,21,255,10,21,255,233,20,255,106,21,254,68,10,254,0,0,246,161,0,24,0,0,0,
    0,0,0,0,0,70,0,0,255,40,20,253,42,21,255,233,20,255,10,21,255,10,21,255,
    10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,
    10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,233,20,255,233,20,255,10,21,255,
    10,21,255,10,21,255,233,20,255,42,21,255,72,20,251,0,0,255,0,0,100,0,0,0,
    0,0,0,0,0,143,225,0,254,42,21,251,10,21,255,10,21,255,10,21,255,10,21,255,
    10,21,255,233,20,255,233,20,255,233,20,255,9,21,255,233,20,255,233,20,255,10,21,255,
    9,21,255,233,20,255,233,20,255,10,21,255,200,4,255,10,21,255,43,37,255,233,20,255,
    233,12,255,10,21,255,10,21,255,10,21,255,42,21,251,225,0,254,0,0,173,0,0,0,
    0,0,0,0,0,192,227,9,255,106,21,252,233,20,255,10,21,255,233,12,255,76,45,255,
    124,215,255,189,231,255,222,247,255,222,239,255,241,101,255,156,223,255,189,239,255,156,223,255,
    156,223,255,190,239,255,182,158,255,200,4,255,149,142,255,189,239,255,190,239,255,156,223,255,
    174,77,255,233,12,255,10,21,255,233,20,255,106,21,253,227,9,255,0,0,217,0,0,0,
    0,0,0,0,0,219,165,10,255,106,21,253,233,20,255,10,21,255,233,12,255,108,53,255,
    255,255,255,25,183,255,208,85,255,17,102,255,109,53,255,18,110,255,83,126,255,255,255,255,
    90,207,255,50,110,255,142,69,255,116,134,255,255,255,255,149,142,255,108,53,255,50,118,255,
    175,77,255,233,12,255,10,21,255,233,20,255,106,21,254,165,10,255,0,0,239,225,0,25,
    65,1,18,0,0,234,38,11,255,74,21,254,233,20,255,10,21,255,233,12,255,108,53,255,
    255,255,255,58,199,255,83,126,255,116,134,255,42,29,255,167,4,255,10,21,255,255,255,255,
    182,150,255,134,4,255,233,12,255,190,239,255,123,215,255,200,4,255,233,12,255,200,4,255,
    233,12,255,10,21,255,10,21,255,233,20,255,74,21,254,38,11,255,0,0,250,128,0,43,
    65,1,19,0,0,235,38,11,255,74,21,254,233,20,255,10,21,255,233,12,255,108,53,255,
    255,255,255,190,239,255,123,215,255,123,215,255,76,45,255,233,12,255,75,45,255,255,255,255,
    215,166,255,200,4,255,43,37,255,222,247,255,57,191,255,200,4,255,42,29,255,10,21,255,
    233,20,255,10,21,255,10,21,255,233,20,255,74,21,254,38,11,255,0,0,251,128,0,43,
    100,3,7,0,0,221,165,10,255,106,21,253,233,20,255,10,21,255,233,12,255,76,45,255,
    255,255,255,116,134,255,134,4,255,200,12,255,233,20,255,233,20,255,43,37,255,255,255,255,
    182,158,255,200,4,255,233,12,255,90,207,255,223,247,255,43,37,255,167,4,255,10,21,255,
    43,37,255,233,20,255,10,21,255,233,20,255,106,21,254,165,10,255,0,0,241,193,0,28,
    0,0,0,0,0,196,227,9,255,106,21,252,233,20,255,10,21,255,233,12,255,109,53,255,
    255,255,255,255,255,255,189,231,255,222,239,255,208,93,255,200,4,255,75,45,255,255,255,255,
    247,166,255,200,4,255,233,12,255,142,61,255,222,239,255,222,247,255,24,183,255,157,231,255,
    17,102,255,200,4,255,10,21,255,233,20,255,106,21,253,227,9,255,0,0,220,0,0,0,
    0,0,0,0,0,151,225,0,254,42,21,251,10,21,255,10,21,255,233,20,255,43,37,255,
    18,110,255,50,118,255,51,118,255,51,118,255,108,53,255,233,12,255,10,29,255,50,110,255,
    174,77,255,233,12,255,10,21,255,233,12,255,75,45,255,83,126,255,182,150,255,50,110,255,
    43,29,255,233,20,255,10,21,255,10,21,255,10,21,252,225,0,255,0,0,180,0,0,0,
    0,0,0,0,0,80,0,0,255,40,20,252,42,21,255,233,20,255,10,21,255,233,20,255,
    200,4,255,200,4,255,200,4,255,200,4,255,233,12,255,10,21,255,233,20,255,200,4,255,
    233,12,255,10,21,255,10,21,255,10,21,255,233,12,255,200,4,255,200,4,255,200,4,255,
    233,20,255,10,21,255,233,20,255,42,21,255,72,20,251,0,0,255,0,0,111,0,0,0,
    0,0,0,34,1,14,0,0,236,68,10,254,106,21,253,233,20,255,10,21,255,10,21,255,
    10,29,255,10,29,255,10,29,255,10,29,255,10,21,255,10,21,255,10,21,255,10,29,255,
    10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,10,29,255,10,29,255,10,21,255,
    10,21,255,10,21,255,233,20,255,106,21,254,68,10,254,0,0,250,96,0,32,0,0,0,
    0,0,0,0,0,0,0,0,143,64,0,255,169,20,251,42,21,255,233,20,255,10,21,255,
    10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,
    10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,
    10,21,255,233,20,255,42,21,255,169,20,251,64,0,255,0,0,176,0,0,0,0,0,0,
    0,0,0,0,0,0,161,0,23,0,0,239,195,1,253,106,21,253,233,20,255,10,21,255,
    10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,
    10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,
    10,21,255,233,20,255,106,21,254,195,1,252,0,0,253,64,0,43,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,32,0,102,0,0,255,6,11,250,106,21,255,233,20,255,
    10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,
    10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,
    233,20,255,106,21,255,6,11,250,0,0,255,0,0,133,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,161,0,0,255,70,11,250,106,21,255,
    233,20,255,233,20,255,10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,
    10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,10,21,255,233,20,255,233,20,255,
    106,21,255,70,11,250,0,0,255,0,0,188,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,64,1,6,0,0,190,0,0,255,6,11,250,
    74,21,254,42,21,255,233,20,255,233,20,255,10,21,255,10,21,255,10,21,255,10,21,255,
    10,21,255,10,21,255,10,21,255,10,21,255,233,20,255,233,20,255,42,21,255,74,21,254,
    6,11,250,0,0,255,0,0,212,96,0,18,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,224,0,9,0,0,164,0,0,255,
    227,9,255,169,20,251,106,21,254,42,21,255,10,21,255,233,20,255,233,20,255,233,20,255,
    233,20,255,233,20,255,233,20,255,10,21,255,42,21,255,106,21,254,169,20,252,227,9,254,
    0,0,255,0,0,185,96,0,20,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,0,113,
    0,0,242,64,0,255,100,10,255,40,20,252,10,21,252,106,21,253,106,21,254,74,21,254,
    74,21,254,106,21,254,106,21,253,10,21,252,72,20,251,100,10,255,64,0,255,0,0,250,
    0,0,134,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    96,0,35,0,0,159,0,0,242,0,0,255,225,0,255,4,10,255,165,10,255,38,11,255,
    38,11,255,165,10,255,4,10,255,225,0,255,0,0,255,0,0,248,0,0,172,64,0,49,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,129,0,28,0,0,100,0,0,170,0,0,215,0,0,237,0,0,250,
    0,0,250,0,0,239,0,0,220,0,0,178,0,0,110,96,0,36,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,225,0,24,128,0,42,
    128,0,42,225,0,27,100,3,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  },
  { // 12 礼让行人
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,231,8,18,
    231,8,18,82,34,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,137,25,10,0,0,71,0,0,141,0,0,190,0,0,217,0,0,233,
    0,0,233,0,0,220,0,0,195,0,0,149,0,0,80,229,8,16,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    101,0,16,0,0,128,0,0,223,0,0,255,165,8,255,76,17,255,208,17,255,18,26,255,
    18,26,255,208,17,255,76,17,255,165,8,255,0,0,255,0,0,230,0,0,142,67,0,27,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,85,
    0,0,224,34,0,255,142,17,255,185,34,253,94,43,251,159,43,252,127,43,253,127,43,254,
    159,43,254,159,43,253,159,43,252,94,43,251,217,34,252,142,17,255,34,0,255,0,0,235,
    0,0,105,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,136,0,0,255,
    75,9,255,27,35,251,159,43,253,127,43,255,94,43,255,61,35,255,61,35,255,61,43,255,
    253,26,255,253,26,255,253,26,255,93,43,255,127,43,255,159,43,254,27,35,251,75,9,255,
    0,0,255,0,0,159,5,0,6,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,166,0,0,255,18,26,251,
    127,43,253,94,43,255,61,35,255,61,43,255,94,43,255,94,43,255,94,43,255,29,27,255,
    158,100,255,63,174,255,62,133,255,61,35,255,61,35,255,61,43,255,94,43,255,127,43,254,
    18,26,251,0,0,255,0,0,189,3,0,8,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,137,0,0,255,52,26,250,159,43,255,
    61,35,255,61,43,255,94,43,255,94,43,255,94,43,255,94,43,255,29,27,255,158,100,255,
    255,255,255,255,255,255,255,255,255,191,149,255,253,26,255,94,43,255,61,43,255,61,35,255,
    159,43,255,52,26,250,0,0,255,0,0,166,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,33,0,81,0,0,255,18,26,250,159,43,254,61,35,255,
    94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,253,26,255,95,182,255,
    255,255,255,223,247,255,223,255,255,127,231,255,94,35,255,94,43,255,94,43,255,94,43,255,
    61,35,255,159,43,255,18,26,250,0,0,255,32,0,111,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,134,16,14,0,0,230,42,9,254,159,43,253,61,35,255,94,43,255,
    94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,253,26,255,62,133,255,
    255,255,255,255,255,255,255,255,255,127,182,255,29,27,255,94,43,255,94,43,255,94,43,255,
    94,43,255,61,35,255,159,43,254,43,9,252,0,0,247,67,8,31,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,127,34,0,255,27,35,251,94,43,255,61,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,62,35,255,94,43,255,
    31,215,255,223,247,255,63,182,255,158,51,255,62,35,255,94,43,255,94,43,255,94,43,255,
    94,43,255,61,43,255,94,43,255,27,35,251,34,0,255,0,0,160,0,0,0,0,0,0,
    0,0,0,206,25,9,0,0,228,141,17,254,159,43,253,61,35,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,29,27,255,158,100,255,
    255,255,255,191,247,255,158,51,255,29,27,255,94,43,255,94,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,61,35,255,159,43,254,141,17,254,0,0,246,165,8,24,0,0,0,
    0,0,0,0,0,70,0,0,255,185,34,253,127,43,255,61,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,221,18,255,255,165,255,
    255,255,255,191,247,255,158,59,255,29,35,255,94,43,255,94,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,61,43,255,127,43,255,217,34,251,0,0,255,0,0,100,0,0,0,
    0,0,0,0,0,143,133,0,254,94,43,251,94,43,255,94,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,94,43,255,62,35,255,253,26,255,158,149,255,255,255,255,
    255,255,255,255,255,255,127,182,255,126,43,255,29,27,255,94,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,94,43,255,94,43,251,133,0,254,0,0,173,0,0,0,
    0,0,0,0,0,192,75,17,255,159,43,252,61,35,255,94,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,62,35,255,126,43,255,31,166,255,255,255,255,255,255,255,
    255,255,255,127,231,255,255,255,255,63,223,255,62,84,255,62,35,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,61,35,255,127,43,253,75,17,255,0,0,217,0,0,0,
    0,0,0,0,0,219,208,17,255,159,43,253,61,35,255,94,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,253,18,255,158,149,255,255,255,255,159,239,255,255,255,255,
    255,255,255,94,84,255,30,125,255,255,255,255,223,206,255,253,26,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,61,35,255,159,43,254,208,17,255,0,0,239,165,8,25,
    231,8,18,0,0,234,18,26,255,127,43,254,61,35,255,94,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,30,35,255,62,76,255,191,149,255,63,174,255,255,255,255,
    127,231,255,94,43,255,253,26,255,190,100,255,254,75,255,62,35,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,61,35,255,127,43,254,18,26,255,0,0,250,66,0,43,
    198,8,19,0,0,235,18,26,255,127,43,254,61,35,255,94,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,94,43,255,62,35,255,221,18,255,31,215,255,255,255,255,
    159,239,255,94,92,255,253,26,255,253,26,255,62,35,255,94,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,61,35,255,127,43,254,18,26,255,0,0,251,66,0,43,
    82,34,7,0,0,221,208,17,255,159,43,253,61,35,255,94,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,94,43,255,29,27,255,126,92,255,255,255,255,255,255,255,
    255,255,255,255,255,255,158,141,255,126,43,255,94,43,255,94,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,61,35,255,159,43,254,208,17,255,0,0,241,132,8,28,
    0,0,0,0,0,196,75,17,255,159,43,252,61,35,255,94,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,94,43,255,62,35,255,255,214,255,255,255,255,223,157,255,
    62,133,255,255,255,255,255,255,255,63,174,255,29,27,255,94,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,61,35,255,127,43,253,75,17,255,0,0,220,0,0,0,
    0,0,0,0,0,151,133,0,254,94,43,251,94,43,255,94,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,221,18,255,62,125,255,255,255,255,95,231,255,126,51,255,
    189,10,255,254,116,255,255,255,255,63,223,255,62,35,255,94,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,94,43,255,94,43,252,133,0,255,0,0,180,0,0,0,
    0,0,0,0,0,80,0,0,255,217,34,252,127,43,255,61,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,62,35,255,30,76,255,159,239,255,255,255,255,222,116,255,253,26,255,
    94,43,255,158,51,255,191,247,255,223,247,255,222,59,255,62,35,255,94,43,255,94,43,255,
    94,43,255,94,43,255,61,43,255,127,43,255,217,34,251,0,0,255,0,0,111,0,0,0,
    0,0,0,41,17,14,0,0,236,141,17,254,159,43,253,61,35,255,94,43,255,94,43,255,
    94,43,255,126,43,255,253,26,255,158,141,255,255,255,255,255,214,255,29,35,255,94,43,255,
    94,43,255,94,43,255,127,231,255,255,255,255,254,116,255,29,27,255,94,43,255,94,43,255,
    94,43,255,94,43,255,61,35,255,159,43,254,141,17,254,0,0,250,99,0,32,0,0,0,
    0,0,0,0,0,0,0,0,143,34,0,255,27,35,251,94,43,255,93,43,255,62,35,255,
    253,26,255,253,26,255,29,35,255,126,51,255,30,125,255,190,59,255,29,27,255,62,35,255,
    253,26,255,221,18,255,30,125,255,127,190,255,126,43,255,221,18,255,62,35,255,94,43,255,
    94,43,255,61,43,255,94,43,255,27,35,251,34,0,255,0,0,176,0,0,0,0,0,0,
    0,0,0,0,0,0,68,8,23,0,0,239,42,9,253,159,43,253,29,35,255,222,67,255,
    31,174,255,63,174,255,30,76,255,158,51,255,223,157,255,31,166,255,30,76,255,190,59,255,
    31,174,255,63,174,255,222,67,255,126,43,255,31,166,255,63,174,255,62,84,255,29,35,255,
    94,43,255,61,35,255,159,43,254,43,9,252,0,0,253,34,0,43,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,32,0,102,0,0,255,18,26,250,95,27,255,157,100,255,
    255,255,255,255,255,255,254,116,255,94,84,255,255,255,255,255,255,255,254,116,255,94,84,255,
    255,255,255,255,255,255,30,125,255,94,92,255,255,255,255,255,255,255,94,133,255,253,18,255,
    93,43,255,159,43,255,18,26,250,0,0,255,0,0,133,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,161,0,0,255,20,18,250,95,76,255,
    158,198,255,191,198,255,62,84,255,222,59,255,191,198,255,191,198,255,62,84,255,222,59,255,
    191,198,255,191,198,255,62,84,255,222,59,255,191,198,255,191,198,255,94,92,255,29,27,255,
    159,43,255,52,26,250,0,0,255,0,0,188,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,5,0,6,0,0,190,0,0,255,242,17,250,
    95,35,254,62,35,255,29,35,255,61,35,255,29,27,255,29,27,255,62,35,255,62,35,255,
    29,27,255,29,27,255,62,35,255,61,35,255,29,27,255,29,27,255,62,35,255,159,43,254,
    18,26,250,0,0,255,0,0,212,1,0,18,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,0,9,0,0,164,0,0,255,
    75,17,255,27,35,251,159,43,254,127,43,255,94,43,255,93,43,255,61,43,255,61,43,255,
    93,43,255,93,43,255,61,43,255,94,43,255,127,43,255,159,43,254,27,35,252,75,9,254,
    0,0,255,0,0,185,1,0,20,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,113,
    0,0,242,34,0,255,142,17,255,217,34,252,94,43,252,127,43,253,127,43,254,127,43,254,
    127,43,254,159,43,254,127,43,253,94,43,252,217,34,251,142,17,255,34,0,255,0,0,250,
    0,0,134,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    34,0,35,0,0,159,0,0,242,0,0,255,165,8,255,76,17,255,208,17,255,18,26,255,
    18,26,255,208,17,255,76,17,255,165,8,255,0,0,255,0,0,248,0,0,172,33,0,49,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,131,8,28,0,0,100,0,0,170,0,0,215,0,0,237,0,0,250,
    0,0,250,0,0,239,0,0,220,0,0,178,0,0,110,98,0,36,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,165,8,24,99,0,42,
    99,0,42,132,8,27,82,34,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  },
  { // 13 违法变道
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,231,8,18,
    231,8,18,82,34,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,137,25,10,0,0,71,0,0,141,0,0,190,0,0,217,0,0,233,
    0,0,233,0,0,220,0,0,195,0,0,149,0,0,80,229,8,16,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    101,0,16,0,0,128,0,0,223,0,0,255,165,8,255,76,17,255,208,17,255,18,26,255,
    18,26,255,208,17,255,76,17,255,165,8,255,0,0,255,0,0,230,0,0,142,67,0,27,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,85,
    0,0,224,34,0,255,142,17,255,185,34,253,94,43,251,159,43,252,127,43,253,159,43,254,
    159,43,254,127,43,253,159,43,252,94,43,251,217,34,252,142,17,255,34,0,255,0,0,235,
    0,0,105,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,136,0,0,255,
    75,9,255,27,35,251,159,43,253,127,43,255,94,43,255,61,35,255,61,43,255,29,27,255,
    29,27,255,61,43,255,61,35,255,94,43,255,127,43,255,159,43,254,27,35,251,75,9,255,
    0,0,255,0,0,159,5,0,6,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,166,0,0,255,18,26,251,
    127,43,253,94,43,255,61,35,255,61,43,255,94,43,255,94,43,255,62,35,255,94,84,255,
    62,84,255,62,35,255,94,43,255,93,43,255,61,35,255,30,35,255,94,43,255,127,43,254,
    18,26,251,0,0,255,0,0,189,3,0,8,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,137,0,0,255,52,26,250,159,43,255,
    61,35,255,61,43,255,94,43,255,94,43,255,94,43,255,94,43,255,29,35,255,95,223,255,
    127,231,255,62,35,255,94,43,255,94,35,255,93,43,255,218,75,255,62,35,255,61,43,255,
    159,43,255,52,26,250,0,0,255,0,0,166,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,33,0,81,0,0,255,18,26,250,159,43,254,61,35,255,
    94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,253,26,255,127,182,255,
    159,239,255,62,35,255,93,43,255,31,19,255,180,132,255,135,246,255,156,59,255,62,35,255,
    61,43,255,159,43,255,18,26,250,0,0,255,32,0,111,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,134,16,14,0,0,230,42,9,254,159,43,253,61,35,255,94,43,255,
    94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,253,26,255,127,182,255,
    159,239,255,61,35,255,62,35,255,187,67,255,104,246,255,197,254,255,142,189,255,31,27,255,
    93,43,255,61,35,255,159,43,254,43,9,252,0,0,247,67,8,31,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,127,34,0,255,27,35,251,94,43,255,61,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,253,26,255,127,182,255,
    159,239,255,62,35,255,31,27,255,204,205,255,198,254,255,104,254,255,198,254,255,149,124,255,
    255,18,255,93,43,255,94,43,255,27,35,251,34,0,255,0,0,160,0,0,0,0,0,0,
    0,0,0,206,25,9,0,0,228,141,17,254,159,43,253,61,35,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,253,26,255,127,182,255,
    159,239,255,31,27,255,17,157,255,4,255,255,136,254,255,135,254,255,198,254,255,198,254,255,
    249,83,255,62,35,255,61,43,255,159,43,254,141,17,254,0,0,246,165,8,24,0,0,0,
    0,0,0,0,0,70,0,0,255,185,34,253,127,43,255,61,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,253,26,255,127,182,255,
    159,239,255,93,43,255,211,140,255,49,157,255,135,254,255,135,254,255,48,165,255,242,148,255,
    86,108,255,62,35,255,93,43,255,127,43,255,217,34,251,0,0,255,0,0,100,0,0,0,
    0,0,0,0,0,143,133,0,254,94,43,251,94,43,255,94,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,253,26,255,127,182,255,
    159,239,255,93,43,255,191,2,255,211,140,255,229,254,255,11,222,255,31,27,255,255,18,255,
    31,27,255,93,43,255,94,43,255,94,43,255,94,43,251,133,0,254,0,0,173,0,0,0,
    0,0,0,0,0,192,75,17,255,159,43,252,61,35,255,94,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,253,26,255,127,182,255,
    159,239,255,255,18,255,211,140,255,197,254,255,166,254,255,55,100,255,62,27,255,93,43,255,
    93,43,255,94,43,255,94,43,255,61,35,255,127,43,253,75,17,255,0,0,217,0,0,0,
    0,0,0,0,0,219,208,17,255,159,43,253,61,35,255,94,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,253,26,255,127,190,255,
    159,231,255,148,124,255,166,254,255,198,254,255,180,132,255,31,27,255,93,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,61,35,255,159,43,254,208,17,255,0,0,239,165,8,25,
    231,8,18,0,0,234,18,26,255,127,43,254,61,35,255,94,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,93,43,255,254,18,255,63,174,255,
    113,255,255,133,254,255,229,254,255,211,148,255,31,19,255,93,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,61,35,255,127,43,254,18,26,255,0,0,250,66,0,43,
    198,8,19,0,0,235,18,26,255,127,43,254,61,35,255,94,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,94,43,255,93,43,255,62,27,255,125,51,255,15,206,255,
    165,254,255,135,254,255,149,124,255,31,27,255,93,43,255,94,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,61,35,255,127,43,254,18,26,255,0,0,251,66,0,43,
    82,34,7,0,0,221,208,17,255,159,43,253,61,35,255,94,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,93,43,255,31,27,255,187,67,255,11,222,255,164,254,255,
    104,246,255,24,92,255,255,18,255,93,43,255,94,43,255,94,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,61,35,255,159,43,254,208,17,255,0,0,241,132,8,28,
    0,0,0,0,0,196,75,17,255,159,43,252,61,35,255,94,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,93,43,255,31,27,255,218,75,255,73,238,255,197,254,255,235,254,255,
    251,222,255,63,35,255,93,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,61,35,255,127,43,253,75,17,255,0,0,220,0,0,0,
    0,0,0,0,0,151,133,0,254,94,43,251,94,43,255,94,43,255,94,43,255,94,43,255,
    94,43,255,93,43,255,31,27,255,249,83,255,104,246,255,229,254,255,108,189,255,158,190,255,
    159,239,255,61,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,94,43,255,94,43,252,133,0,255,0,0,180,0,0,0,
    0,0,0,0,0,80,0,0,255,217,34,252,127,43,255,61,43,255,94,43,255,94,43,255,
    93,43,255,62,35,255,186,67,255,104,246,255,229,254,255,174,197,255,253,26,255,95,182,255,
    159,239,255,62,35,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,61,43,255,127,43,255,217,34,251,0,0,255,0,0,111,0,0,0,
    0,0,0,41,17,14,0,0,236,141,17,254,159,43,253,61,35,255,94,43,255,94,43,255,
    93,43,255,31,27,255,173,197,255,228,254,255,173,197,255,94,43,255,222,18,255,127,190,255,
    159,239,255,62,35,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,61,35,255,159,43,254,141,17,254,0,0,250,99,0,32,0,0,0,
    0,0,0,0,0,0,0,0,143,34,0,255,27,35,251,94,43,255,61,43,255,93,43,255,
    31,27,255,24,92,255,166,254,255,104,238,255,187,67,255,63,35,255,253,26,255,127,182,255,
    159,239,255,62,35,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,
    94,43,255,61,43,255,94,43,255,27,35,251,34,0,255,0,0,176,0,0,0,0,0,0,
    0,0,0,0,0,0,68,8,23,0,0,239,42,9,253,159,43,253,61,35,255,93,43,255,
    31,19,255,242,148,255,4,255,255,110,189,255,31,27,255,125,43,255,253,18,255,127,182,255,
    159,239,255,62,35,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,
    94,43,255,61,35,255,159,43,254,43,9,252,0,0,253,34,0,43,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,32,0,102,0,0,255,18,26,250,159,43,255,61,43,255,
    31,27,255,86,108,255,198,246,255,180,132,255,31,27,255,125,43,255,253,26,255,127,182,255,
    159,239,255,62,35,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,94,43,255,
    61,35,255,159,43,255,18,26,250,0,0,255,0,0,133,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,161,0,0,255,52,26,250,159,43,255,
    61,43,255,62,35,255,187,67,255,94,43,255,94,35,255,94,43,255,29,35,255,95,223,255,
    127,231,255,62,35,255,94,43,255,94,43,255,94,43,255,94,43,255,61,43,255,61,35,255,
    159,43,255,52,26,250,0,0,255,0,0,188,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,5,0,6,0,0,190,0,0,255,18,26,250,
    127,43,254,94,43,255,30,35,255,61,43,255,94,43,255,94,43,255,62,35,255,94,84,255,
    62,84,255,62,35,255,94,43,255,94,43,255,61,43,255,61,35,255,94,43,255,127,43,254,
    18,26,250,0,0,255,0,0,212,1,0,18,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,0,9,0,0,164,0,0,255,
    75,9,255,27,35,251,159,43,254,127,43,255,94,43,255,61,35,255,61,43,255,29,27,255,
    29,27,255,61,43,255,61,35,255,94,43,255,127,43,255,159,43,254,27,35,252,75,9,254,
    0,0,255,0,0,185,1,0,20,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,113,
    0,0,242,34,0,255,142,17,255,217,34,252,94,43,252,127,43,253,127,43,254,159,43,254,
    159,43,254,127,43,254,127,43,253,94,43,252,217,34,251,142,17,255,34,0,255,0,0,250,
    0,0,134,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    34,0,35,0,0,159,0,0,242,0,0,255,165,8,255,76,17,255,208,17,255,18,26,255,
    18,26,255,208,17,255,76,17,255,165,8,255,0,0,255,0,0,248,0,0,172,33,0,49,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,131,8,28,0,0,100,0,0,170,0,0,215,0,0,237,0,0,250,
    0,0,250,0,0,239,0,0,220,0,0,178,0,0,110,98,0,36,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,165,8,24,99,0,42,
    99,0,42,132,8,27,82,34,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  },
  { // 14 违停
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,97,56,18,
    97,56,18,36,145,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,72,10,0,0,71,0,0,141,0,0,190,0,0,217,0,0,233,
    0,0,233,0,0,220,0,0,195,0,0,149,0,0,80,0,40,16,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,40,16,0,0,128,0,0,223,0,0,255,32,40,255,97,88,255,130,120,255,162,136,255,
    162,136,255,130,120,255,97,88,255,32,40,255,0,0,255,0,0,230,0,0,142,0,24,27,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,85,
    0,0,224,0,16,255,97,104,255,195,184,253,4,225,251,4,241,252,4,241,253,3,241,254,
    3,241,254,4,241,253,4,241,252,4,225,251,195,184,252,97,104,255,0,16,255,0,0,235,
    0,0,105,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,136,0,0,255,
    97,80,255,227,208,251,36,241,253,3,233,255,194,232,255,226,232,255,4,217,255,37,209,255,
    37,209,255,4,217,255,226,224,255,194,232,255,227,232,255,36,233,254,227,208,251,97,80,255,
    0,0,255,0,0,159,0,40,6,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,166,0,0,255,130,136,251,
    4,241,253,3,233,255,194,232,255,37,209,255,238,145,255,150,98,255,250,66,255,28,51,255,
    28,51,255,250,58,255,182,90,255,15,138,255,70,201,255,194,232,255,227,232,255,4,241,254,
    130,136,251,0,0,255,0,0,189,0,24,8,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,137,0,0,255,162,144,250,4,241,255,
    227,216,255,5,217,255,16,138,255,60,51,255,127,27,255,127,27,255,94,35,255,94,35,255,
    94,35,255,94,35,255,127,27,255,127,27,255,61,43,255,50,122,255,38,209,255,227,216,255,
    4,241,255,162,144,250,0,0,255,0,0,166,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,8,81,0,0,255,130,136,250,4,241,254,4,209,255,
    226,232,255,15,138,255,159,11,255,94,43,255,61,43,255,61,43,255,61,43,255,93,43,255,
    93,43,255,93,43,255,61,43,255,61,43,255,61,43,255,159,11,255,16,130,255,226,232,255,
    4,209,255,4,241,255,130,136,250,0,0,255,0,0,111,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,48,14,0,0,230,97,80,254,4,241,253,227,216,255,226,232,255,
    227,224,255,4,217,255,183,90,255,127,27,255,61,43,255,94,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,61,43,255,127,27,255,183,90,255,4,217,255,227,224,255,
    226,232,255,227,216,255,4,241,254,65,80,252,0,0,247,0,24,31,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,127,0,8,255,227,208,251,3,233,255,5,217,255,15,138,255,
    4,217,255,226,232,255,4,217,255,183,82,255,127,27,255,61,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,61,43,255,127,27,255,183,82,255,4,217,255,226,232,255,4,225,255,
    49,130,255,38,201,255,227,232,255,227,208,251,0,8,255,0,0,160,0,0,0,0,0,0,
    0,0,0,0,80,9,0,0,228,97,96,254,36,241,253,194,232,255,16,138,255,159,11,255,
    183,90,255,4,217,255,226,232,255,4,217,255,183,82,255,127,27,255,61,43,255,94,43,255,
    94,43,255,61,43,255,127,27,255,183,82,255,4,217,255,226,232,255,4,217,255,183,90,255,
    159,11,255,83,114,255,194,232,255,4,241,254,97,96,254,0,0,246,0,24,24,0,0,0,
    0,0,0,0,0,70,0,0,255,195,184,253,3,233,255,37,209,255,60,51,255,94,43,255,
    127,27,255,183,82,255,4,217,255,226,232,255,4,217,255,183,82,255,127,27,255,61,43,255,
    61,43,255,127,27,255,183,82,255,4,217,255,226,232,255,4,217,255,183,82,255,127,27,255,
    61,43,255,94,35,255,72,193,255,227,240,255,195,184,251,0,0,255,0,0,100,0,0,0,
    0,0,0,0,0,143,32,40,254,4,225,251,194,232,255,238,145,255,127,27,255,61,43,255,
    61,43,255,127,27,255,183,82,255,4,217,255,226,232,255,4,217,255,183,82,255,127,27,255,
    127,27,255,183,82,255,4,217,255,226,232,255,4,217,255,183,82,255,127,27,255,61,43,255,
    61,43,255,127,27,255,49,122,255,194,232,255,4,225,251,32,40,254,0,0,173,0,0,0,
    0,0,0,0,0,192,97,88,255,4,241,252,194,232,255,150,98,255,127,27,255,61,43,255,
    94,43,255,61,43,255,127,27,255,183,82,255,4,217,255,226,232,255,4,217,255,216,74,255,
    216,74,255,4,217,255,226,232,255,4,217,255,183,82,255,127,27,255,61,43,255,94,43,255,
    61,43,255,95,35,255,216,74,255,227,224,255,4,241,253,97,88,255,0,0,217,0,0,0,
    0,0,0,0,0,219,130,120,255,4,241,253,4,217,255,250,66,255,94,35,255,61,43,255,
    94,43,255,94,43,255,61,43,255,127,27,255,183,82,255,4,217,255,226,232,255,37,217,255,
    37,217,255,226,232,255,4,217,255,183,82,255,127,27,255,61,43,255,94,43,255,94,43,255,
    93,43,255,94,35,255,60,51,255,38,201,255,3,241,254,130,120,255,0,0,239,65,40,25,
    97,56,18,0,0,234,162,136,255,3,241,254,37,209,255,28,51,255,94,35,255,93,43,255,
    94,43,255,94,43,255,94,43,255,61,43,255,127,27,255,216,74,255,37,217,255,227,224,255,
    227,224,255,37,217,255,216,74,255,127,27,255,61,43,255,94,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,71,193,255,3,241,254,162,136,255,0,0,250,32,16,43,
    97,48,19,0,0,235,162,136,255,3,241,254,37,209,255,28,51,255,94,35,255,93,43,255,
    94,43,255,94,43,255,94,43,255,61,43,255,127,27,255,216,74,255,37,217,255,227,224,255,
    227,224,255,37,217,255,216,74,255,127,27,255,61,43,255,94,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,72,193,255,3,241,254,162,136,255,0,0,251,32,16,43,
    36,145,7,0,0,221,130,120,255,4,241,253,4,217,255,250,58,255,94,35,255,61,43,255,
    94,43,255,94,43,255,61,43,255,127,27,255,183,82,255,4,217,255,226,232,255,37,217,255,
    37,217,255,226,232,255,4,217,255,183,82,255,127,27,255,61,43,255,94,43,255,94,43,255,
    93,43,255,94,35,255,60,51,255,38,201,255,3,241,254,130,120,255,0,0,241,65,32,28,
    0,0,0,0,0,196,97,88,255,4,241,252,226,224,255,182,90,255,127,27,255,61,43,255,
    94,43,255,61,43,255,127,27,255,183,82,255,4,217,255,226,232,255,4,217,255,216,74,255,
    216,74,255,4,217,255,226,232,255,4,217,255,183,82,255,127,27,255,61,43,255,94,43,255,
    61,43,255,95,35,255,217,74,255,227,216,255,4,241,253,97,88,255,0,0,220,0,0,0,
    0,0,0,0,0,151,32,40,254,4,225,251,194,232,255,15,138,255,127,27,255,61,43,255,
    61,43,255,127,27,255,183,82,255,4,217,255,226,232,255,4,217,255,183,82,255,127,27,255,
    127,27,255,183,82,255,4,217,255,226,232,255,4,217,255,183,82,255,127,27,255,61,43,255,
    61,43,255,127,27,255,82,114,255,194,232,255,4,225,252,32,40,255,0,0,180,0,0,0,
    0,0,0,0,0,80,0,0,255,195,184,252,3,233,255,38,201,255,61,43,255,61,43,255,
    127,27,255,183,82,255,4,217,255,226,232,255,4,217,255,183,82,255,127,27,255,61,43,255,
    61,43,255,127,27,255,183,82,255,4,217,255,226,232,255,4,217,255,183,82,255,127,27,255,
    61,43,255,94,35,255,105,185,255,227,240,255,195,184,251,0,0,255,0,0,111,0,0,0,
    0,0,0,0,48,14,0,0,236,97,96,254,4,241,253,194,232,255,50,122,255,159,11,255,
    183,90,255,4,217,255,226,232,255,4,217,255,183,82,255,127,27,255,61,43,255,94,43,255,
    94,43,255,61,43,255,127,27,255,183,82,255,4,217,255,226,232,255,4,217,255,183,90,255,
    159,11,255,149,98,255,194,224,255,4,241,254,97,96,254,0,0,250,0,16,32,0,0,0,
    0,0,0,0,0,0,0,0,143,0,8,255,227,208,251,227,232,255,38,209,255,16,130,255,
    4,217,255,226,232,255,4,217,255,183,82,255,127,27,255,61,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,61,43,255,127,27,255,183,82,255,4,217,255,227,232,255,4,225,255,
    82,122,255,71,193,255,227,232,255,227,208,251,0,8,255,0,0,176,0,0,0,0,0,0,
    0,0,0,0,0,0,0,32,23,0,0,239,65,80,253,4,241,253,227,216,255,226,232,255,
    227,224,255,4,225,255,183,90,255,127,27,255,61,43,255,94,43,255,94,43,255,94,43,255,
    94,43,255,94,43,255,94,43,255,61,43,255,127,27,255,183,90,255,4,225,255,227,224,255,
    226,232,255,227,216,255,4,241,254,65,80,252,0,0,253,0,16,43,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,102,0,0,255,130,136,250,4,241,255,4,209,255,
    226,232,255,49,130,255,159,11,255,61,43,255,61,43,255,61,43,255,93,43,255,94,43,255,
    94,43,255,93,43,255,61,43,255,61,43,255,61,43,255,159,11,255,82,122,255,226,232,255,
    4,217,255,4,241,255,130,136,250,0,0,255,0,0,133,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,161,0,0,255,162,144,250,4,241,255,
    227,216,255,38,201,255,83,114,255,94,35,255,127,27,255,95,35,255,94,35,255,94,43,255,
    94,43,255,94,35,255,95,35,255,127,27,255,94,35,255,149,98,255,71,193,255,227,216,255,
    4,241,255,162,144,250,0,0,255,0,0,188,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,40,6,0,0,190,0,0,255,162,136,250,
    4,241,254,227,232,255,194,232,255,72,193,255,49,122,255,216,74,255,60,51,255,93,43,255,
    94,43,255,60,51,255,217,74,255,82,114,255,105,185,255,194,224,255,227,232,255,4,233,254,
    162,136,250,0,0,255,0,0,212,0,8,18,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,24,9,0,0,164,0,0,255,
    97,80,255,227,208,251,36,233,254,227,232,255,194,232,255,227,224,255,38,201,255,71,193,255,
    72,193,255,38,201,255,227,216,255,194,232,255,227,240,255,36,241,254,227,200,252,97,80,254,
    0,0,255,0,0,185,0,8,20,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,113,
    0,0,242,0,16,255,97,104,255,195,184,252,4,225,252,4,241,253,3,241,254,3,241,254,
    3,241,254,3,241,254,4,241,253,4,225,252,195,184,251,97,104,255,0,16,255,0,0,250,
    0,0,134,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,16,35,0,0,159,0,0,242,0,0,255,32,40,255,97,88,255,130,120,255,162,136,255,
    162,136,255,130,120,255,97,88,255,32,40,255,0,0,255,0,0,248,0,0,172,0,8,49,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,24,28,0,0,100,0,0,170,0,0,215,0,0,237,0,0,250,
    0,0,250,0,0,239,0,0,220,0,0,178,0,0,110,0,16,36,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,65,40,24,32,24,42,
    32,24,42,65,32,27,36,145,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  },
  { // 15 禁止鸣笛
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,97,56,18,
    97,56,18,36,145,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,72,10,0,0,71,0,0,141,0,0,190,0,0,217,0,0,233,
    0,0,233,0,0,220,0,0,195,0,0,149,0,0,80,0,40,16,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,40,16,0,0,128,0,0,223,0,0,255,32,40,255,97,88,255,130,120,255,162,136,255,
    162,136,255,130,120,255,97,88,255,32,40,255,0,0,255,0,0,230,0,0,142,0,24,27,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,85,
    0,0,224,0,16,255,97,104,255,195,184,253,4,225,251,36,241,252,36,241,253,36,241,254,
    36,241,254,36,241,253,36,241,252,4,225,251,195,184,252,97,104,255,0,16,255,0,0,235,
    0,0,105,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,136,0,0,255,
    97,80,255,227,208,251,36,241,253,36,233,255,227,224,255,162,216,255,130,216,255,97,216,255,
    97,216,255,130,216,255,162,216,255,227,224,255,36,233,255,36,241,254,227,208,251,97,80,255,
    0,0,255,0,0,159,0,40,6,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,166,0,0,255,162,136,251,
    4,241,253,4,225,255,227,216,255,130,216,255,4,225,255,105,226,255,174,235,255,113,244,255,
    113,244,255,174,235,255,105,226,255,4,225,255,130,216,255,227,216,255,36,225,255,4,241,254,
    162,136,251,0,0,255,0,0,189,0,24,8,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,137,0,0,255,162,144,250,4,241,255,
    4,217,255,195,216,255,69,225,255,81,236,255,251,254,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,251,254,255,81,236,255,69,225,255,162,216,255,4,217,255,
    4,241,255,162,144,250,0,0,255,0,0,166,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,8,81,0,0,255,130,136,250,4,241,254,4,217,255,
    195,224,255,8,226,255,60,255,255,255,255,255,255,255,255,255,255,255,255,255,255,223,255,255,
    223,255,255,255,255,255,255,255,255,255,255,255,255,255,255,28,255,255,170,234,255,130,216,255,
    4,217,255,4,241,255,130,136,250,0,0,255,0,0,111,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,48,14,0,0,230,97,80,254,4,241,253,4,217,255,195,224,255,
    195,224,255,101,225,255,219,254,255,255,255,255,223,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,77,235,255,
    130,216,255,4,217,255,4,241,254,97,80,252,0,0,247,0,24,31,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,127,0,8,255,227,208,251,4,225,255,195,216,255,8,226,255,
    101,225,255,97,216,255,134,225,255,219,254,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,93,239,255,255,255,255,255,255,255,255,255,255,
    170,234,255,162,216,255,36,225,255,227,208,251,0,8,255,0,0,160,0,0,0,0,0,0,
    0,0,0,0,80,9,0,0,228,97,96,254,4,241,253,227,216,255,36,225,255,93,255,255,
    219,254,255,134,225,255,97,216,255,134,225,255,219,254,255,255,255,255,223,255,255,255,255,255,
    255,255,255,255,255,255,89,206,255,73,74,255,12,99,255,255,255,255,190,255,255,255,255,255,
    60,255,255,69,225,255,227,216,255,4,241,254,97,96,254,0,0,246,0,24,24,0,0,0,
    0,0,0,0,0,70,0,0,255,195,184,253,36,233,255,97,216,255,113,244,255,255,255,255,
    255,255,255,219,254,255,101,225,255,65,216,255,134,225,255,219,254,255,255,255,255,255,255,255,
    154,214,255,138,82,255,0,0,255,0,0,255,235,90,255,255,255,255,255,255,255,223,255,255,
    255,255,255,113,244,255,97,216,255,36,233,255,195,184,251,0,0,255,0,0,100,0,0,0,
    0,0,0,0,0,143,32,40,254,4,225,251,227,224,255,4,225,255,28,255,255,255,255,255,
    223,255,255,255,255,255,93,255,255,40,242,255,130,224,255,166,225,255,215,213,255,12,91,255,
    32,0,255,0,0,255,32,0,255,0,0,255,77,107,255,215,189,255,20,165,255,255,255,255,
    255,255,255,28,255,255,4,225,255,227,224,255,4,225,251,32,40,254,0,0,173,0,0,0,
    0,0,0,0,0,192,97,88,255,36,241,252,162,216,255,105,234,255,255,255,255,255,255,255,
    255,255,255,24,198,255,44,99,255,8,98,255,195,200,255,4,233,255,130,192,255,0,16,255,
    0,0,255,0,0,255,32,0,255,0,0,255,142,115,255,77,107,255,0,0,255,85,173,255,
    255,255,255,255,255,255,105,234,255,162,216,255,36,241,253,97,88,255,0,0,217,0,0,0,
    0,0,0,0,0,219,130,120,255,36,241,253,130,216,255,207,235,255,255,255,255,190,255,255,
    255,255,255,174,115,255,0,0,255,0,0,255,32,32,255,227,200,255,36,241,255,4,209,255,
    32,32,255,0,0,255,32,8,255,0,0,255,12,99,255,223,255,255,69,41,255,195,24,255,
    190,247,255,255,255,255,174,235,255,130,216,255,36,241,254,130,120,255,0,0,239,65,40,25,
    97,56,18,0,0,234,162,136,255,36,241,254,97,216,255,113,244,255,255,255,255,190,255,255,
    255,255,255,16,132,255,32,0,255,32,8,255,0,0,255,32,32,255,227,200,255,4,241,255,
    227,200,255,32,32,255,0,0,255,0,0,255,203,90,255,255,255,255,146,148,255,0,0,255,
    186,222,255,255,255,255,113,236,255,97,216,255,36,241,254,162,136,255,0,0,250,32,16,43,
    97,48,19,0,0,235,162,136,255,36,241,254,97,216,255,113,244,255,255,255,255,190,255,255,
    255,255,255,16,132,255,0,0,255,32,0,255,0,0,255,0,0,255,32,32,255,227,200,255,
    4,241,255,227,200,255,65,32,255,0,0,255,203,90,255,255,255,255,146,148,255,0,0,255,
    186,222,255,255,255,255,113,236,255,97,216,255,36,241,254,162,136,255,0,0,251,32,16,43,
    36,145,7,0,0,221,130,120,255,36,241,253,130,216,255,207,235,255,255,255,255,190,255,255,
    255,255,255,174,115,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,65,40,255,
    227,200,255,4,241,255,4,209,255,0,24,255,235,82,255,223,255,255,69,41,255,195,24,255,
    190,247,255,255,255,255,174,235,255,130,216,255,36,241,254,130,120,255,0,0,241,65,32,28,
    0,0,0,0,0,196,97,88,255,36,241,252,162,216,255,105,234,255,255,255,255,223,255,255,
    255,255,255,182,181,255,8,66,255,40,66,255,40,66,255,138,74,255,32,8,255,0,0,255,
    32,32,255,4,209,255,36,241,255,32,176,255,207,147,255,109,99,255,0,0,255,85,173,255,
    255,255,255,255,255,255,105,234,255,162,216,255,36,241,253,97,88,255,0,0,220,0,0,0,
    0,0,0,0,0,151,32,40,254,4,225,251,227,224,255,4,225,255,28,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,186,214,255,12,99,255,
    32,0,255,0,24,255,227,208,255,4,241,255,231,233,255,113,172,255,85,165,255,255,255,255,
    255,255,255,28,255,255,4,225,255,227,224,255,4,225,252,32,40,255,0,0,180,0,0,0,
    0,0,0,0,0,80,0,0,255,195,184,252,36,233,255,97,216,255,113,244,255,255,255,255,
    223,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    154,214,255,138,74,255,0,24,255,130,192,255,195,232,255,231,233,255,93,255,255,255,255,255,
    255,255,255,113,244,255,97,216,255,36,233,255,195,184,251,0,0,255,0,0,111,0,0,0,
    0,0,0,0,48,14,0,0,236,97,96,254,4,241,253,227,216,255,69,225,255,60,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,56,190,255,170,114,255,227,200,255,130,224,255,101,217,255,251,254,255,
    125,255,255,36,225,255,227,216,255,4,241,254,97,96,254,0,0,250,0,16,32,0,0,0,
    0,0,0,0,0,0,0,0,143,0,8,255,227,208,251,36,225,255,162,216,255,170,234,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,56,230,255,166,225,255,65,216,255,199,225,255,
    73,226,255,162,216,255,4,225,255,227,208,251,0,8,255,0,0,176,0,0,0,0,0,0,
    0,0,0,0,0,0,0,32,23,0,0,239,97,80,253,4,241,253,4,217,255,130,216,255,
    77,235,255,255,255,255,255,255,255,223,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,223,255,255,255,255,255,28,255,255,199,225,255,162,224,255,
    195,224,255,4,217,255,4,241,254,97,80,252,0,0,253,0,16,43,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,102,0,0,255,130,136,250,4,241,255,4,217,255,
    130,216,255,170,234,255,28,255,255,255,255,255,255,255,255,255,255,255,255,255,255,223,255,255,
    223,255,255,255,255,255,255,255,255,255,255,255,255,255,255,60,255,255,73,226,255,195,224,255,
    4,217,255,4,241,255,130,136,250,0,0,255,0,0,133,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,161,0,0,255,162,144,250,4,241,255,
    4,217,255,162,216,255,69,225,255,81,236,255,251,254,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,251,254,255,81,236,255,36,225,255,162,216,255,4,217,255,
    4,241,255,162,144,250,0,0,255,0,0,188,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,40,6,0,0,190,0,0,255,162,136,250,
    4,241,254,36,225,255,227,216,255,130,216,255,4,225,255,105,226,255,174,235,255,113,244,255,
    113,244,255,174,235,255,105,226,255,4,225,255,130,216,255,227,216,255,4,225,255,4,241,254,
    162,136,250,0,0,255,0,0,212,0,8,18,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,24,9,0,0,164,0,0,255,
    97,80,255,227,208,251,36,241,254,36,233,255,227,224,255,162,216,255,130,216,255,97,216,255,
    97,216,255,130,216,255,162,216,255,227,224,255,36,233,255,36,241,254,227,208,252,97,80,254,
    0,0,255,0,0,185,0,8,20,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,113,
    0,0,242,0,16,255,97,104,255,195,184,252,4,225,252,36,241,253,36,241,254,36,241,254,
    36,241,254,36,241,254,36,241,253,4,225,252,195,192,251,97,104,255,0,16,255,0,0,250,
    0,0,134,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,16,35,0,0,159,0,0,242,0,0,255,32,40,255,97,88,255,130,120,255,162,136,255,
    162,136,255,130,120,255,97,88,255,32,40,255,0,0,255,0,0,248,0,0,172,0,8,49,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,24,28,0,0,100,0,0,170,0,0,215,0,0,237,0,0,250,
    0,0,250,0,0,239,0,0,220,0,0,178,0,0,110,0,16,36,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,65,40,24,32,24,42,
    32,24,42,65,32,27,36,145,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  },
  { // 16 急弯
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,60,
    0,0,78,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,197,
    0,0,228,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,72,0,0,255,
    0,0,255,0,0,99,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,210,0,0,255,
    0,0,253,0,0,229,0,0,13,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,0,100,0,0,255,128,57,252,
    224,81,251,0,0,255,0,0,129,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,15,0,0,231,0,0,253,192,196,253,
    128,229,254,0,0,253,0,0,244,0,8,28,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,32,0,133,0,0,255,192,73,252,96,254,255,
    160,254,255,128,106,251,0,0,255,32,0,158,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,8,30,0,0,245,0,0,253,32,213,254,96,254,255,
    64,254,255,160,229,254,64,8,253,0,0,250,32,0,45,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,32,0,162,0,0,255,128,106,251,160,254,255,32,254,255,
    32,254,255,160,254,255,32,131,252,0,0,255,32,0,183,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,32,0,51,0,0,254,96,8,253,160,229,255,96,254,255,32,254,255,
    32,254,255,64,254,255,0,246,255,160,24,252,0,0,255,0,0,69,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,32,0,189,0,0,255,64,131,252,160,254,255,32,254,255,32,254,255,
    32,254,255,0,246,255,160,254,255,192,155,252,0,0,254,0,0,207,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,32,0,76,0,0,255,192,32,252,32,246,255,64,254,255,32,254,255,128,254,255,
    160,254,255,160,254,255,64,254,255,128,254,255,64,49,252,0,0,255,32,8,94,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,213,0,0,255,0,164,252,160,254,255,32,254,255,128,254,255,224,196,255,
    160,147,255,64,139,255,160,237,255,32,164,255,96,139,253,64,8,254,0,0,225,0,24,10,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    32,0,104,0,0,255,64,49,252,96,254,255,96,254,255,32,246,255,32,82,255,0,0,255,
    0,0,255,0,0,255,224,122,255,96,98,255,128,16,255,160,24,252,0,0,255,0,0,122,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,17,
    0,0,234,0,0,253,160,196,253,160,254,255,0,246,255,128,65,255,0,0,255,64,8,255,
    64,90,255,128,98,255,128,221,255,128,106,255,0,0,255,0,0,254,0,0,253,0,0,240,
    0,0,24,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,0,137,
    0,0,255,0,82,251,128,254,255,160,254,255,32,131,255,0,0,255,192,32,255,32,205,255,
    192,254,255,128,254,255,192,254,255,64,90,255,0,0,255,0,0,255,0,0,252,0,0,255,
    0,0,151,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,0,33,0,0,245,
    32,0,253,64,221,254,128,254,255,32,254,255,0,41,255,0,0,255,64,172,255,192,254,255,
    0,254,255,0,246,255,128,254,255,96,98,255,0,0,255,64,16,255,64,131,254,96,16,253,
    0,0,251,32,0,40,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,0,165,0,0,255,
    192,114,251,160,254,255,64,254,255,192,237,255,96,16,255,0,0,255,96,221,255,96,254,255,
    32,254,255,32,254,255,160,254,255,0,82,255,0,41,255,96,213,255,32,255,255,0,123,252,
    0,0,255,32,0,176,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,8,53,0,0,255,96,16,253,
    192,237,255,64,254,255,64,254,255,192,237,255,64,8,255,32,8,255,160,229,255,96,254,255,
    32,254,255,32,254,255,96,254,255,128,188,255,224,245,255,128,254,255,32,254,255,224,237,255,
    128,24,252,0,0,255,32,8,63,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,192,0,0,255,96,139,252,
    160,254,255,32,254,255,64,254,255,192,237,255,64,8,255,64,8,255,224,245,255,64,254,255,
    32,254,255,64,254,255,32,254,255,128,254,255,64,254,255,32,254,255,32,254,255,160,254,255,
    160,155,252,0,0,255,0,0,201,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,32,0,79,0,0,255,224,32,252,64,254,255,
    64,254,255,32,254,255,64,254,255,192,237,255,64,8,255,64,8,255,192,237,255,64,254,255,
    32,254,255,64,254,255,64,254,255,32,254,255,32,254,255,64,254,255,64,254,255,64,254,255,
    64,254,255,0,41,252,0,0,255,32,8,89,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,215,0,0,254,32,172,253,160,254,255,
    32,254,255,32,254,255,64,254,255,192,237,255,64,8,255,64,8,255,192,237,255,64,254,255,
    32,254,255,64,254,255,64,254,255,64,254,255,64,254,255,64,254,255,64,254,255,32,254,255,
    160,254,255,96,180,253,0,0,254,0,0,220,0,0,8,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,32,0,108,0,0,255,96,57,252,128,254,255,32,254,255,
    64,254,255,32,254,255,64,254,255,192,237,255,96,8,255,96,16,255,192,237,255,64,254,255,
    32,254,255,64,254,255,64,254,255,64,254,255,64,254,255,64,254,255,64,254,255,64,254,255,
    32,254,255,128,254,255,128,65,252,0,0,255,32,0,114,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,18,0,0,236,0,0,254,192,196,253,96,254,255,0,254,255,
    32,254,255,32,254,255,32,254,255,192,237,255,0,0,255,0,0,255,96,221,255,64,254,255,
    0,254,255,32,254,255,32,254,255,32,254,255,32,254,255,32,254,255,32,254,255,32,254,255,
    0,254,255,96,254,255,192,196,253,0,0,253,0,0,237,0,0,20,0,0,0,0,0,0,
    0,0,0,0,0,0,32,0,141,0,0,255,160,106,251,32,255,255,160,254,255,160,254,255,
    160,254,255,160,254,255,160,254,255,224,254,255,32,172,255,160,155,255,192,254,255,160,254,255,
    160,254,255,160,254,255,160,254,255,160,254,255,160,254,255,160,254,255,160,254,255,160,254,255,
    160,254,255,160,254,255,32,255,255,160,106,251,0,0,255,32,0,145,0,0,0,0,0,0,
    0,0,0,0,0,31,0,0,243,0,0,249,96,98,250,64,131,251,32,131,251,32,131,251,
    32,131,251,32,131,251,32,131,251,32,131,251,128,147,251,128,147,251,32,131,251,32,131,251,
    32,131,251,32,131,251,32,131,251,32,131,251,32,131,251,32,131,251,32,131,251,32,131,251,
    32,131,251,32,131,251,64,131,251,96,98,250,0,0,249,0,0,245,0,0,33,0,0,0,
    0,0,0,0,0,183,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,
    0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,
    0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,
    0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,183,0,0,0,
    0,0,22,0,0,157,0,0,158,0,0,159,0,0,159,32,0,159,32,0,159,32,0,159,
    32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,
    32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,
    32,0,159,32,0,159,32,0,159,0,0,159,0,0,159,0,0,159,0,0,157,0,0,22,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  },
  { // 17 学校
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,60,
    0,0,78,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,197,
    0,0,228,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,72,0,0,255,
    0,0,255,0,0,99,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,210,0,0,255,
    0,0,253,0,0,229,0,0,13,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,0,100,0,0,255,128,57,252,
    224,81,251,0,0,255,0,0,129,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,15,0,0,231,0,0,253,192,196,253,
    128,229,254,0,0,253,0,0,244,0,8,28,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,32,0,133,0,0,255,192,73,252,96,254,255,
    160,254,255,128,106,251,0,0,255,32,0,158,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,8,30,0,0,245,0,0,253,32,213,254,96,254,255,
    64,254,255,160,229,254,64,8,253,0,0,250,32,0,45,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,32,0,162,0,0,255,128,98,251,160,254,255,32,254,255,
    32,254,255,160,254,255,32,131,252,0,0,255,32,0,183,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,32,8,51,0,0,254,160,24,253,0,246,255,64,254,255,32,254,255,
    32,254,255,64,254,255,0,246,255,128,24,252,0,0,255,0,0,69,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,189,0,0,255,32,49,252,224,196,255,96,254,255,32,254,255,
    64,254,255,0,254,255,224,254,255,64,180,252,0,0,254,0,0,207,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,76,0,0,255,0,0,252,0,0,255,0,0,255,0,205,255,128,254,255,
    32,254,255,128,254,255,64,213,255,0,164,255,192,32,252,0,0,255,0,0,94,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,213,0,0,255,0,0,252,32,0,255,0,0,255,224,163,255,160,254,255,
    64,254,255,64,213,255,64,8,255,0,0,255,32,0,253,0,0,254,0,0,225,0,0,10,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    32,0,104,0,0,255,128,65,252,224,114,255,0,0,255,160,65,255,32,246,255,32,254,255,
    160,254,255,192,155,255,0,0,255,32,0,255,0,0,255,224,32,252,0,0,255,0,0,122,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,17,
    0,0,234,0,0,253,64,221,253,64,172,255,0,0,255,224,122,255,224,254,255,32,254,255,
    64,254,255,192,229,255,96,57,255,0,0,255,160,114,255,0,205,254,0,0,253,0,0,240,
    0,8,24,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,137,
    0,0,255,224,81,251,96,246,255,128,147,255,0,0,255,32,90,255,0,230,255,0,254,255,
    64,254,255,64,255,255,96,139,255,0,0,255,64,172,255,160,255,255,64,90,252,0,0,255,
    32,0,151,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,0,33,0,0,245,
    64,8,253,192,196,254,96,16,255,0,0,255,0,0,255,32,0,255,0,0,255,128,180,255,
    0,246,255,96,131,255,160,65,255,0,0,255,160,65,255,192,139,255,64,221,254,64,8,253,
    0,0,251,32,0,40,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,0,165,0,0,255,
    192,114,251,128,254,255,128,180,255,192,114,255,0,0,255,192,73,255,128,172,255,224,245,255,
    64,221,255,160,24,255,64,8,255,0,0,255,64,8,255,192,32,255,32,246,255,32,131,252,
    0,0,255,32,0,176,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,8,53,0,0,255,96,16,253,
    192,237,255,64,254,255,0,255,255,32,172,255,0,0,255,160,106,255,0,255,255,32,254,255,
    64,254,255,192,254,255,64,131,255,0,0,255,64,139,255,192,254,255,64,254,255,224,237,255,
    128,24,252,0,0,255,32,8,63,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,192,0,0,255,96,139,252,
    160,254,255,0,246,255,160,254,255,96,139,255,0,0,255,32,90,255,128,254,255,32,254,255,
    32,254,255,160,254,255,128,106,255,0,0,255,224,122,255,160,254,255,0,246,255,160,254,255,
    160,155,252,0,0,255,0,0,201,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,32,0,79,0,0,255,224,32,252,64,254,255,
    64,254,255,32,254,255,128,254,255,192,73,255,0,0,255,128,16,255,160,237,255,96,254,255,
    64,254,255,32,254,255,224,32,255,0,0,255,224,32,255,32,254,255,64,254,255,32,254,255,
    64,254,255,0,41,252,0,0,255,32,8,89,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,215,0,0,254,32,172,253,160,254,255,
    0,254,255,128,254,255,96,221,255,32,0,255,32,49,255,0,0,255,96,139,255,160,254,255,
    128,254,255,64,180,255,0,0,255,160,65,255,0,0,255,64,172,255,160,254,255,0,254,255,
    160,254,255,96,180,253,0,0,254,0,0,220,0,0,8,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,32,0,108,0,0,255,96,57,252,128,254,255,32,254,255,
    32,254,255,160,254,255,160,106,255,0,0,255,32,213,255,0,41,255,96,16,255,32,254,255,
    96,254,255,0,41,255,64,8,255,192,237,255,32,8,255,32,49,255,128,254,255,32,254,255,
    32,254,255,128,254,255,128,65,252,0,0,255,32,0,114,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,18,0,0,236,0,0,254,192,196,253,96,254,255,0,254,255,
    0,254,255,128,254,255,160,106,255,0,82,255,192,254,255,128,147,255,128,57,255,32,246,255,
    32,246,255,128,57,255,96,139,255,224,254,255,192,114,255,0,82,255,96,254,255,32,254,255,
    0,254,255,96,254,255,192,196,253,0,0,253,0,0,237,0,0,20,0,0,0,0,0,0,
    0,0,0,0,0,0,32,0,141,0,0,255,160,106,251,32,255,255,160,254,255,160,254,255,
    160,254,255,160,254,255,224,254,255,224,254,255,160,254,255,192,254,255,224,254,255,192,254,255,
    192,254,255,224,254,255,224,254,255,160,254,255,224,254,255,224,254,255,192,254,255,160,254,255,
    160,254,255,160,254,255,32,255,255,160,106,251,0,0,255,32,0,145,0,0,0,0,0,0,
    0,0,0,0,0,31,0,0,243,0,0,249,96,98,250,64,131,251,32,131,251,32,131,251,
    32,131,251,32,131,251,32,131,251,32,131,251,32,131,251,32,131,251,32,131,251,32,131,251,
    32,131,251,32,131,251,32,131,251,32,131,251,32,131,251,32,131,251,32,131,251,32,131,251,
    32,131,251,32,131,251,64,131,251,96,98,250,0,0,249,0,0,245,0,0,33,0,0,0,
    0,0,0,0,0,183,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,
    0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,
    0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,
    0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,183,0,0,0,
    0,0,22,0,0,157,0,0,158,0,0,159,0,0,159,32,0,159,32,0,159,32,0,159,
    32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,
    32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,
    32,0,159,32,0,159,32,0,159,0,0,159,0,0,159,0,0,159,0,0,157,0,0,22,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  },
  { // 18 事故多发
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,60,
    0,0,78,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,197,
    0,0,228,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,72,0,0,255,
    0,0,255,0,0,99,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,210,0,0,255,
    0,0,253,0,0,229,0,0,13,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,0,100,0,0,255,128,57,252,
    224,81,251,0,0,255,0,0,129,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,15,0,0,231,0,0,253,192,196,253,
    128,229,254,0,0,253,0,0,244,0,8,28,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,32,0,133,0,0,255,192,73,252,96,254,255,
    160,254,255,128,106,251,0,0,255,32,0,158,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,8,30,0,0,245,0,0,253,32,213,254,96,254,255,
    64,254,255,160,229,254,64,8,253,0,0,250,32,0,45,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,32,0,162,0,0,255,128,106,251,160,254,255,32,254,255,
    32,254,255,160,254,255,32,131,252,0,0,255,32,0,183,0,0,0,0,0,6,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,32,0,51,0,0,254,96,8,253,160,229,255,96,254,255,32,254,255,
    32,254,255,64,254,255,0,246,255,160,24,252,0,0,255,0,0,68,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,32,0,189,0,0,255,64,131,252,160,254,255,32,254,255,64,254,255,
    64,254,255,32,254,255,160,254,255,192,163,253,0,0,254,0,0,210,0,0,112,0,0,31,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,32,0,76,0,0,255,192,32,252,32,246,255,64,254,255,32,254,255,64,254,255,
    64,254,255,64,254,255,32,254,255,96,254,255,32,49,255,0,0,255,0,0,255,0,0,153,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,213,0,0,255,0,164,252,128,254,255,0,254,255,32,254,255,32,254,255,
    32,254,255,32,254,255,32,254,255,160,254,255,96,180,255,0,0,250,0,0,251,0,0,160,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    32,0,104,0,0,255,64,49,252,160,254,255,160,254,255,160,254,255,160,254,255,160,254,255,
    160,254,255,96,254,255,32,254,255,32,254,255,128,254,255,192,73,251,0,0,255,0,0,160,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,17,
    0,0,234,0,0,253,224,204,253,160,180,255,64,90,255,96,98,255,64,98,255,96,98,255,
    96,98,255,32,213,255,96,254,255,0,254,255,160,254,255,0,164,254,0,0,254,0,0,234,
    0,0,24,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,0,137,
    0,0,255,0,82,251,96,254,255,64,8,255,0,0,255,0,0,255,0,0,255,0,0,255,
    0,0,255,192,65,255,160,254,255,0,246,255,160,254,255,224,155,255,0,0,252,0,0,255,
    0,0,151,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,0,33,0,0,245,
    32,8,253,128,229,254,160,229,255,96,16,255,0,0,255,32,0,255,0,0,255,32,0,255,
    0,0,255,128,57,255,32,246,255,64,254,255,160,254,255,224,155,255,0,0,254,0,0,253,
    0,0,251,0,0,40,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,0,165,0,0,255,
    224,114,251,224,114,255,96,16,255,32,0,255,0,0,255,0,0,255,0,0,255,0,0,255,
    0,0,255,32,8,255,128,16,255,128,147,255,224,254,255,224,155,255,0,0,255,32,0,252,
    0,0,255,0,0,176,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,8,53,0,0,255,128,16,253,
    96,221,255,64,8,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,
    0,0,255,0,0,255,0,0,255,64,49,255,0,255,255,64,139,255,0,0,255,224,81,255,
    0,41,252,0,0,255,32,8,63,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,192,0,0,255,128,147,252,
    64,254,255,96,16,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,
    0,0,255,0,0,255,0,0,255,160,65,255,160,254,255,128,229,255,224,122,255,160,229,255,
    224,163,252,0,0,255,0,0,201,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,32,0,79,0,0,255,224,32,252,64,254,255,
    224,245,255,32,8,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,
    0,0,255,0,0,255,0,0,255,128,57,255,128,254,255,96,254,255,64,213,255,32,254,255,
    96,254,255,0,41,252,0,0,255,32,8,89,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,215,0,0,254,32,172,253,128,254,255,
    96,254,255,192,147,255,96,16,255,0,0,255,32,0,255,96,57,255,192,65,255,224,32,255,
    0,0,255,32,0,255,0,41,255,192,188,255,192,254,255,64,131,255,0,0,255,192,73,255,
    192,254,255,64,180,253,0,0,254,0,0,220,0,0,8,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,32,0,108,0,0,255,96,57,252,128,254,255,32,254,255,
    32,254,255,160,254,255,224,32,255,0,0,255,0,0,255,128,229,255,32,255,255,64,131,255,
    0,0,255,0,0,255,96,139,255,224,254,255,128,254,255,64,139,255,0,0,255,224,73,255,
    96,254,255,128,254,255,128,65,252,0,0,255,32,0,114,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,18,0,0,236,0,0,254,192,196,253,96,254,255,0,254,255,
    0,254,255,64,254,255,0,164,255,0,41,255,224,122,255,32,246,255,32,246,255,128,221,255,
    128,57,255,128,57,255,128,221,255,32,254,255,32,254,255,64,254,255,192,196,255,224,245,255,
    32,254,255,96,254,255,192,196,253,0,0,253,0,0,237,0,0,20,0,0,0,0,0,0,
    0,0,0,0,0,0,32,0,141,0,0,255,160,106,251,32,255,255,160,254,255,160,254,255,
    160,254,255,160,254,255,224,254,255,192,254,255,0,255,255,192,254,255,160,254,255,224,254,255,
    224,254,255,224,254,255,224,254,255,160,254,255,160,254,255,160,254,255,0,255,255,192,254,255,
    160,254,255,160,254,255,32,255,255,160,106,251,0,0,255,32,0,145,0,0,0,0,0,0,
    0,0,0,0,0,31,0,0,243,0,0,249,96,98,250,64,131,251,32,131,251,32,131,251,
    32,131,251,32,131,251,0,123,251,32,131,251,0,123,251,32,131,251,32,131,251,32,131,251,
    32,131,251,32,131,251,32,131,251,32,131,251,32,131,251,32,131,251,0,123,251,32,131,251,
    32,131,251,32,131,251,64,131,251,96,98,250,0,0,249,0,0,245,0,0,33,0,0,0,
    0,0,0,0,0,183,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,
    0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,
    0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,
    0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,183,0,0,0,
    0,0,22,0,0,157,0,0,158,0,0,159,0,0,159,32,0,159,32,0,159,32,0,159,
    32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,
    32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,
    32,0,159,32,0,159,32,0,159,0,0,159,0,0,159,0,0,159,0,0,157,0,0,22,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  },
  { // 19 注意行人
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,60,
    0,0,78,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,197,
    0,0,228,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,72,0,0,255,
    0,0,255,0,0,99,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,210,0,0,255,
    0,0,253,0,0,229,0,0,13,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,0,100,0,0,255,128,57,252,
    224,81,251,0,0,255,0,0,129,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,15,0,0,231,0,0,253,192,196,253,
    128,229,254,0,0,253,0,0,244,0,8,28,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,32,0,133,0,0,255,192,73,252,96,254,255,
    160,254,255,128,106,251,0,0,255,32,0,158,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,8,30,0,0,245,0,0,253,32,213,254,96,254,255,
    96,254,255,160,229,254,64,8,253,0,0,250,32,0,45,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,32,0,162,0,0,255,128,106,251,160,254,255,96,254,255,
    224,245,255,160,254,255,96,139,252,0,0,255,32,0,183,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,32,0,51,0,0,254,96,8,253,160,229,255,160,254,255,224,155,255,
    128,24,255,96,49,255,64,221,255,224,32,252,0,0,255,0,0,69,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,32,0,189,0,0,255,64,131,252,192,254,255,224,245,255,64,8,255,
    0,0,255,0,0,255,224,114,255,64,180,252,0,0,254,0,0,207,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,32,0,76,0,0,255,192,32,252,32,246,255,64,254,255,32,254,255,96,57,255,
    0,0,255,0,0,255,64,139,255,192,254,255,0,41,252,0,0,255,32,8,94,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,213,0,0,255,0,164,252,160,254,255,0,254,255,160,254,255,192,155,255,
    0,0,255,96,98,255,64,254,255,160,254,255,96,180,253,0,0,254,0,0,225,0,0,10,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    32,0,104,0,0,255,64,49,252,96,254,255,32,254,255,0,254,255,224,254,255,96,98,255,
    0,0,255,0,164,255,224,254,255,0,246,255,128,254,255,160,65,252,0,0,255,32,0,122,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,17,
    0,0,234,0,0,253,160,196,253,128,254,255,32,254,255,192,254,255,192,196,255,128,16,255,
    0,0,255,160,65,255,224,237,255,128,254,255,96,254,255,0,205,254,0,0,253,0,0,240,
    0,8,24,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,0,137,
    0,0,255,0,82,251,128,254,255,32,254,255,96,254,255,32,172,255,0,0,255,0,0,255,
    64,8,255,0,0,255,128,16,255,160,188,255,64,254,255,160,254,255,64,98,252,0,0,255,
    32,0,151,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,0,33,0,0,245,
    32,0,253,64,221,254,96,254,255,96,254,255,0,205,255,0,0,255,224,40,255,0,0,255,
    64,49,255,32,205,255,224,32,255,0,123,255,160,254,255,64,254,255,128,229,254,64,8,253,
    0,0,251,32,0,40,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,0,165,0,0,255,
    192,114,251,160,254,255,32,254,255,64,254,255,224,245,255,192,188,255,64,139,255,0,0,255,
    64,90,255,32,255,255,0,254,255,64,254,255,64,254,255,32,254,255,160,254,255,0,123,252,
    0,0,255,32,0,176,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,8,53,0,0,255,96,16,253,
    192,237,255,96,254,255,32,254,255,64,254,255,32,254,255,224,254,255,0,82,255,0,0,255,
    96,16,255,64,172,255,160,254,255,32,254,255,64,254,255,32,254,255,64,254,255,224,237,255,
    128,24,252,0,0,255,32,8,63,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,192,0,0,255,96,139,252,
    160,254,255,32,254,255,64,254,255,32,254,255,96,254,255,160,237,255,64,8,255,224,32,255,
    160,24,255,0,0,255,96,139,255,128,254,255,32,254,255,64,254,255,32,254,255,160,254,255,
    160,155,252,0,0,255,0,0,201,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,32,0,79,0,0,255,224,32,252,64,254,255,
    64,254,255,64,254,255,64,254,255,32,254,255,160,254,255,160,147,255,0,0,255,0,164,255,
    192,229,255,32,0,255,128,57,255,128,254,255,32,254,255,64,254,255,64,254,255,64,254,255,
    64,254,255,0,41,252,0,0,255,32,8,89,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,215,0,0,254,32,172,253,160,254,255,
    32,254,255,64,254,255,64,254,255,32,254,255,128,254,255,32,41,255,64,8,255,192,237,255,
    160,254,255,192,32,255,32,41,255,128,254,255,32,254,255,64,254,255,64,254,255,32,254,255,
    160,254,255,96,180,253,0,0,254,0,0,220,0,0,8,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,32,0,108,0,0,255,96,57,252,128,254,255,32,254,255,
    64,254,255,64,254,255,32,254,255,128,254,255,128,188,255,0,0,255,64,90,255,160,254,255,
    96,254,255,96,57,255,32,8,255,224,245,255,64,254,255,32,254,255,64,254,255,64,254,255,
    32,254,255,128,254,255,128,65,252,0,0,255,32,0,114,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,18,0,0,236,0,0,254,192,196,253,96,254,255,0,254,255,
    32,254,255,32,254,255,0,246,255,128,254,255,160,147,255,32,0,255,128,188,255,96,254,255,
    96,254,255,32,90,255,64,8,255,160,229,255,64,254,255,0,254,255,32,254,255,32,254,255,
    0,254,255,96,254,255,192,196,253,0,0,253,0,0,237,0,0,20,0,0,0,0,0,0,
    0,0,0,0,0,0,32,0,141,0,0,255,160,106,251,32,255,255,160,254,255,160,254,255,
    160,254,255,160,254,255,160,254,255,160,254,255,160,254,255,0,246,255,192,254,255,160,254,255,
    192,254,255,96,254,255,32,254,255,192,254,255,160,254,255,160,254,255,160,254,255,160,254,255,
    160,254,255,160,254,255,32,255,255,160,106,251,0,0,255,32,0,145,0,0,0,0,0,0,
    0,0,0,0,0,31,0,0,243,0,0,249,96,98,250,64,131,251,32,131,251,32,131,251,
    32,131,251,32,131,251,32,131,251,32,131,251,32,131,251,64,139,251,32,131,251,32,131,251,
    32,131,251,32,131,251,64,131,251,32,131,251,32,131,251,32,131,251,32,131,251,32,131,251,
    32,131,251,32,131,251,64,131,251,96,98,250,0,0,249,0,0,245,0,0,33,0,0,0,
    0,0,0,0,0,183,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,
    0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,
    0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,
    0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,183,0,0,0,
    0,0,22,0,0,157,0,0,158,0,0,159,0,0,159,32,0,159,32,0,159,32,0,159,
    32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,
    32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,
    32,0,159,32,0,159,32,0,159,0,0,159,0,0,159,0,0,159,0,0,157,0,0,22,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  },
  { // 20 陡坡
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,60,
    0,0,78,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,197,
    0,0,228,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,72,0,0,255,
    0,0,255,0,0,99,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,210,0,0,255,
    0,0,253,0,0,229,0,0,13,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,0,100,0,0,255,128,57,252,
    224,81,251,0,0,255,0,0,129,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,15,0,0,231,0,0,253,192,196,253,
    128,229,254,0,0,253,0,0,244,0,8,28,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,32,0,133,0,0,255,192,73,252,96,254,255,
    160,254,255,128,106,251,0,0,255,32,0,158,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,8,30,0,0,245,0,0,253,32,213,254,96,254,255,
    64,254,255,160,229,254,64,8,253,0,0,250,32,0,45,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,32,0,162,0,0,255,128,106,251,160,254,255,32,254,255,
    32,254,255,160,254,255,32,131,252,0,0,255,32,0,183,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,32,0,51,0,0,254,96,8,253,160,229,255,96,254,255,32,254,255,
    32,254,255,64,254,255,0,246,255,160,24,252,0,0,255,0,0,69,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,32,0,189,0,0,255,64,131,252,160,254,255,32,254,255,64,254,255,
    64,254,255,32,254,255,160,254,255,224,163,252,0,0,254,0,0,207,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,32,0,76,0,0,255,192,32,252,32,246,255,64,254,255,32,254,255,64,254,255,
    64,254,255,64,254,255,32,254,255,64,254,255,64,49,252,0,0,255,32,0,94,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,213,0,0,254,0,164,252,160,254,255,32,254,255,64,254,255,64,254,255,
    64,254,255,64,254,255,32,254,255,160,254,255,96,98,253,0,0,254,0,0,225,0,0,10,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    32,0,108,0,0,255,64,49,252,96,254,255,32,254,255,64,254,255,64,254,255,64,254,255,
    64,254,255,64,254,255,32,254,255,128,254,255,64,90,255,0,0,252,0,0,255,0,0,122,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,21,
    0,0,243,32,0,253,32,213,254,160,254,255,0,254,255,64,254,255,64,254,255,64,254,255,
    64,254,255,64,254,255,32,254,255,192,254,255,64,98,255,0,0,254,64,8,253,0,0,240,
    0,0,24,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,136,
    0,0,255,32,0,251,160,106,255,0,246,255,160,254,255,32,254,255,32,254,255,64,254,255,
    64,254,255,64,254,255,224,245,255,32,238,255,224,73,255,192,32,255,32,90,252,0,0,255,
    0,0,151,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,33,0,0,245,
    0,0,253,32,0,254,0,41,255,160,24,255,224,155,255,160,254,255,128,254,255,32,254,255,
    32,254,255,64,254,255,224,237,255,32,41,255,32,0,255,32,0,255,192,32,254,96,8,253,
    0,0,251,32,0,40,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,165,0,0,255,
    0,0,251,0,0,255,160,229,255,96,172,255,160,24,255,96,57,255,32,205,255,192,254,255,
    64,254,255,0,254,255,160,254,255,64,172,255,0,0,255,0,0,255,96,180,255,96,139,252,
    0,0,255,32,0,176,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,53,0,0,255,32,8,253,
    32,8,255,0,0,255,64,213,255,224,254,255,32,246,255,192,114,255,64,8,255,128,98,255,
    0,238,255,160,254,255,32,254,255,160,254,255,0,82,255,96,57,255,192,254,255,224,237,255,
    128,24,252,0,0,255,32,8,63,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,192,0,0,255,0,123,252,
    192,32,255,0,0,255,64,221,255,96,254,255,64,254,255,192,254,255,64,213,255,128,65,255,
    128,16,255,224,155,255,128,254,255,160,254,255,160,229,255,96,229,255,64,254,255,160,254,255,
    160,155,252,0,0,255,0,0,201,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,32,0,79,0,0,255,0,41,252,192,237,255,
    96,16,255,0,0,255,64,213,255,96,254,255,32,254,255,0,254,255,96,254,255,128,254,255,
    32,164,255,160,24,255,32,41,255,224,196,255,192,254,255,128,254,255,0,254,255,64,254,255,
    64,254,255,0,41,252,0,0,255,32,8,89,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,215,0,0,254,64,180,253,64,254,255,
    96,16,255,0,0,255,160,229,255,192,254,255,128,254,255,160,254,255,128,254,255,128,254,255,
    0,255,255,128,254,255,32,131,255,160,24,255,192,106,255,224,237,255,192,254,255,64,254,255,
    160,254,255,96,180,253,0,0,254,0,0,220,0,0,8,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,32,0,108,0,0,255,96,57,252,128,254,255,192,237,255,
    0,0,255,0,0,255,224,40,255,64,49,255,32,49,255,32,49,255,32,49,255,32,49,255,
    0,41,255,64,49,255,160,65,255,64,8,255,0,0,255,64,8,255,64,131,255,224,237,255,
    32,254,255,128,254,255,128,65,252,0,0,255,32,0,114,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,18,0,0,236,0,0,254,192,196,253,128,254,255,192,237,255,
    128,98,255,64,90,255,224,81,255,224,73,255,224,73,255,224,73,255,224,73,255,224,73,255,
    224,73,255,224,73,255,192,73,255,32,82,255,64,90,255,32,82,255,224,73,255,160,147,255,
    0,246,255,96,254,255,192,196,253,0,0,253,0,0,237,0,0,20,0,0,0,0,0,0,
    0,0,0,0,0,0,32,0,141,0,0,255,160,106,251,32,255,255,128,254,255,192,254,255,
    0,255,255,32,255,255,32,255,255,32,255,255,32,255,255,32,255,255,32,255,255,32,255,255,
    32,255,255,32,255,255,32,255,255,32,255,255,32,255,255,32,255,255,32,255,255,224,254,255,
    160,254,255,160,254,255,32,255,255,160,106,251,0,0,255,32,0,145,0,0,0,0,0,0,
    0,0,0,0,0,31,0,0,243,0,0,249,96,98,250,64,131,251,32,131,251,32,131,251,
    0,123,251,0,123,251,0,123,251,0,123,251,0,123,251,0,123,251,0,123,251,0,123,251,
    0,123,251,0,123,251,0,123,251,0,123,251,0,123,251,0,123,251,0,123,251,32,131,251,
    32,131,251,32,131,251,64,131,251,96,98,250,0,0,249,0,0,245,0,0,33,0,0,0,
    0,0,0,0,0,183,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,
    0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,
    0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,
    0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,183,0,0,0,
    0,0,22,0,0,157,0,0,158,0,0,159,0,0,159,32,0,159,32,0,159,32,0,159,
    32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,
    32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,
    32,0,159,32,0,159,32,0,159,0,0,159,0,0,159,0,0,159,0,0,157,0,0,22,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  },
  { // 21 村庄
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,60,
    0,0,78,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,197,
    0,0,228,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,72,0,0,255,
    0,0,255,0,0,99,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,210,0,0,255,
    0,0,253,0,0,229,0,0,13,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,0,100,0,0,255,128,57,252,
    224,81,251,0,0,255,0,0,129,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,15,0,0,231,0,0,253,192,196,253,
    128,229,254,0,0,253,0,0,244,0,8,28,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,32,0,133,0,0,255,192,73,252,96,254,255,
    160,254,255,128,106,251,0,0,255,32,0,158,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,8,30,0,0,245,0,0,253,32,213,254,96,254,255,
    64,254,255,160,229,254,64,8,253,0,0,250,32,0,45,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,32,0,162,0,0,255,128,106,251,160,254,255,32,254,255,
    32,254,255,160,254,255,32,131,252,0,0,255,32,0,183,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,32,0,51,0,0,254,96,8,253,160,229,255,96,254,255,32,254,255,
    32,254,255,64,254,255,0,246,255,160,24,252,0,0,255,0,0,69,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,32,0,189,0,0,255,64,131,252,160,254,255,32,254,255,64,254,255,
    64,254,255,32,254,255,160,254,255,224,163,252,0,0,254,0,0,207,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,32,0,76,0,0,255,192,32,252,0,246,255,64,254,255,64,254,255,64,254,255,
    64,254,255,64,254,255,32,254,255,64,254,255,32,49,252,0,0,255,32,8,94,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,213,0,0,255,0,164,252,224,254,255,32,254,255,32,254,255,64,254,255,
    64,254,255,64,254,255,32,254,255,224,254,255,96,180,253,0,0,254,0,0,225,0,0,10,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    32,0,104,0,0,255,32,49,252,192,254,255,0,205,255,192,237,255,96,254,255,32,254,255,
    64,254,255,32,254,255,96,254,255,192,196,255,128,254,255,160,65,252,0,0,255,32,0,122,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,17,
    0,0,234,0,0,253,224,196,253,224,229,255,64,8,255,0,82,255,160,254,255,32,254,255,
    32,254,255,160,254,255,128,188,255,0,0,255,192,155,255,128,229,254,0,0,253,0,0,240,
    0,8,24,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,0,137,
    0,0,255,32,90,251,128,254,255,0,41,255,0,0,255,0,0,255,0,123,255,160,254,255,
    128,254,255,96,221,255,96,16,255,0,0,255,0,0,255,224,204,255,224,114,252,0,0,255,
    32,0,151,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,0,33,0,0,245,
    64,8,253,32,221,254,224,81,255,0,0,255,0,0,255,0,0,255,0,0,255,224,155,255,
    0,238,255,224,32,255,0,0,255,0,0,255,0,0,255,128,16,255,64,180,254,128,16,253,
    0,0,251,32,0,40,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,0,165,0,0,255,
    224,114,251,224,221,255,224,32,255,0,0,255,32,0,255,32,0,255,0,0,255,64,90,255,
    0,189,255,160,24,255,0,0,255,32,0,255,0,0,255,0,0,255,0,164,255,64,139,252,
    0,0,255,32,0,176,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,8,53,0,0,255,96,16,253,
    160,229,255,224,254,255,128,106,255,0,0,255,0,0,255,0,0,255,0,0,255,0,164,255,
    32,255,255,160,65,255,0,0,255,0,0,255,0,0,255,0,0,255,128,229,255,32,246,255,
    128,16,252,0,0,255,32,8,63,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,192,0,0,255,96,139,252,
    160,254,255,128,254,255,64,90,255,0,0,255,64,98,255,192,73,255,0,0,255,192,155,255,
    224,254,255,96,57,255,96,8,255,160,98,255,0,41,255,0,0,255,32,213,255,224,254,255,
    160,147,252,0,0,255,0,0,201,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,32,0,79,0,0,255,224,32,252,64,254,255,
    32,254,255,160,254,255,64,90,255,0,0,255,160,254,255,32,213,255,0,0,255,224,155,255,
    0,255,255,32,41,255,64,49,255,128,255,255,64,131,255,0,0,255,96,221,255,96,254,255,
    64,254,255,0,41,252,0,0,255,32,8,89,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,215,0,0,254,32,172,253,160,254,255,
    0,246,255,160,254,255,64,90,255,0,0,255,32,254,255,192,196,255,0,0,255,224,163,255,
    0,255,255,64,49,255,64,49,255,224,254,255,0,123,255,0,0,255,96,221,255,96,254,255,
    128,254,255,96,180,253,0,0,254,0,0,220,0,0,8,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,32,0,108,0,0,255,96,57,252,128,254,255,32,254,255,
    32,254,255,160,254,255,224,73,255,0,0,255,64,254,255,160,196,255,0,0,255,192,155,255,
    0,255,255,192,32,255,192,32,255,32,255,255,192,114,255,0,0,255,96,221,255,96,254,255,
    32,254,255,128,254,255,128,65,252,0,0,255,32,0,114,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,18,0,0,236,0,0,254,192,196,253,96,254,255,0,254,255,
    0,254,255,96,254,255,64,172,255,0,123,255,0,254,255,64,221,255,0,123,255,32,213,255,
    128,254,255,160,155,255,128,147,255,128,254,255,96,180,255,0,123,255,224,245,255,32,254,255,
    0,246,255,96,254,255,192,196,253,0,0,253,0,0,237,0,0,20,0,0,0,0,0,0,
    0,0,0,0,0,0,32,0,141,0,0,255,160,106,251,32,255,255,160,254,255,160,254,255,
    160,254,255,160,254,255,0,255,255,32,255,255,160,254,255,192,254,255,32,255,255,224,254,255,
    160,254,255,0,255,255,0,255,255,160,254,255,224,254,255,32,255,255,192,254,255,160,254,255,
    160,254,255,160,254,255,32,255,255,160,106,251,0,0,255,32,0,145,0,0,0,0,0,0,
    0,0,0,0,0,31,0,0,243,0,0,249,96,98,250,64,131,251,32,131,251,32,131,251,
    32,131,251,32,131,251,0,123,251,0,123,251,32,131,251,32,131,251,0,123,251,32,131,251,
    32,131,251,0,123,251,0,123,251,32,131,251,0,123,251,0,123,251,32,131,251,32,131,251,
    32,131,251,32,131,251,64,131,251,96,98,250,0,0,249,0,0,245,0,0,33,0,0,0,
    0,0,0,0,0,183,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,
    0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,
    0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,
    0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,183,0,0,0,
    0,0,22,0,0,157,0,0,158,0,0,159,0,0,159,32,0,159,32,0,159,32,0,159,
    32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,
    32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,
    32,0,159,32,0,159,32,0,159,0,0,159,0,0,159,0,0,159,0,0,157,0,0,22,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  },
  { // 22 窄桥
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,60,
    0,0,78,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,197,
    0,0,228,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,72,0,0,255,
    0,0,255,0,0,99,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,210,0,0,255,
    0,0,253,0,0,229,0,0,13,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,0,100,0,0,255,128,57,252,
    224,81,251,0,0,255,0,0,129,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,15,0,0,231,0,0,253,192,196,253,
    128,229,254,0,0,253,0,0,244,0,8,28,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,32,0,133,0,0,255,192,73,252,96,254,255,
    160,254,255,128,106,251,0,0,255,32,0,158,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,8,30,0,0,245,0,0,253,32,213,254,96,254,255,
    64,254,255,160,229,254,64,8,253,0,0,250,32,0,45,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,32,0,162,0,0,255,128,106,251,160,254,255,32,254,255,
    32,254,255,160,254,255,32,131,252,0,0,255,32,0,183,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,32,0,51,0,0,254,96,8,253,160,229,255,96,254,255,32,254,255,
    32,254,255,64,254,255,224,245,255,160,24,252,0,0,255,0,0,69,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,32,0,189,0,0,255,64,131,252,0,255,255,64,254,255,64,254,255,
    64,254,255,32,254,255,0,255,255,224,163,252,0,0,254,0,0,207,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,32,0,76,0,0,255,192,32,252,0,246,255,64,172,255,160,229,255,64,254,255,
    64,254,255,192,237,255,64,172,255,32,254,255,32,49,252,0,0,255,32,8,94,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,213,0,0,255,96,188,252,160,196,255,0,0,255,160,114,255,128,254,255,
    160,254,255,96,139,255,0,0,255,160,188,255,192,196,253,0,0,254,0,0,225,0,0,10,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    32,0,104,0,0,255,32,49,252,224,254,255,224,155,255,0,0,255,192,155,255,128,254,255,
    160,254,255,32,131,255,0,0,255,128,188,255,224,254,255,160,65,252,0,0,255,32,0,122,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,17,
    0,0,234,0,0,253,160,188,253,0,255,255,224,163,255,0,0,255,0,164,255,128,254,255,
    160,254,255,32,131,255,0,0,255,160,196,255,192,254,255,0,205,254,0,0,253,0,0,240,
    0,8,24,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,0,137,
    0,0,255,0,82,251,128,254,255,128,254,255,0,164,255,0,0,255,0,164,255,128,254,255,
    160,254,255,32,131,255,0,0,255,192,196,255,96,254,255,128,254,255,64,98,252,0,0,255,
    32,0,151,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,0,33,0,0,245,
    32,0,253,64,221,254,96,254,255,128,254,255,224,163,255,0,0,255,0,164,255,128,254,255,
    160,254,255,32,131,255,0,0,255,160,188,255,128,254,255,64,254,255,128,229,254,64,8,253,
    0,0,251,32,0,40,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,0,165,0,0,255,
    192,114,251,160,254,255,0,246,255,160,254,255,32,172,255,0,0,255,0,164,255,128,254,255,
    160,254,255,32,131,255,0,0,255,224,204,255,128,254,255,0,254,255,160,254,255,0,123,252,
    0,0,255,32,0,176,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,8,53,0,0,255,96,16,253,
    192,237,255,96,254,255,32,254,255,160,254,255,128,106,255,0,0,255,128,106,255,128,254,255,
    160,254,255,0,123,255,0,0,255,64,131,255,192,254,255,0,254,255,64,254,255,224,237,255,
    128,24,252,0,0,255,32,8,63,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,192,0,0,255,96,139,252,
    160,254,255,0,254,255,160,254,255,224,163,255,0,0,255,192,32,255,32,205,255,96,254,255,
    64,254,255,96,221,255,224,32,255,0,0,255,96,180,255,160,254,255,0,254,255,160,254,255,
    160,155,252,0,0,255,0,0,201,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,32,0,79,0,0,255,224,32,252,64,254,255,
    32,254,255,128,254,255,32,205,255,64,8,255,64,8,255,32,213,255,160,254,255,32,254,255,
    32,254,255,160,254,255,96,221,255,64,8,255,96,16,255,96,221,255,128,254,255,32,254,255,
    64,254,255,0,41,252,0,0,255,32,8,89,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,215,0,0,254,32,172,253,128,254,255,
    64,254,255,0,246,255,224,40,255,0,0,255,0,164,255,160,254,255,0,254,255,64,254,255,
    64,254,255,32,254,255,160,254,255,96,180,255,0,0,255,0,41,255,32,246,255,64,254,255,
    128,254,255,96,180,253,0,0,254,0,0,220,0,0,8,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,32,0,108,0,0,255,96,57,252,96,254,255,32,254,255,
    32,254,255,96,57,255,0,0,255,160,114,255,160,254,255,32,254,255,64,254,255,64,254,255,
    64,254,255,64,254,255,32,254,255,192,254,255,64,139,255,0,0,255,96,57,255,32,254,255,
    32,254,255,128,254,255,128,65,252,0,0,255,32,0,114,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,18,0,0,236,0,0,254,192,196,253,96,254,255,0,254,255,
    32,246,255,64,49,255,96,57,255,64,254,255,32,254,255,32,254,255,32,254,255,32,254,255,
    32,254,255,32,254,255,32,254,255,32,254,255,64,254,255,96,57,255,32,49,255,32,246,255,
    0,254,255,96,254,255,192,196,253,0,0,253,0,0,237,0,0,20,0,0,0,0,0,0,
    0,0,0,0,0,0,32,0,141,0,0,255,160,106,251,32,255,255,160,254,255,160,254,255,
    192,254,255,160,254,255,160,254,255,192,254,255,160,254,255,160,254,255,160,254,255,160,254,255,
    160,254,255,160,254,255,160,254,255,160,254,255,192,254,255,160,254,255,160,254,255,192,254,255,
    160,254,255,160,254,255,32,255,255,160,106,251,0,0,255,32,0,145,0,0,0,0,0,0,
    0,0,0,0,0,31,0,0,243,0,0,249,96,98,250,64,131,251,32,131,251,32,131,251,
    32,131,251,32,131,251,32,131,251,32,131,251,32,131,251,32,131,251,32,131,251,32,131,251,
    32,131,251,32,131,251,32,131,251,32,131,251,32,131,251,32,131,251,32,131,251,32,131,251,
    32,131,251,32,131,251,64,131,251,96,98,250,0,0,249,0,0,245,0,0,33,0,0,0,
    0,0,0,0,0,183,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,
    0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,
    0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,
    0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,183,0,0,0,
    0,0,22,0,0,157,0,0,158,0,0,159,0,0,159,32,0,159,32,0,159,32,0,159,
    32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,
    32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,
    32,0,159,32,0,159,32,0,159,0,0,159,0,0,159,0,0,159,0,0,157,0,0,22,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  },
  { // 23 落石
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,60,
    0,0,78,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,197,
    0,0,228,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,72,0,0,255,
    0,0,255,0,0,99,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,210,0,0,255,
    0,0,253,0,0,229,0,0,13,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,0,100,0,0,255,128,57,252,
    224,81,251,0,0,255,0,0,129,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,15,0,0,231,0,0,253,192,196,253,
    128,229,254,0,0,253,0,0,244,0,8,28,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,32,0,133,0,0,255,192,73,252,96,254,255,
    160,254,255,128,106,251,0,0,255,32,0,158,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,8,30,0,0,245,0,0,253,32,213,254,96,254,255,
    64,254,255,160,229,254,64,8,253,0,0,250,32,0,45,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,32,0,162,0,0,255,128,106,251,160,254,255,32,254,255,
    32,254,255,160,254,255,32,131,252,0,0,255,32,0,183,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,32,0,51,0,0,254,96,16,253,128,229,255,96,254,255,32,254,255,
    32,254,255,64,254,255,0,246,255,160,24,252,0,0,255,0,0,69,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,189,0,0,255,0,123,252,64,172,255,64,254,255,32,254,255,
    64,254,255,32,254,255,160,254,255,224,155,252,0,0,254,0,0,207,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,32,0,76,0,0,255,32,49,252,64,172,255,32,82,255,160,254,255,32,254,255,
    32,254,255,64,254,255,128,254,255,96,254,255,32,41,252,0,0,255,32,8,94,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,213,0,0,255,128,188,252,32,82,255,192,73,255,192,254,255,0,254,255,
    128,254,255,192,237,255,160,188,255,128,254,255,128,188,253,0,0,254,0,0,225,0,0,10,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    32,0,104,0,0,255,128,57,252,192,237,255,0,0,255,64,90,255,160,254,255,64,254,255,
    128,221,255,192,32,255,0,0,255,160,73,255,128,254,255,160,65,252,0,0,255,32,0,122,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,17,
    0,0,234,0,0,253,32,213,253,128,139,255,0,0,255,96,98,255,128,254,255,160,254,255,
    224,122,255,0,0,255,64,8,255,0,0,255,192,204,255,96,221,254,0,0,253,0,0,240,
    0,8,24,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,0,137,
    0,0,255,0,82,251,96,254,255,160,24,255,0,0,255,96,98,255,128,254,255,96,254,255,
    192,188,255,0,0,255,0,0,255,160,24,255,160,229,255,192,254,255,64,90,252,0,0,255,
    32,0,151,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,0,33,0,0,245,
    32,0,253,192,229,254,32,172,255,0,0,255,0,0,255,64,98,255,160,254,255,0,254,255,
    128,254,255,160,196,255,32,131,255,192,237,255,128,254,255,128,254,255,192,237,254,64,8,253,
    0,0,251,32,0,40,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,0,165,0,0,255,
    160,114,251,224,254,255,32,49,255,0,0,255,0,0,255,64,98,255,160,254,255,32,254,255,
    32,254,255,96,254,255,0,255,255,64,172,255,32,41,255,192,73,255,0,238,255,64,131,252,
    0,0,255,32,0,176,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,8,53,0,0,255,96,8,253,
    32,246,255,224,204,255,0,0,255,32,0,255,0,0,255,64,98,255,160,254,255,0,254,255,
    64,254,255,96,254,255,32,213,255,0,0,255,0,0,255,0,0,255,192,73,255,32,254,255,
    128,24,252,0,0,255,32,8,63,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,192,0,0,255,96,139,252,
    0,255,255,0,82,255,0,0,255,32,8,255,0,0,255,64,98,255,160,254,255,96,254,255,
    0,246,255,96,254,255,0,205,255,0,0,255,32,0,255,0,0,255,192,32,255,192,254,255,
    160,147,252,0,0,255,0,0,201,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,32,0,79,0,0,255,224,32,252,96,254,255,
    128,229,255,32,8,255,0,0,255,32,0,255,0,0,255,64,90,255,224,254,255,0,205,255,
    224,32,255,224,40,255,64,213,255,64,90,255,0,0,255,32,8,255,32,164,255,128,254,255,
    32,254,255,0,41,252,0,0,255,32,8,89,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,215,0,0,254,32,172,253,32,255,255,
    0,123,255,0,0,255,32,0,255,32,0,255,0,0,255,64,90,255,32,255,255,32,90,255,
    0,0,255,0,0,255,64,98,255,192,254,255,192,196,255,96,221,255,128,254,255,32,254,255,
    160,254,255,96,180,253,0,0,254,0,0,220,0,0,8,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,32,0,108,0,0,255,96,57,252,128,254,255,224,237,255,
    96,16,255,0,0,255,0,0,255,0,0,255,0,0,255,32,82,255,0,255,255,32,172,255,
    0,0,255,0,0,255,64,172,255,128,254,255,96,254,255,96,254,255,32,254,255,64,254,255,
    32,254,255,128,254,255,128,65,252,0,0,255,32,0,114,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,18,0,0,236,0,0,254,160,196,253,192,254,255,32,172,255,
    192,32,255,224,32,255,192,32,255,224,32,255,192,32,255,192,114,255,96,254,255,64,254,255,
    0,205,255,0,205,255,96,254,255,32,254,255,0,254,255,0,254,255,32,254,255,32,254,255,
    0,254,255,96,254,255,192,196,253,0,0,253,0,0,237,0,0,20,0,0,0,0,0,0,
    0,0,0,0,0,0,32,0,141,0,0,255,160,106,251,32,255,255,160,254,255,128,254,255,
    160,254,255,160,254,255,160,254,255,160,254,255,160,254,255,160,254,255,160,254,255,160,254,255,
    224,254,255,224,254,255,160,254,255,160,254,255,160,254,255,160,254,255,160,254,255,160,254,255,
    160,254,255,160,254,255,32,255,255,160,106,251,0,0,255,32,0,145,0,0,0,0,0,0,
    0,0,0,0,0,31,0,0,243,0,0,249,96,98,250,64,131,251,32,131,251,32,131,251,
    32,131,251,32,131,251,32,131,251,32,131,251,32,131,251,32,131,251,32,131,251,32,131,251,
    0,123,251,0,123,251,32,131,251,32,131,251,32,131,251,32,131,251,32,131,251,32,131,251,
    32,131,251,32,131,251,64,131,251,96,98,250,0,0,249,0,0,245,0,0,33,0,0,0,
    0,0,0,0,0,183,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,
    0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,
    0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,
    0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,183,0,0,0,
    0,0,22,0,0,157,0,0,158,0,0,159,0,0,159,32,0,159,32,0,159,32,0,159,
    32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,
    32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,
    32,0,159,32,0,159,32,0,159,0,0,159,0,0,159,0,0,159,0,0,157,0,0,22,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  },
  { // 24 隧道
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,60,
    0,0,78,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,197,
    0,0,228,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,72,0,0,255,
    0,0,255,0,0,99,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,210,0,0,255,
    0,0,253,0,0,229,0,0,13,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,0,100,0,0,255,128,57,252,
    224,81,251,0,0,255,0,0,129,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,15,0,0,231,0,0,253,192,196,253,
    128,229,254,0,0,253,0,0,244,0,8,28,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,32,0,133,0,0,255,192,73,252,96,254,255,
    160,254,255,128,106,251,0,0,255,32,0,158,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,8,30,0,0,245,0,0,253,32,213,254,96,254,255,
    64,254,255,160,229,254,64,8,253,0,0,250,32,0,45,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,32,0,162,0,0,255,128,106,251,160,254,255,32,254,255,
    32,254,255,160,254,255,32,131,252,0,0,255,32,0,183,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,32,0,51,0,0,254,96,8,253,160,229,255,96,254,255,32,254,255,
    32,254,255,64,254,255,0,246,255,160,24,252,0,0,255,0,0,69,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,32,0,189,0,0,255,64,131,252,160,254,255,32,254,255,64,254,255,
    64,254,255,32,254,255,160,254,255,224,163,252,0,0,254,0,0,207,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,32,0,76,0,0,255,192,32,252,32,246,255,64,254,255,32,254,255,64,254,255,
    64,254,255,64,254,255,32,254,255,96,254,255,32,49,252,0,0,255,32,8,94,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,213,0,0,255,0,164,252,160,254,255,0,254,255,32,254,255,64,254,255,
    64,254,255,32,254,255,0,254,255,128,254,255,96,180,253,0,0,254,0,0,225,0,0,10,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    32,0,104,0,0,255,64,49,252,64,254,255,64,254,255,160,254,255,160,254,255,64,254,255,
    64,254,255,160,254,255,160,254,255,64,254,255,96,254,255,160,65,252,0,0,255,32,0,122,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,17,
    0,0,234,0,0,253,160,188,253,0,255,255,224,245,255,0,164,255,96,98,255,0,41,255,
    0,41,255,96,98,255,0,164,255,224,245,255,224,254,255,224,204,254,0,0,253,0,0,240,
    0,8,24,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,0,137,
    0,0,255,224,81,251,224,254,255,32,164,255,0,41,255,0,0,255,0,0,255,0,0,255,
    0,0,255,0,0,255,0,0,255,0,41,255,32,164,255,0,255,255,64,90,252,0,0,255,
    32,0,151,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,0,33,0,0,245,
    0,0,253,160,237,254,224,155,255,0,0,255,0,0,255,0,41,255,32,123,255,96,180,255,
    128,180,255,32,123,255,0,41,255,0,0,255,0,0,255,224,155,255,224,245,254,32,8,253,
    0,0,251,32,0,40,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,0,165,0,0,255,
    0,123,251,128,229,255,0,0,255,0,0,255,0,123,255,32,246,255,32,255,255,32,123,255,
    0,123,255,32,255,255,32,246,255,0,123,255,0,0,255,0,0,255,128,229,255,96,139,252,
    0,0,255,32,0,176,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,8,53,0,0,255,96,16,253,
    0,246,255,192,73,255,0,0,255,32,90,255,192,254,255,32,254,255,160,254,255,96,98,255,
    160,65,255,160,254,255,32,254,255,192,254,255,32,90,255,0,0,255,192,73,255,32,254,255,
    128,24,252,0,0,255,32,8,63,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,192,0,0,255,160,155,252,
    192,237,255,0,0,255,32,0,255,96,221,255,96,254,255,0,254,255,192,254,255,160,114,255,
    224,73,255,192,254,255,0,254,255,96,254,255,96,221,255,32,0,255,0,0,255,192,237,255,
    224,163,252,0,0,255,0,0,201,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,32,0,79,0,0,255,192,32,252,160,254,255,
    224,155,255,0,0,255,96,57,255,96,254,255,32,254,255,32,254,255,192,254,255,160,106,255,
    224,73,255,192,254,255,32,254,255,32,254,255,96,254,255,96,57,255,0,0,255,224,155,255,
    160,254,255,224,32,252,0,0,255,32,8,89,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,215,0,0,254,32,172,253,0,255,255,
    32,131,255,0,0,255,96,98,255,160,254,255,32,254,255,32,254,255,192,254,255,160,106,255,
    224,73,255,192,254,255,32,254,255,32,254,255,160,254,255,128,106,255,0,0,255,0,123,255,
    0,255,255,64,180,253,0,0,254,0,0,220,0,0,8,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,32,0,108,0,0,255,96,57,252,96,254,255,160,254,255,
    0,164,255,0,0,255,96,98,255,160,254,255,32,254,255,32,254,255,192,254,255,128,106,255,
    192,73,255,192,254,255,32,254,255,32,254,255,160,254,255,64,131,255,0,0,255,32,131,255,
    160,254,255,96,254,255,128,65,252,0,0,255,32,0,114,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,18,0,0,236,0,0,254,192,196,253,64,254,255,128,254,255,
    64,139,255,0,0,255,64,90,255,128,254,255,0,246,255,0,246,255,160,254,255,64,90,255,
    32,90,255,160,254,255,0,246,255,0,246,255,128,254,255,128,98,255,0,0,255,32,131,255,
    128,254,255,64,254,255,192,196,253,0,0,253,0,0,237,0,0,20,0,0,0,0,0,0,
    0,0,0,0,0,0,32,0,141,0,0,255,160,106,251,32,255,255,128,254,255,192,254,255,
    0,246,255,32,131,255,160,229,255,224,254,255,160,254,255,160,254,255,192,254,255,96,254,255,
    96,254,255,192,254,255,160,254,255,160,254,255,224,254,255,128,229,255,32,131,255,0,246,255,
    192,254,255,128,254,255,32,255,255,160,106,251,0,0,255,32,0,145,0,0,0,0,0,0,
    0,0,0,0,0,31,0,0,243,0,0,249,96,98,250,64,131,251,32,131,251,32,131,251,
    64,131,251,128,147,251,64,139,251,32,131,251,32,131,251,32,131,251,32,131,251,64,131,251,
    32,131,251,32,131,251,32,131,251,32,131,251,32,131,251,64,139,251,128,147,251,64,131,251,
    32,131,251,32,131,251,64,131,251,96,98,250,0,0,249,0,0,245,0,0,33,0,0,0,
    0,0,0,0,0,183,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,
    0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,
    0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,
    0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,183,0,0,0,
    0,0,22,0,0,157,0,0,158,0,0,159,0,0,159,32,0,159,32,0,159,32,0,159,
    32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,
    32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,
    32,0,159,32,0,159,32,0,159,0,0,159,0,0,159,0,0,159,0,0,157,0,0,22,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  },
  { // 25 注意
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,60,
    0,0,78,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,197,
    0,0,228,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,72,0,0,255,
    0,0,255,0,0,99,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,210,0,0,255,
    0,0,253,0,0,229,0,0,13,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,0,100,0,0,255,128,57,252,
    224,81,251,0,0,255,0,0,129,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,15,0,0,231,0,0,253,192,196,253,
    128,229,254,0,0,253,0,0,244,0,8,28,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,32,0,133,0,0,255,192,73,252,96,254,255,
    160,254,255,128,106,251,0,0,255,32,0,158,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,8,30,0,0,245,32,0,253,32,213,254,96,254,255,
    64,254,255,160,229,254,64,8,253,0,0,250,32,0,45,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,32,0,162,0,0,255,128,106,251,192,254,255,128,254,255,
    128,254,255,192,254,255,32,131,252,0,0,255,32,0,183,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,32,0,51,0,0,254,96,8,253,192,229,255,32,246,255,64,98,255,
    96,98,255,32,246,255,32,246,255,160,24,252,0,0,255,0,0,69,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,32,0,189,0,0,255,32,131,252,32,255,255,0,164,255,0,0,255,
    0,0,255,224,163,255,0,255,255,192,155,252,0,0,254,0,0,207,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,32,0,76,0,0,255,192,32,252,0,246,255,128,254,255,0,205,255,32,0,255,
    0,0,255,224,155,255,160,254,255,64,254,255,32,49,252,0,0,255,32,8,94,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,213,0,0,255,0,164,252,160,254,255,64,254,255,64,221,255,0,0,255,
    0,0,255,224,163,255,128,254,255,128,254,255,96,180,253,0,0,254,0,0,225,0,0,10,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    32,0,104,0,0,255,64,49,252,96,254,255,32,254,255,96,254,255,64,221,255,0,0,255,
    0,0,255,224,163,255,160,254,255,0,254,255,128,254,255,160,65,252,0,0,255,32,0,122,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,17,
    0,0,234,0,0,253,160,196,253,128,254,255,32,254,255,96,254,255,64,221,255,0,0,255,
    0,0,255,224,163,255,160,254,255,0,254,255,128,254,255,0,205,254,0,0,253,0,0,240,
    0,8,24,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,0,137,
    0,0,255,0,82,251,160,254,255,32,254,255,32,254,255,96,254,255,64,221,255,0,0,255,
    0,0,255,224,163,255,160,254,255,32,254,255,32,254,255,160,254,255,64,98,252,0,0,255,
    32,0,151,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,0,33,0,0,245,
    32,0,253,64,221,254,96,254,255,32,254,255,32,254,255,96,254,255,64,221,255,0,0,255,
    0,0,255,224,163,255,160,254,255,32,254,255,32,254,255,96,254,255,128,229,254,64,8,253,
    0,0,251,32,0,40,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,0,165,0,0,255,
    192,114,251,160,254,255,32,254,255,64,254,255,32,254,255,96,254,255,64,221,255,32,0,255,
    0,0,255,224,163,255,160,254,255,32,254,255,64,254,255,32,254,255,160,254,255,0,123,252,
    0,0,255,32,0,176,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,8,53,0,0,255,96,16,253,
    192,237,255,96,254,255,32,254,255,64,254,255,32,254,255,160,254,255,0,164,255,0,0,255,
    0,0,255,192,155,255,160,254,255,32,254,255,64,254,255,32,254,255,64,254,255,224,237,255,
    128,24,252,0,0,255,32,8,63,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,192,0,0,255,96,139,252,
    160,254,255,32,254,255,64,254,255,64,254,255,32,254,255,96,254,255,128,221,255,96,57,255,
    96,57,255,128,229,255,96,254,255,32,254,255,64,254,255,64,254,255,32,254,255,160,254,255,
    160,155,252,0,0,255,0,0,201,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,32,0,79,0,0,255,224,32,252,64,254,255,
    64,254,255,64,254,255,64,254,255,64,254,255,64,254,255,32,254,255,160,254,255,128,188,255,
    64,172,255,128,254,255,32,254,255,64,254,255,64,254,255,64,254,255,64,254,255,64,254,255,
    64,254,255,0,41,252,0,0,255,32,8,89,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,215,0,0,254,32,172,253,160,254,255,
    32,254,255,64,254,255,64,254,255,64,254,255,32,254,255,128,254,255,64,172,255,0,0,255,
    0,0,255,128,147,255,160,254,255,32,254,255,64,254,255,64,254,255,64,254,255,32,254,255,
    160,254,255,96,180,253,0,0,254,0,0,220,0,0,8,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,32,0,108,0,0,255,96,57,252,128,254,255,32,254,255,
    64,254,255,64,254,255,64,254,255,64,254,255,32,254,255,160,254,255,64,139,255,0,0,255,
    0,0,255,128,98,255,160,254,255,32,254,255,64,254,255,64,254,255,64,254,255,64,254,255,
    32,254,255,128,254,255,128,65,252,0,0,255,32,0,114,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,18,0,0,236,0,0,254,192,196,253,96,254,255,0,254,255,
    32,254,255,32,254,255,32,254,255,32,254,255,32,254,255,32,254,255,224,237,255,64,90,255,
    0,82,255,160,229,255,64,254,255,32,254,255,32,254,255,32,254,255,32,254,255,32,254,255,
    0,254,255,96,254,255,192,196,253,0,0,253,0,0,237,0,0,20,0,0,0,0,0,0,
    0,0,0,0,0,0,32,0,141,0,0,255,160,106,251,32,255,255,160,254,255,160,254,255,
    160,254,255,160,254,255,160,254,255,160,254,255,160,254,255,160,254,255,192,254,255,0,255,255,
    0,255,255,192,254,255,160,254,255,160,254,255,160,254,255,160,254,255,160,254,255,160,254,255,
    160,254,255,160,254,255,32,255,255,160,106,251,0,0,255,32,0,145,0,0,0,0,0,0,
    0,0,0,0,0,31,0,0,243,0,0,249,96,98,250,64,131,251,32,131,251,32,131,251,
    32,131,251,32,131,251,32,131,251,32,131,251,32,131,251,32,131,251,32,131,251,0,123,251,
    0,123,251,32,131,251,32,131,251,32,131,251,32,131,251,32,131,251,32,131,251,32,131,251,
    32,131,251,32,131,251,64,131,251,96,98,250,0,0,249,0,0,245,0,0,33,0,0,0,
    0,0,0,0,0,183,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,
    0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,
    0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,
    0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,183,0,0,0,
    0,0,22,0,0,157,0,0,158,0,0,159,0,0,159,32,0,159,32,0,159,32,0,159,
    32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,
    32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,32,0,159,
    32,0,159,32,0,159,32,0,159,0,0,159,0,0,159,0,0,159,0,0,157,0,0,22,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  },
};
#define CAMM_S 20
const uint8_t CAM_MARK[CAMM_S * CAMM_S * 3] = {
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,23,
  0,0,68,0,0,98,0,0,100,0,0,70,0,0,26,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,58,0,0,167,0,0,233,133,0,255,232,8,255,232,8,255,133,0,255,
  0,0,236,0,0,174,0,0,67,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,140,33,0,255,175,17,255,218,34,255,
  94,43,254,127,43,253,127,43,253,94,43,254,218,34,255,175,17,255,33,0,255,0,0,154,
  45,1,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,173,
  166,8,255,250,34,253,159,43,252,126,43,254,94,43,255,93,43,255,93,43,255,94,43,255,
  94,43,254,159,43,252,250,34,252,166,8,255,0,0,189,45,1,7,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,140,166,8,255,94,43,249,127,43,255,61,35,255,253,26,255,
  253,26,255,253,26,255,62,35,255,94,43,255,93,43,255,61,43,255,127,43,255,94,43,249,
  166,8,255,0,0,160,0,0,0,0,0,0,0,0,0,32,0,54,33,0,255,251,34,251,
  127,43,254,61,35,255,94,43,255,31,166,255,127,190,255,127,182,255,30,76,255,253,26,255,
  62,35,255,30,35,255,61,43,255,127,43,255,251,34,250,33,0,255,0,0,71,0,0,0,
  0,0,0,0,0,170,142,17,254,159,43,251,61,35,255,30,27,255,62,84,255,255,255,255,
  255,255,255,255,255,255,191,149,255,94,43,255,222,67,255,254,67,255,62,35,255,29,35,255,
  159,43,252,142,17,254,0,0,189,0,0,0,1,0,22,0,0,237,217,34,255,126,43,254,
  61,35,255,63,174,255,223,247,255,223,255,255,255,255,255,127,231,255,95,231,255,255,255,255,
  159,239,255,223,206,255,127,190,255,93,43,255,94,43,255,217,34,255,0,0,246,0,0,36,
  0,0,69,132,0,255,94,43,254,61,35,255,222,67,255,255,255,255,255,255,255,255,255,255,
  94,133,255,62,35,255,62,35,255,158,141,255,31,215,255,30,76,255,255,255,255,30,76,255,
  29,27,255,94,43,253,132,0,255,0,0,89,0,0,100,232,8,255,127,43,252,29,35,255,
  222,67,255,223,247,255,255,255,255,127,182,255,253,26,255,127,190,255,127,182,255,253,26,255,
  95,182,255,191,247,255,255,255,255,30,76,255,29,27,255,159,43,252,232,8,255,0,0,121,
  0,0,101,232,8,255,127,43,252,29,35,255,222,67,255,223,247,255,255,255,255,94,141,255,
  190,59,255,255,255,255,255,255,255,190,59,255,126,141,255,255,255,255,255,255,255,30,76,255,
  29,27,255,159,43,252,232,8,255,0,0,122,0,0,72,132,0,255,94,43,254,29,35,255,
  222,67,255,223,247,255,255,255,255,255,214,255,253,26,255,222,116,255,222,116,255,253,26,255,
  255,214,255,255,255,255,255,255,255,62,84,255,29,27,255,94,43,253,132,0,255,0,0,93,
  1,0,25,0,0,240,217,34,255,94,35,254,190,59,255,255,255,255,255,255,255,255,255,255,
  223,198,255,94,84,255,94,84,255,223,198,255,255,255,255,255,255,255,255,255,255,254,67,255,
  62,35,255,217,34,255,0,0,248,0,0,40,0,0,0,0,0,178,174,17,254,159,43,251,
  29,35,255,94,133,255,31,215,255,31,215,255,95,231,255,95,231,255,95,231,255,95,231,255,
  31,215,255,31,215,255,126,141,255,29,35,255,159,43,252,174,17,254,0,0,196,0,0,0,
  0,0,0,32,0,63,33,0,255,251,34,251,127,43,255,253,26,255,62,35,255,62,35,255,
  62,35,255,62,35,255,62,35,255,62,35,255,62,35,255,62,35,255,253,26,255,127,43,255,
  251,34,250,33,0,255,0,0,80,0,0,0,0,0,0,0,0,0,0,0,155,166,8,255,
  94,43,249,127,43,255,61,43,255,93,43,255,94,43,255,94,43,255,94,43,255,94,43,255,
  93,43,255,61,43,255,127,43,255,94,43,249,166,8,255,0,0,173,0,0,0,0,0,0,
  0,0,0,0,0,0,79,1,6,0,0,189,166,8,255,250,34,252,159,43,252,94,43,254,
  94,43,255,61,43,255,61,43,255,94,43,255,94,43,254,159,43,252,251,34,251,166,8,255,
  0,0,203,135,0,13,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,235,0,8,
  0,0,158,33,0,255,175,17,255,218,34,255,94,43,253,127,43,252,127,43,252,94,43,253,
  218,34,255,175,17,255,33,0,255,0,0,171,134,0,15,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,74,0,0,185,0,0,243,
  133,0,255,232,8,255,232,8,255,133,0,255,0,0,246,0,0,191,0,0,84,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,37,0,0,88,0,0,119,0,0,121,0,0,90,
  0,0,41,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};

#define CAMI_SPEED 2          // 测速：有限速值时画成带数字的限速牌

// 把一张 n×n 的图标（每像素 RGB565 + 透明度）画在 (中心 x, 中心 y)，按透明度和地图混合
void blitRGBA(const uint8_t *src, int n, int cx, int cy) {
  uint16_t *fb = gfx->getFramebuffer();
  if (!fb) return;
  int x0 = cx - n / 2, y0 = cy - n / 2;
  for (int j = 0; j < n; j++) {
    int y = y0 + j;
    if (y < 0 || y >= LCD_HEIGHT) continue;
    uint16_t *row = fb + y * LCD_WIDTH;
    const uint8_t *p = src + j * n * 3;
    for (int i = 0; i < n; i++, p += 3) {
      uint8_t a = p[2];
      if (!a) continue;
      int x = x0 + i;
      if (x < 0 || x >= LCD_WIDTH) continue;
      uint16_t c = p[0] | (p[1] << 8);
      row[x] = a >= 250 ? c : blend565(row[x], c, a);
    }
  }
}
void blitCamIcon(int id, int cx, int cy) {
  if (id < 0 || id >= CAMI_N) id = 0;
  blitRGBA(CAM_ICONS[id], CAMI_S, cx, cy);
}

// 画一个电子眼：测速且有限速值 = 红圈限速牌（数字是实时的）；其他 = 对应类型的图标
void drawOneCam(int i, float x, float y) {
  if (mapCamTyp[i] == CAMI_SPEED && mapCamSpd[i] > 0) {
    aaDisc(x, y, 15, C_BLACK);
    aaDisc(x, y, 13.6f, C_SIGNRED);
    aaDisc(x, y, 10.2f, C_WHITE);
    String t = String(mapCamSpd[i]);
    printCentered(t, (int)x + (t.length() == 3 ? 0 : 1), (int)y - 3, 1, C_BLACK);
  } else {
    blitCamIcon(mapCamTyp[i], (int)lroundf(x), (int)lroundf(y));
  }
}

static float angDiff(float a, float b) {
  float d = a - b;
  while (d > 180) d -= 360;
  while (d < -180) d += 360;
  return fabsf(d);
}

// v4.1 世界坐标 (wx, wy) 离导航路线最近多远（米）；ux, uy 返回最近那一段路的方向（单位向量）。
//      只看外框在 maxD 以内的路段组，算得很快；没有路线返回很大的数
float routeDistW(float wx, float wy, float maxD, float &ux, float &uy) {
  float best = 1e9f;
  ux = 0; uy = 1;
  if (routeN < 2 || routeFrame != posFrame) return best;
  for (int bi = 0; bi * RB_STEP < routeN - 1; bi++) {
    const int32_t *bb = rbBox + 4 * bi;
    if (wx < bb[0] - maxD || wx > bb[2] + maxD || wy < bb[1] - maxD || wy > bb[3] + maxD) continue;
    int s0 = bi * RB_STEP, e0 = min(routeN - 1, s0 + RB_STEP);
    for (int k = s0; k < e0; k++) {
      float ax = routeX[k], ay = routeY[k], dx = routeX[k + 1] - ax, dy = routeY[k + 1] - ay;
      float l2 = dx * dx + dy * dy;
      float t = l2 > 0 ? ((wx - ax) * dx + (wy - ay) * dy) / l2 : 0;
      t = constrain(t, 0.0f, 1.0f);
      float ex = ax + t * dx - wx, ey = ay + t * dy - wy;
      float d = sqrtf(ex * ex + ey * ey);
      if (d < best && l2 > 0) { best = d; float l = sqrtf(l2); ux = dx / l; uy = dy / l; }
    }
  }
  return best;
}

// v3.8 → v4.1 画所有电子眼：
//   1. 路上（电子眼的实际位置）画一个蓝色小摄像头；挨得很近的几个只画一个
//   2. 类型图标（测速牌、闯红灯、违章拍照……）不放在路上，而是摆在小摄像头的旁边：
//      优先垂直于道路的两侧，图标之间、图标和路线/红绿灯/车速等都不重叠；普通监控只有路上的小摄像头
//   3. 位置选好就锁住不动；转了弯（地图朝向和锁定时差 15 度以上）并且已经转稳了，才重新选一次
void drawCamIcons(float cx, float cy, float sc, float cs, float sn) {
  bool turning = angDiff(viewTargetHdg, viewHdg) > 4 || fabsf(hdgRate) > 8;   // 地图还在跟着转（v6.7 含正在转弯）
  const float MKR = 10, R = 15;                                           // 小摄像头半径、类型图标半径
  const float D = MKR + 3 + R;                                            // 类型图标离小摄像头中心的距离（边缘相隔 3 像素）
  // ---- 1. 路上的小摄像头 ----
  float mkX[MCAM_MAX], mkY[MCAM_MAX];
  int mkN = 0, camMk[MCAM_MAX];
  for (int i = 0; i < mapCamN && i < MCAM_MAX; i++) {
    camMk[i] = -1;
    if (!camShown[i]) continue;
    for (int q = 0; q < mkN; q++)
      if (fabsf(mkX[q] - camPx[i]) < 9 && fabsf(mkY[q] - camPy[i]) < 9) { camMk[i] = q; break; }
    if (camMk[i] < 0) { mkX[mkN] = camPx[i]; mkY[mkN] = camPy[i]; camMk[i] = mkN++; }
  }
  // v4.7 小摄像头和类型图标都是圆的：彼此之间按圆来判断重叠（以前按方框算，斜着放时会误判成“挡住了”，
  //      结果图标被推到远处、拉一根长线）
  float ciX[3 * MCAM_MAX], ciY[3 * MCAM_MAX], ciR[3 * MCAM_MAX];
  int ciN = 0;
  for (int q = 0; q < mkN; q++) {
    bool cam = false;                                                    // v4.8 这一处有没有电子眼（道路安全提示不算）
    for (int i = 0; i < mapCamN && i < MCAM_MAX; i++) if (camMk[i] == q && mapCamTyp[i] < 16) cam = true;
    if (cam) {
      blitRGBA(CAM_MARK, CAMM_S, (int)lroundf(mkX[q]), (int)lroundf(mkY[q]));
      ciX[ciN] = mkX[q]; ciY[ciN] = mkY[q]; ciR[ciN] = MKR; ciN++;
    } else {                                                             // 只有安全提示：路上画个小黄点
      aaDisc(mkX[q], mkY[q], 4.5f, C_BLACK);
      aaDisc(mkX[q], mkY[q], 3, 0xFE40);
      ciX[ciN] = mkX[q]; ciY[ciN] = mkY[q]; ciR[ciN] = 5; ciN++;
    }
  }
  // ---- 2. 旁边的类型图标：v4.3 同一处的几个排成一列（和道路平行），整列放在道路的一侧 ----
  int clN[MCAM_MAX], clCam[MCAM_MAX][MCAM_MAX];
  for (int q = 0; q < mkN; q++) clN[q] = 0;
  for (int i = 0; i < mapCamN && i < MCAM_MAX; i++) {
    if (camMk[i] < 0) continue;
    int t = mapCamTyp[i];
    if (t == 0 || t == 1) continue;                                      // 普通监控：路上的小摄像头就够了
    int q = camMk[i];
    bool dup = false;                                                    // 同一处同一种只标一次
    for (int j = 0; j < clN[q]; j++) if (mapCamTyp[clCam[q][j]] == t) { dup = true; break; }
    if (!dup) clCam[q][clN[q]++] = i;
  }
  // 每一列里按类型排序，顺序固定
  for (int q = 0; q < mkN; q++)
    for (int a = 0; a < clN[q]; a++)
      for (int b = a + 1; b < clN[q]; b++)
        if (mapCamTyp[clCam[q][b]] < mapCamTyp[clCam[q][a]]) { int t = clCam[q][a]; clCam[q][a] = clCam[q][b]; clCam[q][b] = t; }
  // 锁住：这一处的每个电子眼都放过、个数没变、地图没转过弯
  bool keep[MCAM_MAX];
  int order[MCAM_MAX], n = 0;
  for (int q = 0; q < mkN; q++) {
    if (clN[q] == 0) continue;
    bool k = true;
    for (int j = 0; j < clN[q]; j++) {
      int i = clCam[q][j];
      if (mapCamPref[i] <= 0 || mapCamPrefK[i] != clN[q] || !(turning || angDiff(viewHdg, mapCamLockH[i]) < 15)) k = false;
    }
    keep[q] = k;
    order[n++] = q;
  }
  for (int a = 0; a < n; a++)                                            // 锁住的在前，再按离车头的距离
    for (int b = a + 1; b < n; b++) {
      int qa = order[a], qb = order[b];
      float da = fabsf(mkX[qa] - CX) + fabsf(mkY[qa] - MAP_PY) + (keep[qa] ? 0 : 10000);
      float db = fabsf(mkX[qb] - CX) + fabsf(mkY[qb] - MAP_PY) + (keep[qb] ? 0 : 10000);
      if (db < da) { order[a] = qb; order[b] = qa; }
    }
  const float GAP = 2 * R + 3;                                           // 一列里相邻两个图标的间距
  for (int o = 0; o < n; o++) {
    int q = order[o], cnt = clN[q], i0 = clCam[q][0];
    float ax = mkX[q], ay = mkY[q];
    float A = mapCamPrefA[i0];
    int lvl = mapCamPref[i0];
    if (!keep[q]) {
      // 道路在屏幕上的方向（没有路线就当作竖直向上）
      float sdx = 0, sdy = -1, ux, uy;
      if (routeDistW(mapCamX[i0], mapCamY[i0], 60, ux, uy) < 60) { sdx = ux * cs - uy * sn; sdy = -(ux * sn + uy * cs); }
      float perp = atan2f(sdy, sdx) + PI / 2;
      // 候选：道路右侧/左侧，先贴近，再远一列
      long bestScore = 0x7FFFFFFF; float bestA = perp; int bestL = 1;
      for (int c = 0; c < 4; c++) {
        float cA = perp + ((c & 1) ? PI : 0);
        int cL = c < 2 ? 1 : 2;
        float d = cL == 1 ? D : D + GAP;
        float px = cosf(cA), py = sinf(cA), tx = -py, ty = px;
        long score = 0;
        for (int j = 0; j < cnt; j++) {
          float off = (j - (cnt - 1) / 2.0f) * GAP;
          float x = ax + px * d + tx * off, y = ay + py * d + ty * off;
          float ex = x - CX, ey = y - CX;
          if (ex * ex + ey * ey > 186.0f * 186.0f) score += 100000;      // 要整个在雷达里
          score += obsOverlap((int)(x - R - 2), (int)(y - R - 2), (int)(x + R + 2), (int)(y + R + 2));
          for (int m = 0; m < ciN; m++) {                                // 和小摄像头、别的图标：按圆算
            float ddx = x - ciX[m], ddy = y - ciY[m], need = R + ciR[m] + 2;
            float dd2 = ddx * ddx + ddy * ddy;
            if (dd2 < need * need) score += (long)((need - sqrtf(dd2)) * 60) + 1;
          }
          float sx = x - CX, sy = MAP_PY - y, rux, ruy;
          float wx = cx + (sx * cs + sy * sn) / sc, wy = cy + (-sx * sn + sy * cs) / sc;
          if (routeDistW(wx, wy, (R + 4) / sc, rux, ruy) < (R + 4) / sc) score += 600;   // 别压在路线上
        }
        if (score < bestScore) { bestScore = score; bestA = cA; bestL = cL; }
        if (score == 0) break;
      }
      A = bestA; lvl = bestL;
      for (int j = 0; j < cnt; j++) {                                    // 整列一起锁住
        int i = clCam[q][j];
        mapCamPref[i] = lvl; mapCamPrefA[i] = A; mapCamPrefK[i] = cnt; mapCamLockH[i] = viewHdg;
      }
    }
    float d = lvl == 2 ? D + GAP : D;
    float px = cosf(A), py = sinf(A), tx = -py, ty = px;
    if (lvl == 2) {                                                      // 远一列时，细线连到小摄像头
      gfx->drawLine((int)(ax + px * (MKR + 1)), (int)(ay + py * (MKR + 1)), (int)(ax + px * (d - R)), (int)(ay + py * (d - R)), C_BLACK);
    }
    for (int j = 0; j < cnt; j++) {
      float off = (j - (cnt - 1) / 2.0f) * GAP;
      float x = ax + px * d + tx * off, y = ay + py * d + ty * off;
      drawOneCam(clCam[q][j], x, y);
      if (ciN < 3 * MCAM_MAX) { ciX[ciN] = x; ciY[ciN] = y; ciR[ciN] = R; ciN++; }
    }
  }
}

// v3.2 红绿灯直接画在地图上的实际位置（图标始终竖直）；已经开过的不画；
//      下一个红绿灯在 300 米内时，旁边标出距离
void drawMapLights(float cx, float cy, float sc, float cs, float sn) {
  if (lightN == 0 || routeFrame != posFrame) return;
  bool first = true;
  for (int i = 0; i < lightN; i++) {
    if (routeProg >= 0 && lightU[i] <= routeProg + 0.02f) continue;     // 已经过了
    float dx = lightX[i] - cx, dy = lightY[i] - cy;
    float px = CX + sc * (dx * cs - dy * sn), py = MAP_PY - sc * (dx * sn + dy * cs);
    bool isNext = first;
    first = false;
    float ex = px - CX, ey = py - CX;
    if (ex * ex + ey * ey > 196.0f * 196.0f) continue;                  // 雷达外面不画
    drawTrafficLight(px, py - 17, 30);
    obsAdd(px - 9, py - 34, px + 9, py);                                // v3.8 电子眼图标别压住红绿灯
    if (isNext && lightDist >= 0) {
      String t = String(lightDist) + "m";
      printOutlined(t, (int)px + 36, (int)py - 25, 2, C_WHITE);
      float tw = t.length() * 12;
      obsAdd(px + 36 - tw / 2 - 3, py - 27, px + 36 + tw / 2 + 3, py - 7);
    }
  }
}

// 转向箭头 + 距离（浮在底部）；离路口 150 米内变大，车道放在半透明深色底上
void drawMapBadge() {
  if (cruiseOn) {                                      // v4.8 巡航：底部只写个“巡航”标记
    printOutlined("CRUISE", CX, 392, 2, C_CYAN);
    return;
  }
  bool isS = navDir == "S";
  bool far = !isS && navDist > STAGE_FAR_M;           // v2.4 远处：先显示直行
  bool nearJ = !isS && navDist <= MAP_NEAR_M;
  bool veryNear = !isS && navDist <= STAGE_NEAR_M;
  uint16_t col = veryNear ? C_ORANGE : C_WHITE;
  String dt = distText(navDist);
  iconHalo = true;
  if (!nearJ) {
    int up = tripOn ? 24 : 0;                          // v3.1 有行程信息时整体上移，下面留一行
    drawBigIconAt((isS || far) ? String("S") : navDir, C_WHITE, 395 - up, 1.0f, CX - 48, 368 - up, 418 - up, 60);
    int sz = dt.length() <= 4 ? 4 : 3;
    printOutlined(dt, CX + 46, (sz == 4 ? 378 : 382) - up, sz, C_WHITE);
    if (tripOn) drawTripLine(404);
  } else {
    const int top = 344;                               // v3.3 车道移到了上面，底部只放箭头和距离
    drawBigIconAt(navDir, col, top + 30, 1.0f, CX - 58, top, top + 60, 84);
    printOutlined(dt, CX + 54, top + 14, dt.length() <= 4 ? 5 : 4, col);
  }
  iconHalo = false;
}

// ---- v2.9 GTA 雷达元素 ----

// 雷达边缘渐暗：半径 r0~r1 之间越往外越暗（最多保留 maxDark/255 的亮度损失）
// v5.7 渐暗查表：每项 = 像素位置 << 8 | 保留亮度（0~255）
uint32_t *vigLut = nullptr;
int vigN = 0;
float vigR0 = -1, vigR1 = -1; int vigMax = -1;

bool buildVignette(float r0, float r1, int maxDark) {
  int cap = (int)(PI * (r1 * r1 - r0 * r0)) + 16 * (int)r1 + 64;
  if (!vigLut) vigLut = (uint32_t *)ps_malloc(sizeof(uint32_t) * cap);
  if (!vigLut) return false;
  vigN = 0;
  float ro2 = r1 * r1;
  for (int y = (int)(CX - r1); y <= (int)(CX + r1); y++) {
    if (y < 0 || y >= LCD_HEIGHT) continue;
    float dy = y + 0.5f - CX, dy2 = dy * dy;
    if (dy2 >= ro2) continue;
    for (int x = max(0, (int)(CX - r1) - 1); x <= min(LCD_WIDTH - 1, (int)(CX + r1) + 1); x++) {
      float dx = x + 0.5f - CX;
      float r = sqrtf(dx * dx + dy2);
      if (r < r0 || r > r1) continue;
      float k = (r - r0) / (r1 - r0);
      int dark = (int)(k * k * maxDark);
      if (dark <= 0 || vigN >= cap) continue;
      vigLut[vigN++] = ((uint32_t)(y * LCD_WIDTH + x) << 8) | (uint32_t)(255 - dark);
    }
  }
  vigR0 = r0; vigR1 = r1; vigMax = maxDark;
  return true;
}

void radarVignette(float r0, float r1, int maxDark) {
  uint16_t *fb = gfx->getFramebuffer();
  if (!fb) return;
  if ((vigR0 == r0 && vigR1 == r1 && vigMax == maxDark) || buildVignette(r0, r1, maxDark)) {
    for (int i = 0; i < vigN; i++) {                     // 查表：位置 + 保留多少亮度
      uint32_t e = vigLut[i];
      uint16_t &px = fb[e >> 8];
      uint32_t k = (e & 0xFF) + 1, c = px;
      px = (uint16_t)((((c >> 11) * k >> 8) << 11) | ((((c >> 5) & 63) * k >> 8) << 5) | ((c & 31) * k >> 8));
    }
    return;
  }
  // 内存不够时的老办法
  float ro2 = r1 * r1, ri2 = r0 * r0;
  for (int y = (int)(CX - r1); y <= (int)(CX + r1); y++) {
    if (y < 0 || y >= LCD_HEIGHT) continue;
    float dy = y + 0.5f - CX, dy2 = dy * dy;
    if (dy2 >= ro2) continue;
    float ho = sqrtf(ro2 - dy2), hi = dy2 < ri2 ? sqrtf(ri2 - dy2) : -1;
    uint16_t *row = fb + y * LCD_WIDTH;
    for (int side = 0; side < 2; side++) {
      int xa, xb;
      if (hi < 0) { if (side) break; xa = (int)(CX - ho); xb = (int)(CX + ho); }
      else if (side == 0) { xa = (int)(CX - ho); xb = (int)(CX - hi) + 1; }
      else { xa = (int)(CX + hi); xb = (int)(CX + ho) + 1; }
      xa = max(xa, 0); xb = min(xb, LCD_WIDTH - 1);
      for (int x = xa; x <= xb; x++) {
        float dx = x + 0.5f - CX;
        float r = sqrtf(dx * dx + dy2);
        if (r < r0 || r > r1) continue;
        float k = (r - r0) / (r1 - r0);
        int dark = (int)(k * k * maxDark);
        if (dark > 0) row[x] = blend565(row[x], C_BLACK, dark);
      }
    }
  }
}

// 北向标记：雷达边缘的黑底白色 “N”，位置随车头方向转动
void drawNorth(float hdgDeg) {
  float a = hdgDeg * PI / 180;
  float nx = CX - sinf(a) * 196, ny = CX - cosf(a) * 196;
  obsAdd(nx - 14, ny - 14, nx + 14, ny + 14);                          // v3.8
  aaDisc(nx, ny, 13, C_BLACK);
  aaDisc(nx, ny, 11, 0x2945);
  printCentered("N", (int)nx + 1, (int)ny - 7, 2, C_WHITE);
}

// 目的地标记：在雷达里就画在实际位置，在外面就贴着边缘指出方向（和游戏里的标记一样）
void drawDestBlip(float cx, float cy, float sc, float cs, float sn) {
  if (routeN < 2 || routeFrame != posFrame) return;
  float dx = routeX[routeN - 1] - cx, dy = routeY[routeN - 1] - cy;
  float sx = sc * (dx * cs - dy * sn), sy = -sc * (dx * sn + dy * cs);   // 相对车头位置
  float px = CX + sx, py = MAP_PY + sy;
  float ex = px - CX, ey = py - CX, er = sqrtf(ex * ex + ey * ey);
  const float RMAX = styleIdx() == ST_DRAGON ? 183 : 192;
  bool outside = er > RMAX;
  if (outside) { px = CX + ex / er * RMAX; py = CX + ey / er * RMAX; }
  if (styleIdx() == ST_DRAGON) {
    if (outside) DragonRadar::placeEdgeBall(px, py, CX, RMAX,
      [](float x0, float y0, float x1, float y1) { return obsOverlap((int)x0, (int)y0, (int)x1, (int)y1); });
    DragonRadar::drawBall(gfx, (int)px, (int)py);
    obsAdd(px - 21, py - 21, px + 21, py + 21); return;
  }
  if (styleIdx() == ST_COMPASS) { drawCompassBlip(px, py, outside); obsAdd(px - 14, py - 14, px + 14, py + 14); return; }
  aaDisc(px, py, outside ? 9 : 11, C_BLACK);
  aaDisc(px, py, outside ? 7 : 9, TH.blip);
  obsAdd(px - 12, py - 12, px + 12, py + 12);                         // v3.8
  if (!outside) aaDisc(px, py, 3, C_BLACK);
}

// v5.0 路程（米）→ 路线上的位置（段号 + 段内比例）
float routeUAt(float m) {
  if (!routeCum || routeN < 2) return 0;
  if (m <= 0) return 0;
  if (m >= routeCum[routeN - 1]) return (float)(routeN - 1);
  int lo = 0, hi = routeN - 1;
  while (hi - lo > 1) { int mid = (lo + hi) / 2; if (routeCum[mid] <= m) lo = mid; else hi = mid; }
  float seg = routeCum[hi] - routeCum[lo];
  return lo + (seg > 0 ? (m - routeCum[lo]) / seg : 0);
}

// v5.0 路况颜色：缓行黄、拥堵红、严重拥堵深红，盖在导航路线上（已经走过的部分不画）
void drawTraffic(float cx, float cy, float sc, float cs, float sn, float viewR, float w0, float w1, float from) {
  if (trafN == 0 || routeN < 2 || routeFrame != posFrame) return;
  for (int i = 0; i < trafN; i++) {
    float a = fmaxf(routeUAt(trafA[i]), from), b = routeUAt(trafB[i]);
    if (b <= a) continue;
    uint16_t c = trafS[i] >= 4 ? 0x9000 : trafS[i] == 3 ? 0xE8E4 : 0xFDA0;
    drawRouteRange(a, b, cx, cy, sc, cs, sn, viewR, w0, w1, C_ROUTE2, c);
  }
}

// v5.0 地图上方/下方的提示条：半透明深色底 + 文字
void drawBanner(const String &t, int y, uint16_t col) {
  int w = t.length() * 12 + 24;
  dimRect(CX - w / 2, y - 6, w, 28, 13, 60);
  printCentered(t, CX, y, 2, col);
}

// v5.0 重新规划中 / 更快路线 / GPS 信号弱（地图模式和大箭头模式都用）
void drawNavHints(int bannerY) {
  unsigned long now = millis();
  if (rerouteAt && now - rerouteAt > 20000) rerouteAt = 0;        // 最多显示 20 秒
  if (fasterAt && now - fasterAt > 30000) fasterAt = 0;           // 更快路线提示 30 秒
  if (rerouteAt) {
    String dots = String("...").substring(0, 1 + (now / 400) % 3);
    drawBanner("REROUTING" + dots, bannerY, C_ORANGE);
  } else if (fasterAt) {
    drawBanner("FASTER ROUTE  -" + String(fasterMin) + "min", bannerY, C_GREEN);
  }
  if (gpsWeak) {                                                  // 左上角：橙色 “GPS” 加一条斜杠
    const int gx = CX - 128, gy = 150;
    aaDisc(gx, gy, 19, C_BLACK);
    aaDisc(gx, gy, 17, 0x4208);
    printCentered("GPS", gx + 1, gy - 7, 1, C_ORANGE);
    printCentered("GPS", gx, gy - 7, 1, C_ORANGE);
    gfx->drawLine(gx - 11, gy + 9, gx + 11, gy - 9, C_RED);
    gfx->drawLine(gx - 11, gy + 10, gx + 11, gy - 8, C_RED);
  }
}

// v3.8 顶部车速/测速牌/车道、底部箭头和行程信息、车头箭头占的地方（这些后画，电子眼图标要提前让开）
void addHudObstacles() {
  obsAdd(CX - 15, MAP_PY - 18, CX + 15, MAP_PY + 16);                 // 车头箭头
  if (speedKmh >= 0) {
    int sx = hasCam ? CX - 40 : CX;
    obsAdd(sx - 60, 36, sx + 60, 92);                                   // 车速数字 + km/h
    if (hasCam) obsAdd(CX + 52 - 34, 30, CX + 52 + 34, 98);             // 测速牌 + 距离
  }
  if (laneN > 0 && navDir != "S" && navDist <= MAP_NEAR_M) {           // 车道（和 drawMapLanes 一致）
    const int H = 30, MAXW = 210;
    int lw = (int)(fminf(36.0f * H / 32, (float)MAXW / laneN) * laneN);
    int y = hasCam ? 114 : 100;
    obsAdd(CX - lw / 2 - 9, y - 5, CX + lw / 2 + 9, y + H + 5);
  }
  obsAdd(0, 340, LCD_WIDTH, LCD_HEIGHT);                                // 底部箭头、距离、行程信息
}

// 画一整帧小地图
uint32_t mapFrames = 0, mapDrawMsAcc = 0, mapMsAcc = 0, ovlMsAcc = 0;
unsigned long statAt = 0;
String pendingStat = "";

// ---- v5.9 外圈和边缘渐暗：逐行、两个核心一起画 ----
static inline uint16_t blendF(uint16_t a, uint16_t b, uint32_t t) {   // t: 0~256，256 = 全是 b
  uint32_t ar = a >> 11, ag = (a >> 5) & 63, ab = a & 31;
  uint32_t br = b >> 11, bg = (b >> 5) & 63, bb = b & 31;
  uint32_t r = (ar * (256 - t) + br * t) >> 8, g = (ag * (256 - t) + bg * t) >> 8, bl = (ab * (256 - t) + bb * t) >> 8;
  return (uint16_t)((r << 11) | (g << 5) | bl);
}

// 快速 atan2（误差约 0.01°），返回 0~1：0 = 正上方，顺时针
static inline float ringAngle(float dx, float dy) {
  float ax = fabsf(dx), ay = fabsf(dy);
  float mx = fmaxf(ax, ay), mn = fminf(ax, ay);
  float a = mx > 0 ? mn / mx : 0, s2 = a * a;
  float r = ((-0.0464964749f * s2 + 0.15931422f) * s2 - 0.327622764f) * s2 * a + a;
  if (ay > ax) r = 1.57079637f - r;
  if (dx < 0) r = 3.14159265f - r;      // 这里的 “x” 是向上的分量，见下面调用
  if (dy < 0) r = -r;
  float t = r / 6.2831853f;
  return t < 0 ? t + 1 : t;
}

// 外圈参数
struct { uint16_t *fb; bool track; float prog; float ph; uint16_t pcol; bool over; float trip; } RP;   // v7.4 trip：行程环（-1 = 不画）
static inline float bandCov(float r, float rIn, float rOut) {
  float a = fminf(r - rIn + 0.5f, rOut - r + 0.5f);
  return a <= 0 ? 0 : (a >= 1 ? 1 : a);
}

// v6.0 整数版。坐标放大 2 倍取整：ix = 2x+1-W，iy = 2y+1-H，q = ix²+iy² = 4×半径²
#define RQ_MIN  (4 * 209 * 209)              // 半径 209 以内不用管
#define RQ_MAX  (4 * 235 * 235)              // 半径 235 以外整段清零
#define RQ_SH   4
uint16_t *rq2r = nullptr;                    // (q - RQ_MIN) >> 4 → 半径 × 64
int16_t ringInH[LCD_HEIGHT], ringOutH[LCD_HEIGHT];   // 每行：内圈半宽（以内跳过）、外圈半宽（以外清零）
float ringUx = 0, ringUy = -1;               // 进度条末端方向
float tripUx = 0, tripUy = -1;               // v7.4 行程环“走过 / 没走”交界方向

bool ringTablesInit() {
  if (rq2r) return true;
  int n = ((RQ_MAX - RQ_MIN) >> RQ_SH) + 2;
  rq2r = (uint16_t *)heap_caps_malloc(n * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!rq2r) return false;
  for (int k = 0; k < n; k++) rq2r[k] = (uint16_t)(sqrtf((float)(RQ_MIN + (k << RQ_SH) + 8)) * 32.0f);   // √q/2×64
  for (int y = 0; y < LCD_HEIGHT; y++) {
    int iy = 2 * y + 1 - LCD_HEIGHT;
    long iy2 = (long)iy * iy;
    int hin = 0, hout = 0;
    for (int x = LCD_WIDTH / 2; x < LCD_WIDTH; x++) {                // 右半边往外找
      int ix = 2 * x + 1 - LCD_WIDTH;
      long q = (long)ix * ix + iy2;
      if (q < RQ_MIN) hin = x - LCD_WIDTH / 2 + 1;
      if (q < RQ_MAX) hout = x - LCD_WIDTH / 2 + 1;
    }
    ringInH[y] = hin; ringOutH[y] = hout;
  }
  return true;
}

// 一段圆环的覆盖率（0~64）：r64 = 半径×64；带宽 [a, b]
static inline int bandCov64(int r64, int a, int b) {
  int c = min(r64 - (a * 64 - 32), (b * 64 + 32) - r64);
  return c <= 0 ? 0 : (c >= 64 ? 64 : c);
}

static inline void ringPixel(uint16_t *px, int ix, long iy2, int iy) {
  long q = (long)ix * ix + iy2;
  int k = (int)((q - RQ_MIN) >> RQ_SH);
  int r64 = rq2r[k < 0 ? 0 : k];
  uint16_t c = *px;
  int a = bandCov64(r64, 211, 240);
  if (a) c = a >= 64 ? C_BLACK : blendF(c, C_BLACK, a * 4);
  if (RP.track) { a = bandCov64(r64, RING_R - 5, RING_R + 5); if (a) c = a >= 64 ? C_TRACK : blendF(c, C_TRACK, a * 4); }
  if (RP.trip >= 0) {                                  // v7.4 行程环：走过的灰，没走的绿
    a = bandCov64(r64, RING_R - TRIP_H, RING_R + TRIP_H);
    if (a) {
      bool right = ix >= 0;
      bool before = (ix * tripUy - iy * tripUx) >= 0;
      bool done = RP.trip >= 0.9999f ? true : RP.trip <= 0.0001f ? false : (RP.trip <= 0.5f ? (right && before) : (right || before));
      uint16_t tc = done ? TRIP_DONE_C : TRIP_LEFT_C;
      c = a >= 64 ? tc : blendF(c, tc, a * 4);
    }
  }
  if (RP.prog > 0.002f) {
    int h = (int)RP.ph;
    a = bandCov64(r64, RING_R - h, RING_R + h);
    if (a) {
      // 在进度范围内吗：正上方顺时针转到末端方向 (ux, uy)
      bool right = ix >= 0;
      bool before = (ix * ringUy - iy * ringUx) >= 0;
      bool in = RP.prog >= 0.9999f ? true : (RP.prog <= 0.5f ? (right && before) : (right || before));
      if (in) c = a >= 64 ? RP.pcol : blendF(c, RP.pcol, a * 4);
    }
  }
  if (RP.over) { a = bandCov64(r64, 228, 233); if (a) c = a >= 64 ? C_RED : blendF(c, C_RED, a * 4); }
  *px = c;
}

void ringRows(int y0, int y1, int step) {
  uint16_t *fb = RP.fb;
  const int W = LCD_WIDTH, HW = LCD_WIDTH / 2;
  for (int y = y0; y < y1; y += step) {
    uint16_t *row = fb + y * W;
    int iy = 2 * y + 1 - LCD_HEIGHT;
    long iy2 = (long)iy * iy;
    int ho = ringOutH[y], hi = ringInH[y];
    // 圆外两头：整段清零（黑色 = 0）
    if (HW - ho > 0) memset(row, 0, (HW - ho) * 2);
    if (HW + ho < W) memset(row + HW + ho, 0, (W - HW - ho) * 2);
    // 左边一段 [HW-ho, HW-hi)，右边一段 [HW+hi, HW+ho)
    for (int x = HW - ho; x < HW - hi; x++) ringPixel(row + x, 2 * x + 1 - W, iy2, iy);
    for (int x = HW + hi; x < HW + ho; x++) ringPixel(row + x, 2 * x + 1 - W, iy2, iy);
  }
}

// 画外圈：黑边 + 进度环底（track）+ 进度条（prog 0~1，粗 thick）+ 超速红圈
void drawRingLayer(bool track, float prog, int thick, uint16_t pcol, bool over, float trip) {
  RP.fb = gfx->getFramebuffer();
  if (!RP.fb) return;
  RP.track = track; RP.prog = prog > 1 ? 1 : prog; RP.ph = thick / 2.0f; RP.pcol = pcol; RP.over = over;
  RP.trip = trip; tripUx = sinf(trip * 2 * PI); tripUy = -cosf(trip * 2 * PI);
  if (!ringTablesInit()) {                             // 内存不够：退回老办法
    fillRingBand(0, 1, 211, 240, C_BLACK);
    if (trip >= 0) drawTripRing(trip);
    if (track) drawRingTrack();
    if (prog > 0.002f) drawRingProgress(prog, thick, pcol);
    if (over) fillRingBand(0, 1, 228, 233, C_RED);
    return;
  }
  ringUx = sinf(RP.prog * 2 * PI); ringUy = -cosf(RP.prog * 2 * PI);
  parallelRows(ringRows, 0, LCD_HEIGHT);
  if (RP.prog > 0.002f) {                              // 进度条两头的圆头
    float h = RP.ph, a = RP.prog * 2 * PI;
    aaDisc(CX, CX - RING_R, h, pcol);
    aaDisc(CX + RING_R * sinf(a), CX - RING_R * cosf(a), h, pcol);
  }
  if (trip >= 0) drawTripDot(trip);
}

// 边缘渐暗：半径 r0~r1 之间越往外越暗；暗度按 “半径平方” 查表（表放在芯片内部内存）
uint8_t *vigKeep = nullptr;                // 下标 = 半径² - r0²，值 = 保留亮度（1~256 存成 0~255）
int vigK0 = 0, vigK1 = 0;
struct { uint16_t *fb; float r0, r1; } VP;

void vignetteRows(int y0, int y1, int step) {
  uint16_t *fb = VP.fb;
  float r0 = VP.r0, r1 = VP.r1, ri2 = r0 * r0, ro2 = r1 * r1;
  for (int y = y0; y < y1; y += step) {
    float dy = y + 0.5f - CX, dy2 = dy * dy;
    if (dy2 >= ro2) continue;
    float ho = sqrtf(ro2 - dy2), hi = dy2 < ri2 ? sqrtf(ri2 - dy2) : -1;
    uint16_t *row = fb + y * LCD_WIDTH;
    for (int side = 0; side < 2; side++) {
      int xa, xb;
      if (hi < 0) { if (side) break; xa = (int)(CX - ho); xb = (int)(CX + ho); }
      else if (side == 0) { xa = (int)(CX - ho); xb = (int)(CX - hi) + 1; }
      else { xa = (int)(CX + hi); xb = (int)(CX + ho) + 1; }
      xa = max(xa, 0); xb = min(xb, LCD_WIDTH - 1);
      for (int x = xa; x <= xb; x++) {
        float dx = x + 0.5f - CX;
        int k = (int)(dx * dx + dy2) - vigK0;
        if (k < 0 || k >= vigK1 - vigK0) continue;
        uint32_t keep = (uint32_t)vigKeep[k] + 1;
        if (keep >= 256) continue;
        uint32_t c = row[x];
        row[x] = (uint16_t)((((c >> 11) * keep >> 8) << 11) | ((((c >> 5) & 63) * keep >> 8) << 5) | ((c & 31) * keep >> 8));
      }
    }
  }
}

void drawVignette(float r0, float r1, int maxDark) {
  VP.fb = gfx->getFramebuffer();
  if (!VP.fb) return;
  if (!vigKeep || vigK0 != (int)(r0 * r0) || vigK1 != (int)(r1 * r1) + 1) {
    vigK0 = (int)(r0 * r0); vigK1 = (int)(r1 * r1) + 1;
    if (!vigKeep) vigKeep = (uint8_t *)heap_caps_malloc(vigK1 - vigK0 + 64, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!vigKeep) { radarVignette(r0, r1, maxDark); return; }
    for (int k = 0; k < vigK1 - vigK0; k++) {
      float r = sqrtf((float)(k + vigK0) + 0.5f);
      float t = (r - r0) / (r1 - r0);
      if (t < 0) t = 0;
      if (t > 1) t = 1;
      int dark = (int)(t * t * maxDark);
      vigKeep[k] = (uint8_t)(255 - dark);
    }
  }
  VP.r0 = r0; VP.r1 = r1;
  parallelRows(vignetteRows, (int)(CX - r1) - 1, (int)(CX + r1) + 2);
}

// ================= v6.1 风格特效 =================
static inline void projXY(float x, float y, float cx, float cy, float sc, float cs, float sn, float &px, float &py) {
  px = CX + sc * ((x - cx) * cs - (y - cy) * sn);
  py = MAP_PY - sc * ((x - cx) * sn + (y - cy) * cs);
}

// 蓝图：跟着地图走的网格，每 50 米一条线
void drawBlueprintGrid(float cx, float cy, float sc, float cs, float sn, float viewR) {
  const float G = 50;
  uint16_t col = nightOn ? 0x1A0F : 0x2AB2;
  float x0 = floorf((cx - viewR) / G) * G, y0 = floorf((cy - viewR) / G) * G;
  for (float g = x0; g <= cx + viewR; g += G) {
    float ax, ay, bx, by;
    projXY(g, cy - viewR, cx, cy, sc, cs, sn, ax, ay); projXY(g, cy + viewR, cx, cy, sc, cs, sn, bx, by);
    gfx->drawLine((int)ax, (int)ay, (int)bx, (int)by, col);
  }
  for (float g = y0; g <= cy + viewR; g += G) {
    float ax, ay, bx, by;
    projXY(cx - viewR, g, cx, cy, sc, cs, sn, ax, ay); projXY(cx + viewR, g, cx, cy, sc, cs, sn, bx, by);
    gfx->drawLine((int)ax, (int)ay, (int)bx, (int)by, col);
  }
}

// 雷达：三圈距离环 + 绕着车转的扫描线（后面拖一串渐暗的线）
void drawRadarSweep() {
  for (int r = 70; r <= 210; r += 70) gfx->drawCircle(CX, MAP_PY, r, 0x0A44);
  float a0 = (millis() % 3200) / 3200.0f * 2 * PI;
  const int TRAIL = 26;
  for (int k = TRAIL - 1; k >= 0; k--) {
    float a = a0 - k * 0.022f;
    uint16_t c = blendF(0x00C1, 0x8FF4, (uint32_t)((TRAIL - k) * 256 / TRAIL));
    gfx->drawLine(CX, MAP_PY, (int)(CX + sinf(a) * 300), (int)(MAP_PY - cosf(a) * 300), c);
  }
}

// 寻宝罗盘：屏幕上一层淡淡的六边形网格（开机第一次用时把网格上的像素位置算好存起来）
uint32_t *hexIdx = nullptr;
int hexN = 0;
bool hexTried = false;
void buildHexGrid() {
  hexTried = true;
  uint8_t *m = (uint8_t *)ps_malloc(LCD_WIDTH * LCD_HEIGHT);
  if (!m) return;
  memset(m, 0, LCD_WIDTH * LCD_HEIGHT);
  const float R = 26, H = R * 1.7320508f;
  auto line = [&](float x0, float y0, float x1, float y1) {
    int n = (int)(fmaxf(fabsf(x1 - x0), fabsf(y1 - y0))) + 1;
    for (int i = 0; i <= n; i++) {
      int x = (int)(x0 + (x1 - x0) * i / n), y = (int)(y0 + (y1 - y0) * i / n);
      if (x >= 0 && x < LCD_WIDTH && y >= 0 && y < LCD_HEIGHT) m[y * LCD_WIDTH + x] = 1;
    }
  };
  for (int col = -1; col * R * 1.5f < LCD_WIDTH + R; col++)
    for (int row = -1; row * H < LCD_HEIGHT + H; row++) {
      float cx = col * R * 1.5f, cy = row * H + ((col & 1) ? H / 2 : 0);
      for (int k = 0; k < 6; k++) {
        float a0 = k * PI / 3, a1 = (k + 1) * PI / 3;
        line(cx + R * cosf(a0), cy + R * sinf(a0), cx + R * cosf(a1), cy + R * sinf(a1));
      }
    }
  int cnt = 0;
  for (int i = 0; i < LCD_WIDTH * LCD_HEIGHT; i++) if (m[i]) cnt++;
  hexIdx = (uint32_t *)ps_malloc(sizeof(uint32_t) * (cnt + 1));
  if (hexIdx) {
    for (int y = 0; y < LCD_HEIGHT; y++) {
      float dy = y + 0.5f - CX;
      for (int x = 0; x < LCD_WIDTH; x++) {
        if (!m[y * LCD_WIDTH + x]) continue;
        float dx = x + 0.5f - CX;
        if (dx * dx + dy * dy < 206.0f * 206.0f) hexIdx[hexN++] = y * LCD_WIDTH + x;
      }
    }
  }
  free(m);
}

void drawHexGrid() {
  if (!hexTried) buildHexGrid();
  if (!hexIdx) return;
  uint16_t *fb = gfx->getFramebuffer();
  for (int i = 0; i < hexN; i++) { uint16_t &p = fb[hexIdx[i]]; p = blendF(p, 0x3A4F, 80); }
}

// 寻宝罗盘：外圈刻度（每 5° 一根，东南西北长一点）+ 方位字母，跟着车头转
void drawCompassRose(float hdgDeg) {
  for (int k = 0; k < 72; k++) {
    float a = (k * 5 - hdgDeg) * PI / 180;
    bool major = k % 18 == 0;
    float r0 = 208, r1 = major ? 192 : (k % 2 == 0 ? 199 : 203);
    float sx = sinf(a), cy = -cosf(a);
    uint16_t c = major ? 0xFD80 : 0x7B48;
    gfx->drawLine((int)(CX + sx * r0), (int)(CX + cy * r0), (int)(CX + sx * r1), (int)(CX + cy * r1), c);
    if (major) gfx->drawLine((int)(CX + sx * r0) + 1, (int)(CX + cy * r0), (int)(CX + sx * r1) + 1, (int)(CX + cy * r1), c);
  }
  const char *L[4] = {"N", "E", "S", "W"};
  for (int k = 0; k < 4; k++) {
    float a = (k * 90 - hdgDeg) * PI / 180;
    float x = CX + sinf(a) * 178, y = CX - cosf(a) * 178;
    if (k == 0) obsAdd(x - 10, y - 10, x + 10, y + 10);
    printOutlined(L[k], (int)x, (int)y - 7, 2, k == 0 ? 0xFD80 : 0x7B48);
  }
}

// 寻宝罗盘：目的地 = 琥珀色菱形 + 往外扩的脉冲圈
void drawCompassBlip(float px, float py, bool outside) {
  if (!outside) {
    float ph = (millis() % 1400) / 1400.0f;
    for (int k = 0; k < 2; k++) {
      float t = fmodf(ph + k * 0.5f, 1.0f);
      int r = (int)(12 + t * 20);
      uint16_t c = blendF(0xFD80, TH.block, (uint32_t)(t * 256));
      gfx->drawCircle((int)px, (int)py, r, c);
      gfx->drawCircle((int)px, (int)py, r + 1, c);
    }
  }
  float s = outside ? 8 : 11;
  gfx->fillTriangle(px, py - s - 2, px + s + 2, py, px - s - 2, py, C_BLACK);
  gfx->fillTriangle(px, py + s + 2, px + s + 2, py, px - s - 2, py, C_BLACK);
  gfx->fillTriangle(px, py - s, px + s, py, px - s, py, 0xFD80);
  gfx->fillTriangle(px, py + s, px + s, py, px - s, py, 0xFD80);
}

// v5.8 分步计时（微秒累计，每 2 秒发一次）
#define PROF_N 11
uint32_t profAcc[PROF_N];
uint32_t profT = 0;
static inline void profMark(int i) { uint32_t n = micros(); profAcc[i] += n - profT; profT = n; }

void drawMapFrame() {
  unsigned long t = millis();
  lastMapDraw = t;
  frameIsMap = true;
  // 位置：按车速和转弯速度往前推算（v6.7 沿弧线，再加一点提前量），让画面连续移动、少“慢半拍”
  float cx, cy, hPred;
  predictPose(t, leadMs / 1000.0f, cx, cy, hPred);
  viewTargetHdg = hPred;
  if (corrAt) {                                         // v2.6 平滑修正
    float f = fmaxf(0, 1 - (t - corrAt) / 400.0f);
    cx += corrX * f; cy += corrY * f;
  }
  // 朝向平滑转动，缩放跟车速（越快看得越远）
  if (!viewInit) { viewHdg = hPred; viewInit = true; }
  float dh = hPred - viewHdg;
  while (dh > 180) dh -= 360;
  while (dh < -180) dh += 360;
  // v3.7 按时间平滑转动（和帧率无关）：时间常数 0.35 秒，最快每秒转 70 度。
  //      每一帧之间画面变化更小、更均匀，转弯时的撕裂感会轻很多（这块板子没接屏幕的刷新同步信号，
  //      没法从硬件上完全消除撕裂，只能让相邻两帧尽量接近）
  static unsigned long lastViewAt = 0;
  float vdt = lastViewAt ? (t - lastViewAt) / 1000.0f : 0.05f;
  lastViewAt = t;
  if (vdt > 0.25f) vdt = 0.25f;
  float hstep = dh * (1.0f - expf(-vdt / 0.22f));       // v6.7 0.35 → 0.22 秒：追得更紧
  float hmax = fminf(70.0f * vdt, 3.5f);                // v6.0 每帧最多转 3.5°（防撕裂）
  if (hstep > hmax) hstep = hmax;
  if (hstep < -hmax) hstep = -hmax;
  viewHdg += hstep;
  const bool dragon = styleIdx() == ST_DRAGON;
  bool vecOK = !dragon && mapValid && mapFrame == posFrame;
  bool rasOK = !dragon && rasterValid && rasterFrame == posFrame;
  bool useRaster = rasOK && (!vecOK || rasterAt >= vectorAt);   // v2.5 两种都有时用最新收到的
  // 图片地图每像素 2 米，放太大会显得粗糙，所以最大放大倍数小一些
  float targetS = constrain(0.85f - posKmh * 0.006f, 0.38f, useRaster ? 0.6f : 0.85f);
  viewScale += (targetS - viewScale) * (1.0f - expf(-vdt / 1.2f));   // v3.7 缩放也按时间平滑
  float sc = viewScale, hs = viewHdg * PI / 180, cs = cosf(hs), sn = sinf(hs);
  float viewR = (CX + (MAP_PY - CX) + 30) / sc;

  float wk = sc / 0.6f;
  profT = micros();
  if (useRaster) drawRasterMap(cx, cy, sc, cs, sn);     // v2.5 天地图图片地图
  else gfx->fillScreen(C_BLOCK);
  profMark(0);
  // 地图块里的要素（文件里已按：水面、绿地、细路 → 粗路 排好序）
  float lx = cx - mapOx, ly = cy - mapOy;
  const float roadW[6] = {0, 15, 12, 10, 7, 5};
  for (int i = 0; i < ((dragon || useRaster) ? 0 : mapFeatN); i++) {
    const int16_t *bx = featBox + 4 * i;
    if (bx[2] < lx - viewR || bx[0] > lx + viewR || bx[3] < ly - viewR || bx[1] > ly + viewR) continue;
    int np = featNp[i];
    if (np > POLY_PTS) np = POLY_PTS;
    const uint8_t *pp = mapShow + featOff[i];
    for (int k = 0; k < np; k++) {
      float dx = rd16(pp + 4 * k) - lx, dy = rd16(pp + 4 * k + 2) - ly;
      tmpX[k] = CX + sc * (dx * cs - dy * sn);
      tmpY[k] = MAP_PY - sc * (dx * sn + dy * cs);
    }
    uint8_t cls = featCls[i];
    if (cls & 0x80) {
      mapFillPoly(tmpX, tmpY, np, (cls & 0x7F) == 11 ? C_WATER : C_PARK);
    } else {
      int c = cls & 0x7F;
      if (c < 1 || c > 5) c = 5;
      mapPolyline(tmpX, tmpY, np, fmaxf(2.0f, roadW[c] * wk), C_ROADM, false);
    }
  }
  {                                                     // v6.1 风格特效（画在路线下面）
    int st = styleIdx();
    if (st == ST_DRAGON) DragonRadar::drawGrid(gfx, LCD_WIDTH, LCD_HEIGHT, nightOn);
    else if (st == ST_BLUEPRINT) drawBlueprintGrid(cx, cy, sc, cs, sn, viewR);
    else if (st == ST_RADAR) drawRadarSweep();
    else if (st == ST_COMPASS) drawHexGrid();
  }
  profMark(1);
  // 导航路线（带深色描边）；v3.0 车已经走过的部分变灰或不画
  // v7.6 巡航没有路线：上一次导航留下的路线、原路线、路况、红绿灯、终点都不画，道路保持原色
  if (!dragon && !cruiseOn) drawGhost(cx, cy, sc, cs, sn, viewR, fmaxf(3, 5 * wk));   // v4.4 原路线（导入的分享路线）
  if (!cruiseOn && routeN > 1 && routeFrame == posFrame) {
    updateRouteProgress(cx, cy);
    float split = routeProg >= 0 ? routeProg : 0;
    float w0 = fmaxf(6, 13 * wk), w1 = fmaxf(4, 9 * wk);
    if (split > 0 && trailMode == 1)
      drawRouteRange(0, split, cx, cy, sc, cs, sn, viewR, w0 * 0.8f, w1 * 0.75f, TH.done2, TH.done);
    drawRouteRange(split, (float)(routeN - 1), cx, cy, sc, cs, sn, viewR, w0, w1, C_ROUTE2, C_ROUTE);
    if (!dragon && styleIdx()!=ST_MINIMAL && curveOn && routeCurv) drawRouteCurved(split, (float)(routeN - 1), cx, cy, sc, cs, sn, viewR, w1);   // v6.1
    if (!dragon && styleIdx()!=ST_MINIMAL) drawTraffic(cx, cy, sc, cs, sn, viewR, w0, w1, split);                       // v5.0 路况颜色
  }
  profMark(2);
  obsN = 0;                                             // v3.8 占位框从头登记
  if (dragon) {
    bool hudCam = hasCam; hasCam = false; // This style hides the camera badge.
    addHudObstacles(); hasCam = hudCam;
  } else addHudObstacles();
  if (!dragon && !cleanOn && !cruiseOn) drawMapLights(cx, cy, sc, cs, sn);   // v3.2 路线上的红绿灯（v4.9 清爽模式不画；v7.6 巡航不画）
  if (!dragon) drawMapCams(cx, cy, sc, cs, sn);                      // v3.5 电子眼
  if (!cruiseOn) drawDestBlip(cx, cy, sc, cs, sn);      // v2.9 目的地标记（v7.6 巡航没有目的地）
  drawPlayer(CX, MAP_PY);
  profMark(3);
  {
    int st = styleIdx();                                // v6.1 纸地图、蓝图、掌机像素不压暗边缘
    if (st != 0 && st != ST_MINIMAL && st != ST_DRAGON && st != ST_PAPER && st != ST_BLUEPRINT && st != ST_PIXEL) drawVignette(178, 211, 150);   // v2.9 雷达边缘渐暗
  }
  profMark(4);
  if (styleIdx() == ST_COMPASS) drawCompassRose(viewHdg);   // v6.1 罗盘刻度
  else if (!dragon) drawNorth(viewHdg);                              // v2.9 北向标记
  if (!dragon && !cleanOn) drawCamIcons(cx, cy, sc, cs, sn);       // v3.8 电子眼图标（v4.9 清爽模式不画）
  profMark(5);
  unsigned long tMapDone = millis();                   // v2.6 统计：画地图用时
  mapMsAcc += tMapDone - t;

  // 边框和信息
  // v4.9 清爽模式：测速牌、超速红圈、底部行程信息（到达时间/里程/红绿灯数）都不显示
  bool saveCam = hasCam, saveTrip = tripOn;
  if (cleanOn || dragon) { hasCam = false; tripOn = false; }
  bool over = hasCam && camLimit > 0 && speedKmh > camLimit;
  {                                                     // v5.9 外圈一次画完
    bool isNear = navDist <= STAGE_NEAR_M;
    // v7.4 平时外圈是行程环；出现转弯倒计时就换成倒计时环（盖掉行程环）
    float tp = ringActive ? -1 : tripFrac();
    drawRingLayer(navDir != "S" && tp < 0, ringActive ? ringShown : 0, isNear ? 14 : 10, isNear ? C_ORANGE : C_YELLOW, over, tp);
  }
  profMark(6);
  drawMapSpeed(over);
  profMark(7);
  drawMapLanes();                                       // v3.3 车道（车速下面）
  profMark(8);
  drawMapBadge();
  profMark(9);
  hasCam = saveCam; tripOn = saveTrip;
  drawNavHints(300);                                    // v5.0 重新规划中 / 更快路线 / GPS 信号弱
  drawBattery(CX - 16, 18, true);                         // v6.8 电量（车速上方）；v7.2 带黑底
  profMark(10);
  // v2.9 小地图里不画顶部的蓝牙小圆点（会和 N 标记重叠；能收到地图就说明连着）
  needFlush = true;
  mapFrames++;
  mapDrawMsAcc += millis() - t;
  ovlMsAcc += millis() - tMapDone;
}

// v2.2 每 2 秒统计一次帧率，发给手机显示
void updateStats() {
  unsigned long now = millis();
  if (now - statAt < 2000) return;
  float secs = (now - statAt) / 1000.0f;
  if (statAt != 0 && mapFrames > 0) {
    uint32_t fc = flushCnt, fm = flushMsAcc;
    pendingStat = "S," + String(mapFrames / secs, 1) + "," + String(mapDrawMsAcc / mapFrames) + "," +
                  String(fc ? fm / fc : 0) + "," + String(qspiMHz) + "," +
                  String(mapMsAcc / mapFrames) + "," + String(ovlMsAcc / mapFrames) + "," + String(fastDev ? 1 : 0) + "," +
                  String(rasMsAcc / mapFrames) + "," + String(helperTask ? 2 : 1);   // v5.7 底图用时、几个核心在画
    String q = "Q";                                    // v5.8 分步用时（0.1 毫秒/帧）
    for (int i = 0; i < PROF_N; i++) q += "," + String(profAcc[i] / 100 / mapFrames);
    pendingProf = q;
  }
  for (int i = 0; i < PROF_N; i++) profAcc[i] = 0;
  statAt = now; mapFrames = 0; mapDrawMsAcc = 0; mapMsAcc = 0; ovlMsAcc = 0; flushCnt = 0; flushMsAcc = 0; rasMsAcc = 0;
}

void updateRingTarget() {
  bool hasRing = (navDir != "S") && navDist <= STAGE_FAR_M;
  float target = hasRing ? (float)(STAGE_FAR_M - navDist) / STAGE_FAR_M : 0;
  if (navDir != ringDir || !ringActive || !hasRing) ringShown = target;  // 换路口/刚进入：直接到位
  ringTarget = target;
  ringActive = hasRing;
  ringDir = navDir;
}

void animateRing() {
  if (!inNav || !ringActive || staleShown) return;
  float diff = ringTarget - ringShown;
  if (fabsf(diff) < 0.0005f) return;
  ringShown += diff * 0.35f;
  if (fabsf(ringTarget - ringShown) < 0.002f) ringShown = ringTarget;
  navDirty = true;
}

// 按当前全部状态画一整帧导航画面
void drawNavFrame() {
  onWaitScreen = false;                                 // v3.6 离开首页，猫猫不再动
  if (mapModeActive()) { drawMapFrame(); return; }      // v2.0 小地图
  viewInit = false;
  if (cruiseOn) {                                       // v4.8 巡航但还没有地图：只显示车速和测速
    gfx->fillScreen(C_BLACK);
    bool over = hasCam && camLimit > 0 && speedKmh > camLimit;
    if (over) fillRingBand(0, 1, 228, 233, C_RED);
    drawStatus();
    drawSpeedBlock(over);
    printCentered("CRUISE", CX, 300, 4, C_CYAN);
    needFlush = true;
    return;
  }
  gfx->fillScreen(C_BLACK);
  bool isS = (navDir == "S");
  bool isFar = !isS && navDist > STAGE_FAR_M;
  bool lanes = laneN > 0;
  bool over = hasCam && camLimit > 0 && speedKmh > camLimit;
  float laneK = lanes ? 0.85f : 1.0f;
  float iconCy = lanes ? ICON_CY_LANE : ICON_CY;

  // 边缘：进度环 / 超速红边
  float tp = ringActive ? -1 : tripFrac();              // v7.4 平时外圈是行程环，转弯倒计时时让位
  if (tp >= 0) drawTripRing(tp);
  else if (!isS) drawRingTrack();
  if (ringActive) {
    bool isNear = navDist <= STAGE_NEAR_M;
    drawRingProgress(ringShown, isNear ? 14 : 10, isNear ? C_ORANGE : C_YELLOW);
  }
  if (over) fillRingBand(0, 1, 228, 233, C_RED);

  drawStatus();
  drawSpeedBlock(over);

  // 中间图标
  // v1.8.2 图标可用区域：车速下方 ~ 车道（或距离数字）上方，各留 8 像素
  float iconTop = ICON_TOP, iconBot = (lanes ? LANES_Y : DIST_Y) - 8;
  if (isFar)     drawPreview(navDir, iconCy, 0.66f * laneK, iconTop, iconBot);
  else if (isS)  drawBigIcon("S", C_WHITE, iconCy, laneK, iconTop, iconBot);
  else           drawBigIcon(navDir, navDist <= STAGE_NEAR_M ? C_ORANGE : C_WHITE, iconCy, laneK, iconTop, iconBot);

  // 车道、距离、路名、底部信息
  if (lanes) drawLanes(LANES_Y);
  printCentered(distText(navDist), CX, lanes ? DIST_Y_LANE : DIST_Y, lanes ? 6 : 7, C_WHITE);
  drawTextImg(0, lanes ? ROAD_Y_LANE : ROAD_Y, 0xDD);
  drawTextImg(1, BOTTOM_Y, 0x99);                      // v1.7.3：常驻显示
  drawNavHints(128);                                    // v5.0
  needFlush = true;
}

void resetNavState() {
  if (idlePowerOffNavigationActive(true, inNav, cruiseOn, navDir == "A"))
    idlePowerOffReset(&idlePowerOffTimer, millis());
  inNav = false;
  tripOn = false; lightDist = -1;                       // v3.1
  mapCamN = 0;                                          // v3.5
  ghostN = 0;                                           // v4.4
  cruiseOn = false;                                     // v4.8
  trafN = 0; rerouteAt = 0; fasterAt = 0; gpsWeak = false;   // v5.0
  speedKmh = -1;
  hasCam = false;
  laneN = 0;
  ringActive = false; ringDir = "";
  for (int i = 0; i < 3; i++) { imgs[i].showW = 0; imgs[i].showH = 0; imgs[i].recvId = -1; }   // v5.3 天气栏 3、4 留着
}

int hexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return 0xF;
}

// 处理文字类消息
void handleText(const String &m) {
  if (m.startsWith("#LY,")) {
    unsigned long gen = 0; unsigned title = 0, artist = 0;
    if (sscanf(m.c_str(), "#LY,%lu,%u,%u", &gen, &title, &artist) == 3 && gen == appleMedia.generation &&
        title == appleMedia.values[0].revision && artist == appleMedia.values[1].revision) {
      lyricImageGeneration = gen; lyricImageTitleRevision = title; lyricImageArtistRevision = artist;
    } else { lyricImageGeneration = 0; imgs[12].showW = 0; imgs[12].showH = 0; imgs[12].recvId = -1; }
    return;
  }
  if (m == "#ML,0" || m == "#ML,1") {
    bool wasEnabled = musicLyrics;
    musicLyrics = m == "#ML,1";
    if (!musicLyrics) {
      lyricImageGeneration = 0; imgs[12].showW = 0; imgs[12].showH = 0; imgs[12].recvId = -1; imgs[12].recvMask = 0; imgs[12].recvParts = 0;
      // Older companion renderers may have put lyrics in the artist bitmap.
      // Ask for a fresh title/artist once per connection or on an actual OFF transition.
      static uint32_t refreshedEpoch = 0xffffffff;
      if (wasEnabled || refreshedEpoch != mediaConnectionEpoch) {
        refreshedEpoch = mediaConnectionEpoch;
        imgs[6].showW = 0; imgs[6].showH = 0; imgs[6].recvId = -1; pendingEvt = "T,M,Q";
      }
    }
    pendingLyricsStatus = String("ML,") + (musicLyrics ? 1 : 0);
    Serial.printf("MEDIA LYRICS enabled=%d\n", musicLyrics);
    musicDirty = true; return;
  }
  if (m == "#HM,0" || m == "#HM,1") {
    bool enabled = m == "#HM,1";
    if (enabled != hidMusicMode) {
      hidMusicMode = enabled; hidPulses.cancelQueued();
      Preferences pr; pr.begin("motonav", false); pr.putBool("mediahid", enabled); pr.end();
    }
    if (!hidMusicMode) { musicLyrics = false; imgs[12].showW = 0; imgs[12].showH = 0; imgs[12].recvId = -1; }
    musicAck = 0; musicAckUntil = 0; musicDirty = true;
    pendingHidStatus = String("HID,") + (hidMusicMode ? 1 : 0) + "," + (hidMediaReady() ? 1 : 0);
    return;
  }
  // A dedicated command: no navigation state or screen invalidation.
  if (m.startsWith("#BOOT,")) {
    int id = BootAnimation::parse(m.c_str() + 6);
    if (id < 0) return;
    bool saved = true;
    if (id != bootBrand) {
      Preferences pr;
      saved = pr.begin("motonav", false);
      if (saved) {
        saved = pr.putInt("bootbrand", id) == sizeof(int32_t);
        pr.end();
      }
      if (saved) bootBrand = id;
    }
    pendingBootAck = "BOOT," + String(saved ? bootBrand : -1);
    return;
  }
  if (m == "#AMS?") { appleMedia.resend(); return; }
  if (m == "#GB?") { pendingBuildingCapability=true; return; }
  if (m.startsWith("#ZACK,")) {
    if (hidMusicMode) return;
    musicAck = constrain(m.substring(6).toInt(), 0, 5);
    musicAckUntil = millis() + 2500; musicDirty = true; return;
  }
  if (m.startsWith("#Z,")) {
    int have = 0, play = 0, actions = 0;
    if (sscanf(m.c_str(), "#Z,%d,%d,%d", &have, &play, &actions) == 3) {
      if (!hidMusicMode) { musicHave = have != 0; musicPlaying = play != 0; musicActions = actions & 7; } musicDirty = true;
    }
    return;
  }
  if (m == "P") return;                                 // 心跳
  if (m == "C") { stopArriveFx(); progReset(); resetNavState(); drawWaiting(WAIT_READY); return; }

  if (m.startsWith("#v,")) { speedKmh = m.substring(3).toInt(); navDirty = true; return; }
  if (m.startsWith("#k")) {
    int c1 = m.indexOf(','), c2 = m.indexOf(',', c1 + 1);
    if (c1 > 0 && c2 > c1) {
      hasCam = true;
      camLimit = m.substring(c1 + 1, c2).toInt();
      camDist = m.substring(c2 + 1).toInt();
    } else {
      hasCam = false;
    }
    navDirty = true;
    return;
  }
  if (m.startsWith("#j")) {
    laneN = 0;
    if (m.length() > 3) {
      String s = m.substring(3);
      int n = s.length() / 2;
      if (n > 8) n = 8;
      for (int i = 0; i < n; i++) { laneBg[i] = hexVal(s[2 * i]); laneFr[i] = hexVal(s[2 * i + 1]); }
      laneN = n;
    }
    navDirty = true;
    return;
  }
  if (m.startsWith("#p,")) {                             // v2.0 位置
    int fr = 0, hd = 0, kmh = 0; long xd = 0, yd = 0;
    if (sscanf(m.c_str(), "#p,%d,%ld,%ld,%d,%d", &fr, &xd, &yd, &hd, &kmh) == 5) {
      // v2.6 记下“按旧数据推算的当前位置”和新位置的偏差，画面在 0.4 秒内平滑修正过去
      unsigned long nowMs = millis();
      // v6.7 转弯速度：和上一次定位的朝向差 / 时间（太慢、隔太久、变化太离谱就当没在转）
      float newRate = 0;
      if (fr == posFrame && posAt) {
        float fdt = (nowMs - posAt) / 1000.0f;
        float dhd = hd - posHdg;
        while (dhd > 180) dhd -= 360;
        while (dhd < -180) dhd += 360;
        if (fdt > 0.3f && fdt < 2.5f && kmh >= 6 && fabsf(dhd) < 120) newRate = constrain(dhd / fdt, -45.0f, 45.0f);
      }
      if (fr == posFrame && posAt) {
        float px, py, ph;
        predictPose(nowMs, 0, px, py, ph);              // v6.7 按旧数据（含转弯）推算的现在位置
        float ex = px - xd / 10.0f, ey = py - yd / 10.0f;
        float rem = corrAt ? fmaxf(0, 1 - (nowMs - corrAt) / 400.0f) : 0;   // 上一次还没修正完的部分
        ex += corrX * rem; ey += corrY * rem;
        if (ex * ex + ey * ey < 60 * 60) { corrX = ex; corrY = ey; corrAt = nowMs; }
        else { corrX = corrY = 0; corrAt = 0; }
      }
      posFrame = fr; posX = xd / 10.0f; posY = yd / 10.0f; posHdg = hd; posKmh = kmh; posAt = nowMs;
      hdgRate = hdgRate * 0.3f + newRate * 0.7f;          // v6.7 稍微平滑一下，GPS 朝向偶尔抖一下不至于乱转
      if (inNav) navDirty = true;
    }
    return;
  }
  if (m.startsWith("#q,")) {                             // v2.3 屏幕数据线速度：保存并重启
    int q = m.substring(3).toInt();
    if ((q == 40 || q == 60 || q == 80) && q != qspiMHz) {
      Preferences pr;
      pr.begin("motonav", false);
      pr.putInt("qspi", q);
      pr.end();
      delay(100);
      ESP.restart();
    }
    return;
  }
  if (m.startsWith("#f,")) {                             // v2.6 快速刷屏开关：保存并重启
    bool on = m.substring(3).toInt() != 0;
    if (on != fastFlushOn) {
      Preferences pr;
      pr.begin("motonav", false);
      pr.putBool("fastf", on);
      pr.end();
      delay(100);
      ESP.restart();
    }
    return;
  }
  if (m.startsWith("#t,")) {                             // v2.9 地图配色主题
    int t = m.substring(3).toInt();
    if (t >= 0 && t < THEME_N) {
      // App 8.3.23 adds a third field: each style now owns its map data.
      // Old Android/iOS #t,t commands keep their legacy recoloring behavior.
      if (t != themeIdx && m.indexOf(',',3)>=0) {
        rasterValid=false; rasterBackValid=false; mapValid=false;
        for(int kind : {1,3}) {
          Bulk &b=bulks[kind];b.id=-1;b.total=0;b.got=0;b.done=false;
          memset(b.mask,0,sizeof(b.mask));
        }
      }
      themeIdx = t; navDirty = true;
    }   // v7.8.11 0~11
    return;
  }
  if (m.startsWith("#Q,")) { curveOn = m.substring(3).toInt() != 0; navDirty = true; return; }   // v6.2 弯道上色
  if (m.startsWith("#R,") || m.startsWith("#D")) {
    // iOS sends #R,nearestQuarter,exactDegrees; Android sends #R then #Ddegrees.
    // Preserve legacy #R,quarter while applying the exact angle when supplied.
    int d;
    if (ScreenRotationCommand::parse(m.c_str(), d) && d != screenDeg) {
      setScreenDeg(d, true); navDirty = true; needFlush = true;
    }
    return;
  }
  if (m.startsWith("#L")) {                              // v6.7 预判提前量："#L300"
    leadMs = constrain(m.substring(2).toInt(), 0, 800);
    return;
  }
  if (m == "#UNPAIR") {                                  // v6.5 忘掉配对过的手机；v6.6 只忘“别的”手机，正在用的这台保留
#if defined(CONFIG_NIMBLE_ENABLED)
    ble_addr_t keep;
    bool haveKeep = false;
    ble_gap_conn_desc d;
    if (bleConnHandle != 0xFFFF && ble_gap_conn_find(bleConnHandle, &d) == 0) { keep = d.peer_id_addr; haveKeep = true; }
    ble_addr_t peers[8];
    int n = 0;
    if (ble_store_util_bonded_peers(peers, &n, 8) == 0) {
      for (int i = 0; i < n; i++) {
        if (haveKeep && ble_addr_cmp(&peers[i], &keep) == 0) continue;
        ble_gap_unpair(&peers[i]);
      }
    }
#endif
    return;
  }
  if (m.startsWith("#e")) {                              // v3.1 行程信息：剩余米,到达时间HHMM,红绿灯数
    int rm = 0, eta = 0, lt = 0;
    if (sscanf(m.c_str(), "#e,%d,%d,%d", &rm, &eta, &lt) == 3) {
      // v7.3 路程进度：剩余变少就算走掉了（一次少太多的当成重新规划，不算）
      unsigned long nowMs = millis();
      if (progLastRm >= 0) {
        long d = (long)progLastRm - rm;
        long cap = 200 + (long)((nowMs - progLastAt) / 1000) * 60;     // 最快按 60 米/秒算
        if (d > 0 && d <= cap) progDone += d;
      }
      progLastRm = rm; progLastAt = nowMs;
      tripRemainM = rm; tripEta = eta; tripLights = lt; tripOn = true;
      long total=progDone+tripRemainM;
      if(total>0)tripProgress.sample(progDone/(float)total,nowMs);
    }
    else tripOn = false;
    navDirty = true;
    return;
  }
  if (m.startsWith("#c")) {                              // v3.5 小地图上的电子眼位置；只有 #c 表示清空
    int oldN = mapCamN;
    int32_t oldX[MCAM_MAX], oldY[MCAM_MAX]; int oldT[MCAM_MAX]; int8_t oldP[MCAM_MAX], oldK[MCAM_MAX]; float oldH[MCAM_MAX], oldA[MCAM_MAX];
    for (int k = 0; k < oldN; k++) { oldX[k] = mapCamX[k]; oldY[k] = mapCamY[k]; oldT[k] = mapCamTyp[k]; oldP[k] = mapCamPref[k]; oldH[k] = mapCamLockH[k]; oldA[k] = mapCamPrefA[k]; oldK[k] = mapCamPrefK[k]; }
    mapCamN = 0;
    const char *q = m.c_str() + 2;
    if (*q == ',') {
      char *e;
      mapCamFrame = strtol(q + 1, &e, 10); q = e;
      while (*q == ',' && mapCamN < MCAM_MAX) {
        long v[4]; bool ok = true;
        for (int k = 0; k < 4; k++) {
          if (*q != ',') { ok = false; break; }
          v[k] = strtol(q + 1, &e, 10); q = e;
        }
        if (!ok) break;
        mapCamX[mapCamN] = v[0]; mapCamY[mapCamN] = v[1]; mapCamSpd[mapCamN] = v[2]; mapCamTyp[mapCamN] = v[3];
        // v3.8 同一个电子眼（坐标一样）沿用上次的图标位置和锁定朝向（v4.0）
        int8_t pref = -1;
        float lockH = 0, prefA = 0; int8_t prefK = 0;
        for (int k = 0; k < oldN; k++)
          if (oldX[k] == v[0] && oldY[k] == v[1] && oldT[k] == v[3]) { pref = oldP[k]; lockH = oldH[k]; prefA = oldA[k]; prefK = oldK[k]; break; }
        mapCamPref[mapCamN] = pref; mapCamLockH[mapCamN] = lockH; mapCamPrefA[mapCamN] = prefA; mapCamPrefK[mapCamN] = prefK;
        mapCamN++;
      }
    }
    navDirty = true;
    return;
  }
  if (m.startsWith("#g")) {                              // v3.1 前方红绿灯距离；只有 #g 表示隐藏
    int d = -1;
    lightDist = (sscanf(m.c_str(), "#g,%d", &d) == 1) ? d : -1;
    navDirty = true;
    return;
  }
  if (m.startsWith("#n,")) { nightOn = m.substring(3).toInt() != 0; navDirty = true; return; }   // v3.0 夜间配色
  if (m == "#o") { ghostN = 0; navDirty = true; return; }   // v4.4 不显示原路线
  if (m == "#WXRESET") { wxHave = false; wxExtra = false; wxDetail = 0; wxDirty = true; return; }
  if (m.startsWith("#WX,")) { parseWeatherExtra(m); return; }
  if (m.startsWith("#W,")) { parseWeather(m); return; }   // v5.3 天气
  if (m.startsWith("#i,")) {                             // v5.1 来电："#i,1,号码" / "#i,0"
    bool on = m.charAt(3) == '1';
    if (on) {
      if (!callOn) { callAt = millis(); callAnswerSent = false; touchHoldAt = millis(); touchHold = 0; }
      callSeenAt = millis();
      int c2 = m.indexOf(',', 4);
      callNum = c2 > 0 ? m.substring(c2 + 1) : "";
      if (callNum.length() > 16) callNum = callNum.substring(0, 16);
    }
    callOn = on;
    if (!on) navDirty = true;
    return;
  }
  if (m.startsWith("#y,")) {                             // v5.0 重新规划中
    rerouteAt = m.substring(3).toInt() ? max(1UL, millis()) : 0;
    if (rerouteAt) fasterAt = 0;
    navDirty = true; return;
  }
  if (m.startsWith("#h")) {                              // v5.0 更快路线：省几分钟；只有 #h 表示收起
    if (m.length() > 3) { fasterMin = m.substring(3).toInt(); fasterAt = max(1UL, millis()); } else fasterAt = 0;
    navDirty = true; return;
  }
  if (m.startsWith("#x,")) { gpsWeak = m.substring(3).toInt() != 0; navDirty = true; return; }   // v5.0 GPS 信号弱
  if (m.startsWith("#u")) {                              // v5.0 路况：起,止,状态,...；只有 #u 表示清空
    trafN = 0;
    const char *q = m.c_str() + 2;
    while (*q == ',' && trafN < TRAF_MAX) {
      char *e; long a = strtol(q + 1, &e, 10); q = e; if (*q != ',') break;
      long b = strtol(q + 1, &e, 10); q = e; if (*q != ',') break;
      long st = strtol(q + 1, &e, 10); q = e;
      trafA[trafN] = a; trafB[trafN] = b; trafS[trafN] = (uint8_t)st; trafN++;
    }
    navDirty = true; return;
  }
  if (m.startsWith("#w,")) { cleanOn = m.substring(3).toInt() != 0; navDirty = true; return; }   // v4.9 清爽模式
  if (m.startsWith("#z,")) {                             // v4.8 巡航模式开关
    cruiseOn = m.substring(3).toInt() != 0;
    if (cruiseOn) { inNav = true; navDir = "S"; navDist = 0; ringActive = false; tripOn = false; laneN = 0; }
    navDirty = true;
    return;
  }
  if (m.startsWith("#r,")) {                             // v3.0 走过的路线：1 变灰，2 擦除
    int r = m.substring(3).toInt();
    if (r == 1 || r == 2) { trailMode = r; navDirty = true; }
    return;
  }
  if (m.startsWith("#m,")) { mapOn = m.substring(3).toInt() != 0; navDirty = true; return; }
  if (m.startsWith("#b,")) {
    int b = constrain(m.substring(3).toInt(), 5, 255);
    if (flushDone) {                                    // v2.2 刷屏任务在用总线时先等它推完
      xSemaphoreTake(flushDone, portMAX_DELAY);
      boardBrightness(b);
      xSemaphoreGive(flushDone);
    } else {
      boardBrightness(b);
    }
    return;
  }

  if (m.startsWith("#O")) {                              // v7.1 行程统计 "#O米/分钟/最高车速"
    long a = -1; int b = -1, c = -1;
    if (sscanf(m.c_str() + 2, "%ld/%d/%d", &a, &b, &c) >= 2) { tripM = a; tripMin = b; tripMaxKmh = c; tripAt = millis(); }
    return;
  }
  if (m == "#M1" || m == "#M0") { progOn = m[2] == '1'; navDirty = true; return; }   // v7.3 路程进度开关
  if (m == "#J") { if (page == 0) startArriveFx(true); return; }   // v7.1 到达途经点
  if (m.startsWith("#")) return;                        // v6.7 不认识的 "#..." 指令：忽略（别当成导航方向）
  int comma = m.indexOf(',');                           // 导航：方向,距离
  if (comma > 0) {
    String prevDir = navDir;
    navDir = m.substring(0, comma);
    // v7.1 导航中 → 到达：放特效（重连补发的 "A,0"、圆屏刚开机收到的都不放）
    if (navDir == "A" && prevDir.length() && prevDir != "A" && page == 0) startArriveFx(false);
    if (prevDir == "A" && navDir != "A") progReset();       // v7.3 到达后又开始新的导航：进度从头算
    else if (navDir != "A" && fxOn) stopArriveFx();
    int comma2 = m.indexOf(',', comma + 1);
    if (comma2 > 0) {
      navDist = m.substring(comma + 1, comma2).toInt();
      navAngle = m.substring(comma2 + 1).toFloat();
    } else {
      navDist = m.substring(comma + 1).toInt();
      navAngle = NAN;
    }
    inNav = true;
    if (navDir != "A") idlePowerOffReset(&idlePowerOffTimer, millis());
    updateRingTarget();
    navDirty = true;
  }
}

void handleMessage(const uint8_t *d, int n) {
  lastRxMs = millis();
  if (staleShown) { staleShown = false; navDirty = true; }   // 收到任何消息，清掉数据中断提示
  if (d[0] == 'B') { handleBitmap(d, n); return; }
  if (d[0] == 'M') { handleBulk(d, n); return; }     // v2.0 地图块 / 路线
  char buf[512];                                      // v4.8 64 → 512：电子眼列表长了会被截断
  int len = n < 511 ? n : 511;
  memcpy(buf, d, len);
  buf[len] = 0;
  String m = String(buf);
  m.trim();
#if DEBUG_SERIAL
  Serial.println("recv: " + m);
#endif
  handleText(m);
}

// v7.0 蓝牙名 = "MotoNav-" + 蓝牙地址最后两个字节
void makeBleName() {
  uint8_t mac[6] = {0};
  if (esp_read_mac(mac, ESP_MAC_BT) != ESP_OK) return;
  char b[20];
  snprintf(b, sizeof(b), "%s-%02X%02X", DEVICE_NAME, mac[4], mac[5]);
  bleName = b;
}

void setup() {
  makeBleName();                                       // v7.0 先算好蓝牙名（首页要显示）
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);   // 没打开串口监视器时打印不等待（v1.2 的修复）
  Serial.printf("Reset reason=%d, sleep wake=%d\n", (int)esp_reset_reason(),
                (int)esp_sleep_get_wakeup_cause());
  // Release the backlight RTC hold left by idle deep sleep.
  rtc_gpio_hold_dis((gpio_num_t)LCD_BL);
  rtc_gpio_deinit((gpio_num_t)LCD_BL);
  pinMode(LCD_BL,OUTPUT);digitalWrite(LCD_BL,LOW);
  backlightPWM=ledcAttach(LCD_BL,20000,8);
  Wire.begin(IIC_SDA, IIC_SCL);
  touchInit();                                         // v5.2 触摸屏
  batInit();                                           // v6.8 电源芯片（电量）

  // v2.3 读取保存的屏幕数据线速度（默认 40MHz，和官方示例一样）
  {
    Preferences pr;
    pr.begin("motonav", true);
    qspiMHz = pr.getInt("qspi", 40);
    screenRot = pr.getInt("rot", 0) & 3;                            // v6.2 圆屏方向
    setScreenDeg(pr.getInt("rotd", screenRot * 90), false);         // v6.4 任意角度
    bootBrand = BootAnimation::valid(pr.getInt("bootbrand", 0));
    hidMusicMode = pr.getBool("mediahid", false);
    fastFlushOn = pr.getBool("fastf", true);
    pr.end();
    if (qspiMHz != 40 && qspiMHz != 60 && qspiMHz != 80) qspiMHz = 40;
  }
  if (!gfx->begin(qspiMHz * 1000000)) { Serial.println("gfx->begin() failed; stopping before framebuffer use");while(true)delay(1000); }
  boardBrightness(200);

  iconBuf = (uint16_t *)ps_malloc(ICON_BUF_W * ICON_BUF_H * 2);   // v1.8.1 图标缓存
  buildRingLut();                                                   // v1.8.3 抗锯齿圆环查找表
  mapInit();                                                        // v2.0 小地图缓冲
  for (int i = 0; i < IMG_SLOTS; i++) {
    imgs[i].show = (uint8_t *)ps_malloc(IMG_MAX);
    imgs[i].recv = (uint8_t *)ps_malloc(IMG_MAX);
    imgs[i].showW = 0; imgs[i].showH = 0; imgs[i].recvId = -1;
  }

  // Begin with black instead of briefly displaying the waiting page before the intro.
  gfx->fillScreen(0);
  gfx->flush();
  initRotSpan();                                                    // v6.4 任意角度用
  initDoubleBuffer();                                               // v2.2 双缓冲 + 核心 0 刷屏
  helperInit();                                                     // v5.7 另一个核心帮忙画底图

  if (bootBrand != BootAnimation::Off && gfx->getFramebuffer()) {
    uint32_t bootAt = millis();
    while (millis() - bootAt < BootAnimation::Duration) {
      uint32_t frameAt = millis();
      BootAnimation::render(gfx->getFramebuffer(), frameAt - bootAt, bootBrand);
      flushSync();
      uint32_t spent = millis() - frameAt;
      delay(spent < 33 ? 33 - spent : 1);
    }
  }
  drawWaiting(WAIT_NOLINK);
  flushSync();
  needFlush = false;

  BLEDevice::init(bleName);                    // v7.0 名字带地址后 4 位；名字放不进广播包时库会自动放到扫描应答里
  BLEDevice::setMTU(517);     // 允许手机协商大数据包，用来传文字图片
#if defined(CONFIG_NIMBLE_ENABLED)
  // v4.5 允许蓝牙 2M 模式（速度翻倍，手机请求时直接同意）和最长空中数据包（251 字节）
  ble_gap_set_prefered_default_le_phy(BLE_GAP_LE_PHY_1M_MASK | BLE_GAP_LE_PHY_2M_MASK,
                                      BLE_GAP_LE_PHY_1M_MASK | BLE_GAP_LE_PHY_2M_MASK);
  ble_gap_write_sugg_def_data_len(251, 2120);
#endif
  // v6.5 配对验证：圆屏显示 6 位随机配对码（每次配对都换），手机输入；要绑定 + 防中间人 + 安全连接
  {
    BLESecurity *sec = new BLESecurity();                 // 构造时设好“交换加密密钥和身份密钥”（手机换地址也认得出）
    sec->setCapability(ESP_IO_CAP_OUT);                   // 圆屏只能显示，不能输入
    sec->setAuthenticationMode(true, true, true);         // 绑定、防中间人、安全连接
    sec->setPassKey(false);                               // 随机配对码
    sec->regenPassKeyOnConnect(true);                     // 每次配对重新随机
    BLEDevice::setSecurityCallbacks(new SecCB());
  }
  BLEServer *server = BLEDevice::createServer();
  server->setCallbacks(new ServerCB());
  BLEService *svc = server->createService(SERVICE_UUID);
  BLECharacteristic *rx = svc->createCharacteristic(
    RX_UUID, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
#if defined(CONFIG_NIMBLE_ENABLED)
             | BLECharacteristic::PROPERTY_WRITE_ENC | BLECharacteristic::PROPERTY_WRITE_AUTHEN   // v6.5 必须配对过才能写
#endif
  );
  rx->setCallbacks(new RxCB());
  txChar = svc->createCharacteristic(TX_UUID, BLECharacteristic::PROPERTY_NOTIFY);   // v2.0
  txChar->addDescriptor(new BLE2902());
  svc->start();
  initMediaHid(server);
  appleMedia.begin();
  BLEAdvertising *adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(SERVICE_UUID);
  adv->addServiceUUID(BLEUUID((uint16_t)0x1812));
  adv->setAppearance(GENERIC_HID);
  adv->setScanResponse(true);
  BLEDevice::startAdvertising();
  idlePowerOffReset(&idlePowerOffTimer, millis());
  Serial.println(String("MotoNav ready, firmware ") + FW_VERSION);
}

// Power down before destroying BLE objects: the host can still dispatch events
// referencing those objects. Shutdown does not need to reclaim their memory.
void idlePowerOffShutdown() {
  powerOffPending = true;
  Serial.println("No navigation for 5 minutes: powering off");
  const bool displayReady = !flushDone ||
      xSemaphoreTake(flushDone, pdMS_TO_TICKS(3000)) == pdTRUE;
  if (displayReady) {
    boardBrightness(0);
    panel->displayOff();
  }
  // 1.85B has a BQ27220 fuel gauge and a separate hardware PWR latch.
  // It has no AXP2101 software power-off register. Do not write 0x34.
  // Enter deep sleep without a timer; BOOT wakes, PWR switches supply.
  Serial.println("1.85B: entering deep sleep (BOOT to wake, PWR for hardware power off)");
  if(backlightPWM) { ledcDetach(LCD_BL);backlightPWM=false; }
  rtc_gpio_init((gpio_num_t)LCD_BL);
  rtc_gpio_set_direction((gpio_num_t)LCD_BL, RTC_GPIO_MODE_OUTPUT_ONLY);
  rtc_gpio_set_level((gpio_num_t)LCD_BL, 0);
  rtc_gpio_hold_en((gpio_num_t)LCD_BL);
  pinMode(PA,OUTPUT);digitalWrite(PA,LOW);
  // Stop the host/controller without deleting live GATT objects. Deep sleep
  // discards RAM anyway. Keep objects valid until pending callbacks have ended.
#if defined(CONFIG_NIMBLE_ENABLED)
  nimble_port_stop();
#elif defined(CONFIG_BLUEDROID_ENABLED)
  esp_bluedroid_disable();
#endif
#if CONFIG_BT_CONTROLLER_ENABLED
  esp_bt_controller_disable();
#endif
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  rtc_gpio_init(GPIO_NUM_0);
  rtc_gpio_set_direction(GPIO_NUM_0, RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_pullup_en(GPIO_NUM_0);
  rtc_gpio_pulldown_dis(GPIO_NUM_0);
  // EXT0 is level-triggered: arming it while BOOT is held low causes an
  // immediate wake/reboot. Require 100 ms released, with a bounded wait.
  uint32_t releasedAt = millis(), waitAt = releasedAt;
  while (millis() - waitAt < 1500 && millis() - releasedAt < 100) {
    if (!rtc_gpio_get_level(GPIO_NUM_0)) releasedAt = millis();
    delay(10);
  }
  if (millis() - releasedAt >= 100 && rtc_gpio_get_level(GPIO_NUM_0)) {
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0);
  } else {
    Serial.println("BOOT is low; leave wake disabled to avoid instant restart (use PWR to power cycle)");
  }
  esp_deep_sleep_start();
}

void loop() {
  hidMediaTick();                                      // press/release without blocking navigation
  static uint32_t lyricConnectionEpoch = 0;
  if (lyricConnectionEpoch != mediaConnectionEpoch) {
    lyricConnectionEpoch = mediaConnectionEpoch; musicLyrics = false; lyricImageGeneration = 0;
    imgs[12].showW = 0; imgs[12].showH = 0; imgs[12].recvId = -1; musicDirty = true;
  }
  appleMedia.tick(bleConnected && bleAuthed && hidMusicMode, bleConnHandle, mediaConnectionEpoch, millis());
  static int lastControlMask = -1;
  int controlMask = appleMedia.controlMask();
  if (lastControlMask != controlMask) { lastControlMask = controlMask; pendingMediaStatus = String("AMC,") + controlMask; musicDirty = true; Serial.printf("MEDIA AMS controls=%d\n", controlMask); }
  if (appleMedia.controlAck) { musicAck = appleMedia.controlAck > 0 ? 9 : 10; appleMedia.controlAck = 0; musicAckUntil = millis() + 2500; musicDirty = true; }
  if (appleMedia.changed) {
    appleMedia.changed = false; musicDirty = true;
    static uint32_t lyricGeneration = 0; static uint16_t lyricTitleRevision = 0, lyricArtistRevision = 0;
    if (lyricGeneration != appleMedia.generation || lyricTitleRevision != appleMedia.values[0].revision || lyricArtistRevision != appleMedia.values[1].revision) {
      lyricImageGeneration = 0;
      lyricGeneration = appleMedia.generation; lyricTitleRevision = appleMedia.values[0].revision; lyricArtistRevision = appleMedia.values[1].revision;
      imgs[12].showW = 0; imgs[12].showH = 0; imgs[12].recvId = -1;
    }
    if (hidMusicMode) { musicHave = appleMedia.available && appleMedia.values[0].text.length(); musicPlaying = appleMedia.playback.known && appleMedia.playback.state == 1; }
  }
  static unsigned long mediaDrawAt = 0;
  if (hidMusicMode && appleMedia.available && millis() - mediaDrawAt >= 1000) { mediaDrawAt = millis(); musicDirty = true; }
  saveRotIfDue();                                      // v6.4
  // v6.5 连上 60 秒还没通过验证（不是自己的手机）：断开，让自己的手机能连
#if defined(CONFIG_NIMBLE_ENABLED)
  if (bleConnected && !bleAuthed && bleConnHandle != 0xFFFF && millis() - bleConnAt > AUTH_TIMEOUT_MS) {
    ble_gap_terminate(bleConnHandle, BLE_ERR_REM_USER_CONN_TERM);
    bleConnAt = millis();                              // 别每圈都断
  }
#endif
  // 1. 连接状态变化（先处理）；v6.5 通过验证才算连上
  bool conn = bleConnected && bleAuthed;
  if (conn != shownConn) {
    shownConn = conn;
    if (conn) {
      lastRxMs = millis();
      drawWaiting(WAIT_READY);
    } else {
      bool wasNav = inNav;
      resetNavState();
      staleShown = false;
      drawWaiting(wasNav ? WAIT_LOST : WAIT_NOLINK);   // 导航途中断开 → 红色 LINK LOST
    }
  }

  // 2. 取出队列里的所有消息逐条处理（最多 30 条，避免卡住画面）
  static RxMsg local;
  for (int i = 0; i < 30; i++) {
    bool got = false;
    portENTER_CRITICAL(&mux);
    if (rxTail != rxHead) {
      local.len = rxq[rxTail].len;
      memcpy(local.data, rxq[rxTail].data, local.len);
      rxTail = (rxTail + 1) % RXQ_SLOTS;
      got = true;
    }
    portEXIT_CRITICAL(&mux);
    if (!got) break;
    handleMessage(local.data, local.len);
  }

  updateStats();
  if (pendingStat.length()) { pendingStat2 = pendingStat; pendingStat = ""; }
  sendPendingAck();                                    // v2.0 地图收齐的确认 / v2.2 帧率

  // 3. 进度环动画；小地图模式下连续刷新（v2.2 不再限制帧间隔）
  animateRing();
  // 地图帧原本连续刷新；大箭头画面也按现有帧节奏推进行程环。
  static unsigned long tripDrawAt=0;
  static float tripDrawValue=-1;
  float tripValue=tripFrac();
  if(inNav && progOn && tripOn && !cruiseOn && !ringActive && !staleShown &&
     tripValue>=0 && tripValue!=tripDrawValue && millis()-tripDrawAt>=33) {
    tripDrawAt=millis();tripDrawValue=tripValue;navDirty=true;
  }
  if(tripValue<0)tripDrawValue=-1;
  static bool wasMap = false;
  bool mapNow = inNav && mapModeActive();
  if (mapNow != wasMap) { wasMap = mapNow; navDirty = true; }   // 进出地图模式时整帧重画
  if (mapNow) navDirty = true;

  touchPoll();                                         // v5.2 按住接听 / v5.3 滑动换页
  batPoll();                                           // v6.8 电量
  // 4. 需要时重画导航画面
  // v6.5 配对：整屏显示配对码，配好 / 失败 / 60 秒 / 断开后回到原来的画面
  if (pairReq) {
    pairReq = false;
    if (sliding) dragAbort();
    pairOn = true; pairShowAt = millis();
    drawPairScreen();
  }
  if (pairOn && (pairDone || millis() - pairShowAt > AUTH_TIMEOUT_MS || !bleConnected)) {
    pairOn = false; pairDone = false;
    if (page == 2) musicDirty = true;
    else if (page == 1) wxDirty = true;
    else if (inNav) navDirty = true;
    else drawWaiting(conn ? WAIT_READY : WAIT_NOLINK);
  }
  // v5.1 来电：整屏显示（约 12 帧/秒的动画），结束后回到原来的画面
  static bool wasCall = false;
  if (callOn && (millis() - callSeenAt > 120000 || !conn)) callOn = false;      // 2 分钟没消息 / 断开：收起
  if (callOn && sliding) dragAbort();                  // v5.5 拖到一半来电：放弃换页
  if (callOn) {
    static unsigned long lastCallDraw = 0;
    if (millis() - lastCallDraw > 80) { lastCallDraw = millis(); drawCallScreen(); }
    wasCall = true;
  } else if (wasCall) {
    wasCall = false;
    if (page == 2) musicDirty = true;
    else if (page == 1) wxDirty = true;
    else if (inNav) navDirty = true;
    else drawWaiting(conn ? WAIT_READY : WAIT_NOLINK);
  }

  // v7.1 到达特效：来电、配对优先；放完 / 被点掉后回到原来的画面
  static bool wasFx = false;
  if (fxOn && (millis() - fxAt > fxDur || !conn)) fxOn = false;
  if (fxOn) {
    static unsigned long lastFxDraw = 0;
    if (!callOn && !pairOn && page == 0 && !sliding && millis() - lastFxDraw >= 33) {
      lastFxDraw = millis();
      drawArriveFx(millis() - fxAt);
    }
    wasFx = true;
  } else if (wasFx) {
    wasFx = false;
    if (!callOn && !pairOn) {
      if (page == 2) musicDirty = true;
      else if (page == 1) wxDirty = true;
      else if (inNav) navDirty = true;
      else drawWaiting(conn ? WAIT_READY : WAIT_NOLINK);
    }
  }

  musicTick();
  weatherTick();                                       // v5.3 天气页
  if (inNav && navDirty && !fxOn && !callOn && !pairOn && page == 0 && !sliding) {
    drawNavFrame();
    navDirty = false;
  }

  // 5. 导航中长时间收不到任何消息 → 数据中断提示
  if (inNav && conn && !fxOn && !callOn && !pairOn && page == 0 && !sliding && !staleShown && millis() - lastRxMs > STALE_MS) {
    staleShown = true;
    drawStaleOverlay();
  }

  animateCat();                                        // v3.6 首页猫猫动画

  // 6. 有变化才把画布推到屏幕（小地图帧异步推送，其他画面同步推送）
  if (needFlush && !sliding) {
    if (frameIsMap) flushAsync(); else flushSync();
    needFlush = false;
    frameIsMap = false;
  }
  // Idle phone traffic (including HID/AMS) does not count as navigation.
  const bool navigating = idlePowerOffNavigationActive(
      bleConnected && bleAuthed, inNav, cruiseOn, navDir == "A");
  const bool interacting = pairOn || pairReq || callOn || touchDown || dragging || sliding;
  if (idlePowerOffDue(&idlePowerOffTimer, millis(), navigating, interacting)) {
    bool queued;
    portENTER_CRITICAL(&mux);
    queued = rxTail != rxHead;
    portEXIT_CRITICAL(&mux);
    if (!queued) idlePowerOffShutdown();
  }
  delay(mapNow ? 1 : 10);
}
