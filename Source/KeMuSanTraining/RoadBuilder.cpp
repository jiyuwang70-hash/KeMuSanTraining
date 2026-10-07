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
	// ---- 主线与横向街道沥青（10m宽主路 + 专用沥青停车泊位带）----
	// 东段 AB：主路 y=[-5.0, 5.0]，南侧专用停车带 y=[-7.8, -5.0]，总沥青 y=[-7.8, 5.0]
	AsphaltRect(FVector2D(-30.f, -7.8f), FVector2D(506.f, 5.0f));
	// 转角 B 区域（充分覆盖 R=14m/16m 弯道内外弧）
	AsphaltRect(FVector2D(504.f, -8.f), FVector2D(532.f, 25.f));
	// 纵向街道（穿转角 B）
	AsphaltRect(FVector2D(510.f, -60.f), FVector2D(530.f, 420.f));
	// 北段 BC：主路 x=[515.0, 525.0]，东侧停车带 x=[525.0, 527.8]
	AsphaltRect(FVector2D(515.0f, 21.f), FVector2D(527.8f, 306.f));
	// 转角 C 区域
	AsphaltRect(FVector2D(494.f, 304.f), FVector2D(536.f, 336.f));
	// 横向街道（转角 C 以东）
	AsphaltRect(FVector2D(534.f, 315.0f), FVector2D(664.f, 325.0f));
	// 西段 CD：主路 y=[315.0, 325.0]，南侧专用沥青停车带 y=[312.2, 315.0]，总沥青 y=[312.2, 325.0]
	AsphaltRect(FVector2D(-30.f, 312.2f), FVector2D(498.f, 325.0f));
	// 信号路口纵向街 CS1（拓宽至 16m 双向宽干道）
	AsphaltRect(FVector2D(164.f, -80.f), FVector2D(182.f, 420.f));
	// 西段平交口 CS2
	AsphaltRect(FVector2D(231.f, 180.f), FVector2D(249.f, 420.f));

	// ---- 人行道（在路口处断开，完整避让行车道与路侧停车泊位）----
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

	// 东段两侧（北侧在行车道外，南侧在停车带外）
	const TArray<FRun> AbRuns = { { -30.f, 162.f }, { 184.f, 228.f }, { 252.f, 504.f } };
	WalkBandX(5.35f, 7.85f, AbRuns);         // 东段北侧人行道
	WalkBandX(-10.45f, -7.95f, AbRuns);      // 东段南侧人行道（在停车带外）

	// 西段两侧（北侧在行车道外，南侧完整后移到泊位外侧，绝不压车）
	const TArray<FRun> CdRuns = { { -30.f, 162.f }, { 184.f, 228.f }, { 252.f, 498.f } };
	const TArray<FRun> CdNorthRuns = { { -30.f, 162.f }, { 184.f, 228.f },
		{ 252.f, UTurnClearanceMinX }, { UTurnClearanceMaxX, 498.f } };
	WalkBandX(325.35f, 327.85f, CdNorthRuns); // 西段北侧人行道
	WalkBandX(309.40f, 311.90f, CdRuns);      // 西段南侧人行道（后移4.3m，与停车带保持间隙）

	// 北段两侧
	WalkBandY(528.15f, 530.65f, { { 21.f, 306.f } }); // 东侧人行道（停车带外侧）
	WalkBandY(512.35f, 514.85f, { { 21.f, 306.f } }); // 西侧人行道

	// 转角 B 纵向街道两侧
	WalkBandY(530.35f, 532.85f, { { -60.f, -10.f }, { 10.f, 306.f }, { 336.f, 420.f } });
	WalkBandY(507.15f, 509.65f, { { -60.f, -10.f }, { 10.f, 306.f }, { 336.f, 420.f } });

	// 信号路口 CS1 两侧
	WalkBandY(161.15f, 163.65f, { { -80.f, -10.f }, { 10.f, 306.f }, { 336.f, 420.f } });
	WalkBandY(182.35f, 184.85f, { { -80.f, -10.f }, { 10.f, 306.f }, { 336.f, 420.f } });

	// 平交口 CS2 两侧
	WalkBandY(228.15f, 230.65f, { { 180.f, 306.f }, { 336.f, 420.f } });
	WalkBandY(249.35f, 251.85f, { { 180.f, 306.f }, { 336.f, 420.f } });

	// 转角 C 以东街道两侧
	WalkwayStrip(FVector2D(534.f, 325.35f), FVector2D(664.f, 327.85f));
	WalkwayStrip(FVector2D(534.f, 312.15f), FVector2D(664.f, 314.65f));

	// ---- 北侧连续掉头区及独立返回车道 ----
	AsphaltRect(FVector2D(UTurnMidA.X - RoadHalfWidth, WestToUTurn.Y - RoadHalfWidth),
		FVector2D(WestToUTurn.X + RoadHalfWidth, ReturnCenterY + RoadHalfWidth));
	AsphaltRect(FVector2D(ReturnStart.X, ReturnCenterY - RoadHalfWidth),
		FVector2D(ReturnEnd.X + 6.f, ReturnCenterY + RoadHalfWidth));
	WalkwayStrip(FVector2D(ReturnStart.X + 5.f, ReturnCenterY + RoadHalfWidth + 0.35f),
		FVector2D(ReturnEnd.X + 6.f, ReturnCenterY + RoadHalfWidth + 2.85f));

	// ---- 主线路缘石 ----
	auto CurbStripX = [&](float YCenter, const TArray<FRun>& Runs)
	{
		for (const FRun& R : Runs)
		{
			AddPiece(CubeMesh, FVector((R.A + R.B) * 0.5f, YCenter, 0.09f),
				FVector(R.B - R.A, 0.34f, 0.18f), ColCurb);
		}
	};
	CurbStripX(5.17f, AbRuns);                // 东段北侧路缘石
	CurbStripX(-7.87f, AbRuns);               // 东段南侧路缘石（泊位外沿）
	CurbStripX(325.17f, CdNorthRuns);         // 西段北侧路缘石
	CurbStripX(312.03f, CdRuns);              // 西段南侧路缘石（泊位外沿）
	const TArray<FRun> ReturnRuns = { { static_cast<float>(ReturnStart.X) + 5.f, static_cast<float>(ReturnEnd.X) + 6.f } };
	CurbStripX(ReturnCenterY + RoadHalfWidth + 0.17f, ReturnRuns);
	CurbStripX(ReturnCenterY - RoadHalfWidth - 0.17f, ReturnRuns);
	AddPiece(CubeMesh, FVector(514.83f, 163.5f, 0.09f), FVector(0.34f, 285.f, 0.18f), ColCurb);
	AddPiece(CubeMesh, FVector(527.97f, 163.5f, 0.09f), FVector(0.34f, 285.f, 0.18f), ColCurb);
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
	// 边缘白色实线（直段长条 + 弯道分段，对应 10m 双向路宽）
	// 东段（行车道边缘 ±5.0m）
	EdgeY(5.0f, -30.f, 160.f);  EdgeY(5.0f, 186.f, 506.f);
	EdgeY(-5.0f, -30.f, 160.f); EdgeY(-5.0f, 186.f, 506.f);
	// 北段（行车道边缘 515.0m 与 525.0m）
	EdgeX(515.0f, 21.f, 306.f); EdgeX(525.0f, 21.f, 306.f);
	// 西段（行车道边缘 315.0m 与 325.0m）
	EdgeY(315.0f, -30.f, 158.f); EdgeY(315.0f, 186.f, 228.f); EdgeY(315.0f, 252.f, 498.f);
	EdgeY(325.0f, -30.f, 158.f); EdgeY(325.0f, 186.f, 228.f);
	EdgeY(325.0f, 252.f, UTurnClearanceMinX); EdgeY(325.0f, UTurnClearanceMaxX, 498.f);
	// 弯道弧段
	auto ArcEdges = [&](float FromS, float ToS)
	{
		for (float S = FromS; S <= ToS; S += 2.2f)
		{
			for (float Lat : { -5.05f, 5.05f })
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
	ArcEdges(UTurnEntryS, UTurnCompleteS);
	EdgeY(ReturnCenterY - RoadHalfWidth, ReturnStart.X, ReturnEnd.X + 6.f);
	EdgeY(ReturnCenterY + RoadHalfWidth, ReturnStart.X, ReturnEnd.X + 6.f);
	// 横向街道边缘
	EdgeX(164.f, -78.f, 418.f); EdgeX(182.f, -78.f, 418.f);
	EdgeX(510.f, -58.f, 418.f); EdgeX(530.f, -58.f, 418.f);
	EdgeX(231.f, 182.f, 418.f); EdgeX(249.f, 182.f, 418.f);
	EdgeY(315.0f, 536.f, 662.f); EdgeY(325.0f, 536.f, 662.f);

	// ---- 路侧专用沥青停车泊位标线（白色规范泊位方格框）----
	auto PaintParkingBayX = [&](float XCenter, float YCenter, float Length = 6.0f, float Width = 2.4f)
	{
		const float HalfL = Length * 0.5f;
		const float HalfW = Width * 0.5f;
		// 前后界线
		AddBox(FVector(XCenter - HalfL, YCenter, 0.068f), FVector(0.12f, Width, 0.03f), ColLineWhite);
		AddBox(FVector(XCenter + HalfL, YCenter, 0.068f), FVector(0.12f, Width, 0.03f), ColLineWhite);
		// 外侧界线（与路缘石留出安全余量）
		AddBox(FVector(XCenter, YCenter - HalfW, 0.068f), FVector(Length, 0.12f, 0.03f), ColLineWhite);
	};
	auto PaintParkingBayY = [&](float XCenter, float YCenter, float Length = 6.0f, float Width = 2.4f)
	{
		const float HalfL = Length * 0.5f;
		const float HalfW = Width * 0.5f;
		AddBox(FVector(XCenter, YCenter - HalfL, 0.068f), FVector(Width, 0.12f, 0.03f), ColLineWhite);
		AddBox(FVector(XCenter, YCenter + HalfL, 0.068f), FVector(Width, 0.12f, 0.03f), ColLineWhite);
		AddBox(FVector(XCenter + HalfW, YCenter, 0.068f), FVector(0.12f, Length, 0.03f), ColLineWhite);
	};

	// 西段商住楼前规范停车泊位（正是用户截图位置）
	PaintParkingBayX(470.f, 313.6f);
	PaintParkingBayX(448.f, 313.6f);
	PaintParkingBayX(280.f, 313.6f);

	// 东段路侧规范停车泊位
	PaintParkingBayX(45.f, -6.4f);
	PaintParkingBayX(62.f, -6.4f);
	PaintParkingBayX(240.f, -6.4f);
	PaintParkingBayX(255.f, -6.4f);
	PaintParkingBayX(305.f, -6.4f);
	PaintParkingBayX(320.f, -6.4f);

	// 北段路侧规范停车泊位
	PaintParkingBayY(526.4f, 85.f);
	PaintParkingBayY(526.4f, 160.f);

	// ---- 起点线（覆盖 10m 全路宽）----
	AddBox(FVector(StartPose.X, 0.f, 0.070f), FVector(0.35f, 10.f, 0.04f), ColLineWhite);

	// ---- 信号路口：停止线 + 斑马线（覆盖 5m 单车道）----
	AddBox(FVector(160.f, 2.5f, 0.070f), FVector(0.32f, 4.8f, 0.04f), ColLineWhite);   // 东行停止线
	AddBox(FVector(186.f, -2.5f, 0.070f), FVector(0.32f, 4.8f, 0.04f), ColLineWhite);  // 西行停止线
	PaintZebra(FVector(163.4f, 0.f, 0.075f), 0.f, 4.8f);
	PaintZebra(FVector(183.4f, 0.f, 0.075f), 180.f, 4.8f);

	// ---- 平交口 CS2：西段西行停止线 + 斑马线 ----
	AddBox(FVector(251.f, 322.5f, 0.070f), FVector(0.3f, 4.8f, 0.04f), ColLineWhite);
	PaintZebra(FVector(230.6f, 320.f, 0.075f), 0.f, 4.8f);
	PaintZebra(FVector(249.4f, 320.f, 0.075f), 180.f, 4.8f);

	// ---- 导向箭头（居于各路段 5m 车道中央）----
	PaintArrow(FVector(140.f, 2.5f, 0.068f), 0.f);      // 东段直行
	PaintArrow(FVector(522.5f, 120.f, 0.068f), 90.f);   // 北段直行
	PaintArrow(FVector(450.f, 322.5f, 0.068f), 180.f);  // 西段直行
	PaintArrow(FVector(400.f, 322.5f, 0.068f), 180.f);  // 掉头区前

	// ---- 靠边停车区：实际路缘石内沿、车辆半宽与评分距离使用同一参数 ----
	const float ParkingFromX = ReturnStart.X + (PullOverMinS - ReturnStartS);
	const float ParkingToX = ReturnStart.X + (PullOverMaxS - ReturnStartS);
	const float ParkingCenterX = (ParkingFromX + ParkingToX) * 0.5f;
	const float ParkingLength = ParkingToX - ParkingFromX;
	AddBox(FVector(ParkingCenterX, ReturnCenterY + PullOverGapBase - 0.30f, 0.072f),
		FVector(ParkingLength, 0.07f, 0.03f), FLinearColor(0.92f, 0.76f, 0.15f));
	AddBox(FVector(ParkingCenterX, ReturnCenterY + PullOverGapBase - 0.50f, 0.072f),
		FVector(ParkingLength, 0.07f, 0.03f), FLinearColor(0.92f, 0.3f, 0.25f));
	for (float X : { ParkingFromX, ParkingToX })
	{
		AddBox(FVector(X, ReturnCenterY + 2.5f, 0.072f), FVector(0.14f, 4.8f, 0.03f), ColLineWhite);
	}
	for (float S : { (UTurnEntryS + UTurnArc1EndS) * 0.5f,
		(UTurnArc1EndS + UTurnStraightEndS) * 0.5f,
		(UTurnStraightEndS + UTurnCompleteS) * 0.5f })
	{
		const FVector P = Track.LocAtS(S, LaneWidth * 0.5f);
		const FVector T = Track.TangentAtS(S);
		PaintArrow(FVector(P.X, P.Y, 0.068f), FMath::RadiansToDegrees(FMath::Atan2(T.Y, T.X)));
	}
	PaintArrow(FVector(ReturnStart.X + 20.f, ReturnCenterY + LaneWidth * 0.5f, 0.068f), 0.f);

	// 锥桶沿掉头区外侧边线放置，不侵入车辆的正常右侧行驶轨迹。
	for (float S = UTurnEntryS + 4.f; S < UTurnCompleteS - 4.f; S += 8.f)
	{
		const FVector P = Track.LocAtS(S, CurbDistance + 0.8f);
		AddCone(P + FVector(0.f, 0.f, 0.35f), FVector(0.5f, 0.5f, 0.7f), FLinearColor(1.f, 0.45f, 0.08f));
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
	// 限速 60 标志（起点前方，人行道外缘）
	AddCylinder(FVector(30.f, 7.2f, 1.7f), FVector(0.16f, 0.16f, 3.4f), FLinearColor(0.4f, 0.4f, 0.43f));
	AddCylinder(FVector(30.f, 7.2f, 3.85f), FVector(1.36f, 1.36f, 0.08f), FLinearColor(0.85f, 0.12f, 0.1f), FRotator(90.f, 0.f, 0.f));
	AddCylinder(FVector(29.93f, 7.2f, 3.85f), FVector(1.12f, 1.12f, 0.06f), FLinearColor(0.95f, 0.95f, 0.92f), FRotator(90.f, 0.f, 0.f));

	// 学校区域提示牌 + 减速标线（覆盖 5m 东行车道）
	PoleWithPlate(FVector(206.f, 7.2f, 0.f), 180.f, FLinearColor(0.10f, 0.32f, 0.85f));
	for (float BarX : { 212.f, 215.f })
	{
		AddBox(FVector(BarX, 2.5f, 0.068f), FVector(0.24f, 4.2f, 0.03f), ColLineYellow);
	}

	// 人行横道警示牌
	PoleWithPlate(FVector(152.f, 7.2f, 0.f), 180.f, FLinearColor(0.95f, 0.78f, 0.1f));

	// 公交车站：站牌 + 雨棚 + 候车亭 + 停靠公交（规范停靠在北侧外延专用候车岛）
	PoleWithPlate(FVector(268.f, 7.6f, 0.f), 180.f, FLinearColor(0.08f, 0.55f, 0.25f));
	AddBox(FVector(282.f, 8.35f, 1.15f), FVector(5.0f, 0.25f, 2.3f), FLinearColor(0.72f, 0.74f, 0.78f));
	AddBox(FVector(282.f, 7.55f, 2.52f), FVector(5.6f, 1.9f, 0.12f), FLinearColor(0.35f, 0.4f, 0.46f));
	AddBox(FVector(282.f, 7.6f, 0.45f), FVector(4.0f, 0.45f, 0.5f), FLinearColor(0.55f, 0.4f, 0.28f));
	// 公交车（停在 6.8m 停靠带）
	AddBox(FVector(290.f, 6.8f, 0.32f), FVector(10.5f, 2.2f, 0.5f), FLinearColor(0.1f, 0.1f, 0.12f));
	AddBox(FVector(290.f, 6.8f, 1.48f), FVector(10.5f, 2.5f, 2.85f), FLinearColor(0.16f, 0.5f, 0.56f));
	AddBox(FVector(290.f, 6.79f, 2.4f), FVector(10.56f, 2.54f, 0.72f), FLinearColor(0.1f, 0.14f, 0.18f));

	// 加减挡操作区提示牌（西段北侧人行道外缘，面向西行学员）
	PoleWithPlate(FVector(486.f, 326.6f, 0.f), 0.f, FLinearColor(0.85f, 0.45f, 0.08f));

	// 掉头区提示牌
	PoleWithPlate(FVector(392.f, 326.6f, 0.f), 0.f, FLinearColor(0.12f, 0.35f, 0.8f));

	// 起点龙门架（跨度拓宽至 14.6m，横跨 10m 拓宽主干道）
	AddCylinder(FVector(0.f, 7.0f, 2.8f), FVector(0.24f, 0.24f, 5.6f), FLinearColor(0.35f, 0.36f, 0.4f));
	AddCylinder(FVector(0.f, -7.0f, 2.8f), FVector(0.24f, 0.24f, 5.6f), FLinearColor(0.35f, 0.36f, 0.4f));
	AddBox(FVector(0.f, 0.f, 5.45f), FVector(0.5f, 14.6f, 0.5f), FLinearColor(0.88f, 0.88f, 0.9f));
	AddBox(FVector(0.f, 0.f, 4.75f), FVector(0.12f, 5.5f, 1.05f), FLinearColor(0.12f, 0.3f, 0.75f));
}

// ---------------------------------------------------------------------------
// 路灯与行道树（完整避让停车泊位与行车道，严禁穿车）
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

		if (bLampSpot)
		{
			// 路灯置于路缘石与人行道内侧（左侧 5.4m，右侧泊位区外沿 8.2m）
			const float Side = (LampIdx % 2 == 0) ? 5.4f : -8.2f;
			const FVector Base = Track.LocAtS(S, Side);
			// 灯臂指向道路中心
			const float ArmYaw = FMath::RadiansToDegrees(FMath::Atan2(-FMath::Sign(Side) * D.Y, -FMath::Sign(Side) * D.X));
			MakeStreetLamp(Base, ArmYaw);
			++LampIdx;
		}
		if (bTreeSpot)
		{
			const float JitterMul = 0.85f + static_cast<float>((TreeIdx * 37) % 40) / 100.f;
			// 行道树退后到人行道外侧绿化带（左侧 9.2m，右侧停车泊位及人行道外侧 11.6m，严防穿车）
			MakeTree(Track.LocAtS(S, 9.2f), JitterMul);
			MakeTree(Track.LocAtS(S, -11.6f), JitterMul * 0.9f + 0.2f);
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
				// 为新返回道路和掉头区预留整幅建筑/树冠余量，避免路上长出楼栋。
				if (GX >= UTurnMidA.X - 18.f && GX <= ReturnEnd.X + 18.f &&
					GY >= WestToUTurn.Y - 18.f && GY <= ReturnCenterY + 18.f)
				{
					continue;
				}
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
