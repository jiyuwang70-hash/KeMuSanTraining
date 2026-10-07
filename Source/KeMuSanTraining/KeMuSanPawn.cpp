#include "KeMuSanPawn.h"
#include "Components/BoxComponent.h"
#include "VehiclePhysicsCore.h"
#include "VehicleDynamicsComponent.h"
#include "VehicleDamageComponent.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/WidgetComponent.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Styling/CoreStyle.h"
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
	SetActorEnableCollision(true);

	Root = CreateDefaultSubobject<UBoxComponent>(TEXT("Root"));
 VehiclePhysics::Configure(Root,FVector(215,98,55),1400.f);
 Dynamics=CreateDefaultSubobject<UVehicleDynamicsComponent>(TEXT("Dynamics"));
 Damage=CreateDefaultSubobject<UVehicleDamageComponent>(TEXT("Damage"));
 Root->OnComponentHit.AddDynamic(this,&AKeMuSanPawn::OnVehicleHit);
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

	auto MakePart = [&](const TCHAR* Name, UStaticMesh* Mesh, const FVector& Loc, const FVector& Scale, const FRotator& Rot, const FLinearColor& Color) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* Comp = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Comp->SetStaticMesh(Mesh);
		Comp->SetRelativeLocation(Loc);
		Comp->SetRelativeRotation(Rot);
		Comp->SetRelativeScale3D(Scale);
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Comp->SetupAttachment(Root);
		UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(BaseMat, this);
		DynMat->SetVectorParameterValue(FName("Color"), Color);
		Comp->SetMaterial(0, DynMat);
		return Comp;
	};

	const FLinearColor ColorTrim(0.06f, 0.07f, 0.08f); // 汽车底盘与防擦饰条哑光黑
	const FLinearColor ColorGlass(0.05f, 0.07f, 0.10f); // 汽车车窗深色反光玻璃

	// 车身（下半部腰线，顶面高度约90cm，底面在18cm，确保座舱通透，绝不遮挡驾驶员视线与镜面）
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetStaticMesh(CubeMesh);
	Body->SetRelativeLocation(FVector(0.f, 0.f, 54.f));
	Body->SetRelativeScale3D(FVector(4.2f, 1.8f, 0.72f));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetupAttachment(Root);
	UMaterialInstanceDynamic* BodyDyn = UMaterialInstanceDynamic::Create(BaseMat, this);
	BodyDyn->SetVectorParameterValue(FName("Color"), ColorBody);
	Body->SetMaterial(0, BodyDyn);

	// 前引擎盖（Hood：前机舱微隆起，平滑过渡到前风挡下沿，保持主驾前方视野极度通透）
	MakePart(TEXT("Hood"), CubeMesh, FVector(140.f, 0.f, 88.f), FVector(1.36f, 1.76f, 0.08f), FRotator::ZeroRotator, ColorBody);

	// 后行李箱盖（TrunkLid：三厢轿车标志性车尾造型，第三人称正后方清晰可见标准车身）
	MakePart(TEXT("TrunkLid"), CubeMesh, FVector(-165.f, 0.f, 96.f), FVector(0.88f, 1.76f, 0.14f), FRotator::ZeroRotator, ColorBody);

	// 侧窗下沿腰线梁（Beltline Trim，左右两侧黑色防擦装饰线）
	MakePart(TEXT("BeltlineL"), CubeMesh, FVector(-25.f, -87.f, 95.f), FVector(2.05f, 0.06f, 0.10f), FRotator::ZeroRotator, ColorTrim);
	MakePart(TEXT("BeltlineR"), CubeMesh, FVector(-25.f, 87.f, 95.f), FVector(2.05f, 0.06f, 0.10f), FRotator::ZeroRotator, ColorTrim);

	// 车顶棚（Cabin：车身同色珍珠白顶棚钣金，与 A/C 柱一体成型，彻底告别悬空不明异物薄板感）
	Cabin = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Cabin"));
	Cabin->SetStaticMesh(CubeMesh);
	Cabin->SetRelativeLocation(FVector(-20.f, 0.f, 183.f));
	Cabin->SetRelativeScale3D(FVector(2.15f, 1.70f, 0.12f));
	Cabin->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Cabin->SetupAttachment(Root);
	UMaterialInstanceDynamic* CabinDyn = UMaterialInstanceDynamic::Create(BaseMat, this);
	CabinDyn->SetVectorParameterValue(FName("Color"), ColorBody);
	Cabin->SetMaterial(0, CabinDyn);

	// 车顶侧边纵梁（与车身同色白）
	MakePart(TEXT("RoofRailL"), CubeMesh, FVector(-20.f, -85.f, 183.f), FVector(2.15f, 0.08f, 0.12f), FRotator::ZeroRotator, ColorBody);
	MakePart(TEXT("RoofRailR"), CubeMesh, FVector(-20.f, 85.f, 183.f), FVector(2.15f, 0.08f, 0.12f), FRotator::ZeroRotator, ColorBody);

	// 前/后风挡上沿横梁（车顶前后外檐，车身同色白）
	MakePart(TEXT("FrontHeader"), CubeMesh, FVector(84.f, 0.f, 182.f), FVector(0.14f, 1.66f, 0.10f), FRotator::ZeroRotator, ColorBody);
	MakePart(TEXT("RearHeader"), CubeMesh, FVector(-124.f, 0.f, 182.f), FVector(0.16f, 1.66f, 0.10f), FRotator::ZeroRotator, ColorBody);

	// A 柱（前挡风玻璃立柱，车身同色珍珠白，左右两侧斜向支撑车顶，更具流线型）
	MakePart(TEXT("PillarFL"), CubeMesh, FVector(68.f, -88.f, 138.f), FVector(0.14f, 0.06f, 0.86f), FRotator(-18.f, 0.f, 0.f), ColorBody);
	MakePart(TEXT("PillarFR"), CubeMesh, FVector(68.f, 88.f, 138.f), FVector(0.14f, 0.06f, 0.86f), FRotator(-18.f, 0.f, 0.f), ColorBody);

	// B 柱（门中柱，汽车标配黑色立柱，位于驾驶员正左/正右侧方）
	MakePart(TEXT("PillarML"), CubeMesh, FVector(-25.f, -87.f, 138.f), FVector(0.12f, 0.05f, 0.86f), FRotator::ZeroRotator, ColorTrim);
	MakePart(TEXT("PillarMR"), CubeMesh, FVector(-25.f, 87.f, 138.f), FVector(0.12f, 0.05f, 0.86f), FRotator::ZeroRotator, ColorTrim);

	// C 柱（后挡风玻璃立柱，左右两侧珍珠白钣金，斜向无缝连接后备箱与车顶，溜背造型自然）
	MakePart(TEXT("PillarRL"), CubeMesh, FVector(-120.f, -84.f, 138.f), FVector(0.38f, 0.10f, 0.86f), FRotator(18.f, 0.f, 0.f), ColorBody);
	MakePart(TEXT("PillarRR"), CubeMesh, FVector(-120.f, 84.f, 138.f), FVector(0.38f, 0.10f, 0.86f), FRotator(18.f, 0.f, 0.f), ColorBody);

	// 后风挡深色车窗玻璃外饰（位于左右 C 柱内侧，形成写实的深色玻璃窗框，中间留出 90cm 宽后视通廊，车内后视镜 100% 通透）
	MakePart(TEXT("RearGlassL"), CubeMesh, FVector(-121.f, -55.f, 138.f), FVector(0.08f, 0.35f, 0.84f), FRotator(18.f, 0.f, 0.f), ColorGlass);
	MakePart(TEXT("RearGlassR"), CubeMesh, FVector(-121.f, 55.f, 138.f), FVector(0.08f, 0.35f, 0.84f), FRotator(18.f, 0.f, 0.f), ColorGlass);

	// 前后保险杠与侧裙底梁（哑光黑色防擦工程塑料造型，强化整车真实比例）
	MakePart(TEXT("FrontBumper"), CubeMesh, FVector(212.f, 0.f, 44.f), FVector(0.16f, 1.84f, 0.28f), FRotator::ZeroRotator, ColorTrim);
	MakePart(TEXT("RearBumper"), CubeMesh, FVector(-212.f, 0.f, 44.f), FVector(0.16f, 1.84f, 0.28f), FRotator::ZeroRotator, ColorTrim);
	MakePart(TEXT("SideSkirtL"), CubeMesh, FVector(0.f, -89.f, 26.f), FVector(3.2f, 0.06f, 0.14f), FRotator::ZeroRotator, ColorTrim);
	MakePart(TEXT("SideSkirtR"), CubeMesh, FVector(0.f, 89.f, 26.f), FVector(3.2f, 0.06f, 0.14f), FRotator::ZeroRotator, ColorTrim);

	// 主驾与副驾舒适座椅（坐垫 + 后仰靠背 + 头枕，深色织物材质，座舱立体感与后视镜真实度跃升）
	const FLinearColor ColorSeat(0.10f, 0.11f, 0.14f);
	MakePart(TEXT("SeatBaseL"), CubeMesh, FVector(-15.f, -42.f, 66.f), FVector(0.48f, 0.44f, 0.16f), FRotator::ZeroRotator, ColorSeat);
	MakePart(TEXT("SeatBackL"), CubeMesh, FVector(-38.f, -42.f, 102.f), FVector(0.12f, 0.42f, 0.60f), FRotator(12.f, 0.f, 0.f), ColorSeat);
	MakePart(TEXT("SeatHeadL"), CubeMesh, FVector(-45.f, -42.f, 136.f), FVector(0.10f, 0.22f, 0.16f), FRotator(12.f, 0.f, 0.f), ColorSeat);

	MakePart(TEXT("SeatBaseR"), CubeMesh, FVector(-15.f, 42.f, 66.f), FVector(0.48f, 0.44f, 0.16f), FRotator::ZeroRotator, ColorSeat);
	MakePart(TEXT("SeatBackR"), CubeMesh, FVector(-38.f, 42.f, 102.f), FVector(0.12f, 0.42f, 0.60f), FRotator(12.f, 0.f, 0.f), ColorSeat);
	MakePart(TEXT("SeatHeadR"), CubeMesh, FVector(-45.f, 42.f, 136.f), FVector(0.10f, 0.22f, 0.16f), FRotator(12.f, 0.f, 0.f), ColorSeat);

	// 车顶驾校标配“教练车”顶灯（明黄灯箱 + 黑色底座 + 前后双面 UWidgetComponent 实打实渲染“教练”汉字）
	MakePart(TEXT("RoofSignBase"), CubeMesh, FVector(-20.f, 0.f, 189.5f), FVector(0.44f, 0.90f, 0.04f), FRotator::ZeroRotator, ColorTrim);
	MakePart(TEXT("RoofSign"), CubeMesh, FVector(-20.f, 0.f, 193.5f), FVector(0.40f, 0.85f, 0.08f), FRotator::ZeroRotator, FLinearColor(0.96f, 0.82f, 0.18f));

	auto CreateSignWidget = [&](const TCHAR* CompName, const FVector& Loc, const FRotator& Rot) -> UWidgetComponent*
	{
		UWidgetComponent* WC = CreateDefaultSubobject<UWidgetComponent>(CompName);
		WC->SetupAttachment(Root);
		WC->SetRelativeLocation(Loc);
		WC->SetRelativeRotation(Rot);
		WC->SetDrawSize(FVector2D(640.f, 64.f));
		WC->SetRelativeScale3D(FVector(0.125f, 0.125f, 0.125f)); // 640x64 * 0.125 = 80x8cm，精确贴合车顶标牌灯箱
		WC->SetWidgetSpace(EWidgetSpace::World);
		WC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return WC;
	};
	// 正面朝车头 (+X，紧贴灯箱前端面 X=0cm)
	RoofSignWidgetF = CreateSignWidget(TEXT("RoofSignWidgetF"), FVector(0.5f, 0.f, 193.5f), FRotator(0.f, 0.f, 0.f));
	// 背面朝车尾 (-X，紧贴灯箱后端面 X=-40cm)
	RoofSignWidgetR = CreateSignWidget(TEXT("RoofSignWidgetR"), FVector(-40.5f, 0.f, 193.5f), FRotator(0.f, 180.f, 0.f));

	// 仪表台（前挡风玻璃下方）
	Dashboard = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Dashboard"));
	Dashboard->SetStaticMesh(CubeMesh);
	Dashboard->SetRelativeLocation(FVector(45.f, 0.f, 98.f));
	Dashboard->SetRelativeScale3D(FVector(0.40f, 1.62f, 0.22f));
	Dashboard->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Dashboard->SetupAttachment(Root);
	UMaterialInstanceDynamic* DashDyn = UMaterialInstanceDynamic::Create(BaseMat, this);
	DashDyn->SetVectorParameterValue(FName("Color"), FLinearColor(0.06f, 0.07f, 0.08f));
	Dashboard->SetMaterial(0, DashDyn);

	// 方向盘（主驾驶位前方，保留该组件作为旋转根节点但无实心Mesh，Scale=1:1:1，Pitch约-35°使局部YZ轮面法线正对主驾眼睛）
	SteeringWheel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SteeringWheel"));
	SteeringWheel->SetRelativeLocation(FVector(20.f, -42.f, 114.f));
	SteeringWheel->SetRelativeRotation(FRotator(-35.f, 0.f, 0.f));
	SteeringWheel->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));
	SteeringWheel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SteeringWheel->SetupAttachment(Root);

	// 16段沿半径18cm切向小立方体构成的真正空心中空外环（绝非实心圆盘）
	const float RimRadius = 18.f;
	const float SegLen = 2.f * PI * RimRadius / 16.f * 1.06f; // 约 7.5cm
	const FLinearColor ColorSteerRim(0.06f, 0.06f, 0.07f);
	for (int32 i = 0; i < 16; ++i)
	{
		const float AngleDeg = i * (360.f / 16.f);
		const float AngleRad = FMath::DegreesToRadians(AngleDeg);
		const float SegY = RimRadius * FMath::Cos(AngleRad);
		const float SegZ = RimRadius * FMath::Sin(AngleRad);
		const FRotator SegRot(0.f, 0.f, -(AngleDeg + 90.f));

		UStaticMeshComponent* Seg = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("SteerRim_%d"), i));
		Seg->SetStaticMesh(CubeMesh);
		Seg->SetRelativeLocation(FVector(0.f, SegY, SegZ));
		Seg->SetRelativeRotation(SegRot);
		Seg->SetRelativeScale3D(FVector(0.035f, SegLen * 0.01f, 0.035f));
		Seg->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Seg->SetupAttachment(SteeringWheel);
		UMaterialInstanceDynamic* SegDyn = UMaterialInstanceDynamic::Create(BaseMat, this);
		SegDyn->SetVectorParameterValue(FName("Color"), ColorSteerRim);
		Seg->SetMaterial(0, SegDyn);
	}

	// 方向盘中心轮毂（小圆柱，Pitch 90°使圆柱轴沿轮面法线局部X，厚度薄约3cm，直径约12cm）
	UStaticMeshComponent* SteerHub = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SteerHub"));
	SteerHub->SetStaticMesh(CylMesh);
	SteerHub->SetRelativeLocation(FVector(0.f, 0.f, 0.f));
	SteerHub->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
	SteerHub->SetRelativeScale3D(FVector(0.12f, 0.12f, 0.03f));
	SteerHub->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SteerHub->SetupAttachment(SteeringWheel);
	UMaterialInstanceDynamic* HubDyn = UMaterialInstanceDynamic::Create(BaseMat, this);
	HubDyn->SetVectorParameterValue(FName("Color"), FLinearColor(0.08f, 0.09f, 0.11f));
	SteerHub->SetMaterial(0, HubDyn);

	// 三幅辐条（左/右/底三根，连接中心轮毂与外环，中间完全镂空，仪表盘清晰可见）
	const FLinearColor ColorSpoke(0.16f, 0.18f, 0.22f);
	auto MakeSpoke = [&](const TCHAR* SpokeName, const FVector& Loc, const FVector& Scale)
	{
		UStaticMeshComponent* Sp = CreateDefaultSubobject<UStaticMeshComponent>(SpokeName);
		Sp->SetStaticMesh(CubeMesh);
		Sp->SetRelativeLocation(Loc);
		Sp->SetRelativeScale3D(Scale);
		Sp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Sp->SetupAttachment(SteeringWheel);
		UMaterialInstanceDynamic* SpDyn = UMaterialInstanceDynamic::Create(BaseMat, this);
		SpDyn->SetVectorParameterValue(FName("Color"), ColorSpoke);
		Sp->SetMaterial(0, SpDyn);
	};
	MakeSpoke(TEXT("SpokeL"), FVector(0.f, -9.5f, 0.f), FVector(0.025f, 0.12f, 0.03f));
	MakeSpoke(TEXT("SpokeR"), FVector(0.f, 9.5f, 0.f), FVector(0.025f, 0.12f, 0.03f));
	MakeSpoke(TEXT("SpokeB"), FVector(0.f, 0.f, -9.5f), FVector(0.025f, 0.03f, 0.12f));

	// 车轮（圆柱绕 X 轴横卧 Roll=90°，局部 XY 为横截面正圆半径 0.68f，Z 为轮宽轴向 0.24f，杜绝径向压扁）
	auto MakeWheel = [&](const TCHAR* Name, const FVector& Loc) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* W = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		W->SetStaticMesh(CylMesh);
		W->SetRelativeLocation(Loc);
		W->SetRelativeRotation(FRotator(0.f, 0.f, 90.f));
		W->SetRelativeScale3D(FVector(0.68f, 0.68f, 0.24f));
		W->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		W->SetupAttachment(Root);
		UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(BaseMat, this);
		D->SetVectorParameterValue(FName("Color"), ColorWheel);
		W->SetMaterial(0, D);
		return W;
	};
	WheelFL = MakeWheel(TEXT("WheelFL"), FVector(135.f, -82.f, 34.f));
	WheelFR = MakeWheel(TEXT("WheelFR"), FVector(135.f, 82.f, 34.f));
	WheelRL = MakeWheel(TEXT("WheelRL"), FVector(-135.f, -82.f, 34.f));
	WheelRR = MakeWheel(TEXT("WheelRR"), FVector(-135.f, 82.f, 34.f));

	// 独立金属轮毂盖（直接附加到 Root，避免父组件非均匀缩放污染，精确定位在轮胎外侧 Y=±94.3cm，正圆形，厚4cm）
	const FLinearColor ColorRim(0.72f, 0.74f, 0.78f);
	auto MakeRim = [&](const TCHAR* Name, const FVector& Loc) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* Rim = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Rim->SetStaticMesh(CylMesh);
		Rim->SetRelativeLocation(Loc);
		Rim->SetRelativeRotation(FRotator(0.f, 0.f, 90.f));
		Rim->SetRelativeScale3D(FVector(0.46f, 0.46f, 0.04f));
		Rim->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Rim->SetupAttachment(Root);
		UMaterialInstanceDynamic* RimDyn = UMaterialInstanceDynamic::Create(BaseMat, this);
		RimDyn->SetVectorParameterValue(FName("Color"), ColorRim);
		Rim->SetMaterial(0, RimDyn);
		return Rim;
	};
	RimFL = MakeRim(TEXT("RimFL"), FVector(135.f, -94.3f, 34.f));
	RimFR = MakeRim(TEXT("RimFR"), FVector(135.f, 94.3f, 34.f));
	RimRL = MakeRim(TEXT("RimRL"), FVector(-135.f, -94.3f, 34.f));
	RimRR = MakeRim(TEXT("RimRR"), FVector(-135.f, 94.3f, 34.f));

	// 前照灯（左舵：左侧 -Y，右侧 +Y）
	LightMatL = MakeMesh(TEXT("LightFL"), CubeMesh, FVector(211.f, -62.f, 72.f), FVector(0.30f, 0.24f, 0.16f), FLinearColor(0.08f, 0.08f, 0.07f), LightFL);
	LightMatR = MakeMesh(TEXT("LightFR"), CubeMesh, FVector(211.f, 62.f, 72.f), FVector(0.30f, 0.24f, 0.16f), FLinearColor(0.08f, 0.08f, 0.07f), LightFR);

	// 尾灯（左侧 -Y，右侧 +Y）
	TailMatL = MakeMesh(TEXT("TailL"), CubeMesh, FVector(-211.f, -62.f, 72.f), FVector(0.30f, 0.24f, 0.14f), FLinearColor(0.22f, 0.02f, 0.02f), TailL);
	TailMatR = MakeMesh(TEXT("TailR"), CubeMesh, FVector(-211.f, 62.f, 72.f), FVector(0.30f, 0.24f, 0.14f), FLinearColor(0.22f, 0.02f, 0.02f), TailR);

	// 转向灯（琥珀色，左侧 -Y，右侧 +Y）
	SigMatL = MakeMesh(TEXT("SigL"), CubeMesh, FVector(-211.f, -92.f, 72.f), FVector(0.26f, 0.14f, 0.12f), FLinearColor(0.16f, 0.12f, 0.03f), SigL);
	SigMatR = MakeMesh(TEXT("SigR"), CubeMesh, FVector(-211.f, 92.f, 72.f), FVector(0.26f, 0.14f, 0.12f), FLinearColor(0.16f, 0.12f, 0.03f), SigR);

	// 追尾摄像机（bInheritYaw 设为 true，转弯时严密跟车追尾，杜绝90°/180°弯道视角失跟）
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(Root);
	SpringArm->TargetArmLength = 680.f;
	SpringArm->SetRelativeLocation(FVector(-40.f, 0.f, 120.f));
	SpringArm->SetRelativeRotation(FRotator(-11.f, 0.f, 0.f));
	SpringArm->bDoCollisionTest = false;
	SpringArm->bInheritPitch = false;
	SpringArm->bInheritYaw = true;
	SpringArm->bInheritRoll = false;
	SpringArm->bEnableCameraLag = true;
	SpringArm->CameraLagSpeed = 8.f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm);
	Camera->FieldOfView = 95.f;

	// 第一人称座舱摄像机（主驾视点，左舵 y = -42cm，后移至-50cm高148cm下倾5°使方向盘中空轮毂/辐条清晰入镜）
	CockpitCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("CockpitCamera"));
	CockpitCamera->SetupAttachment(Root);
	CockpitCamera->SetRelativeLocation(FVector(-50.f, -42.f, 148.f));
	CockpitCamera->SetRelativeRotation(FRotator(-5.f, 0.f, 0.f));
	CockpitCamera->FieldOfView = 85.f;
	CockpitCamera->bAutoActivate = false;

	// 真实光学后视镜捕获组件（左镜外展 Yaw 196°，右镜外展 Yaw 164°，中央镜居中透过后风挡 FOV 35°）
	LeftMirrorCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("LeftMirrorCapture"));
	LeftMirrorCapture->SetupAttachment(Root);
	LeftMirrorCapture->SetRelativeLocation(FVector(70.f, -105.f, 108.f));
	LeftMirrorCapture->SetRelativeRotation(FRotator(-2.f, 196.f, 0.f));
	LeftMirrorCapture->FOVAngle = 55.f;
	LeftMirrorCapture->CaptureSource = SCS_FinalColorLDR;
	LeftMirrorCapture->bCaptureEveryFrame = false;
	LeftMirrorCapture->bCaptureOnMovement = false;
	LeftMirrorCapture->bOverride_CustomNearClippingPlane = true;
	LeftMirrorCapture->CustomNearClippingPlane = 10.0f;

	RightMirrorCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("RightMirrorCapture"));
	RightMirrorCapture->SetupAttachment(Root);
	RightMirrorCapture->SetRelativeLocation(FVector(70.f, 105.f, 108.f));
	RightMirrorCapture->SetRelativeRotation(FRotator(-3.f, 164.f, 0.f));
	RightMirrorCapture->FOVAngle = 55.f;
	RightMirrorCapture->CaptureSource = SCS_FinalColorLDR;
	RightMirrorCapture->bCaptureEveryFrame = false;
	RightMirrorCapture->bCaptureOnMovement = false;
	RightMirrorCapture->bOverride_CustomNearClippingPlane = true;
	RightMirrorCapture->CustomNearClippingPlane = 10.0f;

	InteriorMirrorCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("InteriorMirrorCapture"));
	InteriorMirrorCapture->SetupAttachment(Root);
	InteriorMirrorCapture->SetRelativeLocation(FVector(35.f, 0.0f, 148.f));
	InteriorMirrorCapture->SetRelativeRotation(FRotator(-1.f, 180.f, 0.f));
	InteriorMirrorCapture->FOVAngle = 35.f;
	InteriorMirrorCapture->CaptureSource = SCS_FinalColorLDR;
	InteriorMirrorCapture->bCaptureEveryFrame = false;
	InteriorMirrorCapture->bCaptureOnMovement = false;
	InteriorMirrorCapture->bOverride_CustomNearClippingPlane = true;
	InteriorMirrorCapture->CustomNearClippingPlane = 10.0f;
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

