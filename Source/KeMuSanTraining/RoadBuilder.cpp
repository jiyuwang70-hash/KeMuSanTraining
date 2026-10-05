#include "RoadBuilder.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

using namespace RoadLayout;

namespace
{
	// 真实沉稳的高质感哑光柏油沥青
	const FLinearColor ColAsphalt(0.12f, 0.13f, 0.14f);
	// 细磨水泥人行道（现代暖灰铺装）
	const FLinearColor ColWalkway(0.52f, 0.51f, 0.49f);
	// 立体路缘石与街角石
	const FLinearColor ColCurb(0.66f, 0.66f, 0.68f);
	// 沉稳自然的园林草地墨绿（非刺眼荧光绿）
	const FLinearColor ColGrass(0.07f, 0.18f, 0.08f);
	// 醒目哑光标线
	const FLinearColor ColLineWhite(0.95f, 0.95f, 0.94f);
	const FLinearColor ColLineYellow(0.95f, 0.80f, 0.10f);
	// 真实天然树干深褐色
	const FLinearColor ColTrunk(0.22f, 0.16f, 0.10f);
}

ARoadBuilder::ARoadBuilder()
{
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// 真实午后倾斜日光（产生漂亮长阴影，增强场景纵深与立体感）
	Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetupAttachment(Root);
	Sun->SetRelativeRotation(FRotator(-38.f, 48.f, 0.f));
	Sun->SetIntensity(3.6f); // 均衡真实光照强度，彻底杜绝高光过曝泛白
	Sun->SetLightColor(FLinearColor(1.0f, 0.98f, 0.94f));
	Sun->SetCastShadows(true);

	// 环境天光漫反射
	Sky = CreateDefaultSubobject<USkyLightComponent>(TEXT("Sky"));
	Sky->SetupAttachment(Root);
	Sky->SetIntensity(0.55f); // 柔和天光阴影补光
	Sky->SourceType = SLS_CapturedScene;

	Atmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("Atmosphere"));
	Atmosphere->SetupAttachment(Root);
	Atmosphere->SetVisibility(true);

	// 大气透视高度雾：平滑地平线，消除突兀天空切边，增强空气纵深感
	HeightFog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("HeightFog"));
	HeightFog->SetupAttachment(Root);
	HeightFog->SetFogDensity(0.0008f);
	HeightFog->SetFogHeightFalloff(0.0012f);
	HeightFog->SetFogInscatteringColor(FLinearColor(0.66f, 0.76f, 0.88f));
	HeightFog->SetStartDistance(3200.f);

	// 后期处理：锁定写实曝光与色彩对比度，还原深黑柏油沥青与饱满城市色彩
	PostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcess"));
	PostProcess->SetupAttachment(Root);
	PostProcess->bUnbound = true;

	PostProcess->Settings.bOverride_AutoExposureMethod = true;
	PostProcess->Settings.AutoExposureMethod = AEM_Histogram;
	PostProcess->Settings.bOverride_AutoExposureMinBrightness = true;
	PostProcess->Settings.AutoExposureMinBrightness = 1.0f;
	PostProcess->Settings.bOverride_AutoExposureMaxBrightness = true;
	PostProcess->Settings.AutoExposureMaxBrightness = 2.5f;
	PostProcess->Settings.bOverride_AutoExposureBias = true;
	PostProcess->Settings.AutoExposureBias = -0.7f; // 压暗高光溢出，使路面深沉、白线醒目

	PostProcess->Settings.bOverride_ColorSaturation = true;
	PostProcess->Settings.ColorSaturation = FVector4(1.08f, 1.08f, 1.08f, 1.0f);
	PostProcess->Settings.bOverride_ColorContrast = true;
	PostProcess->Settings.ColorContrast = FVector4(1.06f, 1.06f, 1.06f, 1.0f);

	PostProcess->Settings.bOverride_AmbientOcclusionIntensity = true;
	PostProcess->Settings.AmbientOcclusionIntensity = 0.85f;
	PostProcess->Settings.bOverride_AmbientOcclusionRadius = true;
	PostProcess->Settings.AmbientOcclusionRadius = 160.f;
}

void ARoadBuilder::BeginPlay()
{
	Super::BeginPlay();

	CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	CylMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	ConeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));
	PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	BaseMat = LoadObject<UMaterial>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	Track.Build();

	BuildGround();
	BuildRoadSurface();
	BuildMarkings();
	BuildZoneFacilities();
	BuildStreetFurniture();
	BuildCityBlocks();

	FinishBuild();

	UE_LOG(LogTemp, Log, TEXT("[KeMuSan] road builder done, pieces=%d, hism_groups=%d"), PiecesPlaced, HISMComponents.Num());
}

