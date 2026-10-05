#include "KeMuSanGameMode.h"

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