void AKeMuSanPawn::CycleGearAuto()
{
	// P -> R -> N -> D -> P
	if (Gear == EGear::G1)     { Gear = EGear::N; }         // D->N
	else if (Gear == EGear::G2) { Gear = EGear::G1; }       // (D alias) stay D
	else if (Gear == EGear::R)  { Gear = EGear::G1; }       // R->D
	else if (Gear == EGear::N)  { Gear = EGear::R; }        // N->R
	else                        { Gear = EGear::N; }
	bStalled = false;
}

void AKeMuSanPawn::BeginPlay()
{
 Root->SetSimulatePhysics(true);
	Super::BeginPlay();

	// 创建后视镜 RenderTarget
	LeftMirrorTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("LeftMirrorTarget"));
	LeftMirrorTarget->InitCustomFormat(256, 128, PF_B8G8R8A8, false);
	LeftMirrorTarget->UpdateResourceImmediate(true);
	if (LeftMirrorCapture)
	{
		LeftMirrorCapture->TextureTarget = LeftMirrorTarget;
	}

	RightMirrorTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("RightMirrorTarget"));
	RightMirrorTarget->InitCustomFormat(256, 128, PF_B8G8R8A8, false);
	RightMirrorTarget->UpdateResourceImmediate(true);
	if (RightMirrorCapture)
	{
		RightMirrorCapture->TextureTarget = RightMirrorTarget;
	}

	InteriorMirrorTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("InteriorMirrorTarget"));
	InteriorMirrorTarget->InitCustomFormat(384, 128, PF_B8G8R8A8, false);
	InteriorMirrorTarget->UpdateResourceImmediate(true);
	if (InteriorMirrorCapture)
	{
		InteriorMirrorCapture->TextureTarget = InteriorMirrorTarget;
	}

	// 初始化车顶教练车标牌实际“教 练”汉字（通过 UWidgetComponent 在世界空间真正渲染，640x64配合0.125缩放）
	auto InitSignSlate = [](UWidgetComponent* WC)
	{
		if (!WC) return;
		FSlateFontInfo FontInfo = FCoreStyle::Get().GetFontStyle("NormalFont");
		FontInfo.Size = 48;

		TSharedRef<SWidget> SignWidget = SNew(SBox)
			.WidthOverride(640.f)
			.HeightOverride(64.f)
			[
				SNew(SBorder)
				.BorderBackgroundColor(FLinearColor(0.96f, 0.82f, 0.18f, 0.0f))
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("教练")))
					.Font(FontInfo)
					.ColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.08f, 0.08f, 1.0f)))
				]
			];
		WC->SetSlateWidget(SignWidget);
	};
	InitSignSlate(RoofSignWidgetF);
	InitSignSlate(RoofSignWidgetR);

	ResetAllMirrorsToStandard();
	UpdateMirrorOptics();
	CaptureAllMirrorsImmediate();
}