void ARoadBuilder::FinishBuild()
{
	// 刷新所有 HISM 实例树，批量提交渲染几何
	for (UHierarchicalInstancedStaticMeshComponent* Comp : HISMComponents)
	{
		if (Comp)
		{
			Comp->BuildTreeIfOutdated(true, true);
		}
	}

	// 天光使用 CapturedScene，需要在场景搭建完成后重新捕捉一次，
	// 否则使用引擎默认 cubemap，整体色调会偏蓝/发灰
	if (Sky)
	{
		Sky->RecaptureSky();
	}
}

// ---------------------------------------------------------------------------
// 基础放置工具
// ---------------------------------------------------------------------------
UMaterialInstanceDynamic* ARoadBuilder::GetMat(const FLinearColor& Color)
{
	const FColor C = Color.ToFColor(true);
	const uint32 Key = (static_cast<uint32>(C.R) << 24) | (static_cast<uint32>(C.G) << 16) |
		(static_cast<uint32>(C.B) << 8) | static_cast<uint32>(C.A);
	if (UMaterialInstanceDynamic** Found = MatCache.Find(Key))
	{
		return *Found;
	}
	UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(BaseMat, this);
	// 兼容不同引擎版本：同时设置 Color / BaseColor 两个参数名
	D->SetVectorParameterValue(FName("Color"), Color);
	D->SetVectorParameterValue(FName("BaseColor"), Color);

	MatCache.Add(Key, D);
	return D;
}

UStaticMeshComponent* ARoadBuilder::AddPiece(UStaticMesh* Mesh, const FVector& Loc, const FVector& Extents,
	const FLinearColor& Color, const FRotator& Rot, bool bRaiseZ)
{
	if (!Mesh)
	{
		return nullptr;
	}

	UMaterialInstanceDynamic* Mat = GetMat(Color);
	const FHISMGroupKey Key{ Mesh, Mat };

	UHierarchicalInstancedStaticMeshComponent* HISM = nullptr;
	if (UHierarchicalInstancedStaticMeshComponent** Found = HISMMap.Find(Key))
	{
		HISM = *Found;
	}
	else
	{
		HISM = NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
		HISM->SetStaticMesh(Mesh);
		HISM->SetMaterial(0, Mat);
		HISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		HISM->SetMobility(EComponentMobility::Static);
		HISM->SetCastShadow(true);
		HISM->AttachToComponent(Root, FAttachmentTransformRules::KeepRelativeTransform);
		HISM->RegisterComponent();
		HISMMap.Add(Key, HISM);
		HISMComponents.Add(HISM);
	}

	FVector UseLoc = Loc * 100.0f;
	if (bRaiseZ)
	{
		// Layer-based Z offset in centimeters to avoid Z-fighting
		UseLoc.Z += PieceLayerZ * 100.0f;
	}

	const FTransform InstanceTransform(Rot, UseLoc, Extents);
	HISM->AddInstance(InstanceTransform, true);
	++PiecesPlaced;
	return nullptr;
}

UStaticMeshComponent* ARoadBuilder::AddBox(const FVector& Loc, const FVector& Extents, const FLinearColor& Color,
	const FRotator& Rot)
{
	return AddPiece(CubeMesh, Loc, Extents, Color, Rot, false);
}

UStaticMeshComponent* ARoadBuilder::AddCylinder(const FVector& Loc, const FVector& Extents, const FLinearColor& Color,
	const FRotator& Rot)
{
	return AddPiece(CylMesh, Loc, Extents, Color, Rot, false);
}

UStaticMeshComponent* ARoadBuilder::AddCone(const FVector& Loc, const FVector& Extents, const FLinearColor& Color,
	const FRotator& Rot)
{
	return AddPiece(ConeMesh, Loc, Extents, Color, Rot, false);
}

// ---------------------------------------------------------------------------
// 地面
// ---------------------------------------------------------------------------
void ARoadBuilder::BuildGround()
{
	PieceLayerZ = 0.0f;
	// 大面积草地（覆盖整个城市区域）
	AddPiece(PlaneMesh, FVector(300.f, 160.f, -0.22f), FVector(2800.f, 2800.f, 1.f), ColGrass, FRotator::ZeroRotator, false);

	// 色斑变化，避免大平面单调
	FRandomStream Rand(77);
	for (int32 i = 0; i < 46; ++i)
	{
		const float X = Rand.FRandRange(-900.f, 1500.f);
		const float Y = Rand.FRandRange(-1100.f, 1300.f);
		const float SX = Rand.FRandRange(30.f, 90.f);
		const float SY = Rand.FRandRange(25.f, 70.f);
		FLinearColor C = ColGrass;
		C.R += Rand.FRandRange(-0.04f, 0.05f);
		C.G += Rand.FRandRange(-0.05f, 0.05f);
		C.B += Rand.FRandRange(-0.02f, 0.03f);
		AddPiece(PlaneMesh, FVector(X, Y, -0.20f), FVector(SX, SY, 1.f), C, FRotator::ZeroRotator, false);
	}
}

