#include "KeMuSanGameMode.h"

#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"

#include "KeMuSanPawn.h"
#include "KeMuSanPlayerController.h"
#include "KeMuSanHUD.h"
#include "ExamController.h"
#include "RoadLayout.h"

AKeMuSanGameMode::AKeMuSanGameMode()
{
	DefaultPawnClass = AKeMuSanPawn::StaticClass();
	PlayerControllerClass = AKeMuSanPlayerController::StaticClass();
	HUDClass = AKeMuSanHUD::StaticClass();
}

void AKeMuSanGameMode::BeginPlay()
{
	Super::BeginPlay();

	// 生成考试总控
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ExamController = GetWorld()->SpawnActor<AExamController>(FVector::ZeroVector, FRotator::ZeroRotator, Params);

	UE_LOG(LogTemp, Log, TEXT("[KeMuSan] GameMode BeginPlay, ExamController=%s"), ExamController ? TEXT("OK") : TEXT("NULL"));

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
		APawn* NewPawn = GetWorld()->SpawnActor<APawn>(DefaultPawnClass, RoadLayout::StartPose, FRotator(0.f, 0.f, 0.f), Params);
		if (NewPawn)
		{
			NewPlayer->Possess(NewPawn);
			NewPawn->SetActorLocationAndRotation(RoadLayout::StartPose, FRotator(0.f, 0.f, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
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