void AKeMuSanPawn::CaptureAllMirrorsImmediate()
{
	if (LeftMirrorCapture) LeftMirrorCapture->CaptureScene();
	if (InteriorMirrorCapture) InteriorMirrorCapture->CaptureScene();
	if (RightMirrorCapture) RightMirrorCapture->CaptureScene();
}

void AKeMuSanPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
 SpeedMs=FVector::DotProduct(Root->GetPhysicsLinearVelocity(),GetActorForwardVector())*.01f;
	if (Transmission == ETransmissionType::Auto)
	{
		UpdatePhysicsAuto(DeltaSeconds);
	}
	else
	{
		UpdatePhysics(DeltaSeconds);
	}
	UpdateVisuals(DeltaSeconds);

	// 观察计时与镜面光学视线刷新
	HeadCheckTimer = FMath::Max(0.f, HeadCheckTimer - DeltaSeconds);
	UpdateMirrorOptics();

	// 时间片轮流捕获三面光学后视镜：总刷新 30Hz（每 0.033s 捕获一面），单面镜 ~10Hz，杜绝每帧全城重画3次
	MirrorCaptureTimer += DeltaSeconds;
	if (MirrorCaptureTimer >= 0.033f)
	{
		MirrorCaptureTimer = 0.f;
		if (NextMirrorToCapture == 0 && LeftMirrorCapture)
		{
			LeftMirrorCapture->CaptureScene();
		}
		else if (NextMirrorToCapture == 1 && InteriorMirrorCapture)
		{
			InteriorMirrorCapture->CaptureScene();
		}
		else if (NextMirrorToCapture == 2 && RightMirrorCapture)
		{
			RightMirrorCapture->CaptureScene();
		}
		NextMirrorToCapture = (NextMirrorToCapture + 1) % 3;
	}
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
	HeadCheckTimer = 2.5f;
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
	Damage->Repair();
 SetActorLocationAndRotation(Loc * 100.0f+FVector(0,0,85), Rot, false, nullptr, ETeleportType::TeleportPhysics);
	Root->SetPhysicsLinearVelocity(FVector::ZeroVector);
 Root->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
 LastVehicleHitTime=-1000.f;
 SpeedMs = 0.f;
	YawDeg = Rot.Yaw;
	SteeringAngleDeg = 0.f;
	EngineRpm = 900.f;
	Gear = EGear::N;
	ThrottleInput = 0.f;
	SmoothedThrottle = 0.f;
	BrakeInput = 0.f;
	SteeringInput = 0.f;
	bStalled = false;
	StallCooldown = 0.f;
	SignalYawAccum = 0.f;
	bHandbrake = true;
	bSeatbelt = false;
	bDrivable = false;
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
	HeadCheckTimer = 0.f;
	bMirrorAdjustMode = false;
	ResetAllMirrorsToStandard();
}