// ---------------------------------------------------------------------------
// 路面与人行道
// ---------------------------------------------------------------------------
void ARoadBuilder::AsphaltRect(const FVector2D& Min, const FVector2D& Max)
{
	const FVector Center((Min.X + Max.X) * 0.5f, (Min.Y + Max.Y) * 0.5f, -0.02f);
	const FVector Extents(FMath::Max(0.1f, Max.X - Min.X), FMath::Max(0.1f, Max.Y - Min.Y), 0.16f);
	AddPiece(CubeMesh, Center, Extents, ColAsphalt);
}

void ARoadBuilder::WalkwayStrip(const FVector2D& Min, const FVector2D& Max)
{
	if (Max.X <= Min.X || Max.Y <= Min.Y)
	{
		return;
	}
	const FVector Center((Min.X + Max.X) * 0.5f, (Min.Y + Max.Y) * 0.5f, 0.08f);
	const FVector Extents(Max.X - Min.X, Max.Y - Min.Y, 0.16f);
	AddPiece(CubeMesh, Center, Extents, ColWalkway);
}

void ARoadBuilder::BuildRoadSurface()
{
	PieceLayerZ = 0.001f;
	// ---- 主线与横向街道沥青 ----
	AsphaltRect(FVector2D(-30.f, -3.5f), FVector2D(506.f, 3.5f));     // 东段 AB
	AsphaltRect(FVector2D(506.f, -4.f), FVector2D(529.f, 21.f));      // 转角 B 区域
	AsphaltRect(FVector2D(512.f, -60.f), FVector2D(528.f, 420.f));    // 纵向街道（穿转角 B）
	AsphaltRect(FVector2D(516.5f, 21.f), FVector2D(523.5f, 306.f));   // 北段 BC
	AsphaltRect(FVector2D(498.f, 306.f), FVector2D(534.f, 334.f));    // 转角 C 区域
	AsphaltRect(FVector2D(534.f, 316.5f), FVector2D(664.f, 323.5f));  // 横向街道（转角 C 以东）
	AsphaltRect(FVector2D(-30.f, 316.5f), FVector2D(498.f, 323.5f));  // 西段 CD
	AsphaltRect(FVector2D(166.f, -80.f), FVector2D(180.f, 420.f));    // 信号路口纵向街 CS1
	AsphaltRect(FVector2D(233.f, 180.f), FVector2D(247.f, 420.f));    // 西段平交口 CS2

	// ---- 人行道（在路口处断开）----
	struct FRun { float A, B; };
	auto WalkBandX = [&](float Y1, float Y2, const TArray<FRun>& Runs)
	{
		for (const FRun& R : Runs)
		{
			WalkwayStrip(FVector2D(R.A, Y1), FVector2D(R.B, Y2));
		}
	};
	auto WalkBandY = [&](float X1, float X2, const TArray<FRun>& Runs)
	{
		for (const FRun& R : Runs)
		{
			WalkwayStrip(FVector2D(X1, R.A), FVector2D(X2, R.B));
		}
	};

	// 东段两侧
	const TArray<FRun> AbRuns = { { -30.f, 164.f }, { 182.f, 230.f }, { 250.f, 504.f } };
	WalkBandX(3.85f, 6.35f, AbRuns);
	WalkBandX(-6.35f, -3.85f, AbRuns);

	// 西段两侧
	const TArray<FRun> CdRuns = { { -30.f, 164.f }, { 182.f, 230.f }, { 250.f, 498.f } };
	WalkBandX(323.85f, 326.35f, CdRuns);
	WalkBandX(313.65f, 316.15f, CdRuns);

	// 北段两侧
	WalkBandY(523.85f, 526.35f, { { 21.f, 306.f } });
	WalkBandY(513.65f, 516.15f, { { 21.f, 306.f } });

	// 转角 B 纵向街道两侧
	WalkBandY(528.35f, 530.85f, { { -60.f, -10.f }, { 10.f, 306.f }, { 334.f, 420.f } });
	WalkBandY(509.15f, 511.65f, { { -60.f, -10.f }, { 10.f, 306.f }, { 334.f, 420.f } });

	// 信号路口 CS1 两侧
	WalkBandY(163.15f, 165.65f, { { -80.f, -10.f }, { 10.f, 306.f }, { 334.f, 420.f } });
	WalkBandY(180.35f, 182.85f, { { -80.f, -10.f }, { 10.f, 306.f }, { 334.f, 420.f } });

	// 平交口 CS2 两侧
	WalkBandY(230.15f, 232.65f, { { 180.f, 306.f }, { 334.f, 420.f } });
	WalkBandY(247.35f, 249.85f, { { 180.f, 306.f }, { 334.f, 420.f } });

	// 转角 C 以东街道两侧
	WalkBandY(534.f, 664.f, {}); // 占位避免未使用告警
	WalkwayStrip(FVector2D(534.f, 328.35f), FVector2D(664.f, 330.85f));
	WalkwayStrip(FVector2D(534.f, 309.15f), FVector2D(664.f, 311.65f));

	// ---- 掉头区路面（南绕弧）及返回车道 ----
	// U-turn path: rectangular zone south of west segment for the car to turn through
	AsphaltRect(FVector2D(340.f, 290.f), FVector2D(400.f, 320.f));   // U-turn zone
	// Return lane: y=323.5, x from 340 to 486
	AsphaltRect(FVector2D(340.f, 320.f), FVector2D(486.f, 327.f));

	// ---- 主线路缘石 ----
	auto CurbStripX = [&](float YCenter, const TArray<FRun>& Runs)
	{
		for (const FRun& R : Runs)
		{
			AddPiece(CubeMesh, FVector((R.A + R.B) * 0.5f, YCenter, 0.09f),
				FVector(R.B - R.A, 0.34f, 0.18f), ColCurb);
		}
	};
	CurbStripX(3.67f, AbRuns);
	CurbStripX(-3.67f, AbRuns);
	CurbStripX(323.67f, CdRuns);
	CurbStripX(316.33f, CdRuns);
	AddPiece(CubeMesh, FVector(520.f, 163.5f, 0.09f), FVector(0.34f, 285.f, 0.18f), ColCurb);
	AddPiece(CubeMesh, FVector(523.67f, 163.5f, 0.09f), FVector(0.34f, 285.f, 0.18f), ColCurb);
}

