#include "KeMuSanPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// 挡位表：N, R, 1, 2, 3, 4, 5
	const float GearMaxSpeed[] = { 0.f, 3.33f, 5.56f, 9.72f, 13.89f, 19.44f, 27.78f }; // m/s
	const float GearAccel[] = { 0.f, 1.5f, 2.0f, 1.7f, 1.4f, 1.1f, 0.9f };              // m/s^2

	const FLinearColor ColorBody(0.82f, 0.85f, 0.9f);
	const FLinearColor ColorCabin(0.13f, 0.16f, 0.2f);
	const FLinearColor ColorWheel(0.08f, 0.08f, 0.09f);
}

AKeMuSanPawn::AKeMuSanPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	SetActorEnableCollision(false);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylAsset(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterial> MatAsset(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	UStaticMesh* CubeMesh = CubeAsset.Object;
	UStaticMesh* CylMesh = CylAsset.Object;
	UMaterial* BaseMat = MatAsset.Object;

	auto MakeMesh = [&](const TCHAR* Name, UStaticMesh* Mesh, const FVector& Loc, const FVector& Scale, const FLinearColor& Color, UStaticMeshComponent*& OutComp) -> UMaterialInstanceDynamic*
	{
		OutComp = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		OutComp->SetStaticMesh(Mesh);
		OutComp->SetRelativeLocation(Loc);
		OutComp->SetRelativeScale3D(Scale);
		OutComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		OutComp->SetupAttachment(Root);
		UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(BaseMat, this);
		DynMat->SetVectorParameterValue(FName("Color"), Color);
		OutComp->SetMaterial(0, DynMat);
		return DynMat;
	};

	// 车身 / 座舱
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetStaticMesh(CubeMesh);
	Body->SetRelativeLocation(FVector(0.f, 0.f, 0.72f));
	Body->SetRelativeScale3D(FVector(4.2f, 1.8f, 1.05f));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetupAttachment(Root);
	UMaterialInstanceDynamic* BodyDyn = UMaterialInstanceDynamic::Create(BaseMat, this);
	BodyDyn->SetVectorParameterValue(FName("Color"), ColorBody);
	Body->SetMaterial(0, BodyDyn);

	Cabin = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Cabin"));
	Cabin->SetStaticMesh(CubeMesh);
	Cabin->SetRelativeLocation(FVector(-0.1f, 0.f, 1.62f));
	Cabin->SetRelativeScale3D(FVector(2.3f, 1.7f, 0.66f));
	Cabin->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Cabin->SetupAttachment(Root);
	UMaterialInstanceDynamic* CabinDyn = UMaterialInstanceDynamic::Create(BaseMat, this);
	CabinDyn->SetVectorParameterValue(FName("Color"), ColorCabin);
	Cabin->SetMaterial(0, CabinDyn);

	// 车轮（圆柱绕 Y 轴）
	auto MakeWheel = [&](const TCHAR* Name, const FVector& Loc)
	{
		UStaticMeshComponent* W = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		W->SetStaticMesh(CylMesh);
		W->SetRelativeLocation(Loc);
		W->SetRelativeRotation(FRotator(0.f, 90.f, 0.f));
		W->SetRelativeScale3D(FVector(0.36f, 0.36f, 0.25f));
		W->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		W->SetupAttachment(Root);
		UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(BaseMat, this);
		D->SetVectorParameterValue(FName("Color"), ColorWheel);
		W->SetMaterial(0, D);
	};
	MakeWheel(TEXT("WheelFL"), FVector(1.35f, 0.82f, 0.36f));
	MakeWheel(TEXT("WheelFR"), FVector(1.35f, -0.82f, 0.36f));
	MakeWheel(TEXT("WheelRL"), FVector(-1.35f, 0.82f, 0.36f));
	MakeWheel(TEXT("WheelRR"), FVector(-1.35f, -0.82f, 0.36f));

	// 前照灯
	LightMatL = MakeMesh(TEXT("LightFL"), CubeMesh, FVector(2.11f, 0.62f, 0.72f), FVector(0.30f, 0.24f, 0.16f), FLinearColor(0.08f, 0.08f, 0.07f), LightFL);
	LightMatR = MakeMesh(TEXT("LightFR"), CubeMesh, FVector(2.11f, -0.62f, 0.72f), FVector(0.30f, 0.24f, 0.16f), FLinearColor(0.08f, 0.08f, 0.07f), LightFR);

	// 尾灯
	TailMatL = MakeMesh(TEXT("TailL"), CubeMesh, FVector(-2.11f, 0.62f, 0.72f), FVector(0.30f, 0.24f, 0.14f), FLinearColor(0.22f, 0.02f, 0.02f), TailL);
	TailMatR = MakeMesh(TEXT("TailR"), CubeMesh, FVector(-2.11f, -0.62f, 0.72f), FVector(0.30f, 0.24f, 0.14f), FLinearColor(0.22f, 0.02f, 0.02f), TailR);

	// 转向灯（琥珀色）
	SigMatL = MakeMesh(TEXT("SigL"), CubeMesh, FVector(-2.11f, 0.92f, 0.72f), FVector(0.26f, 0.14f, 0.12f), FLinearColor(0.16f, 0.12f, 0.03f), SigL);
	SigMatR = MakeMesh(TEXT("SigR"), CubeMesh, FVector(-2.11f, -0.92f, 0.72f), FVector(0.26f, 0.14f, 0.12f), FLinearColor(0.16f, 0.12f, 0.03f), SigR);

	// 追尾摄像机
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(Root);
	SpringArm->TargetArmLength = 680.f;
	SpringArm->SetRelativeLocation(FVector(-40.f, 0.f, 120.f));
	SpringArm->SetRelativeRotation(FRotator(-11.f, 0.f, 0.f));
	SpringArm->bDoCollisionTest = false;
	SpringArm->bInheritPitch = false;
	SpringArm->bInheritYaw = false;
	SpringArm->bInheritRoll = false;
	SpringArm->bEnableCameraLag = true;
	SpringArm->CameraLagSpeed = 8.f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm);
	Camera->FieldOfView = 95.f;
}

void AKeMuSanPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	if (!PlayerInputComponent)
	{
		return;
	}

	PlayerInputComponent->BindAxis(TEXT("Throttle"), this, &AKeMuSanPawn::AxisThrottle);
	PlayerInputComponent->BindAxis(TEXT("Brake"), this, &AKeMuSanPawn::AxisBrake);
	PlayerInputComponent->BindAxis(TEXT("Steer"), this, &AKeMuSanPawn::AxisSteer);

	PlayerInputComponent->BindAction(TEXT("Handbrake"), IE_Pressed, this, &AKeMuSanPawn::ToggleHandbrake);
	PlayerInputComponent->BindAction(TEXT("Seatbelt"), IE_Pressed, this, &AKeMuSanPawn::ToggleSeatbelt);
	PlayerInputComponent->BindAction(TEXT("Hazard"), IE_Pressed, this, &AKeMuSanPawn::ToggleHazard);
	PlayerInputComponent->BindAction(TEXT("FogLamp"), IE_Pressed, this, &AKeMuSanPawn::ToggleFogLamp);
	PlayerInputComponent->BindAction(TEXT("Lights"), IE_Pressed, this, &AKeMuSanPawn::CycleLights);
	PlayerInputComponent->BindAction(TEXT("FlashBeam"), IE_Pressed, this, &AKeMuSanPawn::FlashHighBeam);
	PlayerInputComponent->BindAction(TEXT("LeftSignal"), IE_Pressed, this, &AKeMuSanPawn::ToggleLeftSignal);
	PlayerInputComponent->BindAction(TEXT("RightSignal"), IE_Pressed, this, &AKeMuSanPawn::ToggleRightSignal);
	PlayerInputComponent->BindAction(TEXT("Horn"), IE_Pressed, this, &AKeMuSanPawn::PressHorn);
	PlayerInputComponent->BindAction(TEXT("Horn"), IE_Released, this, &AKeMuSanPawn::ReleaseHorn);
	PlayerInputComponent->BindAction(TEXT("Observe"), IE_Pressed, this, &AKeMuSanPawn::NotifyHeadCheck);
}

void AKeMuSanPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdatePhysics(DeltaSeconds);
	UpdateVisuals(DeltaSeconds);
}

void AKeMuSanPawn::AxisThrottle(float V)
{
	ThrottleInput = FMath::Clamp(V, 0.f, 1.f);
}

void AKeMuSanPawn::AxisBrake(float V)
{
	BrakeInput = FMath::Clamp(V, 0.f, 1.f);
}

void AKeMuSanPawn::AxisSteer(float V)
{
	SteeringInput = FMath::Clamp(V, -1.f, 1.f);
}

void AKeMuSanPawn::ToggleHandbrake() { bHandbrake = !bHandbrake; }
void AKeMuSanPawn::ToggleSeatbelt() { bSeatbelt = !bSeatbelt; }
void AKeMuSanPawn::ToggleHazard() { bHazard = !bHazard; }
void AKeMuSanPawn::ToggleFogLamp() { bFogLamp = !bFogLamp; }

void AKeMuSanPawn::CycleLights()
{
	if (!bOutline && !bLowBeam)
	{
		// 关 -> 示廓灯
		bOutline = true;
		bLowBeam = false;
		bHighBeam = false;
	}
	else if (bOutline && !bLowBeam)
	{
		// 示廓灯 -> 近光
		bOutline = true;
		bLowBeam = true;
		bHighBeam = false;
	}
	else if (bLowBeam && !bHighBeam)
	{
		// 近光 -> 远光
		bHighBeam = true;
	}
	else
	{
		bOutline = false;
		bLowBeam = false;
		bHighBeam = false;
	}
}

void AKeMuSanPawn::FlashHighBeam()
{
	bFlashHigh = true;
	FlashHighTimer = 0.4f;
}

void AKeMuSanPawn::ToggleLeftSignal()
{
	bLeftSignal = !bLeftSignal;
	bRightSignal = false;
	SignalYawAccum = 0.f;
}

void AKeMuSanPawn::ToggleRightSignal()
{
	bRightSignal = !bRightSignal;
	bLeftSignal = false;
	SignalYawAccum = 0.f;
}

void AKeMuSanPawn::PressHorn() { bHornHeld = true; }
void AKeMuSanPawn::ReleaseHorn() { bHornHeld = false; }

void AKeMuSanPawn::NotifyHeadCheck()
{
	LastHeadCheckTime = GetWorld()->GetTimeSeconds();
}

void AKeMuSanPawn::SelectGear(int32 GearIndex)
{
	EGear NewGear = static_cast<EGear>(FMath::Clamp(GearIndex, 0, 6));
	if (NewGear == Gear)
	{
		return;
	}
	Gear = NewGear;
	if (bStalled)
	{
		// 重新挂挡 = 重新点火
		bStalled = false;
		EngineRpm = 900.f;
	}
}

void AKeMuSanPawn::ResetVehicle(const FVector& Loc, const FRotator& Rot)
{
	SetActorLocationAndRotation(Loc, Rot, false, nullptr, ETeleportType::TeleportPhysics);
	SpeedMs = 0.f;
	YawDeg = Rot.Yaw;
	SteeringAngleDeg = 0.f;
	EngineRpm = 900.f;
	Gear = EGear::N;
	ThrottleInput = 0.f;
	BrakeInput = 0.f;
	SteeringInput = 0.f;
	bStalled = false;
	StallCooldown = 0.f;
	SignalYawAccum = 0.f;
	bHandbrake = true;
	bSeatbelt = false;
	bLowBeam = false;
	bHighBeam = false;
	bFogLamp = false;
	bOutline = false;
	bHazard = false;
	bLeftSignal = false;
	bRightSignal = false;
	bHornHeld = false;
	bFlashHigh = false;
	LastHeadCheckTime = -9999.f;
}

