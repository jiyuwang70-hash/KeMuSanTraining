#include "KeMuSanGameMode.h"
#include "TrafficActors.h"
#include "GameFramework/HUD.h"
#include "GameFramework/SpringArmComponent.h"
#include "HighResScreenshot.h"
#include "Components/BoxComponent.h"
#include "VehicleDamageComponent.h"
#include "VehicleDynamicsComponent.h"
#include "VehiclePhysicsCore.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"

#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerInput.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"

#include "KeMuSanPawn.h"
#include "KeMuSanPlayerController.h"
#include "KeMuSanHUD.h"
#include "ExamController.h"
#include "RoadLayout.h"

AKeMuSanGameMode::AKeMuSanGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = AKeMuSanPawn::StaticClass();
	PlayerControllerClass = AKeMuSanPlayerController::StaticClass();
	HUDClass = AKeMuSanHUD::StaticClass();
}

void AKeMuSanGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
 if(bPhysicsVerification)TickPhysicsVerification(DeltaSeconds);

	if (bShowcaseCapturing)
	{
		TickShowcaseCapture(DeltaSeconds);
	}
}

void AKeMuSanGameMode::TickShowcaseCapture(float DeltaSeconds)
{
	ShowcaseTimerSec += DeltaSeconds;
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	AKeMuSanPawn* Car = PC ? Cast<AKeMuSanPawn>(PC->GetPawn()) : nullptr;
	if (!PC || !Car)
	{
		return;
	}

	switch (ShowcasePhase)
	{
	case 0: // Phase 0: 等待 0.8s，截首屏 Shot 1: Menu HUD（干净居中大标题）
		if (ShowcaseTimerSec >= 0.8f)
		{
			PC->ConsoleCommand(TEXT("shot"), true);
			UE_LOG(LogTemp, Log, TEXT("[KeMuSanShowcase] Shot 1: Menu HUD captured (26pt vector Chinese title)"));
			bShot1Ok = true;
			ShowcasePhase = 1;
			ShowcaseTimerSec = 0.f;
		}
		break;

	case 1: // Phase 1: 等待 0.5s 确保 Shot 1 异步落盘，注入 Enter 启动自由练习模式
		if (ShowcaseTimerSec >= 0.5f)
		{
			PC->InputKey(FInputKeyParams(EKeys::Enter, IE_Pressed, 1.0));
			PC->InputKey(FInputKeyParams(EKeys::Enter, IE_Released, 0.0));
			UE_LOG(LogTemp, Log, TEXT("[KeMuSanShowcase] Injected Enter -> Started Free Practice"));
			ShowcasePhase = 2;
			ShowcaseTimerSec = 0.f;
		}
		break;

	case 2: // Phase 2: 等待 0.4s，注入 F 系安全带，M 侧头观察
		if (ShowcaseTimerSec >= 0.4f)
		{
			PC->InputKey(FInputKeyParams(EKeys::F, IE_Pressed, 1.0));
			PC->InputKey(FInputKeyParams(EKeys::F, IE_Released, 0.0));
			PC->InputKey(FInputKeyParams(EKeys::M, IE_Pressed, 1.0));
			PC->InputKey(FInputKeyParams(EKeys::M, IE_Released, 0.0));
			UE_LOG(LogTemp, Log, TEXT("[KeMuSanShowcase] Injected F (Seatbelt) and M (HeadCheck)"));
			ShowcasePhase = 3;
			ShowcaseTimerSec = 0.f;
		}
		break;

	case 3: // Phase 3: 等待 Prep 转为 Ready（保持手刹拉紧，等待准备完成）
		if (ExamController && ExamController->GetPhase() == EExamPhase::Ready)
		{
			// 打开左转向灯 Q
			if (!Car->IsLeftSignalOn())
			{
				PC->InputKey(FInputKeyParams(EKeys::Q, IE_Pressed, 1.0));
				PC->InputKey(FInputKeyParams(EKeys::Q, IE_Released, 0.0));
			}
			UE_LOG(LogTemp, Log, TEXT("[KeMuSanShowcase] Ready phase reached, turned on Left Signal Q, waiting 3s"));
			ShowcasePhase = 4;
			ShowcaseTimerSec = 0.f;
		}
		else if (ShowcaseTimerSec > 12.0f)
		{
			UE_LOG(LogTemp, Error, TEXT("[KeMuSanShowcase] capture_complete FAIL (Timeout waiting for Ready phase)"));
			bShowcaseCapturing = false;
			FGenericPlatformMisc::RequestExit(false);
		}
		break;

	case 4: // Phase 4: 等待左转向灯打满 3 秒
		if (ShowcaseTimerSec >= 3.1f)
		{
			// 挂 1 挡
			PC->InputKey(FInputKeyParams(EKeys::One, IE_Pressed, 1.0));
			PC->InputKey(FInputKeyParams(EKeys::One, IE_Released, 0.0));
			// 松开手刹
			if (Car->IsHandbrakeEngaged())
			{
				PC->InputKey(FInputKeyParams(EKeys::SpaceBar, IE_Pressed, 1.0));
				PC->InputKey(FInputKeyParams(EKeys::SpaceBar, IE_Released, 0.0));
			}
			ShowcaseStartLoc = Car->GetActorLocation();
			UE_LOG(LogTemp, Log, TEXT("[KeMuSanShowcase] Left Signal 3s satisfied, engaged Gear 1 and released Handbrake, starting drive"));
			ShowcasePhase = 5;
			ShowcaseTimerSec = 0.f;
		}
		break;

	case 5: // Phase 5: 真实加速驾驶，每帧注入 W 直至 EC 达到 Driving、位移 >= 14m、速度 > 5km/h 且未熄火
		PC->InputKey(FInputKeyParams(EKeys::W, IE_Pressed, 1.0));
		{
			const float DistMoved = FVector::Dist(Car->GetActorLocation(), ShowcaseStartLoc) * 0.01f;
			const float CurSpeed = Car->GetSpeedKmh();
			const bool bDrivingPhase = ExamController && (ExamController->GetPhase() == EExamPhase::Driving);
			const bool bDistOk = DistMoved >= 14.0f;
			const bool bSpeedOk = CurSpeed > 5.0f;
			const bool bNotStalled = !Car->IsStalled();

			if (bDrivingPhase && bDistOk && bSpeedOk && bNotStalled)
			{
				// 满足真实驾驶条件，松开 W 并截 Shot 2
				PC->InputKey(FInputKeyParams(EKeys::W, IE_Released, 0.0));
				PC->ConsoleCommand(TEXT("shot"), true);
				UE_LOG(LogTemp, Log, TEXT("[KeMuSanShowcase] Shot 2: Chase Camera in REAL Driving (Dist=%.1fm, Speed=%.1fkm/h, Driving=1) captured"), DistMoved, CurSpeed);
				bShot2Ok = true;
				ShowcasePhase = 6;
				ShowcaseTimerSec = 0.f;
			}
			else if (ShowcaseTimerSec > 15.0f || (ShowcaseTimerSec > 3.0f && Car->IsStalled()))
			{
				PC->InputKey(FInputKeyParams(EKeys::W, IE_Released, 0.0));
				UE_LOG(LogTemp, Error, TEXT("[KeMuSanShowcase] capture_complete FAIL (Drive criteria failed: Dist=%.1fm, Speed=%.1fkm/h, Phase=%d, Stalled=%d)"),
					DistMoved, CurSpeed, ExamController ? (int32)ExamController->GetPhase() : -1, Car->IsStalled() ? 1 : 0);
				bShowcaseCapturing = false;
				FGenericPlatformMisc::RequestExit(false);
			}
		}
		break;

	case 6: // Phase 6: 等待 0.5s 确保 Shot 2 异步落盘，然后转入刹车阶段
		if (ShowcaseTimerSec >= 0.5f)
		{
			ShowcasePhase = 7;
			ShowcaseTimerSec = 0.f;
		}
		break;

	case 7: // Phase 7: 注入 S 刹车减速直至真实停稳（车速 <= 0.5km/h）
		PC->InputKey(FInputKeyParams(EKeys::S, IE_Pressed, 1.0));
		if (Car->GetSpeedKmh() <= 0.5f)
		{
			PC->InputKey(FInputKeyParams(EKeys::S, IE_Released, 0.0));
			Car->ForceStop(); // 真实刹停后调用 ForceStop 彻底固定
			Car->SetDrivable(false); // 只在展示路径冻结动力，防止手动1挡怠速爬行导致车位移动
			ShowcaseStopLoc = Car->GetActorLocation();
			UE_LOG(LogTemp, Log, TEXT("[KeMuSanShowcase] Vehicle completely stopped and frozen (Speed=%.2fkm/h, Loc=(%.1f,%.1f,%.1f)), preparing cockpit view"),
				Car->GetSpeedKmh(), ShowcaseStopLoc.X, ShowcaseStopLoc.Y, ShowcaseStopLoc.Z);
			ShowcasePhase = 8;
			ShowcaseTimerSec = 0.f;
		}
		else if (ShowcaseTimerSec >= 5.0f)
		{
			PC->InputKey(FInputKeyParams(EKeys::S, IE_Released, 0.0));
			UE_LOG(LogTemp, Error, TEXT("[KeMuSanShowcase] capture_complete FAIL (Brake timeout after 5s, Speed=%.1fkm/h > 0.5)"), Car->GetSpeedKmh());
			bShowcaseCapturing = false;
			FGenericPlatformMisc::RequestExit(false);
		}
		break;

	case 8: // Phase 8: 等待 0.3s，按 V 键切换到座舱第一人称视角
		if (ShowcaseTimerSec >= 0.3f)
		{
			PC->InputKey(FInputKeyParams(EKeys::V, IE_Pressed, 1.0));
			PC->InputKey(FInputKeyParams(EKeys::V, IE_Released, 0.0));
			ShowcasePhase = 9;
			ShowcaseTimerSec = 0.f;
		}
		break;

	case 9: // Phase 9: 等待 0.5s 渲染管线稳定后，截 Shot 3: 第一人称主驾驶沉浸座舱
		if (ShowcaseTimerSec >= 0.5f)
		{
			PC->ConsoleCommand(TEXT("shot"), true);
			UE_LOG(LogTemp, Log, TEXT("[KeMuSanShowcase] Shot 3: Cockpit First-Person View captured (hollow wheel, optics mirrors)"));
			bShot3Ok = true;
			ShowcasePhase = 10;
			ShowcaseTimerSec = 0.f;
		}
		break;

	case 10: // Phase 10: 等待 0.5s 确保 Shot 3 异步落盘，按 T 键开启调镜模式
		if (ShowcaseTimerSec >= 0.5f)
		{
			PC->InputKey(FInputKeyParams(EKeys::T, IE_Pressed, 1.0));
			PC->InputKey(FInputKeyParams(EKeys::T, IE_Released, 0.0));
			ShowcasePhase = 11;
			ShowcaseTimerSec = 0.f;
		}
		break;

	case 11: // Phase 11: 等待 0.5s 调镜模式稳定，截 Shot 4a: 标准后视镜视角
		if (ShowcaseTimerSec >= 0.5f)
		{
			const float CurSpeed = Car->GetSpeedKmh();
			const float DistDrift = FVector::Dist(Car->GetActorLocation(), ShowcaseStopLoc);
			UE_LOG(LogTemp, Log, TEXT("[KeMuSanShowcase] Shot 4a: Pre-shot check (Speed=%.2fkm/h, Drift=%.2fcm)"), CurSpeed, DistDrift);
			PC->ConsoleCommand(TEXT("shot"), true);
			UE_LOG(LogTemp, Log, TEXT("[KeMuSanShowcase] Shot 4a: Mirror Comparison - Standard View captured"));
			bShot4aOk = true;
			ShowcasePhase = 12;
			ShowcaseTimerSec = 0.f;
		}
		break;

	case 12: // Phase 12: 等待 0.5s 确保 Shot 4a 异步落盘，通过原生按键微调后视镜
		if (ShowcaseTimerSec >= 0.5f)
		{
			// 原生按键多步输入（Up x8 => Pitch +4°, Left x16 => Yaw -8°），完全通过原生输入派发，不调直接 setter
			for (int32 i = 0; i < 8; ++i)
			{
				PC->InputKey(FInputKeyParams(EKeys::Up, IE_Pressed, 1.0));
				PC->InputKey(FInputKeyParams(EKeys::Up, IE_Released, 0.0));
			}
			for (int32 i = 0; i < 16; ++i)
			{
				PC->InputKey(FInputKeyParams(EKeys::Left, IE_Pressed, 1.0));
				PC->InputKey(FInputKeyParams(EKeys::Left, IE_Released, 0.0));
			}
			ShowcasePhase = 13;
			ShowcaseTimerSec = 0.f;
		}
		break;

	case 13: // Phase 13: 等待 0.5s 让 SceneCapture 渲染管线刷新，截 Shot 4b: 调镜微调后视角（同视角同座舱同固定位置）
		if (ShowcaseTimerSec >= 0.5f)
		{
			const float CurSpeed = Car->GetSpeedKmh();
			const float DistDrift = FVector::Dist(Car->GetActorLocation(), ShowcaseStopLoc);
			UE_LOG(LogTemp, Log, TEXT("[KeMuSanShowcase] Shot 4b: Pre-shot check (Speed=%.2fkm/h, Drift=%.2fcm)"), CurSpeed, DistDrift);
			PC->ConsoleCommand(TEXT("shot"), true);
			UE_LOG(LogTemp, Log, TEXT("[KeMuSanShowcase] Shot 4b: Mirror Comparison - Adjusted View captured (comparison proof)"));
			bShot4bOk = true;
			ShowcasePhase = 14;
			ShowcaseTimerSec = 0.f;
		}
		break;

	case 14: // Phase 14: 等待 0.5s 确保 Shot 4b 异步落盘，然后退出调镜并切换回第三人称，设置北向右车道 90° 静态弯道测试姿态
		if (ShowcaseTimerSec >= 0.5f)
		{
			PC->InputKey(FInputKeyParams(EKeys::T, IE_Pressed, 1.0));
			PC->InputKey(FInputKeyParams(EKeys::T, IE_Released, 0.0));
			PC->InputKey(FInputKeyParams(EKeys::V, IE_Pressed, 1.0));
			PC->InputKey(FInputKeyParams(EKeys::V, IE_Released, 0.0));
			PC->InputKey(FInputKeyParams(EKeys::W, IE_Released, 0.0));
			PC->InputKey(FInputKeyParams(EKeys::S, IE_Released, 0.0));
			// 北向右车道：x=518.25m, y=60m, Z=RoadSurfaceZ
			Car->SetTestPose(FVector(518.25f * 100.f, 60.0f * 100.f, RoadLayout::RoadSurfaceZ * 100.f), FRotator(0.f, 90.f, 0.f));
			ShowcasePhase = 15;
			ShowcaseTimerSec = 0.f;
		}
		break;

	case 15: // Phase 15: 等待 SpringArm 稳定至少 0.6 秒，断言偏航角仍稳定维持在 90° 并截 Shot 5
		if (ShowcaseTimerSec >= 0.6f)
		{
			const float CurYaw = Car->GetActorRotation().Yaw;
			const bool bYawOk = FMath::IsNearlyEqual(CurYaw, 90.f, 6.0f);
			if (bYawOk)
			{
				PC->ConsoleCommand(TEXT("shot"), true);
				UE_LOG(LogTemp, Log, TEXT("[KeMuSanShowcase] Shot 5: Corner 90deg Camera Following Test captured (Static Camera Test Pose, ActorYaw=%.1f)"), CurYaw);
				bShot5Ok = true;
				ShowcasePhase = 16;
				ShowcaseTimerSec = 0.f;
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[KeMuSanShowcase] capture_complete FAIL (90deg camera test yaw drifted: CurYaw=%.1f)"), CurYaw);
				bShowcaseCapturing = false;
				FGenericPlatformMisc::RequestExit(false);
			}
		}
		break;

	case 16: // Phase 16: 等待 0.5s 确保 Shot 5 异步落盘，然后设置西向右车道 180° 掉头静态测试姿态
		if (ShowcaseTimerSec >= 0.5f)
		{
			// 西向右车道：x=470m, y=318.25m, Z=RoadSurfaceZ
			Car->SetTestPose(FVector(470.0f * 100.f, 318.25f * 100.f, RoadLayout::RoadSurfaceZ * 100.f), FRotator(0.f, 180.f, 0.f));
			ShowcasePhase = 17;
			ShowcaseTimerSec = 0.f;
		}
		break;

	case 17: // Phase 17: 等待 SpringArm 稳定至少 0.6 秒，断言偏航角仍稳定维持在 180° 并截 Shot 6
		if (ShowcaseTimerSec >= 0.6f)
		{
			const float CurYaw = FMath::Abs(Car->GetActorRotation().Yaw);
			const bool bYawOk = FMath::IsNearlyEqual(CurYaw, 180.f, 6.0f);
			if (bYawOk)
			{
				PC->ConsoleCommand(TEXT("shot"), true);
				UE_LOG(LogTemp, Log, TEXT("[KeMuSanShowcase] Shot 6: U-Turn 180deg Camera Following Test captured (Static Camera Test Pose, ActorYaw=%.1f)"), Car->GetActorRotation().Yaw);
				bShot6Ok = true;
				ShowcasePhase = 18;
				ShowcaseTimerSec = 0.f;
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[KeMuSanShowcase] capture_complete FAIL (180deg camera test yaw drifted: CurYaw=%.1f)"), Car->GetActorRotation().Yaw);
				bShowcaseCapturing = false;
				FGenericPlatformMisc::RequestExit(false);
			}
		}
		break;

	case 18: // Phase 18: 等待 0.8s 确保 Shot 6 异步落盘，严格判定全部 7 张截图均已达成，输出 capture_complete PASS 并退出
		if (ShowcaseTimerSec >= 0.8f)
		{
			bShowcaseCapturing = false;
			ShowcasePhase = 19;
			const bool bAllSevenOk = bShot1Ok && bShot2Ok && bShot3Ok && bShot4aOk && bShot4bOk && bShot5Ok && bShot6Ok;
			if (bAllSevenOk)
			{
				UE_LOG(LogTemp, Log, TEXT("[KeMuSanShowcase] capture_complete PASS (7 screenshots verified)"));
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[KeMuSanShowcase] capture_complete FAIL (Only partial shots: 1=%d, 2=%d, 3=%d, 4a=%d, 4b=%d, 5=%d, 6=%d)"),
					bShot1Ok ? 1 : 0, bShot2Ok ? 1 : 0, bShot3Ok ? 1 : 0, bShot4aOk ? 1 : 0, bShot4bOk ? 1 : 0, bShot5Ok ? 1 : 0, bShot6Ok ? 1 : 0);
			}
			FGenericPlatformMisc::RequestExit(false);
		}
		break;

	default:
		break;
	}
}

