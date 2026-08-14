// 科目三玩家控制器：挡位 / 灯光模拟答案 / 开始暂停重考
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ExamTypes.h"
#include "KeMuSanPlayerController.generated.h"

class AKeMuSanGameMode;
class AExamController;
struct FKey;

UCLASS()
class KEMUSANTRAINING_API AKeMuSanPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AKeMuSanPlayerController();

	virtual void SetupInputComponent() override;

	// 挡位键（灯光考试阶段被忽略）
	void GearKey(int32 GearIndex);

	// 灯光模拟答案键（非灯光考试阶段被忽略）
	void AnswerKey(int32 Answer);

	// 数字键/挡位键统一入口（按 FKey 分流）
	void HandleGearKey(FKey Key);
	void HandleAnswerKey(FKey Key);

	void ConfirmPressed();
	void PausePressed();
	void FreePracticePressed();

	AKeMuSanGameMode* GetGameMode() const;
	AExamController* GetExamController() const;

protected:
	int32 LastGearValue = 0;
};
