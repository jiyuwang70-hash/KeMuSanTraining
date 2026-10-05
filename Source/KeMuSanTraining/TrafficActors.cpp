#include "TrafficActors.h"

#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

#include "RoadLayout.h"

// ---------------------------------------------------------------------------
// AAICar
// ---------------------------------------------------------------------------
AAICar::AAICar()
{
	PrimaryActorTick.bCanEverTick = false;
	SetActorEnableCollision(false);
	SetActorHiddenInGame(true);

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylAsset(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterial> MatAsset(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	auto MakePart = [&](const TCHAR* Name, UStaticMesh* Mesh, const FVector& Loc, const FVector& Scale, const FLinearColor& Color) -> UMaterialInstanceDynamic*
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetStaticMesh(Mesh);
		C->SetRelativeLocation(Loc);
		C->SetRelativeScale3D(Scale);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetupAttachment(Root);
		UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(MatAsset.Object, this);
		D->SetVectorParameterValue(FName("Color"), Color);
		C->SetMaterial(0, D);
		return D;
	};

	// 真实高质感车身颜色池（社会车辆外观丰富多样）
	static const FLinearColor BodyColors[] =
	{
		FLinearColor(0.85f, 0.86f, 0.88f), // 珍珠白
		FLinearColor(0.12f, 0.13f, 0.15f), // 曜石黑
		FLinearColor(0.52f, 0.55f, 0.60f), // 钛金银
		FLinearColor(0.68f, 0.12f, 0.10f), // 动感红
		FLinearColor(0.12f, 0.28f, 0.52f), // 深海蓝
		FLinearColor(0.70f, 0.58f, 0.42f), // 香槟金
		FLinearColor(0.35f, 0.38f, 0.42f), // 高级灰
		FLinearColor(0.18f, 0.40f, 0.30f)  // 墨青色
	};
	const int32 Seed = static_cast<int32>(GetTypeHash(GetName())) % 8;
	const int32 ModelType = (Seed % 3); // 0: 轿车, 1: 城市SUV, 2: 商务面包车

	FVector BodyScale(4.2f, 1.8f, 1.0f);
	FVector CabinLoc(-15.f, 0.f, 152.f);
	FVector CabinScale(2.2f, 1.68f, 0.62f);

	if (ModelType == 1) // 城市 SUV
	{
		BodyScale = FVector(4.4f, 1.88f, 1.15f);
		CabinLoc = FVector(-10.f, 0.f, 158.f);
		CabinScale = FVector(2.6f, 1.72f, 0.70f);
	}
	else if (ModelType == 2) // 商务面包/微客
	{
		BodyScale = FVector(4.5f, 1.82f, 1.25f);
		CabinLoc = FVector(5.f, 0.f, 165.f);
		CabinScale = FVector(3.2f, 1.70f, 0.85f);
	}

	MakePart(TEXT("Body"), CubeAsset.Object, FVector(0.f, 0.f, 72.f), BodyScale, BodyColors[Seed]);
	MakePart(TEXT("Cabin"), CubeAsset.Object, CabinLoc, CabinScale, FLinearColor(0.10f, 0.13f, 0.17f));

	// 车轮（横向卧倒轴线指向Y轴，贴合地面）
	auto MakeWheel = [&](const TCHAR* Name, const FVector& Loc)
	{
		UStaticMeshComponent* W = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		W->SetStaticMesh(CylAsset.Object);
		W->SetRelativeLocation(Loc);
		W->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
		W->SetRelativeScale3D(FVector(0.65f, 0.65f, 0.22f));
		W->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		W->SetupAttachment(Root);
		UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(MatAsset.Object, this);
		D->SetVectorParameterValue(FName("Color"), FLinearColor(0.06f, 0.06f, 0.07f));
		W->SetMaterial(0, D);
	};
	MakeWheel(TEXT("WheelFL"), FVector(135.f, 84.f, 32.5f));
	MakeWheel(TEXT("WheelFR"), FVector(135.f, -84.f, 32.5f));
	MakeWheel(TEXT("WheelRL"), FVector(-135.f, 84.f, 32.5f));
	MakeWheel(TEXT("WheelRR"), FVector(-135.f, -84.f, 32.5f));

	// 车灯
	MakePart(TEXT("HeadL"), CubeAsset.Object, FVector(211.f, 60.f, 72.f), FVector(0.10f, 0.30f, 0.16f), FLinearColor(0.95f, 0.95f, 0.85f));
	MakePart(TEXT("HeadR"), CubeAsset.Object, FVector(211.f, -60.f, 72.f), FVector(0.10f, 0.30f, 0.16f), FLinearColor(0.95f, 0.95f, 0.85f));
	MakePart(TEXT("TailL"), CubeAsset.Object, FVector(-211.f, 60.f, 72.f), FVector(0.10f, 0.30f, 0.14f), FLinearColor(0.6f, 0.04f, 0.03f));
	MakePart(TEXT("TailR"), CubeAsset.Object, FVector(-211.f, -60.f, 72.f), FVector(0.10f, 0.30f, 0.14f), FLinearColor(0.6f, 0.04f, 0.03f));
}

void AAICar::InitRoute(const FVector& InStart, const FVector& InEnd, float InSpeedKmh)
{
	StartLoc = InStart;
	EndLoc = InEnd;
	SpeedMs = InSpeedKmh / 3.6f;
	bRouteMode = true;
	bActive = true;
	SetActorLocation(InStart * 100.f);
	SetActorHiddenInGame(false);
	const FVector Dir = (EndLoc - StartLoc).GetSafeNormal();
	SetActorRotation(Dir.Rotation());
}

void AAICar::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bActive || !bRouteMode)
	{
		return;
	}

	const FVector Dir = (EndLoc - StartLoc).GetSafeNormal();
	const FVector NewLoc = GetActorLocation() + Dir * (SpeedMs * 100.f) * DeltaSeconds;
	SetActorLocation(NewLoc);

	const float Traveled = FVector::Dist(StartLoc * 100.f, NewLoc);
	if (Traveled >= FVector::Dist(StartLoc * 100.f, EndLoc * 100.f))
	{
		bActive = false;
		bRouteMode = false;
		SetActorHiddenInGame(true);
	}
}

void AAICar::Activate(float InSpeedKmh)
{
	bActive = true;
	bRouteMode = false;
	SpeedMs = InSpeedKmh / 3.6f;
	SetActorHiddenInGame(false);
}

