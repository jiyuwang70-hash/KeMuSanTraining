# KeMuSanTraining 运行说明

## 运行前准备

1. 安装 Unreal Engine 5.7（或 5.5+）
2. 确认 `D:\honor\Documents\KeMuSanTraining\KeMuSanTraining.uproject` 存在
3. 无需额外依赖 — 项目使用引擎自带基础几何体（Cube/Cylinder/Cone/Plane）和 BasicShapeMaterial

## 编译

```powershell
# 从项目目录生成 VS 工程文件
cd "D:\honor\Documents\KeMuSanTraining"
"<UE_ROOT>\Engine\Build\BatchFiles\Build.bat" KeMuSanTraining Win64 Development "D:\honor\Documents\KeMuSanTraining\KeMuSanTraining.uproject" -WaitMutex
```

或直接在 UE 编辑器中打开 `.uproject`，点击 Compile。

## 运行

### 正常启动（编辑器 PIE）
- 在 UE 编辑器中打开项目，按 Alt+P 启动 Play-In-Editor
- 使用 F1/F2 选择手动挡/自动挡考试模式

### 无头自动测试截图（供 visual_review.py 审核）
```powershell
"<UE_ROOT>\Engine\Binaries\Win64\UnrealEditor.exe" "D:\honor\Documents\KeMuSanTraining\KeMuSanTraining.uproject" /Engine/Maps/Entry -game -autotest -WINDOWED -ResX=1920 -ResY=1080 -NoSplash
```

截图自动保存到 `Saved/Screenshots/`。

### 交通规则回归（可重复）

使用同一套固定种子运行四个交通场景，并自动断言行人让行、社会车辆行为、Lit/CSG 视图状态、道路生成和截图输出：

```powershell
cd "D:\honor\Documents\KeMuSanTraining"
PowerShell -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_traffic_regression.ps1
```

默认场景为 `empty`、`crosswalk_yield`、`follow_and_meet`、`full_mix`，默认种子为 `20260823`。单独复现某个场景：

```powershell
PowerShell -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_traffic_regression.ps1 `
  -Scenario crosswalk_yield -Seed 20260823
```

每个场景会按事件窗口运行：`empty` 约 18 秒，`follow_and_meet` 约 30 秒，`full_mix` 约 34 秒，`crosswalk_yield` 约 52 秒（确保自动驾驶能到达斑马线、停车并放行行人）。日志写入 `Saved/Logs/traffic-regression-*.log`，失败返回退出码 `1`，全部通过返回 `0`。场景含义：

- `empty`：无社会车辆，用于区分路线/考试逻辑与交通逻辑。
- `crosswalk_yield`：行人进入候行后再激活固定同向车/对向车冲突，检查车辆停车让行、行人放行和恢复通行顺序。
- `follow_and_meet`：固定跟车和会车目标，检查安全车距、对向错车和回收。
- `full_mix`：固定同向车、对向车、自行车、路口横向车流和停车车辆的综合场景。

可以用 `-traffic-scenario` 和 `-traffic-seed` 直接传给 Unreal 做单次诊断；回归脚本会自动传入这两个参数并在日志中记录。

## CI 自动回归

仓库中的 `.github/workflows/traffic-regression.yml` 使用 Windows 自托管 Unreal 5.7 runner 执行交通回归。runner 需要满足：

- 标签：`self-hosted`、`Windows`、`ue-5.7`
- 环境变量：`UE_ROOT` 指向 Unreal Engine 5.7 根目录
- 已安装 Visual Studio 2022 C++ 工具链和 Windows SDK
- 执行前没有运行中的 `UnrealEditor.exe`

工作流在 `main` 推送或手动触发时运行，不直接响应 `pull_request`。失败时也会上传 `Saved/Logs` 和 `Saved/Screenshots/WindowsEditor` 作为诊断 artifact。手动运行支持选择单个场景、种子和超时。

## 视觉审核

```powershell
# 设置 API key（不要把真实 key 写进仓库或日志）
set ZHIPU_API_KEY=你的智谱API_KEY

# 批量审核
python3 scripts/visual_review.py --batch "Saved/Screenshots/" --output audit_report.md
```

## 视图模式复位

项目已将 PIE 默认视图固定为 Lit，并关闭 Wireframe、BSP/CSG、Brush、Collision 等调试标志。
如果编辑器仍显示线条，先停止 PIE，再在视口控制台执行：

```text
viewmode lit
showflag.Game 1
showflag.Materials 1
showflag.Wireframe 0
showflag.BSP 0
showflag.BSPTriangles 0
showflag.Brushes 0
showflag.BuilderBrush 0
showflag.Collision 0
showflag.Bounds 0
```

## 控制说明

| 按键 | 功能 |
|------|------|
| F1 | 手动挡考试 |
| F2 | 自动挡考试 |
| F9 | 自由练习 |
| Enter | 确认 / 重新开始 |
| Esc | 暂停 |
| W/S | 油门/刹车 |
| A/D | 转向 |
| 1-5 | 手动换挡 |
| N | 空挡 |
| R | 倒挡 |
| Tab | 自动挡 P/R/N/D 循环 |
| Q/E | 左/右转向灯 |
| L | 灯光循环（关→示廓→近光→远光） |
| J | 远近交替 |
| 空格 | 手刹 |
| F | 安全带 |
| H | 双闪 |
| K | 雾灯 |
| B | 喇叭 |
| M | 观察后视镜 |