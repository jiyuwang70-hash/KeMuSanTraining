// 考试总控制器：阶段流转、灯光模拟、全部考试项目判定与评分
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ExamTypes.h"
#include "ExamController.generated.h"

class AKeMuSanPawn;
class ARoadBuilder;
class AAICar;
class APedestrian;
class ATrafficLight;
class ABeeper;

UCLASS()
class KEMUSANTRAINING_API AExamController : public AActor
{
	GENERATED_BODY()

public:
	AExamController();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// 开始考试 / 练习
	void BeginExam(bool bIsExam);

	// 灯光模拟答案（1..5）
	void SubmitLightAnswer(int32 Answer);

	// 暂停状态变化
	void OnPauseChanged(bool bPaused);

	// ---- 供 HUD 读取 ----
	EExamPhase GetPhase() const { return Phase; }
	int32 GetScore() const { return Score; }
	const TArray<FDeduction>& GetDeductions() const { return Deductions; }
	FString GetPrompt() const { return CurrentPrompt; }
	const TArray<FZoneStatus>& GetZoneStatuses() const { return ZoneStatuses; }
	const FLightQuestion* GetCurrentLightQuestion() const;
	float GetLightCountdown() const { return LightCountdown; }
	int32 GetLightQuestionIndex() const { return LightQuestionNumber; }
	int32 GetLightQuestionTotal() const { return LightQuestionTotal; }
	bool IsPractice() const { return bPractice; }
	bool IsFailIssued() const { return bFailIssued; }
	FString GetResultLine() const { return ResultLine; }
	int32 GetTrafficLightState() const;
	float GetTrafficLightRemaining() const;

protected:
	// ---- 阶段 ----
	EExamPhase Phase = EExamPhase::Menu;
	bool bPractice = false;
	bool bPaused = false;
	int32 Score = 100;
	bool bFailIssued = false;
	FString CurrentPrompt;
	FString ResultLine;

	// ---- 灯光模拟 ----
	TArray<FLightQuestion> LightPool;
	TArray<int32> LightOrder;
	int32 LightIndex = -1;
	float LightCountdown = 0.f;
	int32 LightQuestionNumber = 0;
	int32 LightQuestionTotal = 0;

	// ---- 上车准备 ----
	bool bPrepSeatbeltOk = false;
	bool bPrepObserveOk = false;
	float PrepDoneTimer = 0.f;

	// ---- 起步 ----
	bool bReadySignalOk = false;
	bool bReadyObserveOk = false;
	float StartCarX = 0.f;
	bool bHandbrakeLaunchCharged = false;
	float HandbrakeLaunchTimer = 0.f;

	// ---- 直线行驶 ----
	bool bStraightInit = false;
	float StraightRefYaw = 0.f;
	float StraightRefY = 0.f;
	float StraightBadTime = 0.f;

	// ---- 变更车道 ----
	int32 LaneChangeStage = 0; // 0 未开始 1 变到左道完成 2 变回右道完成
	float LaneChangeSignalTime = 0.f;
	bool bLaneChangeCrossed = false;

	// ---- 路口 / 斑马线 ----
	bool bStopLineCrossed = false;
	bool bIntersectionEntered = false;
	bool bCrosswalkPedFail = false;
	bool bCrosswalkSpeedCharged = false;
	bool bPedStarted = false;

	// ---- 超车 ----
	bool bOvertakePassed = false;
	bool bOvertakeSignalUsed = false;
	float OvertakeSignalTime = 0.f;
	bool bOvertakeObserved = false;
	bool bOvertakeReturnSignal = false;

	// ---- 掉头 ----
	bool bUTurnEntered = false;
	float UTurnYawRef = 0.f;
	float UTurnDeltaMin = 0.f;  // 正向累计最大
	float UTurnDeltaNeg = 0.f;  // 负向累计最小
	bool bUTurnSignalUsed = false;
	bool bUTurnObserved = false;
	bool bUTurnSpeedCharged = false;
	bool bUTurnEvaluated = false;

	// ---- 加减挡 ----
	int32 GearShiftStage = 0; // 0 未开始 1 已加至4挡 2 完成
	int32 LastGearForShift = 0;
	bool bGearJumpCharged = false;

	// ---- 靠边停车 ----
	float PullOverSignalTime = 0.f;
	bool bPullOverObserved = false;
	bool bPullOverStopped = false;
	float PullOverStopTime = 0.f;
	float PullOverGap = 999.f;
	bool bPullOverNeutralOk = false;
	bool bPullOverHandbrakeOk = false;
	bool bPullOverDone = false;

	// ---- 通用判罚 ----
	TArray<FDeduction> Deductions;
	TArray<FZoneStatus> ZoneStatuses;
	bool bStallFlagged = false;
	float HandbrakeDriveTimer = 0.f;
	float CenterlineTime = 0.f;
	float SeatbeltOffTime = 0.f;
	bool bRoadEndCharged = false;

	// ---- 场景对象 ----
	UPROPERTY()
	ARoadBuilder* RoadBuilder = nullptr;

	UPROPERTY()
	AAICar* MeetingCar = nullptr;

	UPROPERTY()
	AAICar* SlowCar = nullptr;

	UPROPERTY()
	APedestrian* Pedestrian = nullptr;

	UPROPERTY()
	ATrafficLight* TrafficLightActor = nullptr;

	UPROPERTY()
	ABeeper* Beeper = nullptr;

	UPROPERTY()
	AKeMuSanPawn* Car = nullptr;

	float PrevCarX = 0.f;
	bool bPrevXValid = false;

	// ---- 内部工具 ----
	void SetPhase(EExamPhase NewPhase);
	void SetPrompt(const FString& Text);
	void AddDeduction(int32 Points, const FString& Reason);
	void FailExam(const FString& Reason);
	void FinishExam();

	void BuildLightPool();
	void BuildLightOrder(int32 Count);
	void SetupZoneStatuses();
	void MarkZone(int32 Index, int32 State);
	void MarkZoneByName(const FString& Name, int32 State);

	void UpdatePrep(float DT);
	void UpdateLightTest(float DT);
	void UpdateReady(float DT);
	void UpdateDriving(float DT);
	void UpdatePullOver(float DT);
	void MonitorGeneral(float DT);

	bool IsExamScoring() const { return !bPractice; }
	float CarX() const;
	float CarY() const;
	float CarYawDeg() const;
	float CarSpeedKmh() const;
	bool IsHeadingMinusX() const;
	bool HeadCheckedRecently(float Seconds) const;
	float LocalX(const FVector& WorldLoc) const;

	// 道路驾驶各项目
	void TickStraight(float DT);
	void TickLaneChange(float DT);
	void TickIntersection(float DT);
	void TickSchoolBus(float DT);
	void TickOvertake(float DT);
	void TickGearShift(float DT);
	void TickUTurn(float DT);
	void TickPullOverTrigger(float DT);
};
