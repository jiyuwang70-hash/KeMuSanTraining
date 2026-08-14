#include "ExamController.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

#include "KeMuSanPawn.h"
#include "RoadBuilder.h"
#include "RoadLayout.h"
#include "TrafficActors.h"
#include "BeepSynth.h"

using namespace RoadLayout;

AExamController::AExamController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AExamController::BeginPlay()
{
	Super::BeginPlay();

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	RoadBuilder = GetWorld()->SpawnActor<ARoadBuilder>(RouteOffset, FRotator::ZeroRotator, Params);
	TrafficLightActor = GetWorld()->SpawnActor<ATrafficLight>(RouteOffset + FVector(StopLineX + 0.5f, 6.6f, 0.f), FRotator::ZeroRotator, Params);
	MeetingCar = GetWorld()->SpawnActor<AAICar>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
	SlowCar = GetWorld()->SpawnActor<AAICar>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
	Pedestrian = GetWorld()->SpawnActor<APedestrian>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
	Beeper = GetWorld()->SpawnActor<ABeeper>(FVector::ZeroVector, FRotator::ZeroRotator, Params);

	BuildLightPool();
	SetupZoneStatuses();

	SetPhase(EExamPhase::Menu);
	SetPrompt(TEXT(""));
}

void AExamController::BeginExam(bool bIsExam)
{
	bPractice = !bIsExam;
	Score = 100;
	Deductions.Reset();
	bFailIssued = false;
	ResultLine.Empty();

	for (FZoneStatus& Z : ZoneStatuses)
	{
		Z.State = 0;
	}

	LightOrder.Reset();
	LightIndex = -1;
	LightQuestionNumber = 0;
	LightQuestionTotal = 0;

	bPrepSeatbeltOk = false;
	bPrepObserveOk = false;
	PrepDoneTimer = 0.f;

	bReadySignalOk = false;
	bReadyObserveOk = false;
	bHandbrakeLaunchCharged = false;
	HandbrakeLaunchTimer = 0.f;

	bStraightInit = false;
	StraightBadTime = 0.f;

	LaneChangeStage = 0;
	LaneChangeSignalTime = 0.f;
	bLaneChangeCrossed = false;

	bStopLineCrossed = false;
	bIntersectionEntered = false;
	bCrosswalkPedFail = false;
	bCrosswalkSpeedCharged = false;
	bPedStarted = false;

	bOvertakePassed = false;
	bOvertakeSignalUsed = false;
	OvertakeSignalTime = 0.f;
	bOvertakeObserved = false;
	bOvertakeReturnSignal = false;

	bUTurnEntered = false;
	UTurnDeltaMin = 0.f;
	UTurnDeltaNeg = 0.f;
	bUTurnSignalUsed = false;
	bUTurnObserved = false;
	bUTurnSpeedCharged = false;
	bUTurnEvaluated = false;

	GearShiftStage = 0;
	LastGearForShift = 0;
	bGearJumpCharged = false;

	PullOverSignalTime = 0.f;
	bPullOverObserved = false;
	bPullOverStopped = false;
	PullOverStopTime = 0.f;
	PullOverGap = 999.f;
	bPullOverNeutralOk = false;
	bPullOverHandbrakeOk = false;
	bPullOverDone = false;

	bStallFlagged = false;
	HandbrakeDriveTimer = 0.f;
	CenterlineTime = 0.f;
	SeatbeltOffTime = 0.f;
	bRoadEndCharged = false;

	// 场景复位
	if (TrafficLightActor)
	{
		TrafficLightActor->ResetLight();
	}
	if (MeetingCar)
	{
		MeetingCar->InitRoute(RouteOffset + FVector(330.f, -1.75f, 0.25f), RouteOffset + FVector(235.f, -1.75f, 0.25f), 18.f);
	}
	if (SlowCar)
	{
		SlowCar->InitRoute(RouteOffset + FVector(302.f, 1.75f, 0.25f), RouteOffset + FVector(352.f, 1.75f, 0.25f), 12.f);
	}
	if (Pedestrian)
	{
		Pedestrian->StopCrossing();
	}

	// 考试车复位
	if (!Car)
	{
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			Car = Cast<AKeMuSanPawn>(PC->GetPawn());
		}
	}
	if (Car)
	{
		const FVector StartLoc = RouteOffset + FVector(StartX, RightLaneY, 1.f);
		Car->ResetVehicle(StartLoc, FRotator(0.f, 0.f, 0.f));
	}

	StartCarX = Car ? CarX() : 0.f;
	PrevCarX = StartCarX;
	bPrevXValid = true;

	SetPhase(EExamPhase::Prep);
	UE_LOG(LogTemp, Log, TEXT("[KeMuSan] BeginExam mode=%s score=%d"), bIsExam ? TEXT("考试") : TEXT("练习"), Score);
	if (Beeper)
	{
		Beeper->PlayDing();
	}
}