void AKeMuSanPawn::ForceStop()
{
 Root->SetPhysicsLinearVelocity(FVector::ZeroVector);
 Root->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
	SpeedMs = 0.f;
	ThrottleInput = 0.f;
}

void AKeMuSanPawn::ToggleCameraView()
{
	bCockpitView = !bCockpitView;
	if (Camera && CockpitCamera)
	{
		Camera->SetActive(!bCockpitView);
		CockpitCamera->SetActive(bCockpitView);
	}
}

void AKeMuSanPawn::ToggleMirrorAdjustMode()
{
	bMirrorAdjustMode = !bMirrorAdjustMode;
}

void AKeMuSanPawn::CycleActiveMirror()
{
	if (ActiveMirror == EMirrorType::Left)
	{
		ActiveMirror = EMirrorType::Interior;
	}
	else if (ActiveMirror == EMirrorType::Interior)
	{
		ActiveMirror = EMirrorType::Right;
	}
	else
	{
		ActiveMirror = EMirrorType::Left;
	}
}

void AKeMuSanPawn::AdjustActiveMirror(float DeltaPitch, float DeltaYaw)
{
	FMirrorOpticalState* TargetState = nullptr;
	if (ActiveMirror == EMirrorType::Left)
	{
		TargetState = &LeftMirrorState;
	}
	else if (ActiveMirror == EMirrorType::Right)
	{
		TargetState = &RightMirrorState;
	}
	else
	{
		TargetState = &InteriorMirrorState;
	}

	if (!TargetState) return;

	TargetState->Pitch = FMath::Clamp(TargetState->Pitch + DeltaPitch, -15.f, 15.f);
	TargetState->Yaw = FMath::Clamp(TargetState->Yaw + DeltaYaw, -20.f, 20.f);

	if (ActiveMirror == EMirrorType::Left)
	{
		// 正 Pitch 为抬头(天空增加)，负 Pitch 为低头(地面增加)
		TargetState->HorizonRatio = FMath::Clamp(0.50f + TargetState->Pitch * 0.025f, 0.1f, 0.9f);
		TargetState->VisibleBodyRatio = FMath::Clamp(0.25f + TargetState->Yaw * 0.02f, 0.05f, 0.6f);
		TargetState->bStandardAdjusted = (FMath::Abs(TargetState->Pitch) <= 2.5f && FMath::Abs(TargetState->Yaw) <= 3.0f);
		if (TargetState->bStandardAdjusted)
		{
			TargetState->Tip = TEXT("标准到位：地平线居中(1/2)，车身占右侧1/4");
		}
		else if (TargetState->Pitch > 2.5f)
		{
			TargetState->Tip = TEXT("上仰过多(天空过多)，请按[↓]压低");
		}
		else if (TargetState->Pitch < -2.5f)
		{
			TargetState->Tip = TEXT("下俯过多(地面过多)，请按[↑]抬高");
		}
		else if (TargetState->Yaw > 3.0f)
		{
			TargetState->Tip = TEXT("内折过多(车身过多)，请按[←]向外展");
		}
		else
		{
			TargetState->Tip = TEXT("外展过多(看不到车身)，请按[→]向内收");
		}
	}
	else if (ActiveMirror == EMirrorType::Right)
	{
		TargetState->HorizonRatio = FMath::Clamp(0.33f + TargetState->Pitch * 0.025f, 0.1f, 0.9f);
		TargetState->VisibleBodyRatio = FMath::Clamp(0.25f - TargetState->Yaw * 0.02f, 0.05f, 0.6f);
		TargetState->bStandardAdjusted = (FMath::Abs(TargetState->Pitch) <= 2.5f && FMath::Abs(TargetState->Yaw) <= 3.0f);
		if (TargetState->bStandardAdjusted)
		{
			TargetState->Tip = TEXT("标准到位：天空占1/3地面占2/3，车身占左侧1/4");
		}
		else if (TargetState->Pitch > 2.5f)
		{
			TargetState->Tip = TEXT("上仰过多(天空过多)，请按[↓]压低");
		}
		else if (TargetState->Pitch < -2.5f)
		{
			TargetState->Tip = TEXT("下俯过多(地面过多)，请按[↑]抬高");
		}
		else if (TargetState->Yaw > 3.0f)
		{
			TargetState->Tip = TEXT("外展过多(车身太少)，请按[←]向内收");
		}
		else
		{
			TargetState->Tip = TEXT("内折过多(车身太多)，请按[→]向外展");
		}
	}
	else
	{
		TargetState->HorizonRatio = FMath::Clamp(0.50f + TargetState->Pitch * 0.03f, 0.1f, 0.9f);
		TargetState->VisibleBodyRatio = 0.50f;
		TargetState->bStandardAdjusted = (FMath::Abs(TargetState->Pitch) <= 2.0f && FMath::Abs(TargetState->Yaw) <= 2.0f);
		if (TargetState->bStandardAdjusted)
		{
			TargetState->Tip = TEXT("标准到位：后挡风玻璃完整居中");
		}
		else if (TargetState->Pitch > 2.0f)
		{
			TargetState->Tip = TEXT("上仰过多，请按[↓]压低使后窗居中");
		}
		else if (TargetState->Pitch < -2.0f)
		{
			TargetState->Tip = TEXT("下俯过多，请按[↑]抬高使后窗居中");
		}
		else
		{
			TargetState->Tip = TEXT("请调整偏航使后窗完整居中");
		}
	}

	UpdateMirrorOptics();
	CaptureAllMirrorsImmediate();
}

