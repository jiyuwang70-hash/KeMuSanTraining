// 考试相关公共类型定义（包含后视镜光学物理类型与游戏模式）
#pragma once

#include "CoreMinimal.h"
#include "ExamTypes.generated.h"

// 游戏模式（引导练习 vs 模拟考试）
UENUM(BlueprintType)
enum class EGamePlayMode : uint8
{
	GuidedPractice UMETA(DisplayName = "引导练习模式"),
	SimulatedExam  UMETA(DisplayName = "模拟考试模式")
};

// 变速箱类型
UENUM(BlueprintType)
enum class ETransmissionType : uint8
{
	Manual    UMETA(DisplayName = "手动挡"),
	Auto      UMETA(DisplayName = "自动挡")
};

// 扣分违规所属维度类别
UENUM(BlueprintType)
enum class EErrorCategory : uint8
{
	ObservationSafety UMETA(DisplayName = "安全观察类"),
	LightingSignal    UMETA(DisplayName = "灯光信号类"),
	VehicleControl    UMETA(DisplayName = "车辆操纵类"),
	RulesAndWay       UMETA(DisplayName = "路权规范类"),
	Other             UMETA(DisplayName = "其他综合类")
};

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

// 后视镜类型
UENUM(BlueprintType)
enum class EMirrorType : uint8
{
	Left     UMETA(DisplayName = "左后视镜"),
	Right    UMETA(DisplayName = "右后视镜"),
	Interior UMETA(DisplayName = "中央后视镜")
};

// 后视镜光学参数与状态
USTRUCT(BlueprintType)
struct FMirrorOpticalState
{
	GENERATED_BODY()

	// 偏转角（度）
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Pitch = 0.f;  // 上下俯仰（上仰看天，下俯看地）

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Yaw = 0.f;    // 左右偏转（内折看车身，外展看路面盲区）

	// 光学计算得出的车身在镜中占比（标准约 1/4 = 0.25）
	UPROPERTY(BlueprintReadOnly)
	float VisibleBodyRatio = 0.25f;

	// 光学计算得出的地平线垂直位置（0.0 镜顶, 0.5 居中, 1.0 镜底）
	UPROPERTY(BlueprintReadOnly)
	float HorizonRatio = 0.5f;

	// 是否达到驾考标准调镜规范
	UPROPERTY(BlueprintReadOnly)
	bool bStandardAdjusted = true;

	// 辅助提示语
	UPROPERTY(BlueprintReadOnly)
	FString Tip;
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
	// 从本场开始计时的秒数（暂停时间不累计）。
	UPROPERTY(BlueprintReadOnly) float TimeSeconds = 0.f;
};

// 考试项目进度（用于 HUD 进度面板）
USTRUCT(BlueprintType)
struct FZoneStatus
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FString Name;
	// 0 未考  1 进行中  2 已完成  3 引导练习跳过
	UPROPERTY(BlueprintReadOnly) int32 State = 0;
};