void AExamController::OnPauseChanged(bool bInPaused)
{
	bPaused = bInPaused;
}

void AExamController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bPaused)
	{
		return;
	}

	if (!Car)
	{
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			Car = Cast<AKeMuSanPawn>(PC->GetPawn());
		}
		if (!Car)
		{
			return;
		}
	}

	switch (Phase)
	{
	case EExamPhase::Prep:
		UpdatePrep(DeltaSeconds);
		break;
	case EExamPhase::LightTest:
		UpdateLightTest(DeltaSeconds);
		break;
	case EExamPhase::Ready:
		UpdateReady(DeltaSeconds);
		break;
	case EExamPhase::Driving:
		UpdateDriving(DeltaSeconds);
		break;
	case EExamPhase::PullOver:
		UpdatePullOver(DeltaSeconds);
		break;
	default:
		break;
	}

	if (Phase == EExamPhase::Ready || Phase == EExamPhase::Driving || Phase == EExamPhase::PullOver)
	{
		MonitorGeneral(DeltaSeconds);
	}

	PrevCarX = CarX();
}

// ---------------------------------------------------------------------------
// 阶段流转
// ---------------------------------------------------------------------------
void AExamController::SetPhase(EExamPhase NewPhase)
{
	Phase = NewPhase;
}

void AExamController::SetPrompt(const FString& Text)
{
	CurrentPrompt = Text;
}

void AExamController::AddDeduction(int32 Points, const FString& Reason)
{
	if (!IsExamScoring())
	{
		return;
	}
	Score = FMath::Max(0, Score - Points);
	FDeduction D;
	D.Points = Points;
	D.Reason = Reason;
	D.TimeSeconds = GetWorld()->GetTimeSeconds();
	Deductions.Add(D);
}

void AExamController::FailExam(const FString& Reason)
{
	if (!IsExamScoring() || bFailIssued)
	{
		return;
	}
	bFailIssued = true;
	AddDeduction(100, Reason);
	if (Beeper)
	{
		Beeper->PlayWarn();
	}
	FinishExam();
}

void AExamController::FinishExam()
{
	if (Phase == EExamPhase::Finished)
	{
		return;
	}
	if (Beeper)
	{
		Beeper->PlayDoubleDing();
	}
	ResultLine = bFailIssued ? TEXT("不合格") : ((Score >= 90) ? TEXT("合格") : TEXT("不合格"));
	if (bFailIssued)
	{
		SetPrompt(TEXT("考试不合格"));
	}
	else if (Score >= 90)
	{
		SetPrompt(TEXT("考试合格！请按 Enter 重新开始"));
	}
	else
	{
		SetPrompt(TEXT("考试不合格（低于90分）请按 Enter 重新开始"));
	}
	SetPhase(EExamPhase::Finished);
}

// ---------------------------------------------------------------------------
// 灯光模拟
// ---------------------------------------------------------------------------
void AExamController::BuildLightPool()
{
	LightPool.Reset();
	LightPool.Add({ TEXT("夜间在没有路灯、照明不良的条件下行驶"), 2 });
	LightPool.Add({ TEXT("夜间通过急弯、坡路、拱桥"), 3 });
	LightPool.Add({ TEXT("夜间通过没有交通信号灯控制的路口"), 3 });
	LightPool.Add({ TEXT("夜间与机动车会车"), 1 });
	LightPool.Add({ TEXT("夜间在道路上发生故障，妨碍交通又难以移动"), 4 });
	LightPool.Add({ TEXT("雾天行驶"), 5 });
	LightPool.Add({ TEXT("夜间超越前方车辆"), 3 });
	LightPool.Add({ TEXT("夜间在窄路、窄桥与非机动车会车"), 1 });
	LightPool.Add({ TEXT("夜间直行通过路口"), 1 });
	LightPool.Add({ TEXT("夜间在照明良好的道路上行驶"), 1 });
	LightPool.Add({ TEXT("夜间路边临时停车"), 4 });
	LightPool.Add({ TEXT("请打开前照灯"), 1 });
}

void AExamController::BuildLightOrder(int32 Count)
{
	LightOrder.Reset();
	for (int32 i = 0; i < LightPool.Num(); ++i)
	{
		LightOrder.Add(i);
	}
	for (int32 i = LightOrder.Num() - 1; i > 0; --i)
	{
		const int32 J = FMath::RandRange(0, i);
		LightOrder.Swap(i, J);
	}
	LightOrder.SetNum(FMath::Min(Count, LightOrder.Num()));
}

