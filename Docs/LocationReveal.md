# Location Reveal 地区揭示

## 调用

剧情蓝图或关卡 Trigger：`Get Player Controller` → Cast 到现有
`The_AwakeningPlayerController` → `Show Location Reveal`。

输入：`ChapterText`、`LocationText`（均为 FText）、`bForceReplay`（默认 false）。
非 Shipping 构建中，游戏操作状态下按主键盘数字 0 可强制重播医院提示（不绑定小键盘 0）。
通过现有 JSON 本地化读取：英文 CHAPTER 1 / HOSPITAL，简体 第一章 / 医院，繁体 第一章 / 醫院。
菜单捕获输入时不触发；切换语言后再次按键会使用当前语言。
例如传入 `CHAPTER 01` 和 `HOSPITAL`；Widget 不含写死的章节或地区。
应由剧情在恢复控制后主动调用。没有绑定 Streaming、Possess 或固定地图事件。
多人/分屏时用对应的本地 PlayerController；非本地控制器不创建 UI。

默认 Widget 类为 `/Game/UI/LocationReveal/WBP_LocationReveal`，第一次请求时创建，
`AddToPlayerScreen(30)`，后续复用；Controller EndPlay 时销毁。
可在既有 PlayerController 的 Class Defaults 修改 LocationRevealWidgetClass / ZOrder。
不要另外 AddToViewport 或为每次 Trigger 创建新 Widget。

## 参数与参考

已读取 `D:\Downloads\src\App.tsx`、`index.css` 和提供的 TTF；
录像通过本机已有 ffmpeg 解码，6.72 秒，2528×1454，提取 0.5 秒间隔关键帧，观察到两轮扫描。
关键帧：`Saved/LocationReveal/Reference/contact.png`。

| 参数 | 默认值 | 依据 |
| --- | --- | --- |
| RevealDuration | 1 秒 | 4 秒循环的 0–25% |
| HoldDuration | 1.75 秒 | 25–68.75% |
| EraseDuration | 1 秒 | 68.75–93.75% |
| LineFadeDuration | 旧值 0.25 秒 | 仅兼容旧资产保留，不再控制正常退场 |
| InterruptFadeDuration | 0.2 秒 | 本任务新增规则 |
| ScreenMargin | 左 6%，底部 14% | 导出 CSS |
| DesignSize | 720×124 | 1920×1080 参考空间 |
| ChapterFontSize / LocationFontSize | 10 / 58 pt | 对应 CSS 约 12.8 / 76.8 px |
| ChapterLetterSpacing / LocationLetterSpacing | 380 / 130 | UMG 的千分之一 em，CSS .38 / .13 em |
| ChapterOpacity | .72 | CSS |
| TextColor | sRGB #e8e5dc | CSS |
| TraceColor | sRGB #d7e0db，alpha .62 | CSS |
| LineThickness | 2.5 | 按后续反馈加粗主线（原 SVG 为 .85） |
| PulseWidth / Height | 80 / 72 | QRS 波形宽度 / R 峰至 S 谷总高度；约 68% 在基线上方、32% 在下方 |
| InEventOffset / OutEventOffset | .02 / 0 秒 | 各自扫描阶段的起点偏移 |

打开 `WBP_LocationReveal` → Class Defaults → Location Reveal，可调整以上参数，以及
ChapterOffset、LocationOffset、Typeface、TraceGlowRadius、TraceGlowOpacity。
Widget 的原生 Initialize 构建 Canvas/TextBlock；排版配置在 Class Defaults，不使用 Designer 的空画布。
两行文字不平移，Mask 的移动通过内部 Canvas 的反向偏移抵消。

ChapterOffset.Y=12.4、LocationOffset.Y=22.4 是对提供的 Google Sans 字体 ascent 和
CSS line-height 1 / .92 的补偿，不应直接当作 CSS top=14 / 35.2。
布局在 UMG DPI 缩放后的局部空间计算，再按 1920 参考宽度缩放；16:9 的位置比例保持一致。
长译名超出 DesignSize.X 会被裁剪，可扩大 DesignSize 或调小字号。

