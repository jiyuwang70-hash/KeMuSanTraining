// 科目三考试路线几何布局与路线投影工具
// 新版：大型城市道路网，东段直行 -> 转角路口右转 -> 北段 ->
// 转角路口右转 -> 西段 -> U-turn掉头返回 -> 靠边停车
// U-turn 由两个 90° 左转弧 + 直连段组成，衔接北侧独立返回车道
#pragma once

#include "CoreMinimal.h"

namespace RoadLayout
{
	// ---- 道路横断面参数（主路拓宽至 10m，双向各 5m）----
	constexpr float LaneWidth = 5.0f;       // 单车道宽度（5m）
	constexpr float RoadHalfWidth = 5.0f;   // 主路路面半宽（双向共 10m）
	constexpr float CurbDistance = RoadHalfWidth + .17f;    // 路缘石离中心线距离（主路段）
	constexpr float CarHalfWidth = 0.9f;    // 考试车半宽
	constexpr float CornerRadius = 14.f;    // 转角路口中心线转弯半径（内弧9m/外弧19m，保障重卡平稳转弯）

	// ---- 主线折线关键点 ----
	// 东段 AB：y=0，x 从 -20 到 506，朝 +X
	// 转角 B：圆弧 (506,0)->(520,14)，圆心 (506,14)
	// 北段 BC：x=520，y 从 14 到 306，朝 +Y
	// 转角 C：圆弧 (520,306)->(506,320)，圆心 (506,306)
	// 西段 CD：y=320，x 从 506 到 U-turn 入口 382，朝 -X
	inline const FVector TrackStart(-20.f, 0.f, 0.f);
	inline const FVector ArcBStart(506.f, 0.f, 0.f);
	inline const FVector ArcBCenter(506.f, 14.f, 0.f);
	inline const FVector ArcBEnd(520.f, 14.f, 0.f);
	inline const FVector ArcCStart(520.f, 306.f, 0.f);
	inline const FVector ArcCCenter(506.f, 306.f, 0.f);
	inline const FVector ArcCEnd(506.f, 320.f, 0.f);
	inline const FVector WestToUTurn(382.f, 320.f, 0.f);   // 西段止于 U-turn 入口

	// ---- 掉头：两段顺时针 90° 左转，中间向北直行 ----
	// 朝西进入、朝北连接、朝东离开；三处接点的位置和切线均连续。
	constexpr float UTurnRadius = 12.f;
	constexpr float UTurnStraightLength = 10.f;
	constexpr float ReturnCenterY = 320.f + 2.f * UTurnRadius + UTurnStraightLength;
	inline const FVector UTurnArc1Center(WestToUTurn.X, WestToUTurn.Y + UTurnRadius, 0.f);
	inline const FVector UTurnMidA(WestToUTurn.X - UTurnRadius, UTurnArc1Center.Y, 0.f);
	inline const FVector UTurnMidB(UTurnMidA.X, UTurnMidA.Y + UTurnStraightLength, 0.f);
	inline const FVector UTurnArc2Center(WestToUTurn.X, UTurnMidB.Y, 0.f);
	inline const FVector ReturnStart(WestToUTurn.X, ReturnCenterY, 0.f);
	inline const FVector ReturnEnd(500.f, ReturnCenterY, 0.f);
	// 在西段北侧的路缘石/人行道上保留掉头入口开口。
	inline const float UTurnClearanceMinX = UTurnMidA.X - CurbDistance - 2.f;
	inline const float UTurnClearanceMaxX = WestToUTurn.X + CurbDistance + 2.f;