void AExamController::SetupZoneStatuses()
{
	ZoneStatuses.Reset();
	ZoneStatuses.Add({ TEXT("上车准备"), 0 });
	ZoneStatuses.Add({ TEXT("灯光模拟"), 0 });
	ZoneStatuses.Add({ TEXT("起步"), 0 });
	ZoneStatuses.Add({ TEXT("直线行驶"), 0 });
	ZoneStatuses.Add({ TEXT("变更车道"), 0 });
	ZoneStatuses.Add({ TEXT("通过路口"), 0 });
	ZoneStatuses.Add({ TEXT("人行横道"), 0 });
	ZoneStatuses.Add({ TEXT("学校区域"), 0 });
	ZoneStatuses.Add({ TEXT("公交车站"), 0 });
	ZoneStatuses.Add({ TEXT("会车"), 0 });
	ZoneStatuses.Add({ TEXT("超车"), 0 });
	ZoneStatuses.Add({ TEXT("加减挡"), 0 });
	ZoneStatuses.Add({ TEXT("掉头"), 0 });
	ZoneStatuses.Add({ TEXT("靠边停车"), 0 });
}

void AExamController::MarkZone(int32 Index, int32 State)
{
	if (ZoneStatuses.IsValidIndex(Index) && ZoneStatuses[Index].State < State)
	{
		ZoneStatuses[Index].State = State;
	}
}

void AExamController::MarkZoneByName(const FString& Name, int32 State)
{
	for (FZoneStatus& Z : ZoneStatuses)
	{
		if (Z.Name == Name && Z.State < State)
		{
			Z.State = State;
		}
	}
}

const FLightQuestion* AExamController::GetCurrentLightQuestion() const
{
	if (LightIndex >= 0 && LightPool.IsValidIndex(LightIndex))
	{
		return &LightPool[LightIndex];
	}
	return nullptr;
}

int32 AExamController::GetTrafficLightState() const
{
	return TrafficLightActor ? TrafficLightActor->GetState() : 2;
}

float AExamController::GetTrafficLightRemaining() const
{
	return TrafficLightActor ? TrafficLightActor->GetRemaining() : 0.f;
}

// ---------------------------------------------------------------------------
// 各阶段更新
// ---------------------------------------------------------------------------
void AExamController::UpdatePrep(float DT)
{
	MarkZoneByName(TEXT("上车准备"), 1);
	SetPrompt(TEXT("上车准备：系好安全带（F）、观察左右后视镜（M）、拉紧手刹（空格）"));

	if (!bPrepSeatbeltOk && Car->IsSeatbeltOn())
	{
		bPrepSeatbeltOk = true;
	}
	if (!bPrepObserveOk && HeadCheckedRecently(10.f))
	{
		bPrepObserveOk = true;
	}

	if (bPrepSeatbeltOk && bPrepObserveOk && Car->IsHandbrakeOn())
	{
		PrepDoneTimer += DT;
		if (PrepDoneTimer > 0.8f)
		{
			MarkZoneByName(TEXT("上车准备"), 2);
			if (!bPractice)
			{
				SetPhase(EExamPhase::LightTest);
				LightIndex = -1;
				BuildLightOrder(8);
				LightQuestionTotal = LightOrder.Num();
				SetPrompt(TEXT("即将开始夜间灯光模拟考试…"));
			}
			else
			{
				SetPhase(EExamPhase::Ready);
				SetPrompt(TEXT("自由练习：请平稳起步"));
			}
		}
	}
	else
	{
		PrepDoneTimer = 0.f;
	}
}

void AExamController::UpdateLightTest(float DT)
{
	if (LightIndex < 0)
	{
		if (LightOrder.Num() == 0)
		{
			MarkZoneByName(TEXT("灯光模拟"), 2);
			SetPhase(EExamPhase::Ready);
			StartCarX = CarX();
			PrevCarX = CarX();
			SetPrompt(TEXT("灯光考试通过！请起步：开左转向灯（Q）、观察（M）、松手刹、挂1挡、平稳起步"));
			if (Beeper)
			{
				Beeper->PlayDoubleDing();
			}
			return;
		}
		LightIndex = LightOrder.Pop();
		LightCountdown = 5.f;
		LightQuestionNumber++;
		if (Beeper)
		{
			Beeper->PlayDing();
		}
		return;
	}

	LightCountdown -= DT;
	if (LightCountdown <= 0.f)
	{
		if (IsExamScoring())
		{
			FailExam(FString::Printf(TEXT("灯光模拟操作错误（第%d题超时）"), LightQuestionNumber));
		}
		else
		{
			LightIndex = -1;
		}
	}
}