void AAICar::Deactivate()
{
	bActive = false;
	bRouteMode = false;
	SetActorHiddenInGame(true);
}

void AAICar::SetPose(const FVector& Pos, float YawDeg)
{
	SetActorLocationAndRotation(Pos * 100.f, FRotator(0.f, YawDeg, 0.f), false);
}

// ---------------------------------------------------------------------------
// APedestrian
// ---------------------------------------------------------------------------
APedestrian::APedestrian()
{
	// 由 TrafficManager 按规则统一驱动，避免等待/放行状态被重复 Tick。
	PrimaryActorTick.bCanEverTick = false;
	SetActorEnableCollision(false);
	SetActorHiddenInGame(true);

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterial> MatAsset(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	auto MakePart = [&](const TCHAR* Name, UStaticMesh* Mesh, const FVector& Loc, const FVector& Scale, const FLinearColor& Color) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetStaticMesh(Mesh);
		C->SetRelativeLocation(Loc);
		C->SetRelativeScale3D(Scale);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetupAttachment(Root);
		UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(MatAsset.Object, this);
		D->SetVectorParameterValue(FName("Color"), Color);
		C->SetMaterial(0, D);
		return C;
	};

	BodyComp = MakePart(TEXT("Body"), CubeAsset.Object, FVector(0.f, 0.f, 105.f), FVector(0.42f, 0.30f, 0.8f), FLinearColor(0.85f, 0.3f, 0.18f));
	HeadComp = MakePart(TEXT("Head"), CubeAsset.Object, FVector(0.f, 0.f, 162.f), FVector(0.32f, 0.32f, 0.32f), FLinearColor(0.95f, 0.8f, 0.65f));
	MakePart(TEXT("LegL"), CubeAsset.Object, FVector(0.f, 12.f, 36.f), FVector(0.14f, 0.14f, 0.72f), FLinearColor(0.15f, 0.18f, 0.3f));
	MakePart(TEXT("LegR"), CubeAsset.Object, FVector(0.f, -12.f, 36.f), FVector(0.14f, 0.14f, 0.72f), FLinearColor(0.15f, 0.18f, 0.3f));
}

void APedestrian::PrepareCrossing(const FVector& InFrom, const FVector& InTo, float InSpeedMs)
{
	From = InFrom;
	To = InTo;
	Speed = FMath::Max(0.2f, InSpeedMs);
	TotalDist = FMath::Max(1.f, FVector::Dist(From, To));
	Traveled = 0.f;
	bQueued = true;
	bWaiting = true;
	bActive = false;
	SetActorLocation(From * 100.f);
	SetActorRotation((To - From).Rotation());
	SetActorHiddenInGame(false);
}

void APedestrian::ReleaseCrossing()
{
	if (!bQueued)
	{
		return;
	}
	bWaiting = false;
	bActive = true;
	SetActorHiddenInGame(false);
}

void APedestrian::StartCrossing(const FVector& InFrom, const FVector& InTo, float InSpeedMs)
{
	PrepareCrossing(InFrom, InTo, InSpeedMs);
	ReleaseCrossing();
}

void APedestrian::StopCrossing()
{
	bActive = false;
	bQueued = false;
	bWaiting = false;
	Traveled = 0.f;
	SetActorHiddenInGame(true);
}

void APedestrian::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bQueued)
	{
		return;
	}

	if (bWaiting)
	{
		// 等待时保持在路缘，只做轻微重心摆动，避免瞬移进车道。
		const float Sway = FMath::Sin(GetWorld()->GetTimeSeconds() * 2.0f) * 1.5f;
		BodyComp->SetRelativeRotation(FRotator(Sway, 0.f, 0.f));
		return;
	}

	if (!bActive)
	{
		return;
	}

	const FVector Dir = (To - From).GetSafeNormal();
	Traveled += Speed * DeltaSeconds;
	SetActorLocation((From + Dir * FMath::Min(Traveled, TotalDist)) * 100.f);

	// 简单行走摆动
	const float Swing = FMath::Sin(Traveled * 4.5f) * 4.f;
	BodyComp->SetRelativeRotation(FRotator(Swing, 0.f, 0.f));

	if (Traveled >= TotalDist)
	{
		bActive = false;
		bQueued = false;
		bWaiting = false;
		SetActorHiddenInGame(true);
		UE_LOG(LogTemp, Log, TEXT("[KeMuSanTraffic] pedestrian_complete"));
	}
}

bool APedestrian::IsOnRoad(float CrosswalkXLocal, float RoadHalfWidth) const
{
	if (!bActive)
	{
		return false;
	}
	const FVector L = GetActorLocation() * 0.01f;
	return FMath::Abs(L.X - CrosswalkXLocal) < 7.f && FMath::Abs(L.Y) < RoadHalfWidth + 1.f;
}

// ---------------------------------------------------------------------------
// ATrafficLight
// ---------------------------------------------------------------------------
ATrafficLight::ATrafficLight()
{
	PrimaryActorTick.bCanEverTick = true;
	SetActorEnableCollision(false);

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylAsset(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterial> MatAsset(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	Pole = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pole"));
	Pole->SetStaticMesh(CylAsset.Object);
	Pole->SetRelativeLocation(FVector(0.f, 0.f, 260.f));
	Pole->SetRelativeScale3D(FVector(0.12f, 0.12f, 2.6f));
	Pole->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Pole->SetupAttachment(Root);
	UMaterialInstanceDynamic* PoleMat = UMaterialInstanceDynamic::Create(MatAsset.Object, this);
	PoleMat->SetVectorParameterValue(FName("Color"), FLinearColor(0.35f, 0.35f, 0.37f));
	Pole->SetMaterial(0, PoleMat);

	auto MakeHead = [&](const TCHAR* Name, float Height, UStaticMeshComponent*& OutComp, UMaterialInstanceDynamic*& OutMat)
	{
		OutComp = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		OutComp->SetStaticMesh(CubeAsset.Object);
		OutComp->SetRelativeLocation(FVector(0.f, 0.f, Height));
		OutComp->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.5f));
		OutComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		OutComp->SetupAttachment(Root);
		OutMat = UMaterialInstanceDynamic::Create(MatAsset.Object, this);
		OutComp->SetMaterial(0, OutMat);
	};

	// 上红 中黄 下绿
	MakeHead(TEXT("HeadRed"), 550.f, HeadRed, RedMat);
	MakeHead(TEXT("HeadYellow"), 490.f, HeadYellow, YellowMat);
	MakeHead(TEXT("HeadGreen"), 430.f, HeadGreen, GreenMat);
}