	// ---- 里程常量（由折线几何计算，单位米）----
	// 东段: 0 .. 526
	// 弧 B: 526 .. 548
	// 北段: 548 .. 840
	// 弧 C: 840 .. 862
	// 985.982m 起掉头，两个 18.850m 弧及 10m 直段后进入返回车道；
	// 精确里程从同一组几何参数计算，避免手填整数里程与采样轨迹错位。
	constexpr float S_Start = 20.f;           // 起点线
	constexpr float S_ReadyEnd = 34.f;        // 起步完成
	constexpr float StraightStartS = 42.f;    // 直线行驶
	constexpr float StraightEndS = 102.f;
	constexpr float LaneChangeStartS = 116.f; // 变更车道（先左后右）
	constexpr float LaneChangeMidS = 134.f;
	constexpr float LaneChangeEndS = 156.f;
	constexpr float StopLineS = 180.f;        // 路口停止线
	constexpr float CrosswalkS = 184.f;       // 人行横道中心
	constexpr float IntersectionMinS = 186.f; // 路口范围
	constexpr float IntersectionMaxS = 202.f;
	constexpr float SchoolStartS = 230.f;     // 学校区域
	constexpr float SchoolEndS = 264.f;
	constexpr float BusStartS = 284.f;        // 公交车站
	constexpr float BusEndS = 316.f;
	constexpr float CornerBStartS = 520.f;    // 转角路口 B（右转弯）
	constexpr float CornerBEndS = 554.f;
	constexpr float MeetingStartS = 586.f;    // 会车
	constexpr float MeetingEndS = 626.f;
	constexpr float OvertakeStartS = 650.f;   // 超车
	constexpr float OvertakeEndS = 722.f;
	constexpr float CornerCStartS = 836.f;    // 转角路口 C（右转弯）
	constexpr float CornerCEndS = 868.f;
	constexpr float GearStartS = 886.f;       // 加减挡操作
	constexpr float GearEndS = 956.f;
	inline const float UTurnEntryS = FVector::Dist(TrackStart, ArcBStart) +
		PI * CornerRadius + FVector::Dist(ArcBEnd, ArcCStart) + FVector::Dist(ArcCEnd, WestToUTurn);
	inline const float UTurnArc1EndS = UTurnEntryS + 0.5f * PI * UTurnRadius;
	inline const float UTurnStraightEndS = UTurnArc1EndS + UTurnStraightLength;
	inline const float UTurnCompleteS = UTurnStraightEndS + 0.5f * PI * UTurnRadius;
	inline const float ReturnStartS = UTurnCompleteS;
	inline const float PullOverEnterS = ReturnStartS + 33.f;
	inline const float PullOverMinS = ReturnStartS + 41.f;
	inline const float PullOverMaxS = ReturnStartS + 93.f;
	inline const float PullOverFailS = ReturnStartS + 99.f;
	inline const float RoadEndS = ReturnStartS + 104.f;

	// ---- 世界坐标锚点（供场景搭建 / 交通流使用）----
	constexpr float RoadSurfaceZ = 0.061f;                              // 路面顶高（米制，对应6.1cm），供车辆/交通对象贴地
	inline const FVector StartPose(0.f, LaneWidth * 0.5f, RoadSurfaceZ); // 考试车起点（精准贴地，车轮不悬空不深陷）
	constexpr float CrossStreet1X = 173.f;   // 信号路口（跨东段）
	constexpr float CrossStreetBX = 520.f;   // 转角 B 纵向道路
	constexpr float CrossStreetCY = 320.f;   // 转角 C 横向道路
	constexpr float CrossStreet2X = 240.f;   // 西段平交口

	// 速度限制 km/h
	constexpr float GeneralLimit = 60.f;
	constexpr float ZoneLimit = 30.f;
	constexpr float PullOverLimit = 20.f;
	constexpr float IntersectionLimit = 35.f;
	constexpr float UTurnLimit = 25.f;

	// 掉头区判定
	constexpr float UTurnMinLat = -8.f;        // 偏左判定（进入掉头转向）
	constexpr float UTurnCompleteLat = 1.5f;   // 回到返回车道
	constexpr float UTurnHeadingDot = 0.55f;    // 车头与返回段切线点积阈值

	// 靠边停车横向参考
	constexpr float ReturnCurbLateral = RoadHalfWidth;     // 3.5
	constexpr float PullOverGapBase = ReturnCurbLateral - CarHalfWidth; // 2.6


	// ---- 横向方向：lateral = (P - C) · D，D = (-T.Y, T.X) ----
	// 正值表示沿行驶方向的右侧车道
	inline FVector2D LateralDir(const FVector& Tangent)
	{
		return FVector2D(-Tangent.Y, Tangent.X);
	}
}

// 路线轨迹：稠密采样中心线，支持 里程S / 横向偏移 双向换算。
// 全线连续：主环线 → U-turn → 返回段，bReturn 标记返回段样本。
class FRouteTrack
{
public:
	struct FSample
	{
		FVector Pos;
		FVector Tangent;
		float S = 0.f;
		bool bReturn = false;  // true = past U-turn (return lane)
	};