void AKeMuSanPawn::ForceStop()
{
	SpeedMs = 0.f;
	ThrottleInput = 0.f;
}

void AKeMuSanPawn::UpdatePhysics(float DT)
{
	StallCooldown = FMath::Max(0.f, StallCooldown - DT);
	if (bFlashHigh)
	{
		FlashHighTimer -= DT;
		if (FlashHighTimer <= 0.f)
		{
			bFlashHigh = false;
		}
	}

	const int32 GearIdx = static_cast<int32>(Gear);
	const bool bHasPower = (Gear != EGear::N) && !bStalled;

	// ---- 驱动力：向目标车速逼近（模拟油门开度决定车速） ----
	if (bHasPower)
	{
		const float Sign = (Gear == EGear::R) ? -1.f : 1.f;
		const float Desired = Sign * GearMaxSpeed[GearIdx] * (0.12f + 0.88f * ThrottleInput);
		const float MaxStep = GearAccel[GearIdx] * DT;
		const float Diff = Desired - SpeedMs;
		SpeedMs += FMath::Clamp(Diff, -MaxStep, MaxStep);
	}

	// ---- 刹车 ----
	if (BrakeInput > 0.f && FMath::Abs(SpeedMs) > 0.01f)
	{
		const float Step = BrakeInput * 7.5f * DT;
		SpeedMs = (FMath::Abs(SpeedMs) <= Step) ? 0.f : SpeedMs - FMath::Sign(SpeedMs) * Step;
	}

	// ---- 手刹 ----
	if (bHandbrake && FMath::Abs(SpeedMs) > 0.01f)
	{
		const float Step = 6.5f * DT;
		SpeedMs = (FMath::Abs(SpeedMs) <= Step) ? 0.f : SpeedMs - FMath::Sign(SpeedMs) * Step;
	}

	// ---- 行驶阻力 ----
	if (FMath::Abs(SpeedMs) > 0.005f)
	{
		const float Resist = (0.12f + 0.0032f * SpeedMs * SpeedMs) * DT;
		SpeedMs = (FMath::Abs(SpeedMs) <= Resist) ? 0.f : SpeedMs - FMath::Sign(SpeedMs) * Resist;
	}

	// ---- 熄火：静止时猛给油 / 高挡位大油门起步 ----
	if (!bStalled && StallCooldown <= 0.f && Gear != EGear::N && FMath::Abs(SpeedMs) < 0.6f)
	{
		const float Rise = ThrottleInput - PrevThrottle;
		bool bWillStall = (Rise > 0.45f);
		bWillStall = bWillStall || (GearIdx >= static_cast<int32>(EGear::G2) && ThrottleInput > 0.55f);
		if (bWillStall)
		{
			bStalled = true;
			EngineRpm = 0.f;
			Gear = EGear::N;
			StallCooldown = 2.f;
		}
	}
	PrevThrottle = ThrottleInput;

	// ---- 转向（速度越快转向角越小） ----
	const float SpeedKmh = FMath::Abs(SpeedMs) * 3.6f;
	const float MaxSteer = 32.f;
	const float SpeedFactor = FMath::Clamp(1.f - SpeedKmh / 40.f, 0.18f, 1.f);
	const float TargetSteer = SteeringInput * MaxSteer * SpeedFactor;
	const float SteerStep = 130.f * DT;
	SteeringAngleDeg = FMath::Clamp(TargetSteer, SteeringAngleDeg - SteerStep, SteeringAngleDeg + SteerStep);

	// ---- 运动学（自行车模型） ----
	const float Wheelbase = 2.6f;
	const float SteerRad = FMath::DegreesToRadians(SteeringAngleDeg);
	float YawDeltaDeg = 0.f;
	if (FMath::Abs(SpeedMs) > 0.01f)
	{
		const float YawRate = SpeedMs / Wheelbase * FMath::Tan(SteerRad);
		YawDeltaDeg = FMath::RadiansToDegrees(YawRate * DT);
	}
	YawDeg += YawDeltaDeg;

	const FRotator NewRot(0.f, YawDeg, 0.f);
	const FVector Fwd = NewRot.Vector();
	FVector NewLoc = GetActorLocation() + Fwd * SpeedMs * DT;
	SetActorLocationAndRotation(NewLoc, NewRot, false);

	// ---- 转向灯回正自动取消 ----
	if (bLeftSignal || bRightSignal)
	{
		SignalYawAccum += FMath::Abs(YawDeltaDeg);
		if (SignalYawAccum > 40.f && FMath::Abs(SteeringAngleDeg) < 8.f)
		{
			bLeftSignal = false;
			bRightSignal = false;
			SignalYawAccum = 0.f;
		}
	}
	else
	{
		SignalYawAccum = 0.f;
	}

	// ---- 发动机转速（HUD 用） ----
	const float IdleRpm = 900.f;
	const float MaxRpm = 6200.f;
	if (bStalled)
	{
		EngineRpm = 0.f;
	}
	else if (Gear == EGear::N)
	{
		EngineRpm = IdleRpm + ThrottleInput * 1500.f;
	}
	else
	{
		const float Ratio = FMath::Clamp(FMath::Abs(SpeedMs) / FMath::Max(0.1f, FMath::Abs(GearMaxSpeed[GearIdx])), 0.f, 1.f);
		EngineRpm = IdleRpm + Ratio * (MaxRpm - IdleRpm) * (0.35f + 0.65f * ThrottleInput);
	}
}