void AKeMuSanPawn::ResetActiveMirrorToStandard()
{
	FMirrorOpticalState* TargetState = (ActiveMirror == EMirrorType::Left) ? &LeftMirrorState : ((ActiveMirror == EMirrorType::Right) ? &RightMirrorState : &InteriorMirrorState);
	if (!TargetState) return;

	TargetState->Pitch = 0.f;
	TargetState->Yaw = 0.f;
	TargetState->bStandardAdjusted = true;
	if (ActiveMirror == EMirrorType::Left)
	{
		TargetState->HorizonRatio = 0.50f;
		TargetState->VisibleBodyRatio = 0.25f;
		TargetState->Tip = TEXT("标准到位：地平线居中(1/2)，车身占右侧1/4");
	}
	else if (ActiveMirror == EMirrorType::Right)
	{
		TargetState->HorizonRatio = 0.33f;
		TargetState->VisibleBodyRatio = 0.25f;
		TargetState->Tip = TEXT("标准到位：天空占1/3地面占2/3，车身占左侧1/4");
	}
	else
	{
		TargetState->HorizonRatio = 0.50f;
		TargetState->VisibleBodyRatio = 0.50f;
		TargetState->Tip = TEXT("标准到位：后挡风玻璃完整居中");
	}

	UpdateMirrorOptics();
	CaptureAllMirrorsImmediate();
}