提供的字体是静态 `Google Sans Medium`（内部 subfamily 为 Regular），并非含 600/700 轴的可变字体。
使用它的真实 Medium 字形，没有制造不存在的 Bold 字体文件。
导出 CSS 声明 600/700 且没有 @font-face 加载本地 TTF，因此录像中浏览器实际回退字体无法仅凭导出文件确认。
按用户指定，以提供的 Google Sans Medium 为 UE 字体。
心电图光晕用多层抗锯齿描边近似 CSS drop-shadow；浏览器的微弱 24px text-shadow 没有进行高斯模糊的逐像素复刻。
心电图按最新红框 QRS 参考改为「水平线 → 小 Q 下探 → 高尖 R 峰 → 较深 S 下谷 → 平滑回到水平线」。
以参考图比例近似：R 峰在基线上方 .68 × PulseHeight，S 谷在下方 .32 × PulseHeight，Q 下探为 .165 × PulseHeight。
Q 下探与 S 恢复段使用采样三次贝塞尔曲线，R 峰和 R→S 段保持尖锐；仅采用红框 QRS，不加入框外 P/T 波。
ECG 的绘制裁剪区域向下扩展，确保 S 下谷完整可见。
建议最终在真实游戏背景中确认文字大小、位置、间距与光晕强度。

## 架构和中断

复用当前项目由 PlayerController 持有被动 HUD 的方式。现有输入统一经过
TAInputRouter / TAInputOwnershipAdapter；本系统不注册输入预处理器、不申请 Ownership、
不修改 InputMode、焦点、鼠标或角色系统。
根 Widget 为 HitTestInvisible，所有后代不参与命中测试，IsFocusable=false。
UMG TextBlock 负责字体 shaping/字距和 FText；必要的 NativePaint 仅绘制 ECG 连续折线。
没有每帧 Actor 搜索，没有 UI Material 或 Editor-only 运行时依赖。

状态：Hidden → Revealing → Holding → Erasing → FadingLine → Hidden。
按后续反馈，Erasing 时不绘制波峰左侧的长水平线，只保留 QRS 和右侧尚未擦掉的线。
文字完全擦除后，QRS 继续以擦除阶段相同速度向右移出裁剪边界，不停顿、不渐隐，也不会恢复整条水平线。
末端使用与入场相同的右侧裁剪边界，直到整个 QRS 和光晕都移出后进入 Hidden。
FadingLine 枚举名为蓝图兼容保留，现表示最后一小段移出；时长按波形半宽、光晕及擦除速度计算。
新请求使 RequestVersion 递增，PlayingVersion 保留旧版本，立即取消旧版本未触发的音效。
活跃画面的 Phase / Progress 被冻结，进入 Interrupting，从当前 GetRenderOpacity()
线性淡出到 0。完全淡出后，最新待播请求开始新的完整流程。
连续请求只替换 PendingChapter / PendingLocation，不重置正在进行的中断计时。
已有音效是一次性播放；不停止已发出的事件，取消的是尚未发出的旧事件。

同地区按两份 FText 的 IdenticalTo 比较（章节也参与），自然播放结束后仍记住最后地区。
中断期间以待播地区判重；重新进入旧地区会替换待播目的地。
强制重播遵循同样的中断淡出流程。
单一 NativeTick 时钟推进状态，带大 delta 跨阶段处理；没有 Timer、Timeline、Delay、异步音效完成回调。
NativeDestruct 使版本失效并清理待播状态，PlayerController EndPlay 移除并释放 Widget。

## 字体与本地化

新增 `/Game/Fonts/GoogleSans/GoogleSans_Medium` UFontFace（Inline 嵌入数据）。
在已有 `/Game/Fonts/TA_GlobalFont` 增加命名样式 `GoogleSansMedium`，并为既有
文化/字符范围的字体回退增加同名别名；没有替换原有默认字体或新增字体管理器。
原始 TTF 保存在 `Content/Fonts/GoogleSans/Source/GoogleSans-Medium.ttf`，运行时不读取下载目录。

接口保留 FText 历史，可传 String Table Text / 可本地化 Text 变量。
项目的 TALocalizeSubsystem.GetText 当前返回 JSON 解析后的 FText::FromString；
如果从这个 API 取字，切换语言后由剧情重新获取文本并更新请求。
UE String Table / LOCTEXT 的文化切换会随 FText 自身更新。
默认 bUppercase=true，使用 FText.ToUpper，保留本地化历史。