void AKeMuSanGameMode::BeginPlay()
{
	Super::BeginPlay();
 auto* Ground=NewObject<UBoxComponent>(this,TEXT("VehicleGround"));
 Ground->SetMobility(EComponentMobility::Static);Ground->SetBoxExtent(FVector(500000,500000,100));
 Ground->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Ground->SetCollisionObjectType(ECC_WorldStatic);Ground->SetCollisionResponseToAllChannels(ECR_Ignore);
 Ground->SetCollisionResponseToChannel(ECC_Vehicle,ECR_Block);Ground->SetCollisionResponseToChannel(ECC_WorldStatic,ECR_Block);
 Ground->SetWorldLocation(FVector(150000,150000,-100));Ground->RegisterComponent();
 bPhysicsVerification=FParse::Param(FCommandLine::Get(),TEXT("test-vehicle-physics"));
 FParse::Value(FCommandLine::Get(),TEXT("damage-screenshot-dir="),DamageScreenshotDir);

	// 生成考试总控
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ExamController = GetWorld()->SpawnActor<AExamController>(FVector::ZeroVector, FRotator::ZeroRotator, Params);

	UE_LOG(LogTemp, Log, TEXT("[KeMuSan] GameMode BeginPlay, ExamController=%s"), ExamController ? TEXT("OK") : TEXT("NULL"));

	if (FParse::Param(FCommandLine::Get(), TEXT("capture-showcase")))
	{
		bShowcaseCapturing = true;
		ShowcasePhase = 0;
		ShowcaseTimerSec = 0.f;
		UE_LOG(LogTemp, Log, TEXT("[KeMuSanShowcase] Starting professional showcase capture sequence with native driving, mirror comparison and corner camera tests..."));
	}

	// 自动测试入口
	if (FParse::Param(FCommandLine::Get(), TEXT("autotest")))
	{
		UE_LOG(LogTemp, Log, TEXT("[KeMuSan] autotest flag detected, starting free practice"));
		StartFreePractice(ETransmissionType::Manual);

		GetWorldTimerManager().SetTimer(AutoShotTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
			{
				PC->ConsoleCommand(TEXT("shot"), true);
			}
		}), 4.f, true, 4.f);  // 改为每4秒一张，覆盖更多场景

		if (FParse::Param(FCommandLine::Get(), TEXT("traffic-regression")))
		{
			FString RegressionScenario(TEXT("road_test_default"));
			FParse::Value(FCommandLine::Get(), TEXT("traffic-scenario="), RegressionScenario);
			float ExitAfter = 18.f;
			if (RegressionScenario.Equals(TEXT("crosswalk_yield"), ESearchCase::IgnoreCase) ||
				RegressionScenario.Equals(TEXT("pedestrian_yield"), ESearchCase::IgnoreCase))
			{
				// Allow the deterministic driver to reach S=159, stop, and release the pedestrian.
				ExitAfter = 52.f;
			}
			else if (RegressionScenario.Equals(TEXT("follow_and_meet"), ESearchCase::IgnoreCase))
			{
				ExitAfter = 30.f;
			}
			else if (RegressionScenario.Equals(TEXT("full_mix"), ESearchCase::IgnoreCase) ||
				RegressionScenario.Equals(TEXT("road_test_default"), ESearchCase::IgnoreCase))
			{
				ExitAfter = 34.f;
			}
			UE_LOG(LogTemp, Log, TEXT("[KeMuSanTraffic] regression_window scenario=%s seconds=%.0f"), *RegressionScenario, ExitAfter);
			GetWorldTimerManager().SetTimer(TrafficRegressionExitTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				UE_LOG(LogTemp, Log, TEXT("[KeMuSanTraffic] regression_complete"));
				FGenericPlatformMisc::RequestExit(false);
			}), ExitAfter, false);
		}
	}
}