// ---------------------------------------------------------------------------
// 标线
// ---------------------------------------------------------------------------
void ARoadBuilder::PaintZebra(const FVector& Center, float FlowYawDeg, float HalfSpan)
{
	const float Rad = FMath::DegreesToRadians(FlowYawDeg);
	const FVector Flow(FMath::Cos(Rad), FMath::Sin(Rad), 0.f);
	const FVector SideDir(-Flow.Y, Flow.X, 0.f);
	for (float Lat = -HalfSpan + 0.31f; Lat <= HalfSpan - 0.31f + 0.01f; Lat += 0.64f)
	{
		const FVector P = Center + SideDir * Lat;
		AddBox(P, FVector(3.4f, 0.42f, 0.04f), ColLineWhite, FRotator(0.f, FlowYawDeg, 0.f));
	}
}

void ARoadBuilder::PaintArrow(const FVector& Pos, float YawDeg)
{
	const float Rad = FMath::DegreesToRadians(YawDeg);
	const FVector Flow(FMath::Cos(Rad), FMath::Sin(Rad), 0.f);
	AddBox(Pos, FVector(2.6f, 0.26f, 0.03f), ColLineWhite, FRotator(0.f, YawDeg, 0.f));
	const FVector HeadPos = Pos + Flow * 2.1f + FVector(0.f, 0.f, 0.005f);
	AddCone(HeadPos, FVector(1.5f, 0.06f, 1.4f), ColLineWhite, FRotator(-90.f, YawDeg, 0.f));
}

