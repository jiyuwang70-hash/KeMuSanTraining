#include "TrafficActors.h"

#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

// ---------------------------------------------------------------------------
// AAICar
// ---------------------------------------------------------------------------
AAICar::AAICar()
{
	PrimaryActorTick.bCanEverTick = true;
	SetActorEnableCollision(false);
	SetActorHiddenInGame(true);

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterial> MatAsset(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	auto MakePart = [&](const TCHAR* Name, const FVector& Loc, const FVector& Scale, const FLinearColor& Color) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetStaticMesh(CubeAsset.Object);
		C->SetRelativeLocation(Loc);
		C->SetRelativeScale3D(Scale);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetupAttachment(Root);
		UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(MatAsset.Object, this);
		D->SetVectorParameterValue(FName("Color"), Color);
		C->SetMaterial(0, D);
		return C;
	};

	Body = MakePart(TEXT("Body"), FVector(0.f, 0.f, 0.7f), FVector(4.2f, 1.8f, 1.0f), FLinearColor(0.35f, 0.4f, 0.55f));
	Cabin = MakePart(TEXT("Cabin"), FVector(-0.1f, 0.f, 1.55f), FVector(2.2f, 1.7f, 0.6f), FLinearColor(0.12f, 0.14f, 0.18f));
}

void AAICar::InitRoute(const FVector& InStart, const FVector& InEnd, float InSpeedKmh)
{
	StartLoc = InStart;
	EndLoc = InEnd;
	SpeedMs = InSpeedKmh / 3.6f;
	bActive = true;
	SetActorLocation(InStart);
	SetActorHiddenInGame(false);
	const FVector Dir = (EndLoc - StartLoc).GetSafeNormal();
	const FRotator Rot = Dir.Rotation();
	SetActorRotation(Rot);
}

void AAICar::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bActive)
	{
		return;
	}

	const FVector Dir = (EndLoc - StartLoc).GetSafeNormal();
	const FVector NewLoc = GetActorLocation() + Dir * SpeedMs * DeltaSeconds;
	SetActorLocation(NewLoc);

	const float Traveled = FVector::Dist(StartLoc, NewLoc);
	if (Traveled >= FVector::Dist(StartLoc, EndLoc))
	{
		bActive = false;
		SetActorHiddenInGame(true);
	}
}

// ---------------------------------------------------------------------------
// APedestrian
// ---------------------------------------------------------------------------
APedestrian::APedestrian()
{
	PrimaryActorTick.bCanEverTick = true;
	SetActorEnableCollision(false);
	SetActorHiddenInGame(true);

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterial> MatAsset(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	auto MakePart = [&](const TCHAR* Name, const FVector& Loc, const FVector& Scale, const FLinearColor& Color) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetStaticMesh(CubeAsset.Object);
		C->SetRelativeLocation(Loc);
		C->SetRelativeScale3D(Scale);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetupAttachment(Root);
		UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(MatAsset.Object, this);
		D->SetVectorParameterValue(FName("Color"), Color);
		C->SetMaterial(0, D);
		return C;
	};

	BodyComp = MakePart(TEXT("Body"), FVector(0.f, 0.f, 0.85f), FVector(0.42f, 0.30f, 1.7f), FLinearColor(0.9f, 0.25f, 0.2f));
	HeadComp = MakePart(TEXT("Head"), FVector(0.f, 0.f, 1.9f), FVector(0.32f, 0.32f, 0.32f), FLinearColor(0.95f, 0.8f, 0.65f));
}

void APedestrian::StartCrossing(const FVector& InFrom, const FVector& InTo, float InSpeedMs)
{
	From = InFrom;
	To = InTo;
	Speed = InSpeedMs;
	TotalDist = FMath::Max(1.f, FVector::Dist(From, To));
	Traveled = 0.f;
	bActive = true;
	SetActorLocation(From);
	SetActorHiddenInGame(false);
}

void APedestrian::StopCrossing()
{
	bActive = false;
	SetActorHiddenInGame(true);
}

void APedestrian::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bActive)
	{
		return;
	}

	const FVector Dir = (To - From).GetSafeNormal();
	Traveled += Speed * DeltaSeconds;
	SetActorLocation(From + Dir * FMath::Min(Traveled, TotalDist));

	if (Traveled >= TotalDist)
	{
		bActive = false;
		SetActorHiddenInGame(true);
	}
}

bool APedestrian::IsOnRoad(float CrosswalkXLocal, float RoadHalfWidth) const
{
	if (!bActive)
	{
		return false;
	}
	const FVector L = GetActorLocation();
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
	Pole->SetRelativeLocation(FVector(0.f, 0.f, 2.6f));
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
	MakeHead(TEXT("HeadRed"), 5.5f, HeadRed, RedMat);
	MakeHead(TEXT("HeadYellow"), 4.9f, HeadYellow, YellowMat);
	MakeHead(TEXT("HeadGreen"), 4.3f, HeadGreen, GreenMat);
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
