#include "RoadBuilder.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

#include "RoadLayout.h"

using namespace RoadLayout;

ARoadBuilder::ARoadBuilder()
{
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// 保证场景有光照与天空（不依赖模板地图的灯光）
	Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetupAttachment(Root);
	Sun->SetRelativeRotation(FRotator(-52.f, 28.f, 0.f));
	Sun->SetIntensity(6.5f);

	Sky = CreateDefaultSubobject<USkyLightComponent>(TEXT("Sky"));
	Sky->SetupAttachment(Root);
	Sky->SetIntensity(1.1f);
	Sky->SourceType = SLS_CapturedScene;

	Atmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("Atmosphere"));
	Atmosphere->SetupAttachment(Root);
}

void ARoadBuilder::BeginPlay()
{
	Super::BeginPlay();

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylAsset(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeAsset(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneAsset(TEXT("/Engine/BasicShapes/Plane.Plane"));
	static ConstructorHelpers::FObjectFinder<UMaterial> MatAsset(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	CubeMesh = CubeAsset.Object;
	CylMesh = CylAsset.Object;
	ConeMesh = ConeAsset.Object;
	PlaneMesh = PlaneAsset.Object;
	BaseMat = MatAsset.Object;

	BuildRoad();
	BuildIntersection();
	BuildScenery();
}

UStaticMeshComponent* ARoadBuilder::AddBox(const FVector& Loc, const FVector& Scale, const FLinearColor& Color)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(CubeMesh);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetMobility(EComponentMobility::Static);
	C->SetCastShadow(true);
	C->RegisterComponent();
	C->SetWorldLocation(GetActorLocation() + Loc);
	C->SetWorldScale3D(Scale);
	UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(BaseMat, this);
	D->SetVectorParameterValue(FName("Color"), Color);
	C->SetMaterial(0, D);
	C->AttachToComponent(Root, FAttachmentTransformRules::KeepWorldTransform);
	return C;
}

UStaticMeshComponent* ARoadBuilder::AddCylinder(const FVector& Loc, const FVector& Scale, const FLinearColor& Color)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(CylMesh);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetMobility(EComponentMobility::Static);
	C->SetCastShadow(true);
	C->RegisterComponent();
	C->SetWorldLocation(GetActorLocation() + Loc);
	C->SetWorldScale3D(Scale);
	UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(BaseMat, this);
	D->SetVectorParameterValue(FName("Color"), Color);
	C->SetMaterial(0, D);
	C->AttachToComponent(Root, FAttachmentTransformRules::KeepWorldTransform);
	return C;
}

UStaticMeshComponent* ARoadBuilder::AddCone(const FVector& Loc, const FVector& Scale, const FLinearColor& Color)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(ConeMesh);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetMobility(EComponentMobility::Static);
	C->SetCastShadow(true);
	C->RegisterComponent();
	C->SetWorldLocation(GetActorLocation() + Loc);
	C->SetWorldScale3D(Scale);
	UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(BaseMat, this);
	D->SetVectorParameterValue(FName("Color"), Color);
	C->SetMaterial(0, D);
	return C;
}