void AExamController::SubmitLightAnswer(int32 Answer)
{
	if (Phase != EExamPhase::LightTest || LightIndex < 0 || !LightPool.IsValidIndex(LightIndex))
	{
		return;
	}

	if (Answer == LightPool[LightIndex].CorrectAnswer)
	{
		if (Beeper)
		{
			Beeper->PlayDing();
		}
		LightIndex = -1;
	}
	else
	{
		if (IsExamScoring())
		{
			FailExam(FString::Printf(TEXT("灯光模拟操作错误（第%d题）"), LightQuestionNumber));
		}
		else
		{
			if (Beeper)
			{
				Beeper->PlayWarn();
			}
			LightIndex = -1;
		}
	}
}

void AExamController::UpdateReady(float DT)
{
	if (Car->IsLeftSignalOn())
	{
		bReadySignalOk = true;
	}
	if (HeadCheckedRecently(8.f))
	{
		bReadyObserveOk = true;
	}

	// 未松手刹强行起步
	if (Car->IsHandbrakeOn() && CarSpeedKmh() > 0.5f)
	{
		HandbrakeLaunchTimer += DT;
		if (HandbrakeLaunchTimer > 1.5f && !bHandbrakeLaunchCharged)
		{
			bHandbrakeLaunchCharged = true;
			AddDeduction(10, TEXT("起步未松开驻车制动器"));
		}
	}
	else
	{
		HandbrakeLaunchTimer = FMath::Max(0.f, HandbrakeLaunchTimer - DT);
	}

	// 起步后溜
	if (CarX() < StartCarX - 0.35f)
	{
		FailExam(TEXT("起步时车辆后溜超过30厘米"));
		return;
	}

	if (CarX() > StartCarX + 12.f)
	{
		if (IsExamScoring())
		{
			if (!bReadySignalOk)
			{
				AddDeduction(10, TEXT("起步未开启左转向灯"));
			}
			if (!bReadyObserveOk)
			{
				AddDeduction(10, TEXT("起步未观察左后方交通情况"));
			}
		}
		MarkZoneByName(TEXT("起步"), 2);
		SetPhase(EExamPhase::Driving);
		SetPrompt(TEXT("起步完成，进入考试路段"));
		if (Beeper)
		{
			Beeper->PlayDing();
		}
	}
}

void AExamController::UpdateDriving(float DT)
{
	const float X = CarX();
	const bool HeadingMinus = IsHeadingMinusX();

	TickStraight(DT);
	TickLaneChange(DT);
	TickIntersection(DT);
	TickSchoolBus(DT);

	// 会车（仅提示）
	if (!HeadingMinus && X >= MeetingStartX && X <= MeetingEndX)
	{
		MarkZone(9, 1);
		SetPrompt(TEXT("会车：前方来车，请靠右行驶，注意观察"));
	}
	else if (!HeadingMinus && X > MeetingEndX)
	{
		MarkZone(9, 2);
	}

	TickOvertake(DT);
	TickGearShift(DT);
	TickUTurn(DT);

	// 行人触发
	if (!bPedStarted && !HeadingMinus && X > 90.f && Pedestrian)
	{
		bPedStarted = true;
		Pedestrian->StartCrossing(RouteOffset + FVector(CrosswalkX, 9.f, 0.f), RouteOffset + FVector(CrosswalkX, -9.f, 0.f), 1.2f);
	}

	// 掉头后触发靠边停车
	TickPullOverTrigger(DT);
}

void AExamController::TickStraight(float DT)
{
	const float X = CarX();
	if (X < StraightStartX || X > StraightEndX || IsHeadingMinusX())
	{
		return;
	}

	MarkZone(3, 1);
	if (!bStraightInit)
	{
		bStraightInit = true;
		StraightRefYaw = CarYawDeg();
		StraightRefY = CarY();
		SetPrompt(TEXT("直线行驶：保持车辆直线行驶，方向稳定"));
		if (Beeper)
		{
			Beeper->PlayDing();
		}
	}

	const float Dev = FMath::Abs(FMath::UnwindDegrees(CarYawDeg() - StraightRefYaw));
	const float YDev = FMath::Abs(CarY() - StraightRefY);
	if (Dev > 3.2f || YDev > 0.45f)
	{
		StraightBadTime += DT;
	}
	else
	{
		StraightBadTime = FMath::Max(0.f, StraightBadTime - DT * 2.f);
	}

	if (IsExamScoring() && StraightBadTime > 1.0f)
	{
		FailExam(TEXT("直线行驶方向控制不稳"));
	}
}