	void Build()
	{
		using namespace RoadLayout;
		Samples.Reset();
		NextS = 0.f;
		MainLength = 0.f;
		const float Step = 1.0f;

		auto AddSeg = [&](const FVector& From, const FVector& To, bool bIsReturn = false)
		{
			const FVector DirV = (To - From).GetSafeNormal();
			const float Len = FVector::Dist(From, To);
			const int32 N = FMath::Max(1, FMath::RoundToInt(Len / Step));
			for (int32 i = 0; i < N; ++i)
			{
				FVector P = From + DirV * (Len * i / N);
				P.Z = 0.f;
				Samples.Add({ P, DirV, NextS + Len * i / N, bIsReturn });
			}
			NextS += Len;
		};

		auto AddArc = [&](const FVector& Center, const FVector& From, const FVector& To, bool bIsReturn = false, bool bClockwise = false)
		{
			const float A0 = FMath::Atan2(From.Y - Center.Y, From.X - Center.X);
			float A1 = FMath::Atan2(To.Y - Center.Y, To.X - Center.X);
			float DA = FMath::UnwindDegrees(FMath::RadiansToDegrees(A1 - A0));
			if (bClockwise && DA > 0.f)
			{
				DA -= 360.f;
			}
			else if (!bClockwise && DA < 0.f)
			{
				DA += 360.f;
			}
			const float Rad = FVector::Dist(Center, From);
			const float Len = FMath::Abs(FMath::DegreesToRadians(DA)) * Rad;
			const int32 N = FMath::Max(2, FMath::RoundToInt(Len / Step));
			for (int32 i = 0; i < N; ++i)
			{
				const float A = A0 + FMath::DegreesToRadians(DA) * (static_cast<float>(i) / N);
				FVector P(Center.X + Rad * FMath::Cos(A), Center.Y + Rad * FMath::Sin(A), 0.f);
				const float Direction = bClockwise ? -1.f : 1.f;
				FVector T(-FMath::Sin(A) * Direction, FMath::Cos(A) * Direction, 0.f);
				Samples.Add({ P, T, NextS + Len * i / N, bIsReturn });
			}
			NextS += Len;
		};

		// Main line: East → Corner B → North → Corner C → West → U-turn entry
		AddSeg(TrackStart, ArcBStart);
		AddArc(ArcBCenter, ArcBStart, ArcBEnd);
		AddSeg(ArcBEnd, ArcCStart);
		AddArc(ArcCCenter, ArcCStart, ArcCEnd);
		AddSeg(ArcCEnd, WestToUTurn);

		MainLength = NextS;  // Main line ends at U-turn entry

		// 掉头段仍属于掉头过程；只有最终东行直段才是返回车道。
		AddArc(UTurnArc1Center, WestToUTurn, UTurnMidA, false, true);
		AddSeg(UTurnMidA, UTurnMidB);
		AddArc(UTurnArc2Center, UTurnMidB, ReturnStart, false, true);

		// Return straight
		AddSeg(ReturnStart, ReturnEnd, true);

		if (Samples.Num() > 0)
		{
			const int32 Mid = Samples.Num() / 3;
			const int32 Last = Samples.Num() - 1;
			UE_LOG(LogTemp, Log, TEXT("[KeMuSan] track built n=%d s0=(%.1f,%.1f,S%.1f,r=%d) smid=(%.1f,%.1f,S%.1f,r=%d) slast=(%.1f,%.1f,S%.1f,r=%d) mainlen=%.1f totallen=%.1f"),
				Samples.Num(),
				Samples[0].Pos.X, Samples[0].Pos.Y, Samples[0].S, Samples[0].bReturn ? 1 : 0,
				Samples[Mid].Pos.X, Samples[Mid].Pos.Y, Samples[Mid].S, Samples[Mid].bReturn ? 1 : 0,
				Samples[Last].Pos.X, Samples[Last].Pos.Y, Samples[Last].S, Samples[Last].bReturn ? 1 : 0,
				MainLength, NextS);
		}
	}

	int32 Num() const { return Samples.Num(); }
	float TotalLength() const { return NextS; }
	float GetMainLength() const { return MainLength; }

	const FSample& GetSample(int32 Idx) const { return Samples[Idx]; }

	// 里程 -> 世界坐标（Lateral 为横向偏移，正值靠右）
	FVector LocAtS(float S, float Lateral) const
	{
		const FSample& Sm = SampleAtS(S);
		const FVector2D D = RoadLayout::LateralDir(Sm.Tangent);
		return FVector(Sm.Pos.X + D.X * Lateral, Sm.Pos.Y + D.Y * Lateral, 0.f);
	}

