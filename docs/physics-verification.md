# 道路、重卡与真实物理验收（2026-10-07，历史构建）

本页对应提交 `71eb7d3` 的水平碰撞构建。后续已增加悬挂、形变与翻覆，最新实现和构建证据见[本轮验收](damage-rollover-verification.md)。

主路由7米拓宽为10米，独立沥青停车带将路侧车辆与人行道分开。交通车池加入9米长、2.5米宽的三轴重卡；14000kg为本项目仿真设定，未声称具体车型实测参数。

车辆使用UE5.7.4内置Chaos动态刚体，考试车1400kg、普通车1500kg、重卡14000kg。配置接触摩擦0.55、恢复系数0.12、CCD与最多8个物理子步。驾驶施加力和转向力矩，AI通过实际位置投影及力控制跟随路线，碰撞后有恢复时间；隐藏回收车辆关闭碰撞。考试车辆接触事件进入原有碰撞扣分/结束规则；练习提示倒车脱离。

当前模拟平整道路的水平位移与偏航，锁定高度、侧倾与俯仰，尚无完整悬挂、翻车或车身变形。碰撞体为车辆包络，不是逐片钣金形状；一般装饰和路缘未加入阻挡物理。

## 本页历史构建与验收

Win64 Development Editor 编译成功（UE5.7.4），仍有既有InputKey弃用警告。DLL SHA-256：`73DB259A7C5313942B82188A7F6AF1D98DCDF691A06BB8739B906FA2444A1FDB`。

以下五套脚本均在此DLL上正常退出，读取当次日志，正式学员.sav与JSON测试前后哈希不变：

- `run_vehicle_physics_test.ps1`：实际Chaos模拟的六类撞击，初速度由夹具赋予，不代表原生按键撞车全流程。
- `run_route_geometry_test.ps1`：连续位置/切线/里程/投影及掉头判定。
- `run_archive_and_analysis_test.ps1`：两进程重新读取档案、分类、去重、失败保护与F3暂停交互。
- `run_input_chain_test.ps1`：原生按键完成准备、打灯、起步、真实位移与刹停，及F1/F2启动契约。
- `run_traffic_regression.ps1`：empty/crosswalk_yield/follow_and_meet/full_mix四场景的事件、显示状态、新截图和完成标志；未声称已验证全程路考、所有跟车间距或所有信号灯通行规则。

| 撞击夹具 | 被撞车峰值速度 m/s | 被撞车峰值偏航 °/s | 被撞车位移 m | 被撞车质量 kg | 结果 |
| --- | ---: | ---: | ---: | ---: | --- |
| 普通车追尾 | 4.253 | 0.522 | 5.321 | 1500.0 | PASS |
| 14吨重卡质量差 | 0.844 | 0.052 | 1.266 | 14000.0 | PASS |
| 偏置侧撞 | 3.873 | 62.113 | 4.268 | 1500.0 | PASS |
| 正面对撞 | 7.989 | 1.039 | 1.203 | 1500.0 | PASS |
| 倒车接触 | 4.258 | 0.589 | 5.294 | 1500.0 | PASS |
| 高速CCD | 33.063 | 5.104 | 53.732 | 1500.0 | PASS |

峰值来自实际游戏帧采样；正面对撞的峰值包含初始运动。重卡与普通车的相同方向撞击显示明显质量差响应。测试不是碰撞方程离线计算，也不以只打印PASS代替实际物理过程。

## 实机画面

道路与停车带来自程序化场景，白车及近处重卡为验收摆位，截图不代表完整人工驾驶通过。

![加宽道路、停车带与重卡](screenshots/12_wide_road_heavy_truck.png)

## 已查阅的GitHub参考

- [Async-Physics-Suspension](https://github.com/fgrenoville/Async-Physics-Suspension)：Apache-2.0，查看了[Vehicle.cpp](https://github.com/fgrenoville/Async-Physics-Suspension/blob/main/Source/AsyncPhxSuspension/Private/Vehicle.cpp)和[AsyncCallback.cpp](https://github.com/fgrenoville/Async-Physics-Suspension/blob/main/Source/AsyncPhxSuspension/Private/AsyncCallback.cpp)，借鉴物理组件作为根、质量配置及力/力矩驱动的架构。本项目使用引擎子步与普通物理组件API，没有复制其异步悬挂回调或实现完整悬挂。
- [JoltPhysics](https://github.com/jrouwe/JoltPhysics)：MIT，查看[VehicleTest.cpp](https://github.com/jrouwe/JoltPhysics/blob/master/Samples/Tests/Vehicle/VehicleTest.cpp)的实际物理场景，参考动态物体/车辆测试方向。未复制代码或引入第二套Jolt运行时。

README中2026-10-06的帧率属于加入本轮刚体物理前的历史构建，本轮未重新测量帧率。