void AExamController::TickLaneChange(float DT)
{
	const float X = CarX();
	const float Y = CarY();
	if (X < LaneChangeStartX || X > LaneChangeEndX || IsHeadingMinusX())
	{
		return;
	}

	MarkZone(4, 1);

	if (LaneChangeStage == 0 && X < LaneChangeMidX)
	{
		SetPrompt(TEXT("变更车道：开启左转向灯（Q），观察（M）后向左变更车道"));
	}
	else if (LaneChangeStage == 0 && X >= LaneChangeMidX)
	{
		SetPrompt(TEXT("变更车道：开启右转向灯（E），观察（M）后向右变更回原车道"));
	}

	// 转向灯持续计时
	if (Car->IsLeftSignalOn() || Car->IsRightSignalOn())
	{
		LaneChangeSignalTime += DT;
	}
	else
	{
		LaneChangeSignalTime = 0.f;
	}

	if (LaneChangeStage == 0 && Y < -0.2f)
	{
		// 变到左道
		if (IsExamScoring())
		{
			if (!Car->IsLeftSignalOn())
			{
				AddDeduction(10, TEXT("变更车道未开启转向灯"));
			}
			else if (LaneChangeSignalTime < 2.5f)
			{
				AddDeduction(10, TEXT("转向灯开启不足3秒"));
			}
			if (!HeadCheckedRecently(5.f))
			{
				AddDeduction(10, TEXT("变更车道未观察后视镜"));
			}
		}
		LaneChangeStage = 1;
		bLaneChangeCrossed = true;
		LaneChangeSignalTime = 0.f;
	}
	else if (LaneChangeStage == 1 && Y > -0.2f && bLaneChangeCrossed)
	{
		// 变回右道
		if (IsExamScoring())
		{
			if (!Car->IsRightSignalOn())
			{
				AddDeduction(10, TEXT("驶回原车道未开启转向灯"));
			}
			else if (LaneChangeSignalTime < 2.5f)
			{
				AddDeduction(10, TEXT("转向灯开启不足3秒"));
			}
			if (!HeadCheckedRecently(5.f))
			{
				AddDeduction(10, TEXT("变更车道未观察后视镜"));
			}
		}
		LaneChangeStage = 2;
		bLaneChangeCrossed = false;
		LaneChangeSignalTime = 0.f;
	}

	if (X > LaneChangeEndX - 2.f)
	{
		if (IsExamScoring() && LaneChangeStage < 2)
		{
			AddDeduction(10, TEXT("未按要求完成变更车道"));
		}
		MarkZone(4, 2);
	}
}

void AExamController::TickIntersection(float DT)
{
	if (IsHeadingMinusX())
	{
		return;
	}

	// 停止线判定（红 / 黄灯）
	if (!bStopLineCrossed && PrevCarX < StopLineX && CarX() >= StopLineX)
	{
		bStopLineCrossed = true;
		const int32 S = GetTrafficLightState();
		if (IsExamScoring())
		{
			if (S == 0)
			{
				FailExam(TEXT("闯红灯"));
			}
			else if (S == 1)
			{
				AddDeduction(10, TEXT("黄灯抢行"));
			}
		}
	}

	// 进入路口
	if (!bIntersectionEntered && PrevCarX < IntersectionMinX && CarX() >= IntersectionMinX)
	{
		bIntersectionEntered = true;
		if (IsExamScoring() && CarSpeedKmh() > IntersectionLimit)
		{
			AddDeduction(10, TEXT("通过路口未减速慢行"));
		}
		MarkZone(5, 2);
	}

	// 人行横道
	const float X = CarX();
	if (X >= CrosswalkX - 2.5f && X <= CrosswalkX + 2.5f)
	{
		MarkZone(6, 1);
		if (IsExamScoring())
		{
			if (!bCrosswalkSpeedCharged && CarSpeedKmh() > ZoneLimit)
			{
				bCrosswalkSpeedCharged = true;
				AddDeduction(10, TEXT("通过人行横道未减速"));
			}
			if (!bCrosswalkPedFail && Pedestrian && Pedestrian->IsOnRoad(CrosswalkX, RoadHalfWidth) && CarSpeedKmh() > 5.f)
			{
				bCrosswalkPedFail = true;
				FailExam(TEXT("人行横道遇行人未停车让行"));
			}
		}
	}
	else if (X > CrosswalkX + 2.5f)
	{
		MarkZone(6, 2);
	}
}

void AExamController::TickSchoolBus(float DT)
{
	if (IsHeadingMinusX())
	{
		return;
	}
	const float X = CarX();

	if (X >= SchoolStartX && X <= SchoolEndX)
	{
		MarkZone(7, 1);
		SetPrompt(TEXT("通过学校区域：减速至30km/h以下，注意观察"));
		if (IsExamScoring() && CarSpeedKmh() > ZoneLimit + 1.f)
		{
			FailExam(TEXT("通过学校区域未减速慢行"));
		}
	}
	else if (X > SchoolEndX)
	{
		MarkZone(7, 2);
	}

	if (X >= BusStartX && X <= BusEndX)
	{
		MarkZone(8, 1);
		SetPrompt(TEXT("通过公交车站：减速至30km/h以下，注意观察"));
		if (IsExamScoring() && CarSpeedKmh() > ZoneLimit + 1.f)
		{
			FailExam(TEXT("通过公交车站未减速慢行"));
		}
	}
	else if (X > BusEndX)
	{
		MarkZone(8, 2);
	}
}