void ATrafficLight::ResetLight()
{
	State = 2;
	Remaining = 18.f;
}

void ATrafficLight::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Remaining -= DeltaSeconds;
	if (Remaining <= 0.f)
	{
		if (State == 2)      { State = 1; Remaining = 3.f; }  // 绿 -> 黄
		else if (State == 1) { State = 0; Remaining = 15.f; } // 黄 -> 红
		else                 { State = 2; Remaining = 18.f; } // 红 -> 绿
	}

	const FLinearColor Dim(0.13f, 0.13f, 0.14f);
	RedMat->SetVectorParameterValue(FName("Color"), State == 0 ? FLinearColor(1.f, 0.05f, 0.02f) : Dim);
	YellowMat->SetVectorParameterValue(FName("Color"), State == 1 ? FLinearColor(1.f, 0.8f, 0.05f) : Dim);
	GreenMat->SetVectorParameterValue(FName("Color"), State == 2 ? FLinearColor(0.05f, 0.9f, 0.1f) : Dim);
}

// ---------------------------------------------------------------------------
// ATrafficManager
// ---------------------------------------------------------------------------
namespace
{
	// OBB 重叠近似：把 P2 变换到 P1 局部系后做 AABB 判断
	bool BoxesOverlap(const FVector& P1, float Yaw1, const FVector& P2, float Yaw2,
		float HalfLen1, float HalfWid1, float HalfLen2, float HalfWid2)
	{
		const float Rad = FMath::DegreesToRadians(Yaw1);
		const float Cos = FMath::Cos(Rad);
		const float Sin = FMath::Sin(Rad);
		const float DX = P2.X - P1.X;
		const float DY = P2.Y - P1.Y;
		const float LocalX = DX * Cos + DY * Sin;
		const float LocalY = -DX * Sin + DY * Cos;
		// 对向车头对车头时用长度和的一半做保守估计
		return FMath::Abs(LocalX) < (HalfLen1 + HalfLen2) * 0.92f &&
			FMath::Abs(LocalY) < (HalfWid1 + HalfWid2) * 0.88f;
	}

	float YawOfTangent(const FVector& T)
	{
		return FMath::RadiansToDegrees(FMath::Atan2(T.Y, T.X));
	}
}

// ---------------------------------------------------------------------------
// ABicycle
// ---------------------------------------------------------------------------
ABicycle::ABicycle()
{
	// 由 TrafficManager 统一驱动，避免 Actor Tick 与管理器 Tick 重复移动。
	PrimaryActorTick.bCanEverTick = false;
	SetActorEnableCollision(false);
	SetActorHiddenInGame(true);

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylAsset(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterial> MatAsset(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	auto MakePart = [&](UStaticMeshComponent*& Comp, const TCHAR* Name, UStaticMesh* Mesh, const FVector& Loc, const FVector& Scale, const FLinearColor& Color)
	{
		Comp = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Comp->SetStaticMesh(Mesh);
		Comp->SetRelativeLocation(Loc);
		Comp->SetRelativeScale3D(Scale);
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Comp->SetupAttachment(Root);
		UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(MatAsset.Object, this);
		D->SetVectorParameterValue(FName("Color"), Color);
		Comp->SetMaterial(0, D);
	};
	MakePart(Frame, TEXT("Frame"), CubeAsset.Object, FVector(0.f, 0.f, 60.f), FVector(0.08f, 1.6f, 0.08f), FLinearColor(0.12f, 0.13f, 0.15f));
	MakePart(Seat, TEXT("Seat"), CubeAsset.Object, FVector(0.f, 0.f, 85.f), FVector(0.15f, 0.25f, 0.06f), FLinearColor(0.08f, 0.09f, 0.10f));

	auto MakeWheel = [&](UStaticMeshComponent*& Comp, const TCHAR* Name, const FVector& Loc)
	{
		Comp = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Comp->SetStaticMesh(CylAsset.Object);
		Comp->SetRelativeLocation(Loc);
		Comp->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
		Comp->SetRelativeScale3D(FVector(0.64f, 0.64f, 0.06f));
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Comp->SetupAttachment(Root);
		UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(MatAsset.Object, this);
		D->SetVectorParameterValue(FName("Color"), FLinearColor(0.06f, 0.06f, 0.07f));
		Comp->SetMaterial(0, D);
	};
	MakeWheel(WheelF, TEXT("WheelF"), FVector(40.f, 0.f, 32.f));
	MakeWheel(WheelR, TEXT("WheelR"), FVector(-40.f, 0.f, 32.f));
}

void ABicycle::Activate(const FVector& Pos, float YawDeg, float InSpeedMs)
{
	bActive = true;
	SpeedMs = InSpeedMs;
	SetActorLocationAndRotation(Pos * 100.f, FRotator(0.f, YawDeg, 0.f), false);
	SetActorHiddenInGame(false);
}

void ABicycle::Deactivate()
{
	bActive = false;
	SetActorHiddenInGame(true);
}

void ABicycle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bActive) return;

	const FVector Fwd = GetActorRotation().Vector();
	SetActorLocation(GetActorLocation() + Fwd * (SpeedMs * 100.f) * DeltaSeconds);

	// Simple wobble animation
	if (FMath::Abs(SpeedMs) > 0.2f)
	{
		const float T = GetWorld()->GetTimeSeconds();
		Frame->SetRelativeRotation(FRotator(FMath::Sin(T * 8.f) * 2.5f, 0.f, 0.f));
	}
}

ATrafficManager::ATrafficManager()
{
	PrimaryActorTick.bCanEverTick = true;
	SetActorEnableCollision(false);
}