	// 里程处的路线切线方向
	FVector TangentAtS(float S) const
	{
		return SampleAtS(S).Tangent;
	}

	// 世界坐标 -> 最近样本索引
	int32 NearestSample(const FVector& WorldPos, bool bReturnOnly, bool bAlignedOnly,
		const FVector* Heading, float* OutDistSq = nullptr) const
	{
		int32 Best = INDEX_NONE;
		float BestSq = 1.e9f;
		for (int32 i = 0; i < Samples.Num(); ++i)
		{
			const FSample& Sm = Samples[i];
			if (bReturnOnly != Sm.bReturn)
			{
				continue;
			}
			if (bAlignedOnly && Heading)
			{
				if (FVector::DotProduct(*Heading, Sm.Tangent) < 0.25f)
				{
					continue;
				}
			}
			const float DX = WorldPos.X - Sm.Pos.X;
			const float DY = WorldPos.Y - Sm.Pos.Y;
			const float Sq = DX * DX + DY * DY;
			if (Sq < BestSq)
			{
				BestSq = Sq;
				Best = i;
			}
		}
		if (OutDistSq)
		{
			*OutDistSq = BestSq;
		}
		return Best;
	}

	struct FProjResult
	{
		float S = 0.f;
		float Lateral = 0.f;   // 正值 = 行驶方向右侧
		bool bReturn = false;  // 是否落在返回段
		float DistSq = 0.f;    // 离中心线距离平方
		bool bAligned = false; // 车头方向与该处路线切线一致
	};

	// 投影：优先选择车头朝向一致的支线；U-turn 区提高方向权重
	FProjResult Project(const FVector& WorldPos, const FVector& Heading) const
	{
		FProjResult Res;
		float AlignedSq = 1.e9f;
		int32 AlignedIdx = INDEX_NONE;
		float AnySq = 1.e9f;
		int32 AnyIdx = INDEX_NONE;

		for (int32 i = 0; i < Samples.Num(); ++i)
		{
			const FSample& Sm = Samples[i];
			const float DX = WorldPos.X - Sm.Pos.X;
			const float DY = WorldPos.Y - Sm.Pos.Y;
			const float Sq = DX * DX + DY * DY;
			if (Sq < AnySq)
			{
				AnySq = Sq;
				AnyIdx = i;
			}
			const float Dot = FVector::DotProduct(Heading, Sm.Tangent);
			if (Dot > 0.25f && Sq < AlignedSq)
			{
				AlignedSq = Sq;
				AlignedIdx = i;
			}
		}

		// 掉头区提高方向权重，边界使用同源几何里程。
		const bool bNearUTurn = (Samples.IsValidIndex(AnyIdx) &&
			Samples[AnyIdx].S > RoadLayout::UTurnEntryS - 20.f &&
			Samples[AnyIdx].S < RoadLayout::ReturnStartS + 25.f);
		const float AlignTolerance = bNearUTurn ? 400.f : 25.f;  // 20m vs 5m

		int32 UseIdx = AnyIdx;
		if (AlignedIdx != INDEX_NONE && AlignedSq <= AnySq + AlignTolerance)
		{
			UseIdx = AlignedIdx;
			Res.bAligned = true;
		}
		if (UseIdx == INDEX_NONE)
		{
			Res.DistSq = 1.e9f;
			return Res;
		}

		const FSample& Sm = Samples[UseIdx];
		Res.S = Sm.S;
		Res.bReturn = Sm.bReturn;
		Res.DistSq = (UseIdx == AlignedIdx) ? AlignedSq : AnySq;
		const FVector2D D = RoadLayout::LateralDir(Sm.Tangent);
		Res.Lateral = (WorldPos.X - Sm.Pos.X) * D.X + (WorldPos.Y - Sm.Pos.Y) * D.Y;
		return Res;
	}

private:
	const FSample& SampleAtS(float S) const
	{
		S = FMath::Clamp(S, 0.f, NextS - 0.01f);
		int32 Lo = 0, Hi = Samples.Num() - 1;
		while (Lo < Hi)
		{
			const int32 Mid = (Lo + Hi) / 2;
			if (Samples[Mid].S < S)
			{
				Lo = Mid + 1;
			}
			else
			{
				Hi = Mid;
			}
		}
		return Samples[FMath::Clamp(Lo, 0, Samples.Num() - 1)];
	}

	TArray<FSample> Samples;
	float NextS = 0.f;
	float MainLength = 0.f;
};