void AExamController::TickOvertake(float DT)
{
	if (IsHeadingMinusX())
	{
		return;
	}
	const float X = CarX();
	if (X < OvertakeStartX || X > OvertakeEndX)
	{
		return;
	}

	MarkZone(10, 1);
	SetPrompt(TEXT("超车：开启左转向灯（Q），观察（M），从左侧超越前方车辆"));

	if (Car->IsLeftSignalOn())
	{
		OvertakeSignalTime += DT;
		if (OvertakeSignalTime > 0.1f)
		{
			bOvertakeSignalUsed = true;
		}
	}
	if (HeadCheckedRecently(6.f))
	{
		bOvertakeObserved = true;
	}

	if (!bOvertakePassed && SlowCar && SlowCar->IsActive())
	{
		const float SlowX = LocalX(SlowCar->GetCarLocation());
		if (X > SlowX + 4.f)
		{
			bOvertakePassed = true;
			if (IsExamScoring())
			{
				if (CarY() > -0.5f)
				{
					FailExam(TEXT("从右侧超车"));
				}
				else
				{
					if (!bOvertakeSignalUsed)
					{
						AddDeduction(10, TEXT("超车未开启左转向灯"));
					}
					if (!bOvertakeObserved)
					{
						AddDeduction(10, TEXT("超车前未观察后方交通情况"));
					}
				}
			}
			SetPrompt(TEXT("超车完成：开启右转向灯（E），驶回原车道"));
		}
	}

	if (bOvertakePassed)
	{
		if (Car->IsRightSignalOn())
		{
			bOvertakeReturnSignal = true;
		}
		if (bOvertakeReturnSignal && CarY() > -0.2f)
		{
			MarkZone(10, 2);
		}
	}
	if (X > OvertakeEndX - 1.f && ZoneStatuses.IsValidIndex(10) && ZoneStatuses[10].State < 2)
	{
		if (IsExamScoring())
		{
			if (!bOvertakePassed)
			{
				AddDeduction(10, TEXT("未按规定完成超车"));
			}
			else if (!bOvertakeReturnSignal)
			{
				AddDeduction(10, TEXT("超车后未开启右转向灯"));
			}
			if (bOvertakePassed && CarY() < -0.2f)
			{
				AddDeduction(10, TEXT("超车后未驶回原车道"));
			}
		}
		MarkZone(10, 2);
	}
}

void AExamController::TickGearShift(float DT)
{
	if (IsHeadingMinusX())
	{
		return;
	}
	const float X = CarX();
	if (X < GearStartX || X > GearEndX)
	{
		return;
	}

	MarkZone(11, 1);

	const int32 GearIdx = static_cast<int32>(Car->GetGear());

	// 越级换挡检测
	if (GearIdx != LastGearForShift && IsExamScoring() && !bGearJumpCharged)
	{
		const bool bJumpUp = (LastGearForShift >= 2 && LastGearForShift <= 3 && GearIdx >= 5);   // 2/3 -> 4/5
		const bool bJumpDown = (LastGearForShift >= 5 && GearIdx <= 3);                           // 4/5 -> 1/2
		if (bJumpUp || bJumpDown)
		{
			bGearJumpCharged = true;
			AddDeduction(10, TEXT("越级换挡"));
		}
	}
	LastGearForShift = GearIdx;

	if (GearShiftStage == 0)
	{
		SetPrompt(TEXT("加减挡操作：逐级加挡至4挡"));
		if (GearIdx >= 5) // 4挡
		{
			GearShiftStage = 1;
			SetPrompt(TEXT("加减挡操作：逐级减挡至2挡"));
		}
	}
	else if (GearShiftStage == 1)
	{
		if (GearIdx <= 3) // 2挡
		{
			GearShiftStage = 2;
			MarkZone(11, 2);
			SetPrompt(TEXT("加减挡完成"));
		}
	}

	if (X > GearEndX - 1.f)
	{
		if (IsExamScoring() && GearShiftStage < 2)
		{
			AddDeduction(10, TEXT("未按指令完成加减挡操作"));
		}
		MarkZone(11, 2);
	}
}