void ATrafficManager::Setup(const FRouteTrack* InTrack)
{
	Track = InTrack;
	ScenarioName = TEXT("road_test_default");
	if (FParse::Value(FCommandLine::Get(), TEXT("traffic-scenario="), ScenarioName))
	{
		ScenarioName.TrimStartAndEndInline();
	}
	ScenarioSeed = 20260823;
	FParse::Value(FCommandLine::Get(), TEXT("traffic-seed="), ScenarioSeed);
	ScenarioRandom.Initialize(ScenarioSeed);
	UE_LOG(LogTemp, Log, TEXT("[KeMuSanTraffic] scenario=%s seed=%d"), *ScenarioName, ScenarioSeed);

	if (CarPool.Num() == 0)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		for (int32 i = 0; i < 28; ++i) // 扩充环境社会车辆池至28辆，支撑复杂车水马龙路况
		{
			AAICar* Car = GetWorld()->SpawnActor<AAICar>(FVector(0.f, 0.f, -100.f), FRotator::ZeroRotator, Params);
			if (Car) { CarPool.Add(Car); }
		}
		for (int32 i = 0; i < 8; ++i) // 扩充自行车至8辆
		{
			ABicycle* Bike = GetWorld()->SpawnActor<ABicycle>(FVector(0.f, 0.f, -200.f), FRotator::ZeroRotator, Params);
			if (Bike) { BikePool.Add(Bike); }
		}
		MeetingCar = AcquireCar();
		SlowCar = AcquireCar();
		Crosser = AcquireCar();

		// 路侧规范停放社会车辆（营造真实城市街景）
		auto SpawnParked = [&](const FVector& Pos, float Yaw)
		{
			AAICar* P = GetWorld()->SpawnActor<AAICar>(Pos, FRotator(0.f, Yaw, 0.f), Params);
			if (P) { P->Activate(0.f); P->SetPose(Pos, Yaw); ParkedCars.Add(P); }
		};
		using namespace RoadLayout;
		SpawnParked(FVector(45.f, -7.2f, 0.25f), 0.f);      // 起点东段商铺前
		SpawnParked(FVector(62.f, -7.2f, 0.25f), 0.f);
		SpawnParked(FVector(240.f, -7.2f, 0.25f), 0.f);     // 学校区域南侧泊位
		SpawnParked(FVector(255.f, -7.2f, 0.25f), 0.f);
		SpawnParked(FVector(305.f, -7.2f, 0.25f), 0.f);     // 公交车站后方泊位
		SpawnParked(FVector(320.f, -7.2f, 0.25f), 0.f);
		SpawnParked(FVector(527.2f, 85.f, 0.25f), 90.f);    // 北段路侧停放车
		SpawnParked(FVector(527.2f, 160.f, 0.25f), 90.f);
		SpawnParked(FVector(470.f, 313.8f, 0.25f), 180.f);  // 西段商住楼前
		SpawnParked(FVector(448.f, 313.8f, 0.25f), 180.f);
		SpawnParked(FVector(280.f, 313.8f, 0.25f), 180.f);

		CrosserFrom = FVector(CrossStreet1X, -70.f, 0.25f);
		CrosserTo = FVector(CrossStreet1X, 150.f, 0.25f);
		CrosserFrom2 = FVector(CrossStreet2X, 290.f, 0.25f);
		CrosserTo2 = FVector(CrossStreet2X, 350.f, 0.25f);
		CrosserSpeed = 9.f;
	}
}

AAICar* ATrafficManager::AcquireCar()
{
	for (AAICar* Car : CarPool)
	{
		if (Car && !Ambients.ContainsByPredicate([Car](const FAmbientCar& A) { return A.Car == Car; }) &&
			Car != MeetingCar && Car != SlowCar && Car != Crosser)
		{
			return Car;
		}
	}
	return nullptr;
}

void ATrafficManager::RecycleCar(AAICar* Car)
{
	if (Car)
	{
		Car->Deactivate();
	}
	Ambients.RemoveAll([Car](const FAmbientCar& A) { return A.Car == Car; });
}

void ATrafficManager::SetActive(bool bInActive)
{
	bActive = bInActive;
	if (!bActive)
	{
		for (FAmbientCar& A : Ambients)
		{
			if (A.Car)
			{
				A.Car->Deactivate();
			}
		}
		Ambients.Reset();
		for (ABicycle* B : BikePool)
		{
			if (B) B->Deactivate();
		}
	}
}

void ATrafficManager::UpdatePlayer(const FVector& InPos, const FVector& InHeading, float InPlayerS, bool bOnReturn,
	float InPlayerSpeedKmh, bool bInPlayerStopped, int32 InTrafficLightState, float InTrafficLightRemaining,
	bool bInPedestrianWaiting, bool bInPedestrianCrossing)
{
	PlayerPos = InPos;
	PlayerHeading = InHeading;
	PlayerS = InPlayerS;
	bPlayerOnReturn = bOnReturn;
	PlayerSpeedKmh = InPlayerSpeedKmh;
	bPlayerStopped = bInPlayerStopped;
	TrafficLightState = InTrafficLightState;
	TrafficLightRemaining = InTrafficLightRemaining;
	bPedestrianWaiting = bInPedestrianWaiting;
	bPedestrianCrossing = bInPedestrianCrossing;
}

void ATrafficManager::SetPedestrian(APedestrian* InPedestrian)
{
	PedestrianActor = InPedestrian;
}

float ATrafficManager::GetRecommendedSpeedKmh() const
{
	if (TrafficLightState == 0 && IsPlayerApproachingCrosswalk())
	{
		return 0.f;
	}
	if (bPedestrianWaiting || bPedestrianCrossing)
	{
		return 0.f;
	}
	if (IsPlayerApproachingCrosswalk())
	{
		return RoadLayout::ZoneLimit;
	}
	return RoadLayout::GeneralLimit;
}

void ATrafficManager::ResetTraffic()
{
	bScenarioStarted = false;
	bCrosswalkVehiclesActivated = false;
	PlayerYieldTimer = 0.f;
	CrosswalkYieldTimer = 0.f;
	ScenarioRandom.Initialize(ScenarioSeed);
	for (FAmbientCar& A : Ambients)
	{
		if (A.Car)
		{
			A.Car->Deactivate();
		}
	}
	Ambients.Reset();
	if (MeetingCar)
	{
		MeetingCar->Deactivate();
	}
	if (SlowCar)
	{
		SlowCar->Deactivate();
	}
	if (Crosser)
	{
		Crosser->Deactivate();
	}
	for (ABicycle* B : BikePool)
	{
		if (B) B->Deactivate();
	}
	BikeSpawnTimer = 3.f;
	SpawnTimer = 3.f;
	CrossCooldown = 0.f;
	UE_LOG(LogTemp, Log, TEXT("[KeMuSanTraffic] reset scenario=%s seed=%d"), *ScenarioName, ScenarioSeed);
}

