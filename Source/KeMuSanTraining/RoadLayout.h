// 科目三考试路线几何布局（局部坐标，单位：米）
#pragma once

#include "CoreMinimal.h"

namespace RoadLayout
{
	// 整条路线在世界中的偏移（远离模板地图自带的赛道）
	inline const FVector RouteOffset(3000.f, 3000.f, 0.f);

	// 车道
	constexpr float LaneWidth = 3.5f;       // 单车道宽度
	constexpr float RoadHalfWidth = 3.5f;   // 路面半宽（双向两车道共 7m）
	constexpr float CurbDistance = 4.6f;    // 路缘石离中心线距离
	constexpr float RightLaneY = LaneWidth * 0.5f;   // 朝 +X 行驶时的右车道中心
	constexpr float LeftLaneY = -LaneWidth * 0.5f;   // 朝 +X 行驶时的左车道中心
	constexpr float CarHalfWidth = 0.9f;    // 考试车半宽

	// 起点与终点
	constexpr float StartX = 0.f;           // 起点（上车准备 / 灯光模拟 / 起步）
	constexpr float RoadMinX = -20.f;
	constexpr float RoadEndX = 520.f;       // 道路尽头（掉头点之后）
	constexpr float FinishX = 370.f;        // 终点（靠边停车完成处）

	// 考试路段（沿 +X 方向）
	constexpr float StraightStartX = 20.f;  // 直线行驶
	constexpr float StraightEndX = 70.f;
	constexpr float LaneChangeStartX = 82.f;  // 变更车道（先左后右）
	constexpr float LaneChangeMidX = 97.f;
	constexpr float LaneChangeEndX = 112.f;
	constexpr float CrosswalkX = 113.f;     // 人行横道中心
	constexpr float StopLineX = 114.8f;     // 停止线
	constexpr float IntersectionMinX = 116.f; // 路口范围
	constexpr float IntersectionMaxX = 124.f;
	constexpr float SchoolStartX = 150.f;   // 学校区域
	constexpr float SchoolEndX = 185.f;
	constexpr float BusStartX = 200.f;      // 公交车站
	constexpr float BusEndX = 235.f;
	constexpr float MeetingStartX = 250.f;  // 会车
	constexpr float MeetingEndX = 290.f;
	constexpr float OvertakeStartX = 300.f; // 超车
	constexpr float OvertakeEndX = 350.f;
	constexpr float GearStartX = 365.f;     // 加减挡操作
	constexpr float GearEndX = 430.f;
	constexpr float UTurnStartX = 442.f;    // 掉头
	constexpr float UTurnEndX = 482.f;
	constexpr float PullOverStartX = 432.f; // 靠边停车（掉头后朝 -X，X 递减）
	constexpr float PullOverEndX = 385.f;

	// 速度限制 km/h
	constexpr float GeneralLimit = 60.f;
	constexpr float ZoneLimit = 30.f;
	constexpr float PullOverLimit = 20.f;
	constexpr float IntersectionLimit = 35.f;

	// 靠边停车（朝 -X 时右侧路缘石 Y）
	constexpr float CurbYHeadingMinusX = -CurbDistance;
}