void ARoadBuilder::BuildRoad()
{
	const float MidX = (RoadMinX + RoadEndX) * 0.5f;
	const float LenX = (RoadEndX - RoadMinX);

	// 草地
	{
		UStaticMeshComponent* G = NewObject<UStaticMeshComponent>(this);
		G->SetStaticMesh(PlaneMesh);
		G->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		G->SetMobility(EComponentMobility::Static);
		G->SetCastShadow(false);
		G->RegisterComponent();
		G->SetWorldLocation(GetActorLocation() + FVector(MidX, 0.f, -0.08f));
		G->SetWorldScale3D(FVector(LenX * 0.6f, 90.f, 1.f));
		UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(BaseMat, this);
		D->SetVectorParameterValue(FName("Color"), FLinearColor(0.28f, 0.48f, 0.22f));
		G->SetMaterial(0, D);
		G->AttachToComponent(Root, FAttachmentTransformRules::KeepWorldTransform);
	}

	// 沥青路面
	AddBox(FVector(MidX, 0.f, -0.06f), FVector(LenX * 0.5f, RoadHalfWidth, 0.08f), FLinearColor(0.14f, 0.14f, 0.15f));

	// 掉头区加宽（朝 -Y 侧，左转掉头用）
	AddBox(FVector(462.f, -5.5f, -0.06f), FVector(26.f, 2.4f, 0.08f), FLinearColor(0.15f, 0.15f, 0.16f));

	// 中心虚线
	for (float X = RoadMinX + 4.f; X < RoadEndX - 2.f; X += 2.0f)
	{
		AddBox(FVector(X, 0.f, 0.012f), FVector(1.0f, 0.14f, 0.015f), FLinearColor(0.85f, 0.75f, 0.2f));
	}

	// 车道边缘线
	AddBox(FVector(MidX, LaneWidth, 0.012f), FVector(LenX * 0.5f, 0.14f, 0.015f), FLinearColor(0.85f, 0.85f, 0.85f));
	AddBox(FVector(MidX, -LaneWidth, 0.012f), FVector(LenX * 0.5f, 0.14f, 0.015f), FLinearColor(0.85f, 0.85f, 0.85f));

	// 路缘石
	AddBox(FVector(MidX, CurbDistance, 0.04f), FVector(LenX * 0.5f, 0.34f, 0.16f), FLinearColor(0.5f, 0.5f, 0.52f));
	AddBox(FVector(MidX, -CurbDistance, 0.04f), FVector(LenX * 0.5f, 0.34f, 0.16f), FLinearColor(0.5f, 0.5f, 0.52f));

	// 人行道
	AddBox(FVector(MidX, CurbDistance + 0.75f, 0.11f), FVector(LenX * 0.5f, 0.8f, 0.22f), FLinearColor(0.55f, 0.55f, 0.58f));
	AddBox(FVector(MidX, -CurbDistance - 0.75f, 0.11f), FVector(LenX * 0.5f, 0.8f, 0.22f), FLinearColor(0.55f, 0.55f, 0.58f));

	// 起点 / 终点线
	AddBox(FVector(StartX, 0.f, 0.013f), FVector(0.3f, RoadHalfWidth, 0.015f), FLinearColor(0.9f, 0.9f, 0.9f));
	AddBox(FVector(FinishX, 0.f, 0.013f), FVector(0.3f, RoadHalfWidth, 0.015f), FLinearColor(0.9f, 0.9f, 0.9f));

	// 靠边停车参考线（距路缘石 30cm / 50cm 的提示线，朝 -X 行驶时的右侧）
	AddBox(FVector(408.5f, CurbYHeadingMinusX + 0.30f, 0.013f), FVector(23.5f, 0.05f, 0.015f), FLinearColor(0.9f, 0.75f, 0.2f));
	AddBox(FVector(408.5f, CurbYHeadingMinusX + 0.50f, 0.013f), FVector(23.5f, 0.05f, 0.015f), FLinearColor(0.9f, 0.3f, 0.25f));

	// 掉头引导锥桶
	const FVector Cones[] =
	{
		FVector(447.f, -3.2f, 0.f),
		FVector(441.f, -4.4f, 0.f),
		FVector(447.f, -5.6f, 0.f),
		FVector(457.f, -6.6f, 0.f),
		FVector(469.f, -7.1f, 0.f)
	};
	for (const FVector& C : Cones)
	{
		AddCone(C, FVector(0.34f, 0.34f, 0.7f), FLinearColor(1.f, 0.45f, 0.08f));
	}

	// 学校区域提示牌（右侧）
	{
		AddCylinder(FVector(SchoolStartX - 2.f, 6.3f, 1.6f), FVector(0.07f, 0.07f, 1.6f), FLinearColor(0.4f, 0.4f, 0.42f));
		AddBox(FVector(SchoolStartX - 2.f, 6.3f, 3.3f), FVector(1.1f, 0.08f, 0.8f), FLinearColor(0.08f, 0.3f, 0.85f));
	}

	// 公交车站（左侧）
	{
		AddCylinder(FVector(BusStartX - 2.f, -6.3f, 1.6f), FVector(0.07f, 0.07f, 1.6f), FLinearColor(0.4f, 0.4f, 0.42f));
		AddBox(FVector(BusStartX - 2.f, -6.3f, 3.3f), FVector(1.1f, 0.08f, 0.8f), FLinearColor(0.05f, 0.55f, 0.2f));
		AddBox(FVector(BusStartX + 4.f, -6.9f, 1.1f), FVector(2.6f, 0.7f, 1.1f), FLinearColor(0.72f, 0.75f, 0.78f));
	}

	// 加减挡操作区提示牌
	{
		AddCylinder(FVector(GearStartX + 2.f, 6.3f, 1.6f), FVector(0.07f, 0.07f, 1.6f), FLinearColor(0.4f, 0.4f, 0.42f));
		AddBox(FVector(GearStartX + 2.f, 6.3f, 3.3f), FVector(1.1f, 0.08f, 0.8f), FLinearColor(0.75f, 0.45f, 0.05f));
	}
}