void ATrafficManager::SpawnMeetingCar(float InPlayerS)
{
	if (!Track || !MeetingCar)
	{
		return;
	}
	// 对向来车：逆路线行驶，位于其自身前进方向的右侧车道
	MeetingCar->Activate(30.f);
	const float SpawnS = FMath::Min(InPlayerS + 150.f, Track->GetMainLength() - 40.f);
	MeetingCar->SetPose(Track->LocAtS(SpawnS, -RoadLayout::LaneWidth * 0.5f), YawOfTangent(-Track->TangentAtS(SpawnS)));
	FAmbientCar A;
	A.Car = MeetingCar;
	A.S = SpawnS;
	A.Dir = -1;
	A.CruiseMs = 30.f / 3.6f;
	bool bFound = false;
	for (FAmbientCar& Existing : Ambients)
	{
		if (Existing.Car == MeetingCar)
		{
			Existing = A;
			bFound = true;
			break;
		}
	}
	if (!bFound)
	{
		Ambients.Add(A);
	}
}

bool ATrafficManager::IsMeetingCarActive() const
{
	return MeetingCar && MeetingCar->IsActive();
}

void ATrafficManager::SpawnSlowCar(float InPlayerS)
{
	if (!Track || !SlowCar)
	{
		return;
	}
	// 同车道前方低速车
	SlowCar->Activate(13.f);
	const float SpawnS = InPlayerS + 70.f;
	SlowCar->SetPose(Track->LocAtS(SpawnS, RoadLayout::LaneWidth * 0.5f), YawOfTangent(Track->TangentAtS(SpawnS)));
	FAmbientCar A;
	A.Car = SlowCar;
	A.S = SpawnS;
	A.Dir = 1;
	A.CruiseMs = 13.f / 3.6f;
	bool bFound = false;
	for (FAmbientCar& Existing : Ambients)
	{
		if (Existing.Car == SlowCar)
		{
			Existing = A;
			bFound = true;
			break;
		}
	}
	if (!bFound)
	{
		Ambients.Add(A);
	}
}

bool ATrafficManager::IsSlowCarActive() const
{
	return SlowCar && SlowCar->IsActive();
}

float ATrafficManager::GetSlowCarS() const
{
	for (const FAmbientCar& A : Ambients)
	{
		if (A.Car == SlowCar)
		{
			return A.S;
		}
	}
	return -1000.f;
}