void AKeMuSanPawn::ResetAllMirrorsToStandard()
{
	LeftMirrorState.Pitch = 0.f;
	LeftMirrorState.Yaw = 0.f;
	LeftMirrorState.HorizonRatio = 0.50f;
	LeftMirrorState.VisibleBodyRatio = 0.25f;
	LeftMirrorState.bStandardAdjusted = true;
	LeftMirrorState.Tip = TEXT("标准到位：地平线居中(1/2)，车身占右侧1/4");

	RightMirrorState.Pitch = 0.f;
	RightMirrorState.Yaw = 0.f;
	RightMirrorState.HorizonRatio = 0.33f;
	RightMirrorState.VisibleBodyRatio = 0.25f;
	RightMirrorState.bStandardAdjusted = true;
	RightMirrorState.Tip = TEXT("标准到位：天空占1/3地面占2/3，车身占左侧1/4");

	InteriorMirrorState.Pitch = 0.f;
	InteriorMirrorState.Yaw = 0.f;
	InteriorMirrorState.HorizonRatio = 0.50f;
	InteriorMirrorState.VisibleBodyRatio = 0.50f;
	InteriorMirrorState.bStandardAdjusted = true;
	InteriorMirrorState.Tip = TEXT("标准到位：后挡风玻璃完整居中");

	UpdateMirrorOptics();
	CaptureAllMirrorsImmediate();
}

