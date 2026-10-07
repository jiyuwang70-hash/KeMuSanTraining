// 科目三模拟游戏 GameMode
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ExamTypes.h"
#include "KeMuSanGameMode.generated.h"

class AController;
class AAICar;
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
 void TickPhysicsVerification(float Dt);
 bool bPhysicsVerification=false;
 int32 PhysicsCase=-1,PhysicsFailures=0;
 float PhysicsTime=0.f,PhysicsPeakSpeed=0.f,PhysicsPeakAngular=0.f;
 float LightImpactSpeed=0.f,LightDent=0.f,PhysicsMinUp=1.f,PhysicsMaxRoll=0.f;
 bool bRollShot=false,bCornerStarted=false,bPlayerImpactStarted=false,bDamageShot=false;
 UPROPERTY() class ACameraActor* TestCamera=nullptr;
 FString DamageScreenshotDir;
 UPROPERTY() AAICar* PhysicsA=nullptr;
 UPROPERTY() AAICar* PhysicsB=nullptr;
 FVector PhysicsOrigin=FVector::ZeroVector;
 FString PhysicsScreenshot;

};