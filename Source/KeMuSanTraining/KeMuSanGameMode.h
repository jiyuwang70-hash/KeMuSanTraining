// 科目三模拟游戏 GameMode
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
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

	// 开始考试模式
	UFUNCTION(BlueprintCallable, Category = "Exam")
	void StartExam();

	// 开始自由练习模式
	UFUNCTION(BlueprintCallable, Category = "Exam")
	void StartFreePractice();

	// 重开一局
	UFUNCTION(BlueprintCallable, Category = "Exam")
	void RestartGame();

	// 暂停 / 恢复
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

	UPROPERTY(BlueprintReadOnly, Category = "Exam")
	bool bPaused = false;

protected:
	UPROPERTY()
	AExamController* ExamController = nullptr;
};