void AExamController::TickUTurn(float DT)
{
	const float X = CarX();
	const bool HeadingMinus = IsHeadingMinusX();

	if (!bUTurnEntered && X >= UTurnStartX && !HeadingMinus)
	{
		bUTurnEntered = true;
		UTurnYawRef = CarYawDeg();
		UTurnDeltaMin = 0.f;
		UTurnDeltaNeg = 0.f;
		SetPrompt(TEXT("掉头：开启左转向灯（Q），观察（M），减速后在掉头区掉头"));
		if (Beeper)
		{
			Beeper->PlayDing();
		}
	}

	if (!bUTurnEntered)
	{
		return;
	}

	MarkZone(12, 1);

	if (Car->IsLeftSignalOn())
	{
		bUTurnSignalUsed = true;
	}
	if (HeadCheckedRecently(6.f))
	{
		bUTurnObserved = true;
	}

	if (X <= UTurnStartX + 4.f && !bUTurnSpeedCharged && CarSpeedKmh() > ZoneLimit + 1.f)
	{
		bUTurnSpeedCharged = true;
		AddDeduction(10, TEXT("掉头前未减速"));
	}

	const float Delta = FMath::UnwindDegrees(CarYawDeg() - UTurnYawRef);
	if (Delta > 0.f)
	{
		UTurnDeltaMin = FMath::Max(UTurnDeltaMin, Delta);
	}
	else
	{
		UTurnDeltaNeg = FMath::Min(UTurnDeltaNeg, Delta);
	}

	if (!bUTurnEvaluated && HeadingMinus && (FMath::Abs(Delta) > 150.f || X < UTurnStartX - 2.f))
	{
		bUTurnEvaluated = true;
		if (IsExamScoring())
		{
			if (UTurnDeltaNeg < -120.f)
			{
				FailExam(TEXT("未按指定方向掉头"));
			}
			else
			{
				if (UTurnDeltaMin < 140.f)
				{
					AddDeduction(10, TEXT("未按规定完成掉头"));
				}
				if (!bUTurnSignalUsed)
				{
					AddDeduction(10, TEXT("掉头未开启左转向灯"));
				}
				if (!bUTurnObserved)
				{
					AddDeduction(10, TEXT("掉头未观察后方交通情况"));
				}
			}
		}
		MarkZone(12, 2);
		SetPrompt(TEXT("掉头完成：沿右侧车道行驶"));
	}

	// 越过掉头区仍未掉头
	if (!bUTurnEvaluated && !HeadingMinus && X > UTurnEndX + 10.f)
	{
		bUTurnEvaluated = true;
		AddDeduction(10, TEXT("未在掉头区完成掉头"));
		MarkZone(12, 2);
	}
}

void AExamController::TickPullOverTrigger(float DT)
{
	if (Phase != EExamPhase::Driving)
	{
		return;
	}
	if (IsHeadingMinusX() && CarX() < PullOverStartX)
	{
		SetPhase(EExamPhase::PullOver);
		SetPrompt(TEXT("靠边停车：开启右转向灯（E），观察（M），减速后靠右停车"));
		MarkZone(13, 1);
		if (Beeper)
		{
			Beeper->PlayDing();
		}
	}
}

void AExamController::UpdatePullOver(float DT)
{
	const float X = CarX();

	// 停车区外继续行驶 -> 未按规定地点停车
	if (IsExamScoring() && X < PullOverEndX && !bPullOverStopped && CarSpeedKmh() > 1.f)
	{
		FailExam(TEXT("未在规定区域内停车"));
		return;
	}

	if (Car->IsRightSignalOn())
	{
		PullOverSignalTime += DT;
	}
	if (HeadCheckedRecently(8.f))
	{
		bPullOverObserved = true;
	}

	// 停车判定
	if (CarSpeedKmh() < 0.6f)
	{
		PullOverStopTime += DT;
	}
	else
	{
		PullOverStopTime = 0.f;
	}

	if (!bPullOverStopped && PullOverStopTime > 0.9f)
	{
		bPullOverStopped = true;
		if (IsExamScoring() && X < PullOverEndX - 1.f)
		{
			FailExam(TEXT("未在规定区域内停车"));
			return;
		}
		PullOverGap = (CarY() - CarHalfWidth) - CurbYHeadingMinusX;
		if (IsExamScoring())
		{
			if (PullOverGap < -0.02f)
			{
				FailExam(TEXT("靠边停车时车轮压路缘石"));
				return;
			}
			else if (PullOverGap > 0.50f)
			{
				FailExam(TEXT("停车后车身距路缘石超过50厘米"));
				return;
			}
			else if (PullOverGap > 0.30f)
			{
				AddDeduction(10, TEXT("停车后车身距路缘石超过30厘米"));
			}
		}
		SetPrompt(TEXT("已停车：请挂空挡（N）、拉紧手刹（空格）"));
	}

	if (bPullOverStopped && !bPullOverDone)
	{
		if (Car->GetGear() == EGear::N)
		{
			bPullOverNeutralOk = true;
		}
		if (Car->IsHandbrakeOn())
		{
			bPullOverHandbrakeOk = true;
		}
		if (bPullOverNeutralOk && bPullOverHandbrakeOk)
		{
			bPullOverDone = true;
			if (IsExamScoring())
			{
				if (PullOverSignalTime < 2.5f)
				{
					AddDeduction(10, TEXT("靠边停车未提前开启右转向灯"));
				}
				if (!bPullOverObserved)
				{
					AddDeduction(10, TEXT("靠边停车未观察右侧交通情况"));
				}
			}
			MarkZoneByName(TEXT("靠边停车"), 2);
			FinishExam();
		}
	}
}

