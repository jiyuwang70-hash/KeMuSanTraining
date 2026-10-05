// 科目三玩家控制器：挡位 / 灯光模拟答案 / 开始暂停重考
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ExamTypes.h"
#include "KeMuSanPlayerController.generated.h"

class AKeMuSanGameMode;
class AExamController;
class AKeMuSanPawn;
struct FKey;

UCLASS()
class KEMUSANTRAINING_API AKeMuSanPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AKeMuSanPlayerController();

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void Tick(float DeltaSeconds) override;

	// 挡位键（灯光考试阶段被忽略，调镜模式下 R 键拦截为复位镜面）
	void GearKey(int32 GearIndex);

	// 灯光模拟答案键（非灯光考试阶段被忽略）
	void AnswerKey(int32 Answer);

	// 数字键/挡位键统一入口（按 FKey 分流）
	void HandleGearKey(FKey Key);
	void HandleAnswerKey(FKey Key);

	void ConfirmPressed();
	void PausePressed();
	void FreePracticePressed();
	void ManualExamPressed();   // F1
	void AutoExamPressed();     // F2
	void CycleGearOrMirror();   // Tab (调镜模式下切换镜面；自动挡模式下切换P/R/N/D)
	void ToggleCameraPressed(); // V (切换追尾视角与座舱视点)
	void ToggleMirrorModePressed(); // T (开启/关闭后视镜校准模式)
	void MirrorUp();
	void MirrorDown();
	void MirrorLeft();
	void MirrorRight();

	AKeMuSanGameMode* GetGameMode() const;
	AExamController* GetExamController() const;
	AKeMuSanPawn* GetTrainingPawn() const;

protected:
	int32 LastGearValue = 0;

	// 自动化输入链测试驱动（真实 PlayerInput/InputKey 队列）
	bool bInputChainTesting = false;
	float InputChainTimer = 0.f;
	int32 InputChainStep = 0;
	int32 InputChainSubStep = 0;
	int32 InputChainFailures = 0;
	float InputChainJourneyTimer = 0.f;
	FVector InputChainStartLocation = FVector::ZeroVector;
	void TickInputChainTest(float DeltaSeconds);

	// 独立考试模式启动核验（-test-exam-start=manual/auto 真实按键注入）
	bool bExamStartTesting = false;
	FString ExamStartTarget;
	int32 ExamStartStep = 0;
	float ExamStartTimer = 0.f;
	void TickExamStartTest(float DeltaSeconds);
};