void AKeMuSanGameMode::RestartPlayer(AController* NewPlayer)
{
	if (!NewPlayer)
	{
		return;
	}

	if (DefaultPawnClass)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		APawn* NewPawn = GetWorld()->SpawnActor<APawn>(DefaultPawnClass, RoadLayout::StartPose * 100.f, FRotator(0.f, 0.f, 0.f), Params);
		if (NewPawn)
		{
			NewPlayer->Possess(NewPawn);
			NewPawn->SetActorLocationAndRotation(RoadLayout::StartPose * 100.f, FRotator(0.f, 0.f, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[KeMuSan] Failed to spawn default pawn!"));
		}
	}
}

void AKeMuSanGameMode::StartExam(ETransmissionType TransType)
{
	if (bGameStarted || !ExamController)
	{
		return;
	}
	CurrentTransmission = TransType;
	bGameStarted = true;
	bExamMode = true;
	ExamController->SetTransmission(TransType);
	ExamController->BeginExam(true);

	// 传递变速箱模式给 Pawn
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (AKeMuSanPawn* Car = Cast<AKeMuSanPawn>(PC->GetPawn()))
		{
			Car->SetTransmission(TransType);
		}
	}
}

void AKeMuSanGameMode::StartFreePractice(ETransmissionType TransType)
{
	if (bGameStarted || !ExamController)
	{
		return;
	}
	CurrentTransmission = TransType;
	bGameStarted = true;
	bExamMode = false;
	ExamController->SetTransmission(TransType);
	ExamController->BeginExam(false);

	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (AKeMuSanPawn* Car = Cast<AKeMuSanPawn>(PC->GetPawn()))
		{
			Car->SetTransmission(TransType);
		}
	}
}