void ATrafficManager::TrySpawnScenarioTraffic()
{
	if (bScenarioStarted || !Track)
	{
		return;
	}
	bScenarioStarted = true;

	const bool bEmpty = ScenarioName.Equals(TEXT("empty"), ESearchCase::IgnoreCase);
	const bool bCrosswalk = ScenarioName.Equals(TEXT("crosswalk_yield"), ESearchCase::IgnoreCase) ||
		ScenarioName.Equals(TEXT("pedestrian_yield"), ESearchCase::IgnoreCase);
	const bool bFollow = ScenarioName.Equals(TEXT("follow_and_meet"), ESearchCase::IgnoreCase);
	const bool bFull = ScenarioName.Equals(TEXT("full_mix"), ESearchCase::IgnoreCase) ||
		ScenarioName.Equals(TEXT("road_test_default"), ESearchCase::IgnoreCase);

	if (bEmpty)
	{
		UE_LOG(LogTemp, Log, TEXT("[KeMuSanTraffic] scenario traffic disabled"));
		return;
	}

	auto AddScripted = [&](float S, int32 Dir, float SpeedKmh, const TCHAR* RoleName)
	{
		if (!Track || S < 24.f || S > Track->GetMainLength() - 24.f)
		{
			return;
		}
		AAICar* Car = AcquireCar();
		if (!Car)
		{
			return;
		}
		FAmbientCar A;
		A.Car = Car;
		A.S = S;
		A.Dir = Dir;
		A.CruiseMs = SpeedKmh / 3.6f;
		Car->Activate(SpeedKmh);
		const float Lateral = Dir > 0 ? RoadLayout::LaneWidth * 0.5f : -RoadLayout::LaneWidth * 0.5f;
		const FVector Pos = Track->LocAtS(S, Lateral);
		const FVector Tangent = Track->TangentAtS(S) * static_cast<float>(Dir);
		Car->SetPose(Pos, YawOfTangent(Tangent));
		Ambients.Add(A);
		UE_LOG(LogTemp, Log, TEXT("[KeMuSanTraffic] scripted_vehicle role=%s s=%.1f dir=%d speed=%.1f"), RoleName, S, Dir, SpeedKmh);
	};

	if (bCrosswalk)
	{
		UE_LOG(LogTemp, Log, TEXT("[KeMuSanTraffic] crosswalk_vehicle_trigger=waiting_or_crossing"));
	}
	else if (bFollow)
	{
		AddScripted(PlayerS + 48.f, 1, 22.f, TEXT("following_target"));
		AddScripted(RoadLayout::MeetingStartS + 70.f, -1, 30.f, TEXT("meeting_target"));
	}
	else if (bFull)
	{
		AddScripted(32.f, 1, 22.f, TEXT("same_direction_slow")); // 前方同向慢速引导车（距考生32m，清晰可见）
		AddScripted(48.f, -1, 30.f, TEXT("oncoming_close"));      // 对向近距离交会车（距考生48m，迎面错车）
		AddScripted(110.f, -1, 32.f, TEXT("oncoming_mid"));       // 连续对向错车流
		AddScripted(170.f, -1, 34.f, TEXT("oncoming_far"));
		AddScripted(260.f, 1, 28.f, TEXT("same_direction_mid"));
		AddScripted(340.f, -1, 30.f, TEXT("oncoming_3"));
		AddScripted(420.f, 1, 26.f, TEXT("same_direction_far"));

		// 起点附近路侧顺行自行车（增强复杂交通参与者交互）
		if (BikePool.Num() > 0 && BikePool[0] && !BikePool[0]->IsActive())
		{
			const FVector BikePos = Track->LocAtS(22.f, -(RoadLayout::CurbDistance + 0.8f));
			const FVector BikeTan = Track->TangentAtS(22.f);
			BikePool[0]->Activate(BikePos, FMath::RadiansToDegrees(FMath::Atan2(BikeTan.Y, BikeTan.X)), 4.0f);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[KeMuSanTraffic] unknown scenario=%s, using full_mix layout"), *ScenarioName);
		AddScripted(32.f, 1, 22.f, TEXT("same_direction_slow"));
		AddScripted(48.f, -1, 30.f, TEXT("oncoming_close"));
		AddScripted(110.f, -1, 32.f, TEXT("oncoming_mid"));
	}
}

void ATrafficManager::ActivateCrosswalkScenarioTraffic()
{
	if (bCrosswalkVehiclesActivated || !Track || !PedestrianActor)
	{
		return;
	}

	const bool bCrosswalkScenario = ScenarioName.Equals(TEXT("crosswalk_yield"), ESearchCase::IgnoreCase) ||
		ScenarioName.Equals(TEXT("pedestrian_yield"), ESearchCase::IgnoreCase);
	if (!bCrosswalkScenario || !(PedestrianActor->IsWaiting() || PedestrianActor->IsCrossing()))
	{
		return;
	}

	bCrosswalkVehiclesActivated = true;
	UE_LOG(LogTemp, Log, TEXT("[KeMuSanTraffic] crosswalk_traffic_activated player_s=%.1f"), PlayerS);

	auto AddCrosswalkVehicle = [&](float S, int32 Dir, float SpeedKmh, const TCHAR* RoleName)
	{
		AAICar* Car = AcquireCar();
		if (!Car)
		{
			UE_LOG(LogTemp, Error, TEXT("[KeMuSanTraffic] crosswalk_vehicle_pool_exhausted role=%s"), RoleName);
			return;
		}

		FAmbientCar A;
		A.Car = Car;
		A.S = S;
		A.Dir = Dir;
		A.CruiseMs = SpeedKmh / 3.6f;
		Car->Activate(SpeedKmh);
		const float Lateral = Dir > 0 ? RoadLayout::LaneWidth * 0.5f : -RoadLayout::LaneWidth * 0.5f;
		Car->SetPose(Track->LocAtS(S, Lateral), YawOfTangent(Track->TangentAtS(S) * static_cast<float>(Dir)));
		Ambients.Add(A);
		UE_LOG(LogTemp, Log, TEXT("[KeMuSanTraffic] scripted_vehicle role=%s s=%.1f dir=%d speed=%.1f"), RoleName, S, Dir, SpeedKmh);
	};

	// Both vehicles begin inside the deterministic crosswalk conflict window.
	AddCrosswalkVehicle(RoadLayout::CrosswalkS - 14.f, 1, 24.f, TEXT("crosswalk_follow"));
	AddCrosswalkVehicle(RoadLayout::CrosswalkS + 14.f, -1, 30.f, TEXT("crosswalk_oncoming"));
}

void ATrafficManager::TrySpawnAmbient()
{
	if (!Track || Ambients.Num() >= 16 || ScenarioName.Equals(TEXT("empty"), ESearchCase::IgnoreCase))
	{
		return;
	}

	FAmbientCar A;
	A.Dir = (ScenarioRandom.FRand() < 0.60f) ? -1 : 1;

	if (A.Dir < 0)
	{
		// 对向车：出现在考生前方远处（80~220m），产生连续会车动态
		A.S = PlayerS + ScenarioRandom.FRandRange(80.f, 220.f);
		A.CruiseMs = ScenarioRandom.FRandRange(28.f, 42.f) / 3.6f;
	}
	else
	{
		// 同向车：与考生保持合理前后跟驰距离（50~140m）
		A.S = PlayerS + ScenarioRandom.FRandRange(50.f, 140.f);
		A.CruiseMs = ScenarioRandom.FRandRange(22.f, 36.f) / 3.6f;
	}

	// 避免干扰考试事件区
	if (A.S > RoadLayout::MeetingStartS - 25.f && A.S < RoadLayout::MeetingEndS + 15.f)
	{
		return;
	}
	if (A.S > RoadLayout::OvertakeStartS - 25.f && A.S < RoadLayout::OvertakeEndS + 15.f)
	{
		return;
	}
	// 避免同车道过密重叠
	for (const FAmbientCar& Other : Ambients)
	{
		if (Other.Dir == A.Dir && FMath::Abs(Other.S - A.S) < 28.f)
		{
			return;
		}
	}
	if (A.S < 25.f || A.S > Track->GetMainLength() - 25.f)
	{
		return;
	}

	AAICar* Car = AcquireCar();
	if (!Car)
	{
		return;
	}
	Car->Activate(A.CruiseMs * 3.6f);
	A.Car = Car;
	Ambients.Add(A);
	UE_LOG(LogTemp, Verbose, TEXT("[KeMuSanTraffic] ambient_spawn s=%.1f dir=%d speed=%.1f"), A.S, A.Dir, A.CruiseMs * 3.6f);
}

void ATrafficManager::TickAmbient(float DT)
{
	const float LatOffset = RoadLayout::LaneWidth * 0.5f;

	for (int32 i = Ambients.Num() - 1; i >= 0; --i)
	{
		FAmbientCar& A = Ambients[i];
		if (!A.Car)
		{
			Ambients.RemoveAtSwap(i);
			continue;
		}
		// 会车车和超车目标由 TickScripted 单独驱动，不能在这里移动两次。
		if (A.Car == MeetingCar || A.Car == SlowCar)
		{
			continue;
		}

		// 与考生距离过远则回收；驶出主线边界也回收。
		const bool bBehindFar = (A.Dir > 0) ? (A.S < PlayerS - 90.f) : (A.S > PlayerS + 90.f || A.S < PlayerS - 280.f);
		const bool bAheadFar = A.S > PlayerS + 320.f;
		if (bBehindFar || bAheadFar || A.S < 12.f || A.S > Track->GetMainLength() - 12.f)
		{
			RecycleCar(A.Car);
			continue;
		}

		float TargetMs = A.CruiseMs;
		if (A.Dir > 0)
		{
			// 同向车辆保持至少约8m的时间/空间余量，前车不会突然贴近考生。
			const float Gap = A.S - PlayerS;
			A.bFollowing = Gap > 0.f && Gap < 45.f;
			if (A.bFollowing && PlayerSpeedKmh > A.CruiseMs * 3.6f + 2.f)
			{
				TargetMs = FMath::Min(TargetMs, FMath::Clamp((Gap - 8.f) / 22.f, 0.f, 1.f) * A.CruiseMs);
			}
		}
		if (A.bYielding)
		{
			TargetMs = 0.f;
		}

		if (A.bFollowing != A.bLastFollowing || A.bYielding != A.bLastYielding)
		{
			UE_LOG(LogTemp, Log, TEXT("[KeMuSanTraffic] vehicle_policy s=%.1f dir=%d following=%d yielding=%d gap_to_player=%.1f"),
				A.S, A.Dir, A.bFollowing ? 1 : 0, A.bYielding ? 1 : 0, A.S - PlayerS);
			A.bLastFollowing = A.bFollowing;
			A.bLastYielding = A.bYielding;
		}

		A.Car->SetSpeedMs(TargetMs);
		A.S += A.Dir * TargetMs * DT;

		const FVector Pos = Track->LocAtS(A.S, A.Dir > 0 ? LatOffset : -LatOffset);
		const FVector Tangent = Track->TangentAtS(A.S) * static_cast<float>(A.Dir);
		A.Car->SetPose(Pos, YawOfTangent(Tangent));
	}
}

void ATrafficManager::TickScripted(float DT)
{
	// 会车车：驶过考生后回收
	for (int32 i = Ambients.Num() - 1; i >= 0; --i)
	{
		FAmbientCar& A = Ambients[i];
		if (A.Car != MeetingCar)
		{
			continue;
		}
		const float TargetMs = A.bYielding ? 0.f : FMath::Max(0.f, A.Car->GetSpeedMs());
		A.S += A.Dir * TargetMs * DT;
		const FVector Pos = Track->LocAtS(A.S, A.Dir > 0 ? RoadLayout::LaneWidth * 0.5f : -RoadLayout::LaneWidth * 0.5f);
		const FVector Tangent = Track->TangentAtS(A.S) * static_cast<float>(A.Dir);
		A.Car->SetPose(Pos, YawOfTangent(Tangent));

		if (A.S < PlayerS - 70.f)
		{
			MeetingCar->Deactivate();
			Ambients.RemoveAtSwap(i);
		}
	}

	// 超车慢车：低速前行，被超越一段距离后回收
	for (int32 i = Ambients.Num() - 1; i >= 0; --i)
	{
		FAmbientCar& A = Ambients[i];
		if (A.Car != SlowCar)
		{
			continue;
		}
		A.S += A.Dir * FMath::Max(0.f, A.Car->GetSpeedMs()) * DT;
		const FVector Pos = Track->LocAtS(A.S, RoadLayout::LaneWidth * 0.5f);
		const FVector Tangent = Track->TangentAtS(A.S);
		A.Car->SetPose(Pos, YawOfTangent(Tangent));

		if (A.S > PlayerS + 50.f || A.S > RoadLayout::OvertakeEndS + 40.f)
		{
			SlowCar->Deactivate();
			Ambients.RemoveAtSwap(i);
		}
	}
}

void ATrafficManager::NotifyLightRed(bool bPlayerNearStopLine)
{
	if (bPlayerNearStopLine && TrafficLightState == 0)
	{
		TrySpawnCrosser();
	}
}

void ATrafficManager::TrySpawnCrosser()
{
	if (!Crosser || !Track || CrossCooldown > 0.f || Crosser->IsActive())
	{
		return;
	}
	// 红灯时横向车拥有优先权；固定随机流保证同一 seed 的方向一致。
	const bool bFromNorth = (ScenarioRandom.FRand() < 0.5f);
	Crosser->InitRoute(bFromNorth ? CrosserTo : CrosserFrom, bFromNorth ? CrosserFrom : CrosserTo, CrosserSpeed * 3.6f);
	CrossCooldown = 4.f;
	UE_LOG(LogTemp, Log, TEXT("[KeMuSanTraffic] crosser_spawn direction=%s light_remaining=%.1f"),
		bFromNorth ? TEXT("north_to_south") : TEXT("south_to_north"), TrafficLightRemaining);
}

void ATrafficManager::TickCrosser(float DT)
{
	if (!Crosser || !Crosser->IsActive())
	{
		return;
	}
	Crosser->Tick(DT); // 直线模式由管理器显式驱动
	if (!Crosser->IsActive())
	{
		CrossCooldown = 6.f;
	}
}

bool ATrafficManager::IsPlayerApproachingCrosswalk() const
{
	return !bPlayerOnReturn && PlayerS > RoadLayout::CrosswalkS - 42.f && PlayerS < RoadLayout::CrosswalkS + 14.f;
}

bool ATrafficManager::IsPlayerStoppedForCrosswalk() const
{
	return IsPlayerApproachingCrosswalk() && bPlayerStopped && PlayerSpeedKmh < 1.2f;
}

void ATrafficManager::UpdatePedestrianPolicy(float DT)
{
	if (!PedestrianActor)
	{
		return;
	}

	if (PedestrianActor->IsWaiting())
	{
		if (IsPlayerStoppedForCrosswalk())
		{
			CrosswalkYieldTimer += DT;
		}
		else
		{
			CrosswalkYieldTimer = FMath::Max(0.f, CrosswalkYieldTimer - DT * 2.f);
		}

		// 车辆已稳定停车后才放行，避免行人在车辆尚未减速时踏入冲突区。
		if (CrosswalkYieldTimer >= 0.6f ||
			(!IsPlayerApproachingCrosswalk() && PlayerS > RoadLayout::CrosswalkS + 8.f))
		{
			PedestrianActor->ReleaseCrossing();
			UE_LOG(LogTemp, Log, TEXT("[KeMuSanTraffic] pedestrian_release progress=%.2f player_s=%.1f player_speed=%.1f"),
				PedestrianActor->GetProgress01(), PlayerS, PlayerSpeedKmh);
		}
	}
	else if (PedestrianActor->IsCrossing())
	{
		CrosswalkYieldTimer = 0.f;
	}

	// APedestrian 关闭自身 Tick，由 TrafficManager 保证单一时钟和可复现顺序。
	PedestrianActor->Tick(DT);
}

void ATrafficManager::ApplyVehiclePolicy(float DT)
{
	// Read the actor state directly as well as the controller snapshot. Actor tick order
	// can make the snapshot one frame stale exactly when a pedestrian is released.
	const bool bPedestrianQueued = bPedestrianWaiting || (PedestrianActor && PedestrianActor->IsWaiting());
	const bool bPedestrianInConflict = bPedestrianCrossing ||
		(PedestrianActor && (PedestrianActor->IsCrossing() || PedestrianActor->IsOnRoad(164.f, RoadLayout::RoadHalfWidth)));
	const bool bCrosswalkApproach = bPedestrianQueued || bPedestrianInConflict;

	for (FAmbientCar& A : Ambients)
	{
		if (!A.Car || A.Car == Crosser)
		{
			continue;
		}
		A.bYielding = false;

		if (bCrosswalkApproach)
		{
			const bool bSameDirectionApproach = A.Dir > 0 && A.S < RoadLayout::CrosswalkS &&
				RoadLayout::CrosswalkS - A.S < 28.f;
			const bool bOncomingApproach = A.Dir < 0 && A.S > RoadLayout::CrosswalkS &&
				A.S - RoadLayout::CrosswalkS < 28.f;
			const bool bCrosswalkScenario = ScenarioName.Equals(TEXT("crosswalk_yield"), ESearchCase::IgnoreCase) ||
				ScenarioName.Equals(TEXT("pedestrian_yield"), ESearchCase::IgnoreCase);
			const bool bScriptedCrosswalkApproach = bCrosswalkScenario &&
				FMath::Abs(A.S - RoadLayout::CrosswalkS) < 60.f;
			A.bYielding = bSameDirectionApproach || bOncomingApproach || bScriptedCrosswalkApproach;
		}

		if (A.bYielding && !A.bLastYielding)
		{
			UE_LOG(LogTemp, Log, TEXT("[KeMuSanTraffic] pedestrian_vehicle_yield s=%.1f dir=%d"), A.S, A.Dir);
		}

		// 红灯停止线前，社会车辆也必须停在停止线后，不占用斑马线。
		if (TrafficLightState == 0)
		{
			const bool bApproachStopLine = (A.Dir > 0 && A.S < RoadLayout::StopLineS && RoadLayout::StopLineS - A.S < 22.f) ||
				(A.Dir < 0 && A.S > RoadLayout::StopLineS && A.S - RoadLayout::StopLineS < 22.f);
			A.bYielding = A.bYielding || bApproachStopLine;
		}
	}

	if (Crosser && Crosser->IsActive() && bPlayerStopped)
	{
		// 横向车通过时不被考生车辆抢行打断；这里只记录决策，车辆本身走完路线。
		UE_LOG(LogTemp, Verbose, TEXT("[KeMuSanTraffic] crosser_has_priority remaining=%.1f"), TrafficLightRemaining);
	}
}

bool ATrafficManager::HitsPlayer(const FVector& InPlayerPos, float PlayerYawDeg) const
{
	constexpr float HL = 2.15f;
	constexpr float HW = 0.98f;

	auto CheckOne = [&](const AAICar* Car) -> bool
	{
		return Car && Car->IsActive() && !Car->IsHidden() &&
			BoxesOverlap(InPlayerPos, PlayerYawDeg, Car->GetActorLocation() * 0.01f, Car->GetHeadingDeg(), HL, HW, HL, HW);
	};

	if (CheckOne(MeetingCar) || CheckOne(SlowCar))
	{
		return true;
	}
	for (const FAmbientCar& A : Ambients)
	{
		if (CheckOne(A.Car))
		{
			return true;
		}
	}
	// 横向车流
	if (CheckOne(Crosser))
	{
		return true;
	}
	// 路边停车
	for (AAICar* Parked : ParkedCars)
	{
		if (CheckOne(Parked)) return true;
	}
	return false;
}

bool ATrafficManager::HitsPedestrian(const FVector& InPlayerPos) const
{
	if (!PedestrianActor || !PedestrianActor->IsCrossing())
	{
		return false;
	}
	return FVector::DistSquared2D(InPlayerPos, PedestrianActor->GetLocation() * 0.01f) < FMath::Square(3.2f);
}

bool ATrafficManager::HitsBicycle(const FVector& InPlayerPos, float PlayerYawDeg) const
{
	for (const ABicycle* Bike : BikePool)
	{
		if (!Bike || !Bike->IsActive())
		{
			continue;
		}
		if (BoxesOverlap(InPlayerPos, PlayerYawDeg, Bike->GetLocation() * 0.01f, Bike->GetHeadingDeg(), 2.15f, 0.98f, 0.75f, 0.45f))
		{
			return true;
		}
	}
	return false;
}

void ATrafficManager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bActive || !Track)
	{
		return;
	}

	TrySpawnScenarioTraffic();
	UpdatePedestrianPolicy(DeltaSeconds);
	ActivateCrosswalkScenarioTraffic();
	ApplyVehiclePolicy(DeltaSeconds);

	// 定期补充环境车流
	SpawnTimer -= DeltaSeconds;
	if (SpawnTimer <= 0.f)
	{
		SpawnTimer = ScenarioRandom.FRandRange(1.5f, 4.f);
		TrySpawnAmbient();
	}

	TickAmbient(DeltaSeconds);
	TickScripted(DeltaSeconds);
	TickCrosser(DeltaSeconds);
	TickBikes(DeltaSeconds);

	// Periodic bike spawning
	BikeSpawnTimer -= DeltaSeconds;
	if (BikeSpawnTimer <= 0.f)
	{
		BikeSpawnTimer = ScenarioRandom.FRandRange(6.f, 14.f);
		TrySpawnBike(DeltaSeconds);
	}

	CrossCooldown = FMath::Max(0.f, CrossCooldown - DeltaSeconds);
}

