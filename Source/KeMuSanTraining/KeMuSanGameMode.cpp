#include "KeMuSanGameMode.h"

#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
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

	// 生成考试总控（负责生成道路、AI 车、行人、红绿灯与全部判定规则）
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ExamController = GetWorld()->SpawnActor<AExamController>(FVector::ZeroVector, FRotator::ZeroRotator, Params);

	UE_LOG(LogTemp, Log, TEXT("[KeMuSan] GameMode BeginPlay, ExamController=%s"), ExamController ? TEXT("OK") : TEXT("NULL"));

	// 无头冒烟测试入口：-autotest 自动开始考试
	if (FParse::Param(FCommandLine::Get(), TEXT("autotest")))
	{
		UE_LOG(LogTemp, Log, TEXT("[KeMuSan] autotest flag detected, starting exam"));
		StartExam();
	}
}

void AKeMuSanGameMode::RestartPlayer(AController* NewPlayer)
{
	if (!NewPlayer)
	{
		return;
	}

	// 优先使用地图中的 PlayerStart，否则在原点生成
	AActor* StartSpot = FindPlayerStart(NewPlayer);
	APawn* NewPawn = SpawnDefaultPawnFor(NewPlayer, StartSpot);

	// 无论在哪生成，都把考试车放到科目三路线起点（模板地图的 PlayerStart 与考试路线无关）
	if (NewPawn)
	{
		const FVector StartLoc = RoadLayout::RouteOffset + FVector(RoadLayout::StartX, RoadLayout::RightLaneY, 1.f);
		const FRotator StartRot = FRotator(0.f, 0.f, 0.f); // 车头朝 +X
		NewPawn->SetActorLocationAndRotation(StartLoc, StartRot, false, nullptr, ETeleportType::TeleportPhysics);
	}
}

void AKeMuSanGameMode::StartExam()
{
	if (bGameStarted || !ExamController)
	{
		return;
	}
	bGameStarted = true;
	bExamMode = true;
	ExamController->BeginExam(true);
}

void AKeMuSanGameMode::StartFreePractice()
{
	if (bGameStarted || !ExamController)
	{
		return;
	}
	bGameStarted = true;
	bExamMode = false;
	ExamController->BeginExam(false);
}

void AKeMuSanGameMode::RestartGame()
{
	UGameplayStatics::SetGamePaused(GetWorld(), false);
	bPaused = false;
	// 重新加载当前关卡
	UGameplayStatics::OpenLevel(this, FName(*GetWorld()->GetMapName()));
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