void ARoadBuilder::BuildIntersection()
{
	// 横向道路（穿越主路）
	AddBox(FVector((IntersectionMinX + IntersectionMaxX) * 0.5f, 0.f, -0.06f), FVector(4.5f, 42.f, 0.08f), FLinearColor(0.14f, 0.14f, 0.15f));

	// 人行横道斑马线
	for (int32 i = 0; i < 6; ++i)
	{
		AddBox(FVector(CrosswalkX - 0.75f + i * 0.3f, 0.f, 0.012f), FVector(0.18f, RoadHalfWidth, 0.015f), FLinearColor(0.82f, 0.82f, 0.82f));
	}

	// 停止线
	AddBox(FVector(StopLineX, 0.f, 0.013f), FVector(0.32f, RoadHalfWidth, 0.015f), FLinearColor(0.95f, 0.95f, 0.95f));
}

void ARoadBuilder::BuildScenery()
{
	// 两侧建筑
	const FLinearColor BuildingColors[] =
	{
		FLinearColor(0.72f, 0.68f, 0.6f),
		FLinearColor(0.6f, 0.66f, 0.72f),
		FLinearColor(0.76f, 0.6f, 0.55f),
		FLinearColor(0.62f, 0.7f, 0.62f)
	};
	int32 ColorIdx = 0;
	for (float X = RoadMinX + 10.f; X < RoadEndX - 6.f; X += 34.f)
	{
		const float Height = 7.f + FMath::Fmod(X * 0.37f, 7.f);
		AddBox(FVector(X, 11.5f, Height * 0.5f), FVector(9.f, 4.5f, Height * 0.5f), BuildingColors[ColorIdx % 4]);
		AddBox(FVector(X + 16.f, -11.5f, (Height - 2.f) * 0.5f), FVector(7.f, 4.5f, (Height - 2.f) * 0.5f), BuildingColors[(ColorIdx + 1) % 4]);
		ColorIdx += 2;
	}

	// 两侧行道树
	for (float X = RoadMinX + 6.f; X < RoadEndX; X += 13.f)
	{
		AddCylinder(FVector(X, 9.6f, 0.9f), FVector(0.16f, 0.16f, 0.9f), FLinearColor(0.4f, 0.3f, 0.2f));
		AddCone(FVector(X, 9.6f, 2.2f), FVector(1.3f, 1.3f, 1.9f), FLinearColor(0.16f, 0.45f, 0.18f));
		AddCylinder(FVector(X + 6.5f, -9.6f, 0.9f), FVector(0.16f, 0.16f, 0.9f), FLinearColor(0.4f, 0.3f, 0.2f));
		AddCone(FVector(X + 6.5f, -9.6f, 2.2f), FVector(1.3f, 1.3f, 1.9f), FLinearColor(0.16f, 0.45f, 0.18f));
	}

	// 路口四角建筑
	AddBox(FVector(IntersectionMinX - 6.f, 12.f, 6.f), FVector(7.f, 7.f, 6.f), FLinearColor(0.68f, 0.7f, 0.75f));
	AddBox(FVector(IntersectionMaxX + 6.f, 12.f, 5.f), FVector(7.f, 7.f, 5.f), FLinearColor(0.72f, 0.62f, 0.55f));
	AddBox(FVector(IntersectionMinX - 6.f, -12.f, 5.f), FVector(7.f, 7.f, 5.f), FLinearColor(0.62f, 0.68f, 0.72f));
	AddBox(FVector(IntersectionMaxX + 6.f, -12.f, 6.f), FVector(7.f, 7.f, 6.f), FLinearColor(0.7f, 0.66f, 0.6f));
}