void ATrafficManager::TrySpawnBike(float DT)
{
	if (!Track || BikePool.Num() == 0) return;

	for (ABicycle* B : BikePool)
	{
		if (B && !B->IsActive())
		{
			// Spawn on sidewalk near player
			const float SideMul = (ScenarioRandom.FRand() < 0.5f) ? 1.f : -1.f;
			const float S = PlayerS + ScenarioRandom.FRandRange(40.f, 200.f);
			if (S < 20.f || S > Track->TotalLength() - 20.f) return;

			const FVector Pos = Track->LocAtS(S, SideMul * (RoadLayout::CurbDistance + 0.8f));
			const FVector Tan = Track->TangentAtS(S) * ((ScenarioRandom.FRand() < 0.5f) ? 1.f : -1.f);
			B->Activate(Pos, FMath::RadiansToDegrees(FMath::Atan2(Tan.Y, Tan.X)), 4.5f + ScenarioRandom.FRandRange(0.f, 3.f));
			break;
		}
	}
}

void ATrafficManager::TickBikes(float DT)
{
	for (ABicycle* B : BikePool)
	{
		if (B && B->IsActive())
		{
			B->Tick(DT);
			const FVector BLoc = B->GetLocation();
			// Recycle if far from player (校准为400米比较)
			if (FVector::DistSquared(BLoc * 0.01f, PlayerPos) > FMath::Square(400.f))
			{
				B->Deactivate();
			}
		}
	}
}