void ARoadBuilder::BuildMarkings()
{
	PieceLayerZ = 0.005f;
	// ---- 中心黄色虚线（沿主线采样，路口区域断开）----
	auto InSkipZone = [](float S)
	{
		return (S > 174.f && S < 208.f) ||   // 信号路口
			(S > 520.f && S < 556.f) ||      // 转角 B
			(S > 836.f && S < 872.f) ||      // 转角 C
			(S > 1116.f && S < 1140.f);      // 平交口 CS2
	};
	const float MainLen = Track.GetMainLength();
	for (float S = 8.f; S < MainLen - 6.f; S += 4.f)
	{
		if (InSkipZone(S))
		{
			continue;
		}
		const FVector Pos = Track.LocAtS(S + 1.f, 0.f);
		const FVector T = Track.TangentAtS(S + 1.f);
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(T.Y, T.X));
		AddBox(FVector(Pos.X, Pos.Y, 0.068f), FVector(2.0f, 0.14f, 0.03f), ColLineYellow, FRotator(0.f, Yaw, 0.f));
	}

	// ---- 边缘白色实线（直段长条 + 弯道分段）----
	auto EdgeX = [&](float X, float Y1, float Y2)
	{
		AddBox(FVector(X, (Y1 + Y2) * 0.5f, 0.066f), FVector(0.14f, Y2 - Y1, 0.03f), ColLineWhite);
	};
	auto EdgeY = [&](float Y, float X1, float X2)
	{
		AddBox(FVector((X1 + X2) * 0.5f, Y, 0.066f), FVector(X2 - X1, 0.14f, 0.03f), ColLineWhite);
	};
	// 东段
	EdgeY(3.5f, -30.f, 160.f);  EdgeY(3.5f, 190.f, 506.f);
	EdgeY(-3.5f, -30.f, 160.f); EdgeY(-3.5f, 190.f, 506.f);
	// 北段
	EdgeX(516.5f, 21.f, 306.f); EdgeX(523.5f, 21.f, 306.f);
	// 西段
	EdgeY(316.5f, -30.f, 158.f); EdgeY(316.5f, 190.f, 228.f); EdgeY(316.5f, 252.f, 498.f);
	EdgeY(323.5f, -30.f, 158.f); EdgeY(323.5f, 190.f, 228.f); EdgeY(323.5f, 252.f, 498.f);
	// 弯道弧段
	auto ArcEdges = [&](float FromS, float ToS)
	{
		for (float S = FromS; S <= ToS; S += 2.2f)
		{
			for (float Lat : { -3.55f, 3.55f })
			{
				const FVector P = Track.LocAtS(S, Lat);
				const FVector T = Track.TangentAtS(S);
				const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(T.Y, T.X));
				AddBox(FVector(P.X, P.Y, 0.066f), FVector(2.3f, 0.08f, 0.03f), ColLineWhite, FRotator(0.f, Yaw, 0.f));
			}
		}
	};
	ArcEdges(527.f, 547.f);
	ArcEdges(841.f, 861.f);
	// 横向街道边缘
	EdgeX(166.f, -78.f, 418.f); EdgeX(180.f, -78.f, 418.f);
	EdgeX(512.f, -58.f, 418.f); EdgeX(528.f, -58.f, 418.f);
	EdgeX(233.f, 182.f, 418.f); EdgeX(247.f, 182.f, 418.f);
	EdgeY(316.5f, 536.f, 662.f); EdgeY(323.5f, 536.f, 662.f);

	// ---- 起点线 ----
	AddBox(FVector(StartPose.X, 0.f, 0.070f), FVector(0.35f, 7.f, 0.04f), ColLineWhite);

	// ---- 信号路口：停止线 + 斑马线 ----
	AddBox(FVector(160.f, 1.78f, 0.070f), FVector(0.32f, 3.35f, 0.04f), ColLineWhite);   // 东行停止线
	AddBox(FVector(186.f, -1.78f, 0.070f), FVector(0.32f, 3.35f, 0.04f), ColLineWhite);  // 西行停止线
	PaintZebra(FVector(163.4f, 0.f, 0.075f), 0.f, 3.3f);
	PaintZebra(FVector(183.4f, 0.f, 0.075f), 180.f, 3.3f);

	// ---- 平交口 CS2：西段西行停止线 + 斑马线 ----
	AddBox(FVector(251.f, 318.25f, 0.070f), FVector(0.3f, 3.3f, 0.04f), ColLineWhite);
	PaintZebra(FVector(230.6f, 320.f, 0.075f), 0.f, 3.2f);
	PaintZebra(FVector(249.4f, 320.f, 0.075f), 180.f, 3.2f);

	// ---- 导向箭头 ----
	PaintArrow(FVector(140.f, 1.75f, 0.068f), 0.f);      // 东段直行
	PaintArrow(FVector(518.25f, 120.f, 0.068f), 90.f);   // 北段直行
	PaintArrow(FVector(450.f, 318.25f, 0.068f), 180.f);  // 西段直行
	PaintArrow(FVector(400.f, 318.25f, 0.068f), 180.f);  // 掉头区前

	// ---- 靠边停车参考线（返回段右侧路缘石 30cm / 50cm）----
	AddBox(FVector(412.f, 323.4f, 0.072f), FVector(112.f, 0.07f, 0.04f), FLinearColor(0.92f, 0.76f, 0.15f));
	AddBox(FVector(412.f, 323.1f, 0.072f), FVector(112.f, 0.07f, 0.04f), FLinearColor(0.92f, 0.3f, 0.25f));

	// ---- 掉头区标线 ----
	for (int32 i = 0; i < 4; ++i)
	{
		AddBox(FVector(384.f, 317.2f + i * 3.2f, 0.068f), FVector(2.2f, 0.14f, 0.03f), ColLineWhite);
		AddBox(FVector(340.f, 317.2f + i * 3.2f, 0.068f), FVector(2.2f, 0.14f, 0.03f), ColLineWhite);
	}
	// 掉头区锥桶（外侧弧线）
	const FVector Cones[] =
	{
		FVector(379.f, 327.6f, 0.f),
		FVector(370.f, 328.1f, 0.f),
		FVector(361.f, 328.1f, 0.f),
		FVector(352.f, 327.6f, 0.f),
		FVector(384.f, 315.8f, 0.f),
		FVector(340.f, 315.8f, 0.f)
	};
	for (const FVector& C : Cones)
	{
		AddCone(C + FVector(0.f, 0.f, 0.35f), FVector(0.5f, 0.5f, 0.7f), FLinearColor(1.f, 0.45f, 0.08f));
	}
}

