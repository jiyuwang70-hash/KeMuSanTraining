// 考试总控制器：阶段流转、灯光模拟、全部考试项目判定与评分
// 新版：基于路线投影坐标（里程 S / 横向偏移）判定，支持大型多段路网
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ExamTypes.h"
#include "RoadLayout.h"
#include "ExamController.generated.h"

class AKeMuSanPawn;
class ARoadBuilder;
class AAICar;
class APedestrian;
class ATrafficLight;
class ABeeper;
class ATrafficManager;

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

	// 设置变速箱类型
	void SetTransmission(ETransmissionType InType) { Transmission = InType; }

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
	bool IsAutoTransmission() const { return Transmission == ETransmissionType::Auto; }
	bool IsFailIssued() const { return bFailIssued; }
	FString GetResultLine() const { return ResultLine; }
	int32 GetTrafficLightState() const;
	float GetTrafficLightRemaining() const;

	// 全程进度 0..1（HUD 进度条）
	UFUNCTION(BlueprintPure, Category = "Exam")
	float GetProgress01() const;

	// 真实路段限速与靠边停车数据（供 HUD 复用）
	float GetCurrentSpeedLimit() const;
	float GetCurrentEdgeDistance() const;
	FString GetCurrentExamItemName() const;

	// 游戏模式（引导练习 vs 模拟考试）
	void SetPlayMode(EGamePlayMode InMode);
	EGamePlayMode GetPlayMode() const { return PlayMode; }

protected:
	// ---- 模式与阶段 ----
	EGamePlayMode PlayMode = EGamePlayMode::GuidedPractice;
	EExamPhase Phase = EExamPhase::Menu;
	ETransmissionType Transmission = ETransmissionType::Manual;
	bool bPractice = false;
	bool bPaused = false;
	bool bAutoTest = false;
	bool bPresentationDefaultsApplied = false;
	FTimerHandle ViewModeFixTimer;
	int32 ViewModeFixAttempts = 0;
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
	float StartS = 0.f;
	bool bHandbrakeLaunchCharged = false;
	float HandbrakeLaunchTimer = 0.f;

	// ---- 直线行驶 ----
	bool bStraightInit = false;
	float StraightRefYaw = 0.f;
	float StraightRefLat = 0.f;
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
	bool bCrosswalkYieldCharged = false;
	bool bPedStarted = false;

	// ---- 会车 ----
	bool bMeetingCarSpawned = false;

	// ---- 超车 ----
	bool bOvertakePassed = false;
	bool bOvertakeSignalUsed = false;
	float OvertakeSignalTime = 0.f;
	bool bOvertakeObserved = false;
	bool bOvertakeReturnSignal = false;

	// ---- 掉头 ----
	bool bUTurnEntered = false;
	float UTurnYawRef = 180.f;
	float UTurnDeltaMin = 0.f;
	float UTurnDeltaMax = 0.f;
	bool bUTurnSignalUsed = false;
	bool bUTurnObserved = false;
	bool bUTurnSpeedCharged = false;
	bool bUTurnEvaluated = false;
	bool bUTurnArc1Done = false;
	bool bUTurnStraightDone = false;

	// ---- 加减挡 ----
	int32 GearShiftStage = 0; // 0 未开始 1 已加至4挡 2 完成
	int32 LastGearForShift = 0;
	bool bGearJumpCharged = false;

	// ---- 道路尽头处理 ----
	float RoadEndTimer = 0.f;
	bool bRoadEndCharged = false;

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

	// ---- 场景对象 ----
	UPROPERTY()
	ARoadBuilder* RoadBuilder = nullptr;

	UPROPERTY()
	ATrafficManager* Traffic = nullptr;

	UPROPERTY()
	APedestrian* Pedestrian = nullptr;

	UPROPERTY()
	ATrafficLight* TrafficLightActor = nullptr;

	UPROPERTY()
	ABeeper* Beeper = nullptr;

	UPROPERTY()
	AKeMuSanPawn* Car = nullptr;

	// ---- 路线投影状态 ----
	const FRouteTrack* Track = nullptr;
	float CurS = 0.f;        // 当前里程
	float PrevS = 0.f;       // 上一帧里程
	float CurLat = 0.f;      // 当前横向偏移（正值靠右）
	float CurDistSq = 0.f;   // 离中心线的距离平方
	bool bCurOnReturn = false;
	bool bCurAligned = false;
	bool bProjValid = false;

	// ---- 自动驾驶（-autotest 截图验证用）----
	float AutoSteerSmooth = 0.f;

	// ---- 内部工具 ----
	void SetPhase(EExamPhase NewPhase);
	void SetPrompt(const FString& Text);
	void AddDeduction(int32 Points, const FString& Reason);
	void FailExam(const FString& Reason);
	void FinishExam();
	void ApplyPresentationDefaults(const TCHAR* Context);
	void StartPresentationGuard();

	void BuildLightPool();
	void BuildLightOrder(int32 Count);
	void SetupZoneStatuses();
	void MarkZone(int32 Index, int32 State);
	void MarkZoneByName(const FString& Name, int32 State);

	void UpdateProjection();
	void SyncTrafficState();
	void UpdatePrep(float DT);
	void UpdateLightTest(float DT);
	void UpdateReady(float DT);
	void UpdateDriving(float DT);
	void UpdatePullOver(float DT);
	void MonitorGeneral(float DT);

	bool IsExamScoring() const { return !bPractice; }
	float CarSpeedKmh() const;
	float CarYawDeg() const;
	bool HeadCheckedRecently(float Seconds) const;

	// 道路驾驶各项目
	void TickStraight(float DT);
	void TickLaneChange(float DT);
	void TickIntersection(float DT);
	void TickSchoolBus(float DT);
	void TickMeeting(float DT);
	void TickOvertake(float DT);
	void TickGearShift(float DT);
	void TickUTurn(float DT);
	void TickPullOverTrigger(float DT);

	// 自动驾驶（仅 -autotest）
	void UpdateAutoDrive(float DT);
};