const FMirrorOpticalState& AKeMuSanPawn::GetMirrorState(EMirrorType Type) const
{
	if (Type == EMirrorType::Left) return LeftMirrorState;
	if (Type == EMirrorType::Right) return RightMirrorState;
	return InteriorMirrorState;
}

UTextureRenderTarget2D* AKeMuSanPawn::GetMirrorRenderTarget(EMirrorType Type) const
{
	if (Type == EMirrorType::Left) return LeftMirrorTarget;
	if (Type == EMirrorType::Right) return RightMirrorTarget;
	return InteriorMirrorTarget;
}

void AKeMuSanPawn::UpdateMirrorOptics()
{
	if (LeftMirrorCapture)
	{
		LeftMirrorCapture->SetRelativeRotation(FRotator(-2.f + LeftMirrorState.Pitch, 196.f + LeftMirrorState.Yaw, 0.f));
	}
	if (RightMirrorCapture)
	{
		RightMirrorCapture->SetRelativeRotation(FRotator(-3.f + RightMirrorState.Pitch, 164.f + RightMirrorState.Yaw, 0.f));
	}
	if (InteriorMirrorCapture)
	{
		InteriorMirrorCapture->SetRelativeRotation(FRotator(-1.f + InteriorMirrorState.Pitch, 180.f + InteriorMirrorState.Yaw, 0.f));
	}
}

void AKeMuSanPawn::ApplyDebugCamera()
{
	SpringArm->TargetArmLength = 5000.f;
	SpringArm->SetRelativeLocation(FVector(-1000.f, 0.f, 4000.f));
	SpringArm->SetRelativeRotation(FRotator(-65.f, 0.f, 0.f));
	Camera->FieldOfView = 75.f;
}

void AKeMuSanPawn::UpdatePhysics(float DT)
{
	UpdatePhysicsManual(DT);
}

