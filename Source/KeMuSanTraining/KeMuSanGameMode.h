// 科目三模拟游戏 GameMode
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ExamTypes.h"
#include "KeMuSanGameMode.generated.h"

class AController;
class AExamController;

UCLASS()
class KEMUSANTRAINING_API AKeMuSanGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AKeMuSanGameMode();

	virtual void BeginPlay() override;
	virtual void RestartPlayer(AController* NewPlayer) override;

	// Start exam with specified transmission type
	UFUNCTION(BlueprintCallable, Category = "Exam")
	void StartExam(ETransmissionType TransType = ETransmissionType::Manual);

	// Start free practice
	UFUNCTION(BlueprintCallable, Category = "Exam")
	void StartFreePractice(ETransmissionType TransType = ETransmissionType::Manual);

	// Restart
	UFUNCTION(BlueprintCallable, Category = "Exam")
	void RestartGame();

	// Pause
	UFUNCTION(BlueprintCallable, Category = "Exam")
	void TogglePause();

	UFUNCTION(BlueprintPure, Category = "Exam")
	AExamController* GetExamController() const { return ExamController; }

	UFUNCTION(BlueprintPure, Category = "Exam")
	bool IsGameStarted() const { return bGameStarted; }

	UPROPERTY(BlueprintReadOnly, Category = "Exam")
	bool bExamMode = false;

	UPROPERTY(BlueprintReadOnly, Category = "Exam")
	bool bGameStarted = false;

	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(BlueprintReadOnly, Category = "Exam")
	bool bPaused = false;

	UPROPERTY(BlueprintReadOnly, Category = "Exam")
	ETransmissionType CurrentTransmission = ETransmissionType::Manual;

protected:
	UPROPERTY()
	AExamController* ExamController = nullptr;

	FTimerHandle AutoShotTimer;
	FTimerHandle TrafficRegressionExitTimer;

	bool bShowcaseCapturing = false;
	int32 ShowcasePhase = 0;
	float ShowcaseTimerSec = 0.f;
	FVector ShowcaseStartLoc = FVector::ZeroVector;
	FVector ShowcaseStopLoc = FVector::ZeroVector;
	bool bShot1Ok = false;
	bool bShot2Ok = false;
	bool bShot3Ok = false;
	bool bShot4aOk = false;
	bool bShot4bOk = false;
	bool bShot5Ok = false;
	bool bShot6Ok = false;
	void TickShowcaseCapture(float DeltaSeconds);
};