void AKeMuSanPawn::UpdateVisuals(float DT)
{
	const float T = GetWorld()->GetTimeSeconds();
	const bool BlinkOn = (FMath::Fmod(T, 0.7f) < 0.35f);

	// 尾灯：刹车亮起 / 双闪
	FLinearColor TailColor(0.22f, 0.02f, 0.02f);
	if (BrakeInput > 0.05f)
	{
		TailColor = FLinearColor(1.f, 0.03f, 0.03f);
	}
	if (bHazard && BlinkOn)
	{
		TailColor = FLinearColor(1.f, 0.25f, 0.02f);
	}
	TailMatL->SetVectorParameterValue(FName("Color"), TailColor);
	TailMatR->SetVectorParameterValue(FName("Color"), TailColor);

	// 转向灯
	const FLinearColor SigOff(0.16f, 0.12f, 0.03f);
	const FLinearColor SigOn(1.f, 0.7f, 0.05f);
	const bool LeftBlink = (bLeftSignal || bHazard) && BlinkOn;
	const bool RightBlink = (bRightSignal || bHazard) && BlinkOn;
	SigMatL->SetVectorParameterValue(FName("Color"), LeftBlink ? SigOn : SigOff);
	SigMatR->SetVectorParameterValue(FName("Color"), RightBlink ? SigOn : SigOff);

	// 前照灯
	FLinearColor HeadColor(0.07f, 0.07f, 0.06f);
	if (bLowBeam || bOutline)
	{
		HeadColor = FLinearColor(0.9f, 0.9f, 0.72f);
	}
	if (bHighBeam || bFlashHigh)
	{
		HeadColor = FLinearColor(0.75f, 0.85f, 1.3f);
	}
	if (bFogLamp)
	{
		HeadColor = FLinearColor(1.f, 0.95f, 0.4f);
	}
	LightMatL->SetVectorParameterValue(FName("Color"), HeadColor);
	LightMatR->SetVectorParameterValue(FName("Color"), HeadColor);
}