void AKeMuSanPawn::UpdatePhysicsManual(float DT)
{
 const float BeforeSpeed=SpeedMs;
	StallCooldown = FMath::Max(0.f, StallCooldown - DT);
	if (bFlashHigh)
	{
		FlashHighTimer -= DT;
		if (FlashHighTimer <= 0.f)
		{
			bFlashHigh = false;
		}
	}

	// ---- 物理踏板平滑：将键盘瞬态 (0->1) 模拟为人脚物理踩踏与离合联动响应 ----
	const float TargetThrottle = ThrottleInput;
	if (TargetThrottle > SmoothedThrottle)
	{
		// 踩油门平滑上升率（约 0.4 秒踩满），消除键盘 W 单帧跳变造成的非物理冲击
		SmoothedThrottle = FMath::Min(TargetThrottle, SmoothedThrottle + 2.5f * DT);
	}
	else
	{
		// 松油门回弹
		SmoothedThrottle = FMath::Max(TargetThrottle, SmoothedThrottle - 4.5f * DT);
	}

	const int32 GearIdx = static_cast<int32>(Gear);
	// 驱动力仅在车辆处于可驾驶阶段（Ready/Driving/PullOver）且挂挡未熄火时生效；Menu/Prep/LightTest 阶段严禁滑走
	const bool bHasPower = bDrivable && (Gear != EGear::N) && !bStalled;

	// ---- 驱动力：向目标车速逼近（模拟油门开度决定车速） ----
	if (bHasPower)
	{
		const float Sign = (Gear == EGear::R) ? -1.f : 1.f;
		const float Desired = Sign * GearMaxSpeed[GearIdx] * (0.12f + 0.88f * SmoothedThrottle);
		const float MaxStep = GearAccel[GearIdx] * DT;
		const float Diff = Desired - SpeedMs;
		SpeedMs += FMath::Clamp(Diff, -MaxStep, MaxStep);
	}
	else if (!bDrivable)
	{
		SpeedMs = 0.f;
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

	// ---- 熄火判定：1挡正常键盘连续W起步平稳不熄火；静止2挡及以上大油门起步、或高挡极低速拖挡合理熄火 ----
	if (!bStalled && StallCooldown <= 0.f && Gear != EGear::N && FMath::Abs(SpeedMs) < 0.6f && bDrivable)
	{
		bool bWillStall = (GearIdx >= static_cast<int32>(EGear::G2) && SmoothedThrottle > 0.40f);
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

	const float YawDeltaDeg=FMath::FindDeltaAngleDegrees(YawDeg,GetActorRotation().Yaw);
 ApplyPhysicalDrive(DT,BeforeSpeed,SpeedMs/2.6f*FMath::Tan(FMath::DegreesToRadians(SteeringAngleDeg)));

	// ---- 转向灯回正自动取消 ----
	if (bLeftSignal || bRightSignal)
	{
		SignalYawAccum += FMath::Abs(YawDeltaDeg);
		if (SignalYawAccum > 55.f && FMath::Abs(SteeringAngleDeg) < 10.f)
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
	else if (Gear == EGear::N || Gear == EGear::R)
	{
		EngineRpm = IdleRpm + SmoothedThrottle * 1500.f;
	}
	else
	{
		const float Ratio = FMath::Clamp(FMath::Abs(SpeedMs) / FMath::Max(0.1f, FMath::Abs(GearMaxSpeed[GearIdx])), 0.f, 1.f);
		EngineRpm = IdleRpm + Ratio * (MaxRpm - IdleRpm) * (0.35f + 0.65f * SmoothedThrottle);
	}
}

// Automatic transmission physics: throttle auto-shifts, no stalling, brake/coast auto-downshifts
void AKeMuSanPawn::UpdatePhysicsAuto(float DT)
{
 const float BeforeSpeed=SpeedMs;
	// No stalling in auto mode
	bStalled = false;
	StallCooldown = FMath::Max(0.f, StallCooldown - DT);
	if (bFlashHigh)
	{
		FlashHighTimer -= DT;
		if (FlashHighTimer <= 0.f) { bFlashHigh = false; }
	}

	const float TargetThrottle = ThrottleInput;
	if (TargetThrottle > SmoothedThrottle)
	{
		SmoothedThrottle = FMath::Min(TargetThrottle, SmoothedThrottle + 2.5f * DT);
	}
	else
	{
		SmoothedThrottle = FMath::Max(TargetThrottle, SmoothedThrottle - 4.5f * DT);
	}

	// Auto shift based on speed
	const float SpdKmh = FMath::Abs(SpeedMs) * 3.6f;
	if (Gear == EGear::G1 || Gear == EGear::G2 || Gear == EGear::G3 || Gear == EGear::G4 || Gear == EGear::G5)
	{
		if (SmoothedThrottle > 0.3f && SpdKmh > 10.f) Gear = EGear::G2;
		if (SmoothedThrottle > 0.3f && SpdKmh > 18.f) Gear = EGear::G3;
		if (SmoothedThrottle > 0.3f && SpdKmh > 30.f) Gear = EGear::G4;
		if (SmoothedThrottle > 0.3f && SpdKmh > 42.f) Gear = EGear::G5;
		if (SpdKmh < 10.f && Gear != EGear::G1 && Gear != EGear::G2)
		{
			Gear = EGear::G1;
		}
		else if (SpdKmh < 20.f && Gear != EGear::G1 && Gear != EGear::G2)
		{
			Gear = EGear::G2;
		}
	}

	const int32 GearIdx = static_cast<int32>(Gear);
	const bool bHasPower = bDrivable && (Gear != EGear::N) && !bStalled;

	// ---- 驱动力 ----
	if (bHasPower)
	{
		const float Sign = (Gear == EGear::R) ? -1.f : 1.f;
		const float Desired = Sign * GearMaxSpeed[FMath::Clamp(GearIdx, 0, 6)] * (0.15f + 0.85f * SmoothedThrottle);
		const int32 AccelIdx = FMath::Clamp(GearIdx, 0, 6);
		const float MaxStep = GearAccel[AccelIdx] * DT;
		const float Diff = Desired - SpeedMs;
		SpeedMs += FMath::Clamp(Diff, -MaxStep, MaxStep);
	}
	else if (!bDrivable)
	{
		SpeedMs = 0.f;
	}

	// ---- 刹车 ----
	if (BrakeInput > 0.f && FMath::Abs(SpeedMs) > 0.01f)
	{
		const float Step = BrakeInput * 8.f * DT;
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
		const float Resist = (0.10f + 0.0028f * SpeedMs * SpeedMs) * DT;
		SpeedMs = (FMath::Abs(SpeedMs) <= Resist) ? 0.f : SpeedMs - FMath::Sign(SpeedMs) * Resist;
	}

	// ---- 转向 ----
	const float SpeedKmh = FMath::Abs(SpeedMs) * 3.6f;
	const float MaxSteer = 32.f;
	const float SpeedFactor = FMath::Clamp(1.f - SpeedKmh / 40.f, 0.18f, 1.f);
	const float TargetSteer = SteeringInput * MaxSteer * SpeedFactor;
	const float SteerStep = 130.f * DT;
	SteeringAngleDeg = FMath::Clamp(TargetSteer, SteeringAngleDeg - SteerStep, SteeringAngleDeg + SteerStep);

	const float YawDeltaDeg=FMath::FindDeltaAngleDegrees(YawDeg,GetActorRotation().Yaw);
 ApplyPhysicalDrive(DT,BeforeSpeed,SpeedMs/2.6f*FMath::Tan(FMath::DegreesToRadians(SteeringAngleDeg)));

	// ---- 转向灯 ----
	if (bLeftSignal || bRightSignal)
	{
		SignalYawAccum += FMath::Abs(YawDeltaDeg);
		if (SignalYawAccum > 55.f && FMath::Abs(SteeringAngleDeg) < 10.f)
		{
			bLeftSignal = false;
			bRightSignal = false;
			SignalYawAccum = 0.f;
		}
	}
	else { SignalYawAccum = 0.f; }

	// ---- 转速 ----
	const float IdleRpm = 800.f;
	const float MaxRpm = 5500.f;
	if (Gear == EGear::N)
	{
		EngineRpm = IdleRpm + SmoothedThrottle * 1200.f;
	}
	else
	{
		const float Ratio = FMath::Clamp(FMath::Abs(SpeedMs) / FMath::Max(0.1f, FMath::Abs(GearMaxSpeed[FMath::Clamp(GearIdx, 0, 6)])), 0.f, 1.f);
		EngineRpm = IdleRpm + Ratio * (MaxRpm - IdleRpm) * (0.4f + 0.6f * SmoothedThrottle);
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

	// 前轮与外侧独立金属轮毂随转向角动态旋转（圆柱轮轴沿Y横卧 Roll=90°，Yaw叠加转向角）
	if (WheelFL) WheelFL->SetRelativeRotation(FRotator(0.f, SteeringAngleDeg, 90.f));
	if (WheelFR) WheelFR->SetRelativeRotation(FRotator(0.f, SteeringAngleDeg, 90.f));
	if (RimFL) RimFL->SetRelativeRotation(FRotator(0.f, SteeringAngleDeg, 90.f));
	if (RimFR) RimFR->SetRelativeRotation(FRotator(0.f, SteeringAngleDeg, 90.f));
}

void AKeMuSanPawn::ApplyPhysicalDrive(float Dt,float Before,float YawRate)
{
 const float Acceleration=(SpeedMs-Before)/FMath::Max(.001f,Dt);
 const bool Recovering=GetWorld()->GetTimeSeconds()-LastVehicleHitTime<.45f;
 VehiclePhysics::Drive(Root,Recovering?0.f:Acceleration,Recovering?Root->GetPhysicsAngularVelocityInRadians().Z:YawRate,Dt,!Recovering);
 SpeedMs=Before;
 YawDeg=GetActorRotation().Yaw;
}
bool AKeMuSanPawn::IsOverturned() const { return Dynamics && Dynamics->IsOverturned(); }
void AKeMuSanPawn::OnVehicleHit(UPrimitiveComponent*,AActor* Other,UPrimitiveComponent* OtherComponent,FVector Impulse,const FHitResult& Hit)
{
 if(!Other || Other==this || Impulse.Size()<100.f)return;
 Damage->Impact(Hit,Impulse,Root->GetMass());
 if(!OtherComponent||OtherComponent->GetCollisionObjectType()!=ECC_Vehicle)return;
 LastVehicleHitTime=GetWorld()->GetTimeSeconds();
 UE_LOG(LogTemp,Log,TEXT("[KeMuSanPhysics] vehicle_hit other=%s impulse=%.1f speed=%.2f yaw_rate=%.3f"),*Other->GetName(),Impulse.Size(),Root->GetPhysicsLinearVelocity().Size()*.01f,Root->GetPhysicsAngularVelocityInRadians().Z);
}