// ---------------------------------------------------------------------------
// 考试区域设施
// ---------------------------------------------------------------------------
void ARoadBuilder::PoleWithPlate(const FVector& BasePos, float FaceYawDeg, const FLinearColor& PlateColor,
	float PlateW, float PlateH)
{
	AddCylinder(BasePos + FVector(0.f, 0.f, 1.7f), FVector(0.16f, 0.16f, 3.4f), FLinearColor(0.4f, 0.4f, 0.43f));
	AddBox(BasePos + FVector(0.f, 0.f, 3.4f + PlateH * 0.5f + 0.2f),
		FVector(0.12f, PlateW, PlateH), PlateColor, FRotator(0.f, FaceYawDeg, 0.f));
}

void ARoadBuilder::MakeTree(const FVector& BasePos, float ScaleMul)
{
	// 真实天然树干
	AddCylinder(BasePos + FVector(0.f, 0.f, ScaleMul * 1.1f), FVector(0.28f * ScaleMul, 0.28f * ScaleMul, 2.2f * ScaleMul), ColTrunk);
	const uint32 Hash = GetTypeHash(BasePos);
	const float Hue = static_cast<float>(Hash % 100) / 100.f;

	// 底层饱满主树冠（自然园林圆润树冠，非刺眼尖锥）
	const FLinearColor BaseCanopy(0.06f + Hue * 0.02f, 0.16f + Hue * 0.03f, 0.07f);
	AddCylinder(BasePos + FVector(0.f, 0.f, ScaleMul * 2.6f),
		FVector(2.4f * ScaleMul, 2.4f * ScaleMul, 1.8f * ScaleMul), BaseCanopy);

	// 顶层透光圆冠（稍微收窄，形成饱满茂密的现代城市绿化冠层）
	const FLinearColor TopCanopy(0.08f + Hue * 0.03f, 0.22f + Hue * 0.04f, 0.09f);
	AddCylinder(BasePos + FVector(0.f, 0.f, ScaleMul * 3.8f),
		FVector(1.7f * ScaleMul, 1.7f * ScaleMul, 1.2f * ScaleMul), TopCanopy);
}

void ARoadBuilder::MakeStreetLamp(const FVector& BasePos, float ArmYawDeg)
{
	AddCylinder(BasePos + FVector(0.f, 0.f, 3.5f), FVector(0.18f, 0.18f, 7.f), FLinearColor(0.36f, 0.38f, 0.42f));
	const float Rad = FMath::DegreesToRadians(ArmYawDeg);
	const FVector Dir(FMath::Cos(Rad), FMath::Sin(Rad), 0.f);
	AddBox(BasePos + Dir * 1.0f + FVector(0.f, 0.f, 6.9f), FVector(2.2f, 0.14f, 0.14f),
		FLinearColor(0.36f, 0.38f, 0.42f), FRotator(0.f, ArmYawDeg, 0.f));
	AddBox(BasePos + Dir * 2.0f + FVector(0.f, 0.f, 6.78f), FVector(0.9f, 0.36f, 0.18f),
		FLinearColor(1.f, 0.96f, 0.78f), FRotator(0.f, ArmYawDeg, 0.f));
}

