// 考试相关公共类型定义
#pragma once

#include "CoreMinimal.h"
#include "ExamTypes.generated.h"

// 考试阶段
UENUM(BlueprintType)
enum class EExamPhase : uint8
{
	Menu       UMETA(DisplayName = "主菜单"),
	Prep       UMETA(DisplayName = "上车准备"),
	LightTest  UMETA(DisplayName = "夜间灯光模拟"),
	Ready      UMETA(DisplayName = "起步"),
	Driving    UMETA(DisplayName = "道路驾驶"),
	PullOver   UMETA(DisplayName = "靠边停车"),
	Finished   UMETA(DisplayName = "考试结束")
};

// 挡位
UENUM(BlueprintType)
enum class EGear : uint8
{
	N   UMETA(DisplayName = "空挡"),
	R   UMETA(DisplayName = "倒挡"),
	G1  UMETA(DisplayName = "1挡"),
	G2  UMETA(DisplayName = "2挡"),
	G3  UMETA(DisplayName = "3挡"),
	G4  UMETA(DisplayName = "4挡"),
	G5  UMETA(DisplayName = "5挡")
};

// 夜间灯光模拟题目
USTRUCT(BlueprintType)
struct FLightQuestion
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Text;
	// 正确答案: 1近光 2远光 3远近交替 4示廓灯+危险报警闪光灯 5雾灯+危险报警闪光灯
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 CorrectAnswer = 1;
};

// 扣分记录
USTRUCT(BlueprintType)
struct FDeduction
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) int32 Points = 0;
	UPROPERTY(BlueprintReadOnly) FString Reason;
	UPROPERTY(BlueprintReadOnly) float TimeSeconds = 0.f;
};

// 考试项目进度（用于 HUD 进度面板）
USTRUCT(BlueprintType)
struct FZoneStatus
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FString Name;
	// 0 未考  1 进行中  2 已完成
	UPROPERTY(BlueprintReadOnly) int32 State = 0;
};