void AExamController::MonitorGeneral(float DT)
{
	if (!Car)
	{
		return;
	}

	if (IsExamScoring())
	{
		const float Spd = CarSpeedKmh();
		const float X = CarX();
		const float Y = CarY();
		const bool HeadingMinus = IsHeadingMinusX();

		// 超速
		if (Spd > GeneralLimit + 0.5f)
		{
			FailExam(TEXT("超过规定时速（60km/h）"));
			return;
		}

		// 骑轧车道中心分界线（变更车道区与掉头区除外）
		const bool bLegalZone =
			(X >= LaneChangeStartX && X <= LaneChangeEndX + 2.f) ||
			(X >= UTurnStartX - 4.f && X <= UTurnEndX + 4.f);
		bool bViolate = false;
		if (!bLegalZone)
		{
			bViolate = HeadingMinus ? (Y > -0.15f) : (Y < 0.15f);
		}
		if (bViolate)
		{
			CenterlineTime += DT;
		}
		else
		{
			CenterlineTime = FMath::Max(0.f, CenterlineTime - DT);
		}
		if (CenterlineTime > 0.8f)
		{
			FailExam(TEXT("骑轧车道中心分界线"));
			return;
		}

		// 安全带
		if (!Car->IsSeatbeltOn())
		{
			SeatbeltOffTime += DT;
			if (SeatbeltOffTime > 1.0f)
			{
				FailExam(TEXT("未系安全带"));
				return;
			}
		}
		else
		{
			SeatbeltOffTime = 0.f;
		}

		// 手刹行驶
		if (Car->IsHandbrakeOn() && Spd > 3.f)
		{
			HandbrakeDriveTimer += DT;
			if (HandbrakeDriveTimer > 2.f)
			{
				AddDeduction(10, TEXT("行驶中未松开驻车制动器"));
				HandbrakeDriveTimer = -10.f;
			}
		}
		else
		{
			HandbrakeDriveTimer = FMath::Min(0.f, HandbrakeDriveTimer + DT);
		}

		// 熄火
		if (Car->IsStalled() && !bStallFlagged)
		{
			bStallFlagged = true;
			AddDeduction(10, TEXT("车辆熄火"));
		}
		if (!Car->IsStalled())
		{
			bStallFlagged = false;
		}

		// 驶出路面
		if (FMath::Abs(Y) > 8.5f)
		{
			FailExam(TEXT("驶出路面"));
			return;
		}

		// 越过路线终点（未掉头）
		if (!HeadingMinus && X > RoadEndX - 8.f)
		{
			if (!bRoadEndCharged)
			{
				bRoadEndCharged = true;
				AddDeduction(10, TEXT("未在掉头区掉头，驶向路线终点"));
			}
			Car->ForceStop();
			SetPrompt(TEXT("请在掉头区掉头！挂倒挡（R）或掉头返回"));
		}
	}

	// 提示音驱动
	if (Beeper)
	{
		Beeper->SetHorn(Car->IsHornHeld());
		Beeper->SetIndicator(Car->IsLeftSignalOn() || Car->IsRightSignalOn() || Car->IsHazardOn());
		Beeper->SetEngineRpm(Car->GetEngineRpm());
	}
}

// ---------------------------------------------------------------------------
// 工具
// ---------------------------------------------------------------------------
float AExamController::CarX() const
{
	return Car ? Car->GetActorLocation().X - RouteOffset.X : 0.f;
}

float AExamController::CarY() const
{
	return Car ? Car->GetActorLocation().Y - RouteOffset.Y : 0.f;
}

float AExamController::CarYawDeg() const
{
	return Car ? Car->GetYawDeg() : 0.f;
}

float AExamController::CarSpeedKmh() const
{
	return Car ? Car->GetSpeedKmh() : 0.f;
}

bool AExamController::IsHeadingMinusX() const
{
	return FMath::Abs(FMath::UnwindDegrees(CarYawDeg())) > 90.f;
}

bool AExamController::HeadCheckedRecently(float Seconds) const
{
	if (!Car)
	{
		return false;
	}
	return (GetWorld()->GetTimeSeconds() - Car->GetLastHeadCheckTime()) < Seconds;
}

float AExamController::LocalX(const FVector& WorldLoc) const
{
	return WorldLoc.X - RouteOffset.X;
}