void ARoadBuilder::BuildZoneFacilities()
{
	PieceLayerZ = 0.0f; // Facilities use their own Z in FVector
	// 限速 60 标志（起点前方）
	AddCylinder(FVector(30.f, 6.9f, 1.7f), FVector(0.16f, 0.16f, 3.4f), FLinearColor(0.4f, 0.4f, 0.43f));
	AddCylinder(FVector(30.f, 6.9f, 3.85f), FVector(1.36f, 1.36f, 0.08f), FLinearColor(0.85f, 0.12f, 0.1f), FRotator(90.f, 0.f, 0.f));
	AddCylinder(FVector(29.93f, 6.9f, 3.85f), FVector(1.12f, 1.12f, 0.06f), FLinearColor(0.95f, 0.95f, 0.92f), FRotator(90.f, 0.f, 0.f));

	// 学校区域提示牌 + 减速标线
	PoleWithPlate(FVector(206.f, 6.9f, 0.f), 180.f, FLinearColor(0.10f, 0.32f, 0.85f));
	for (float BarX : { 212.f, 215.f })
	{
		AddBox(FVector(BarX, 1.95f, 0.068f), FVector(0.24f, 2.9f, 0.03f), ColLineYellow);
	}

	// 人行横道警示牌
	PoleWithPlate(FVector(152.f, 6.9f, 0.f), 180.f, FLinearColor(0.95f, 0.78f, 0.1f));

	// 公交车站：站牌 + 雨棚 + 候车亭 + 停靠公交
	PoleWithPlate(FVector(268.f, 6.9f, 0.f), 180.f, FLinearColor(0.08f, 0.55f, 0.25f));
	AddBox(FVector(282.f, 7.35f, 1.15f), FVector(5.0f, 0.25f, 2.3f), FLinearColor(0.72f, 0.74f, 0.78f));
	AddBox(FVector(282.f, 6.55f, 2.52f), FVector(5.6f, 1.9f, 0.12f), FLinearColor(0.35f, 0.4f, 0.46f));
	AddBox(FVector(282.f, 6.6f, 0.45f), FVector(4.0f, 0.45f, 0.5f), FLinearColor(0.55f, 0.4f, 0.28f));
	// 公交车
	AddBox(FVector(290.f, 5.55f, 0.32f), FVector(10.5f, 2.2f, 0.5f), FLinearColor(0.1f, 0.1f, 0.12f));
	AddBox(FVector(290.f, 5.55f, 1.48f), FVector(10.5f, 2.5f, 2.85f), FLinearColor(0.16f, 0.5f, 0.56f));
	AddBox(FVector(290.f, 5.54f, 2.4f), FVector(10.56f, 2.54f, 0.72f), FLinearColor(0.1f, 0.14f, 0.18f));

	// 加减挡操作区提示牌（西段）
	PoleWithPlate(FVector(486.f, 313.8f, 0.f), 0.f, FLinearColor(0.85f, 0.45f, 0.08f));

	// 掉头区提示牌
	PoleWithPlate(FVector(392.f, 313.8f, 0.f), 0.f, FLinearColor(0.12f, 0.35f, 0.8f));

	// 起点龙门架
	AddCylinder(FVector(0.f, 5.6f, 2.8f), FVector(0.24f, 0.24f, 5.6f), FLinearColor(0.35f, 0.36f, 0.4f));
	AddCylinder(FVector(0.f, -5.6f, 2.8f), FVector(0.24f, 0.24f, 5.6f), FLinearColor(0.35f, 0.36f, 0.4f));
	AddBox(FVector(0.f, 0.f, 5.45f), FVector(0.5f, 11.6f, 0.5f), FLinearColor(0.88f, 0.88f, 0.9f));
	AddBox(FVector(0.f, 0.f, 4.75f), FVector(0.12f, 4.5f, 1.05f), FLinearColor(0.12f, 0.3f, 0.75f));
}

// ---------------------------------------------------------------------------
// 路灯与行道树
// ---------------------------------------------------------------------------
void ARoadBuilder::BuildStreetFurniture()
{
	PieceLayerZ = 0.0f;
	auto InJunction = [](float S)
	{
		return (S > 174.f && S < 208.f) || (S > 516.f && S < 560.f) ||
			(S > 832.f && S < 876.f) || (S > 1116.f && S < 1140.f);
	};

	int32 LampIdx = 0;
	int32 TreeIdx = 0;
	const float MainLen = Track.GetMainLength();
	for (float S = 16.f; S < MainLen - 20.f; S += 1.f)
	{
		const bool bLampSpot = (static_cast<int32>(S) % 38 == 0);
		const bool bTreeSpot = (static_cast<int32>(S) % 13 == 0);
		if ((!bLampSpot && !bTreeSpot) || InJunction(S))
		{
			continue;
		}

		const FVector T = Track.TangentAtS(S);
		const FVector2D D = LateralDir(T);
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(T.Y, T.X));

		if (bLampSpot)
		{
			const float Side = (LampIdx % 2 == 0) ? 5.7f : -5.7f;
			const FVector Base = Track.LocAtS(S, Side);
			// 灯臂指向道路中心
			const float ArmYaw = FMath::RadiansToDegrees(FMath::Atan2(-FMath::Sign(Side) * D.Y, -FMath::Sign(Side) * D.X));
			MakeStreetLamp(Base, ArmYaw);
			++LampIdx;
		}
		if (bTreeSpot)
		{
			const float JitterMul = 0.85f + static_cast<float>((TreeIdx * 37) % 40) / 100.f;
			MakeTree(Track.LocAtS(S, 7.6f), JitterMul);
			MakeTree(Track.LocAtS(S, -7.6f), JitterMul * 0.9f + 0.2f);
			++TreeIdx;
		}
	}
}