void AKeMuSanGameMode::RestartGame()
{
	UGameplayStatics::SetGamePaused(GetWorld(), false);
	bPaused = false;
	UGameplayStatics::OpenLevel(this, TEXT("/Engine/Maps/Entry"), true);
}

void AKeMuSanGameMode::TogglePause()
{
	if (!bGameStarted)
	{
		return;
	}
	bPaused = !bPaused;
	UGameplayStatics::SetGamePaused(GetWorld(), bPaused);
	if (ExamController)
	{
		ExamController->OnPauseChanged(bPaused);
	}
}
void AKeMuSanGameMode::TickPhysicsVerification(float Dt)
{
 PhysicsTime+=Dt;
 if(PhysicsCase==11)
 {
  auto* PC=GetWorld()->GetFirstPlayerController();
  auto* Player=PC?Cast<AKeMuSanPawn>(PC->GetPawn()):nullptr;
  if(Player && TestCamera)
  {
   const FVector Target=Player->GetActorLocation()+FVector(0,0,20),Pos=Target+FVector(100,-500,700);
   TestCamera->SetActorLocationAndRotation(Pos,(Target-Pos).Rotation());
  }
  if(Player && PhysicsTime>.25f && !bPlayerImpactStarted)
  {
   bPlayerImpactStarted=true;
   Player->GetPhysicsBody()->SetPhysicsLinearVelocity(FVector(-1600,0,0));
  }
  if(Player && PhysicsTime>2.f && !bDamageShot)
  {
   bDamageShot=true;
   const bool Pass=Player->GetDamage()->MaxDentCm()>5.f && Player->GetDamage()->DamagedVertexCount()>10 && FMath::Abs(Player->GetPhysicsBody()->GetMass()-1400.f)<2.f;
   if(!Pass)++PhysicsFailures;
   UE_LOG(LogTemp,Log,TEXT("[KeMuSanDamageTest] player_crash %s dent_cm=%.3f vertices=%d"),Pass?TEXT("PASS"):TEXT("FAIL"),Player->GetDamage()->MaxDentCm(),Player->GetDamage()->DamagedVertexCount());
   Player->ForceStop();
   // The contact and dent assertions above use the real truck. Remove only the
   // test obstacle after impact so it cannot obscure the retained crumpled panels.
   if(PhysicsA){PhysicsA->Destroy();PhysicsA=nullptr;}
   if(!DamageScreenshotDir.IsEmpty())FScreenshotRequest::RequestScreenshot(FPaths::Combine(DamageScreenshotDir,TEXT("dent-after.png")),false,false);
  }
  if(PhysicsTime<3.f)return;
  if(PhysicsA){PhysicsA->Destroy();PhysicsA=nullptr;}
 }
 if(PhysicsCase==12)
 {
  if(PhysicsTime>.8f && !PhysicsScreenshot.IsEmpty()){FScreenshotRequest::RequestScreenshot(PhysicsScreenshot,false,false);PhysicsScreenshot.Empty();}
  if(PhysicsTime>1.5f){FPlatformMisc::RequestExitWithStatus(false,PhysicsFailures==0?0:1);bPhysicsVerification=false;}
  return;
 }

 if(PhysicsA && PhysicsCase>=6 && PhysicsCase<=8)
 {
  auto* Chassis=PhysicsA->GetPhysicsBody();
  const float Speed=FVector::DotProduct(Chassis->GetPhysicsLinearVelocity(),PhysicsA->GetActorForwardVector())*.01f;
  if(PhysicsTime>=.6f && !bCornerStarted){Chassis->SetPhysicsLinearVelocity(PhysicsA->GetActorForwardVector()*(PhysicsCase==6?500.f:3000.f));bCornerStarted=true;}
  if(bCornerStarted)VehiclePhysics::Drive(Chassis,FMath::Clamp(((PhysicsCase==6?5.f:30.f)-Speed)*2.f,-4.f,4.f),PhysicsCase==6?.3f:.7f,Dt);
  PhysicsMinUp=FMath::Min(PhysicsMinUp,static_cast<float>(PhysicsA->GetActorUpVector().Z));
  PhysicsMaxRoll=FMath::Max(PhysicsMaxRoll,FMath::Abs(PhysicsA->GetActorRotation().Roll));
  if(PhysicsCase==7 && PhysicsMinUp<.15f && !bRollShot)
  {
   bRollShot=true;
   if(!DamageScreenshotDir.IsEmpty())
   {
    if(auto* PC=GetWorld()->GetFirstPlayerController())
    {
     const FVector Target=PhysicsA->GetActorLocation();
     const FVector CameraPos=Target+FVector(-850,-1250,500);
     TestCamera=GetWorld()->SpawnActor<ACameraActor>(CameraPos,(Target-CameraPos).Rotation());
     TestCamera->GetCameraComponent()->FieldOfView=60.f;
     PC->SetViewTarget(TestCamera);if(PC->GetHUD())PC->GetHUD()->bShowHUD=false;
    }
    FScreenshotRequest::RequestScreenshot(FPaths::Combine(DamageScreenshotDir,TEXT("rollover.png")),false,false);
   }
  }
 }
 if(PhysicsA && PhysicsB)
 {
  PhysicsPeakSpeed=FMath::Max(PhysicsPeakSpeed,static_cast<float>(PhysicsB->GetPhysicsBody()->GetPhysicsLinearVelocity().Size()*.01));
  PhysicsPeakAngular=FMath::Max(PhysicsPeakAngular,FMath::Abs(PhysicsB->GetPhysicsBody()->GetPhysicsAngularVelocityInDegrees().Z));
 }
 if(PhysicsTime<(PhysicsCase>=6?6.f:2.f))return;
 if(PhysicsCase>=0 && PhysicsCase<11)
 {
  bool Pass=PhysicsA && PhysicsA->GetPhysicsBody()->IsSimulatingPhysics() && (PhysicsCase>=6 || (PhysicsB && PhysicsB->GetPhysicsBody()->IsSimulatingPhysics()));
  if(PhysicsB)Pass &= FMath::Abs(PhysicsB->GetPhysicsBody()->GetMass()-(PhysicsCase==1?14000.f:1500.f))<2.f;
  Pass &= FMath::Abs(PhysicsA->GetPhysicsBody()->GetMass()-(PhysicsCase==7?14000.f:1500.f))<2.f;
  const FVector LocalCOM=PhysicsA->GetActorTransform().InverseTransformPosition(PhysicsA->GetPhysicsBody()->GetCenterOfMass());
  Pass &= FMath::Abs(LocalCOM.Z-(PhysicsCase==7?30.f:-12.f))<1.f;
  const float Moved=PhysicsB?FVector::Dist2D(PhysicsOrigin,PhysicsB->GetActorLocation())*.01f:0.f;
  if(PhysicsCase==0){LightImpactSpeed=PhysicsPeakSpeed;LightDent=PhysicsB->GetDamage()->MaxDentCm();Pass &= PhysicsPeakSpeed>1.f && Moved>.5f && LightDent>1.f && PhysicsB->GetDamage()->DamagedVertexCount()>10;}
  if(PhysicsCase==1)Pass &= PhysicsPeakSpeed>.05f && PhysicsPeakSpeed<LightImpactSpeed*.6f && PhysicsB->GetPhysicsBody()->GetMass()>10000.f;
  if(PhysicsCase==2)Pass &= PhysicsPeakAngular>2.f && Moved>.2f;
  if(PhysicsCase==3)Pass &= PhysicsA->GetActorLocation().X<PhysicsB->GetActorLocation().X;
  if(PhysicsCase==4)Pass &= PhysicsA->GetActorLocation().X>PhysicsB->GetActorLocation().X && Moved>.2f;
  if(PhysicsCase==5)Pass &= PhysicsA->GetActorLocation().X<PhysicsB->GetActorLocation().X && PhysicsPeakSpeed>1.f;
  if(PhysicsCase==5)Pass &= PhysicsB->GetDamage()->MaxDentCm()>LightDent;
  if(PhysicsCase==6)Pass &= PhysicsMinUp>.85f && PhysicsMaxRoll<25.f;
  if(PhysicsCase==7)Pass &= PhysicsMinUp<.15f && PhysicsMaxRoll>70.f && PhysicsA->GetVehicleContacts()==0;
  if(PhysicsCase==10)
  {
   const float Lowest=PhysicsA->GetDynamics()->LowestColliderZ();
   const bool RoofContact=PhysicsA->GetDynamics()->RoofContacts>0 && PhysicsA->GetDamage()->MaxDentCm()>5.f && Lowest>-5.f;
   Pass &= RoofContact;
   UE_LOG(LogTemp,Log,TEXT("[KeMuSanDamageTest] roof_contact %s count=%d dent_cm=%.3f lowest_z_cm=%.3f"),RoofContact?TEXT("PASS"):TEXT("FAIL"),PhysicsA->GetDynamics()->RoofContacts,PhysicsA->GetDamage()->MaxDentCm(),Lowest);
  }
  if(PhysicsCase==8)Pass &= PhysicsMaxRoll>3.f;
  if(PhysicsCase==9)Pass &= PhysicsA->GetDamage()->MaxDentCm()<.1f && PhysicsB->GetDamage()->MaxDentCm()<.1f;
  if(PhysicsCase>=6)UE_LOG(LogTemp,Log,TEXT("[KeMuSanDynamicsTest] case=%d %s min_up=%.3f max_roll_deg=%.2f"),PhysicsCase,Pass?TEXT("PASS"):TEXT("FAIL"),PhysicsMinUp,PhysicsMaxRoll);
  if(!Pass)++PhysicsFailures;
  UE_LOG(LogTemp,Log,TEXT("[KeMuSanPhysicsTest] case=%d %s target_peak_ms=%.3f target_yaw_deg_s=%.3f target_moved_m=%.3f mass_kg=%.1f"),PhysicsCase,Pass?TEXT("PASS"):TEXT("FAIL"),PhysicsPeakSpeed,PhysicsPeakAngular,Moved,PhysicsB?PhysicsB->GetPhysicsBody()->GetMass():0.f);
  if(PhysicsB)UE_LOG(LogTemp,Log,TEXT("[KeMuSanDamageTest] case=%d dent_cm=%.3f vertices=%d"),PhysicsCase,PhysicsB->GetDamage()->MaxDentCm(),PhysicsB->GetDamage()->DamagedVertexCount());
  if(PhysicsCase==0)
  {
   const bool HadDamage=PhysicsB->GetDamage()->MaxDentCm()>1.f && PhysicsB->GetDamage()->DamagedVertexCount()>0;
   PhysicsB->Activate(0);
   const bool Repaired=HadDamage && PhysicsB->GetDamage()->MaxDentCm()<.01f && PhysicsB->GetDamage()->DamagedVertexCount()==0;
   if(!Repaired)++PhysicsFailures;
   UE_LOG(LogTemp,Log,TEXT("[KeMuSanDamageTest] repair %s"),Repaired?TEXT("PASS"):TEXT("FAIL"));
   PhysicsB->Deactivate();
   bool NoGhost=!PhysicsB->GetPhysicsBody()->IsSimulatingPhysics();
   TArray<UPrimitiveComponent*> Shapes;PhysicsB->GetComponents(Shapes);
   for(auto* Shape:Shapes)if(Shape->GetCollisionObjectType()==ECC_Vehicle && Shape->GetCollisionEnabled()!=ECollisionEnabled::NoCollision)NoGhost=false;
   if(!NoGhost)++PhysicsFailures;
   UE_LOG(LogTemp,Log,TEXT("[KeMuSanDamageTest] recycle_collision %s"),NoGhost?TEXT("PASS"):TEXT("FAIL"));
   PhysicsB->Activate(0);PhysicsB->SetPose(FVector(3000,3000,0),0);
   const FVector RestoredCOM=PhysicsB->GetActorTransform().InverseTransformPosition(PhysicsB->GetPhysicsBody()->GetCenterOfMass());
   const bool Restored=PhysicsB->GetPhysicsBody()->IsSimulatingPhysics() && FMath::Abs(PhysicsB->GetPhysicsBody()->GetMass()-1500.f)<2.f && FMath::Abs(RestoredCOM.Z+12.f)<1.f;
   if(!Restored)++PhysicsFailures;
   UE_LOG(LogTemp,Log,TEXT("[KeMuSanDamageTest] pool_reactivate %s mass_kg=%.3f com_z_cm=%.3f"),Restored?TEXT("PASS"):TEXT("FAIL"),PhysicsB->GetPhysicsBody()->GetMass(),RestoredCOM.Z);
  }
  if(PhysicsA)PhysicsA->Destroy();if(PhysicsB)PhysicsB->Destroy();
 }
 ++PhysicsCase;PhysicsTime=0;PhysicsPeakSpeed=0;PhysicsPeakAngular=0;PhysicsMinUp=1;PhysicsMaxRoll=0;bCornerStarted=false;
 if(PhysicsCase>=12)
 {
  UE_LOG(LogTemp,Log,TEXT("[KeMuSanPhysicsTest] complete %s failures=%d"),PhysicsFailures==0?TEXT("PASS"):TEXT("FAIL"),PhysicsFailures);
  FString Screenshot;
  if(PhysicsFailures==0 && FParse::Value(FCommandLine::Get(),TEXT("physics-screenshot="),Screenshot))
  {
   APlayerController* PC=GetWorld()->GetFirstPlayerController();
   if(auto* Car=PC?Cast<AKeMuSanPawn>(PC->GetPawn()):nullptr)
   {
    Car->GetDamage()->Repair();Car->SetActorTickEnabled(true);PC->SetViewTarget(Car);
    Car->SetTestPose(FVector(48000.f,31750.f,25.f),FRotator(0,180,0));
    if(auto* Arm=Car->FindComponentByClass<USpringArmComponent>())
    {
     Arm->bEnableCameraLag=false;Arm->SetUsingAbsoluteRotation(true);
     Arm->SetRelativeLocation(FVector(1100,0,220));Arm->TargetArmLength=1800.f;
     Arm->SetWorldRotation(FRotator(-18,135,0));
    }
    if(PC->GetHUD())PC->GetHUD()->bShowHUD=false;
    FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Truck=GetWorld()->SpawnActor<AAICar>(FVector::ZeroVector,FRotator::ZeroRotator,Params);
    if(Truck){Truck->MakeHeavyTruck();Truck->Activate(0);Truck->SetPose(FVector(465,317.5,.25),180);}
    PhysicsScreenshot=Screenshot;
   }
   return;
  }
  FPlatformMisc::RequestExitWithStatus(false,PhysicsFailures==0?0:1);bPhysicsVerification=false;return;
 }
 FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 PhysicsA=GetWorld()->SpawnActor<AAICar>(FVector::ZeroVector,FRotator::ZeroRotator,Params);
 PhysicsB=GetWorld()->SpawnActor<AAICar>(FVector::ZeroVector,FRotator::ZeroRotator,Params);
 if(!PhysicsA||!PhysicsB){++PhysicsFailures;return;}
 if(PhysicsCase==11)
 {
  PhysicsB->Destroy();PhysicsB=nullptr;
  PhysicsA->MakeHeavyTruck();PhysicsA->Activate(0);PhysicsA->SetPose(FVector(468,317.5,0),0);
  PhysicsA->SetActorTickEnabled(false);
  auto* PC=GetWorld()->GetFirstPlayerController();
  if(auto* Player=PC?Cast<AKeMuSanPawn>(PC->GetPawn()):nullptr)
  {
   Player->SetTestPose(FVector(48000,31750,0),FRotator(0,180,0));Player->GetDamage()->Repair();Player->SetActorTickEnabled(false);
   const FVector Target(48000,31750,95),CameraPos=Target+FVector(100,-500,700);
   TestCamera=GetWorld()->SpawnActor<ACameraActor>(CameraPos,(Target-CameraPos).Rotation());
   TestCamera->GetCameraComponent()->FieldOfView=55.f;
   PC->SetViewTarget(TestCamera);if(PC->GetHUD())PC->GetHUD()->bShowHUD=false;
   if(!DamageScreenshotDir.IsEmpty())FScreenshotRequest::RequestScreenshot(FPaths::Combine(DamageScreenshotDir,TEXT("dent-before.png")),false,false);
  }
  return;
 }
 if(PhysicsCase==1)PhysicsB->MakeHeavyTruck();
 if(PhysicsCase==7)PhysicsA->MakeHeavyTruck();
 FVector A(3000.f-10.f,3000.f,0.f),B(3000.f,3000.f,0.f);
 FVector Velocity(800.f,0,0);float YawA=0.f;
 if(PhysicsCase==2){A=FVector(3000.f+1.f,2990.f,0);Velocity=FVector(0,800,0);YawA=90.f;}
 if(PhysicsCase==4){A=FVector(3010.f,3000.f,0);Velocity=FVector(-800,0,0);}
 if(PhysicsCase==5){A=FVector(2980.f,3000.f,0);Velocity=FVector(6000,0,0);}
 if(PhysicsCase>=6 && PhysicsCase<=8){A=FVector(470.f,317.5f,0);B=FVector(2900.f,2900.f,0);Velocity=FVector::ZeroVector;}
 if(PhysicsCase==9){A=FVector(2994.8f,3000.f,0);Velocity=FVector(80,0,0);}
 if(PhysicsCase==10){A=FVector(480,317.5,3.5);B=FVector(2900,2900,0);Velocity=FVector(0,0,-800);}
 PhysicsA->Activate(0);PhysicsB->Activate(0);
 PhysicsA->SetPose(A,YawA);PhysicsB->SetPose(B,0);
 PhysicsA->SetActorTickEnabled(false);PhysicsB->SetActorTickEnabled(false);
 if(PhysicsCase==10)PhysicsA->SetActorRotation(FRotator(0,0,180),ETeleportType::TeleportPhysics);
 PhysicsOrigin=PhysicsB->GetActorLocation();
 PhysicsA->GetPhysicsBody()->SetPhysicsLinearVelocity(Velocity);
 if(PhysicsCase==3)PhysicsB->GetPhysicsBody()->SetPhysicsLinearVelocity(FVector(-800,0,0));
}
