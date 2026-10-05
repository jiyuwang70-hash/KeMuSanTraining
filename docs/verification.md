# 科目三模拟驾驶系统 - 验收与实测报告
(KeMuSan Road Test Simulator - Verification & Benchmark Report)

## 一、构建与二进制指纹

- **引擎版本**：Unreal Engine 5.7.4 (CL-51494982)
- **编译构建**：Win64 Development Editor Target
- **编译结果**：编译成功，0 errors，存在 UE 5.7 旧 InputKey 接口弃用警告
- **核心二进制哈希**：
  - `Binaries/Win64/UnrealEditor-KeMuSanTraining.dll`: `EB374D3A0B375A168CB24080A428B3CB59B61B226CA0E5EDD510DCCC2F550FA7`

---

## 二、自动化回归测试结果

所有套件串行单进程执行；驾驶输入旅程与 F1/F2 通过 InputKey，交通场景与静态镜头分别验证：

| 测试套件 | 脚本 | 测试内容 | 结果 |
| :--- | :--- | :--- | :--- |
| **真实按键输入链** | `scripts/run_input_chain_test.ps1` | case 0～24 共 25 个原生按键动作步（包含 Enter/Space/1/W/A/Q/B/M/T/Tab/Arrows/R/V/Esc/F/Driving/Brake 等完整闭环）与 case 25 汇总断言，及 F1/F2 考试模式独立启动契约 | **PASS** (全部 25 步及双考试模式通过) |
| **四场景交通流回归** | `scripts/run_traffic_regression.ps1` | 4 大场景（`empty`、`crosswalk_yield`、`follow_and_meet`、`full_mix`） | **PASS** (4 场景全部按既定规则完成) |
| **全流程展示截图捕获** | `scripts/run_showcase_capture.ps1` | 7 阶段实机画面捕获与镜头跟车断言 | **PASS** (7 张截图全部达成且通过物理断言) |

---

## 三、性能实测采样（CSV Profiler）

在真实 `-autotest` 完整实机管线（包含每 4 秒定时截图任务与动态交通流）下，通过官方 CSV Profiler 执行长时物理墙钟采样：

- **测试场景**：`full_mix`（综合交通场景，包含社会车巡航、超车、行人过街避让与红绿灯路口）
- **随机种子**：`20260823`
- **渲染分辨率**：1280 × 720 (窗口化，Lit 完整真实光照，HISM 批量实例化)
- **后视镜模式**：3 面动态后视镜时间切片轮询渲染 (Round-Robin 30Hz)
- **冷启动预热**：20.058 秒（严格丢弃预热阶段数据）
- **采样物理墙钟时间**：30.032 秒
- **有效采样总帧数**：1466 帧
- **实测平均帧率**：**48.814 FPS**
- **原始数据 CSV SHA-256**：`E5C969AAAF3C0DA6AB11A0A49CF20695CBB693320F131F73C2AE70857FEDE027`

---

## 四、核心系统展示图集

保存在 `docs/screenshots/` 目录下：

1. `01_main_menu.png`：主菜单与 26pt 矢量汉字大标题、模式选择卡片；
2. `02_real_driving.png`：真实加速行车动态（车速 20km/h，位移 >14m，起步完成进入考试路段）；
3. `03_cockpit_view.png`：第一人称主驾驶沉浸座舱（中空三辐方向盘、中心轮毂、三面后视镜）；
4. `04a_mirror_standard.png`：后视镜光学微调工作台 - 默认镜面角度；
5. `04b_mirror_adjusted.png`：后视镜光学微调工作台 - 多步俯仰与偏航微调（车身完全静止同位反射对比）；
6. `05_corner_90deg.png`：北向右车道 90° 静态镜头测试（跟车摄像机物理对齐）；
7. `06_uturn_180deg.png`：西向右车道 180° 掉头静态镜头测试（跟车摄像机物理对齐）。
