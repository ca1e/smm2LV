# Toost

基于 [JiXiaomai 的 SMM2LevelViewer](https://github.com/JiXiaomai/SMM2LevelViewer)
与 TheGreatRambler 的上游 [toost](https://github.com/TheGreatRambler/toost)
打造的 Super Mario Maker 2 关卡查看器,采用现代 Web 架构。

关卡的解密、解析与光栅化全部在 WebAssembly 核心内完成;前端只负责界面、
网络与交互,以 Docker 内的轻量静态服务器交付。

## 功能

### 加载与浏览

- **三种入口**:输入 9 位关卡 ID 直接下载;加载本地 `.bcd` 文件(加密、
  明文、gzip 三种形态通吃);按来源与难度随机抽取(New / Popular /
  Endless × 四档难度)。
- **Live 实时流**:🔥 Hot 一键载入此刻最多人观看的关卡,Live list 列出
  当前正在被观看的全部关卡,点击即渲染。
- **双视口**:overworld 与 subworld 上下分区,各自独立拖拽平移、以光标
  为锚点的滚轮缩放,悬停显示瓦片坐标;分割条可拖动调整两区高度,配合
  OVER / SPLIT / SUB 焦点预设与 ⟲ 复位(快捷键 R,双击复位当前视口)。
- **小地图**:每个视口自带 minimap,红框标记当前视野,点击直接跳转。

### 顶栏

顶栏固定分两行:第一行是发现类控件(本地加载、ID 输入、随机抽取、
Live),第二行是显示类控件:

- 五个渲染开关:**Grid**(瓦片网格)、**Pipes**(物体绘制于管道之上)、
  **Deaths**(死亡热力图)、**Pins**(评论图钉)、**Compare**(官方渲染对比);
- **导出倍率**:PNG 导出的缩放系数(0.125–8);
- **关卡名与 🌙 night 徽标**:当前关卡的 ID、名称与夜晚标记;
- **用户卡片**(见下节);
- **ℹ Info**:开关信息面板。

### 用户卡片

载入在线关卡后,顶栏右侧显示上传者的紧凑卡片:Mii 头像、[国家码] 名字
与 maker code。点击卡片进入其制作者页;悬停展开完整面板,包含:

- **Uploader / First clear / World record** 三个用户,均可点击进入各自
  的制作者页;
- **关卡统计**:上传时间、通关时间、游玩数、通关数、通关率、点赞、
  踩、评论数。

### 信息面板

右上角浮层,在四个视图间切换:

- **关卡信息**:名称、描述、难度、标签、通关条件、风格/主题/夜晚、
  尝试次数、计时器、终点坐标、自动卷轴、物体数量;
- **制作者页**:游玩/通关/尝试/死亡/首通/获赞/制作者点/世界纪录/上传数/
  最近活跃等统计,对战战绩与 Endless 高分,徽章列表,Super World 入口,
  以及上传关卡完整列表(点击任意一关立即渲染);
- **Super World**:世界地图缩略图与全部关卡,点击渲染;
- **Live list**:实时观看列表。

### 叠加层

由 wasm 核心渲染为与关卡同网格的透明 PNG,前端只做图层合成:

- **死亡热力图**:基于 API 的死亡日志,标出玩家真正倒下的位置;
- **评论图钉**:关卡内评论按原始坐标显示,悬停气泡展示内容、作者与
  日期;
- **官方渲染对比**:官方整关缩略图恰为 1/3 等比的 overworld 渲染,以
  50% 透明度叠加即可逐像素比对差异。

### 导出与分享

- **Save PNG**:按世界导出渲染图,当前激活的热力图/评论层一并合成;
- **Save JSON**:导出完整结构化关卡数据(地面、物体、轨道、音效等);
- **分享链接**:`?level=关卡ID`、`?maker=制作者ID`、`?super=Super World ID`
  打开即直达对应内容。

## 架构

```
index.html(单文件 ES module,无构建步骤)
├── 界面:两行顶栏 + 双视口 canvas + 信息面板浮层
├── 网络:tgrcode API(统一缓存与退避)、Live 流、资产按需加载
├── 交互:拖拽平移 / 锚点缩放 / 悬停 / 评论气泡 / 视口分割
├── 约 40 行 WASI shim(模块所需的全部宿主导入)
└── WebAssembly.instantiate → toost.wasm

toost.wasm(wasi-sdk clang,wasm32-wasip1,reactor 模式)
├── 关卡解密(AES-128-CBC + PRNG 密钥表)、gzip 解码(vendored zlib)
├── 二进制解析(kaitai)与 JSON 导出(rapidjson)
├── 软件光栅化:自动贴图、斜坡、液体、夜晚调色板
└── 叠加层渲染:死亡热力图、评论图钉(游戏坐标 → 渲染坐标)
```

一切与几何或像素相关的逻辑都在 wasm 核心内;前端只负责搬运字节,在
2D canvas 上按层合成 `drawImage`。

## 构建与运行

Docker(推荐,多阶段构建:wasi-sdk → binaryen → busybox httpd):

```
docker build --tag toost .
docker run -p 8080:8080 toost
```

本地构建需要 [wasi-sdk](https://github.com/WebAssembly/wasi-sdk) 在 PATH
中(官方镜像挂载于 `/opt/wasi-sdk`):

```
make PLATFORM=web BUILD=release
# → bin/toost.wasm,与 index.html、img/ 一同由服务器提供
```

发布模块经 `wasm-opt -Oz` 压缩,并同时提供 brotli 预压缩的 `.wasm.br`;
支持 `DecompressionStream('br')` 的浏览器自动拉取约 150KB 的压缩版本,
其余回退到原始模块。

## 数据来源

关卡数据与元数据来自
[TheGreatRambler 的公开 MM2 API](https://tgrcode.com/mm2/docs)
([MariOver](https://github.com/TheGreatRambler/MariOver) 的托管实例)。
该 API 运行在作者的个人 Nintendo 账号上——本查看器对所有响应做缓存,
随机抽取每次只请求一关,绝不批量抓取。

## 致谢

* Ji Xiaomai — 核心关卡渲染器基于其工作
* TheGreatRambler — 上游 toost、公开 API 与素材
* The Spriters Resource — 素材