// ---------------------------------------------------------------------------
// 城市街区建筑群（现代立面结构、窗带分层与天际线）
// ---------------------------------------------------------------------------
void ARoadBuilder::BuildCityBlocks()
{
	PieceLayerZ = 0.0f;
	struct FBlock { float X1, X2, Y1, Y2; bool bSkyline; };
	const FBlock Blocks[] =
	{
		{ -40.f, 700.f, -150.f, -16.f, false },  // 南侧街区
		{ -40.f, 158.f, 12.f, 308.f, false },    // 中部西
		{ 188.f, 225.f, 12.f, 308.f, false },    // 中部中（窄条）
		{ 255.f, 504.f, 12.f, 308.f, false },    // 中部东
		{ 536.f, 700.f, 12.f, 308.f, false },    // 北段以东
		{ -40.f, 700.f, 336.f, 480.f, true }     // 北侧街区（远景高楼天际线）
	};

	// 现代写实高质感建筑立面调色板（米白、暖灰、深钢、香槟灰）
	static const FLinearColor FacadePalette[] =
	{
		FLinearColor(0.78f, 0.77f, 0.74f), // 米白花岗岩
		FLinearColor(0.64f, 0.65f, 0.68f), // 现代钛金灰
		FLinearColor(0.72f, 0.68f, 0.62f), // 暖砂岩
		FLinearColor(0.48f, 0.50f, 0.54f), // 深色石墨
		FLinearColor(0.75f, 0.73f, 0.70f), // 浅灰石材
		FLinearColor(0.58f, 0.60f, 0.64f), // 现代商办青灰
		FLinearColor(0.70f, 0.67f, 0.60f), // 浅米黄
		FLinearColor(0.52f, 0.54f, 0.58f)  // 钢结构深灰
	};

	// 深色采光玻璃窗带颜色
	const FLinearColor WindowColor(0.12f, 0.16f, 0.22f);
	// 屋顶机房设备层深灰
	const FLinearColor RooftopColor(0.25f, 0.26f, 0.28f);

	FRandomStream Rand(1337);
	for (const FBlock& Blk : Blocks)
	{
		for (float GX = Blk.X1 + 12.f; GX < Blk.X2 - 10.f; GX += 27.f)
		{
			for (float GY = Blk.Y1 + 12.f; GY < Blk.Y2 - 10.f; GY += 24.f)
			{
				const float Roll = Rand.FRand();
				if (Roll > 0.74f)
				{
					// 空地：种多株错落行道树
					if (Roll > 0.88f)
					{
						MakeTree(FVector(GX + Rand.FRandRange(-4.f, 4.f), GY + Rand.FRandRange(-4.f, 4.f), 0.f), 1.15f);
						MakeTree(FVector(GX + Rand.FRandRange(-5.f, 5.f), GY + Rand.FRandRange(-5.f, 5.f), 0.f), 0.95f);
					}
					continue;
				}

				const float CX = GX + Rand.FRandRange(-4.f, 4.f);
				const float CY = GY + Rand.FRandRange(-3.f, 3.f);
				const float W = Rand.FRandRange(12.f, 18.f);
				const float Dp = Rand.FRandRange(9.f, 15.f);
				float H = Rand.FRandRange(8.f, 22.f);
				if (Blk.bSkyline && Rand.FRand() > 0.65f)
				{
					H = Rand.FRandRange(24.f, 42.f); // 远景高楼天际线，气势恢宏
				}

				// 1. 建筑主体楼栋
				const FLinearColor& MainColor = FacadePalette[Rand.RandRange(0, 7)];
				AddBox(FVector(CX, CY, H * 0.5f), FVector(W, Dp, H), MainColor);

				// 2. 现代建筑特征：立体采光玻璃窗带（Window Strips）
				// 沿楼高分层生成 1~3 条深色采光横带，形成现代建筑立体感
				const int32 NumBands = FMath::Clamp(static_cast<int32>(H / 7.f), 1, 3);
				for (int32 b = 1; b <= NumBands; ++b)
				{
					const float BandZ = (H / (NumBands + 1)) * b;
					AddBox(FVector(CX, CY, BandZ), FVector(W + 0.12f, Dp + 0.12f, 1.4f), WindowColor);
				}

				// 3. 楼顶构架/女儿墙机房（Rooftop Box）
				const float TopW = W * Rand.FRandRange(0.45f, 0.65f);
				const float TopDp = Dp * Rand.FRandRange(0.45f, 0.65f);
				const float TopH = Rand.FRandRange(1.8f, 2.8f);
				AddBox(FVector(CX, CY, H + TopH * 0.5f), FVector(TopW, TopDp, TopH), RooftopColor);
			}
		}
	}
}