## Wwise

项目已有 Wwise 2025.1 插件、AkAudioEvent 资产和 SoundBank 配置，原游戏 C++ 未封装统一 UI PostEvent API。
本系统通过 AkAudio 的 `UAkGameplayStatics::PostEvent` 发出环境 UI 事件。
名称仅集中定义在 UTALocationRevealWidget::InEventName / OutEventName：

- Play_UI_LocationReveal_In
- Play_UI_LocationReveal_Out

Wwise 中创建 Event、生成 Windows SoundBanks、在 UE 生成对应 AkAudioEvent 资产后，
在 WBP_LocationReveal 的 InEvent / OutEvent 选择资产。不会从名字猜测路径。
软引用为空或对应包不存在时静默跳过；已加载资产被强引用保持到 Widget 生命周期结束。
只在收到有效新请求时解析配置，不在每帧加载资源。
音效偏移以对应阶段起点计时；偏移超出该阶段时，该次事件不会播放。
尚未绑定真实音效，未生成占位文件；实际听感和精确同步仍需未来绑定后验收。

## 修改文件

- Source/The_Awakening/UI/TALocationRevealWidget.h / .cpp：原生 UMG、配置、状态、ECG、Wwise。
- Source/The_Awakening/The_AwakeningPlayerController.h / .cpp：公开蓝图入口及生命周期。
- Source/The_Awakening/The_Awakening.Build.cs：私有 AkAudio 模块依赖。
- Source/The_Awakening/Tests/TALocationRevealTests.cpp：状态/生命周期、字体、被动输入测试。
- Source/The_AwakeningEditor/TALocationRevealSetupCommandlet.h / .cpp：幂等导入与可重复渲染工具。
- Content/UI/LocationReveal/WBP_LocationReveal.uasset：参数配置入口。
- Content/Fonts/GoogleSans/GoogleSans_Medium.uasset、Source/GoogleSans-Medium.ttf：字体。
- Content/Fonts/TA_GlobalFont.uasset：增加命名样式和原有回退别名。
- Docs/LocationReveal.md：本说明。

## 已执行验证（2026-10-27）

编辑器 Development 和 Windows 游戏 Development 目标编译成功。
`TheAwakening.LocationReveal` 3 组自动化测试全部成功，测试内零警告、零错误：

- 正常完成四阶段；Reveal / Holding / Erase 中断。
- 中断期间连续请求保留最新；重复触发和强制重播。
- 正常末端移出不降低 RenderOpacity；外部设为 .5 时中断仍从 .5 开始，半程为 .25。
- 请求版本使旧 cue 无效，重启清理 cue 标记；销毁取消待播工作。
- 空引用及不存在的 Wwise 资产软路径均正常完成。
- 真实项目 PlayerController 的 Ownership 不发生任何转换，Slate 不接受焦点和命中。
- 统一字体中存在新样式并指向导入字体。

报告：`Saved/Automation/LocationReveal/index.html` / `index.json`。
真实 UE 渲染：`Saved/LocationReveal/{Reveal,Holding,Erase,Tail,Interrupted,Holding720,Holding1440}.png`。
1280×720、1920×1080、2560×1440 均生成并检查布局。

对 WBP_LocationReveal、TA_GlobalFont、GoogleSans_Medium 的显式包列表执行 Windows
CookSinglePackage 成功，零错误、零警告；已核实三个包均输出到 Saved/Cooked/Windows。
最初使用 NoGameAlwaysCook 的普通定向 Cook 遇到 Wwise 未初始化平台 cooker 的错误；
插件源码明确允许的 CookSinglePackage 模式解决了该次验证命令的问题，无需修改 Wwise 插件。

完整打包并启动实际剧情关卡的端到端回归、真实鼠标/手柄操作和未来 Wwise 音效听感尚未执行。
已验证编译、目标资产 Cook、实际离屏渲染、输入策略与状态逻辑；这些不等同于完整包实机验收。

重建资产：`UnrealEditor-Cmd <uproject> -run=TALocationRevealSetup -unattended -nop4 -NullRHI`。
渲染：改用 `-Render -AllowCommandletRendering` 并移除 `-NullRHI`。
现有字体和 Widget BP 不会被覆盖；全局字体的同名样式可重复同步。
