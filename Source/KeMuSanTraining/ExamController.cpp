#include "ExamController.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "GameFramework/HUD.h"
#include "HighResScreenshot.h"

#include "KeMuSanPawn.h"
#include "RoadBuilder.h"
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

	bAutoTest = FParse::Param(FCommandLine::Get(), TEXT("autotest"));

	// PIE can inherit editor debug view flags. Apply presentation defaults early,
	// then keep a short guard active while the PIE viewport finishes initializing.
	ApplyPresentationDefaults(TEXT("BeginPlay"));
	StartPresentationGuard();

	// 自动测试：仅在显式传入 -debug-abs-cam 时才切换调试相机，避免篡夺正常驾驶第一/第三人称视点
	if (bAutoTest && FParse::Param(FCommandLine::Get(), TEXT("debug-abs-cam")))
	{
		FTimerHandle AbsCamHandle;
		GetWorld()->GetTimerManager().SetTimer(AbsCamHandle, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			APlayerController* PC = GetWorld()->GetFirstPlayerController();
			if (!PC || !GetWorld())
			{
				return;
			}
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ACameraActor* Cam = GetWorld()->SpawnActor<ACameraActor>(FVector(15000.f, 0.f, 2200.f), FRotator(-15.f, 0.f, 0.f), Params);
			if (Cam)
			{
				if (UCameraComponent* CC = Cast<UCameraComponent>(Cam->FindComponentByClass(UCameraComponent::StaticClass())))
				{
					CC->FieldOfView = 95.f;
				}
				PC->SetViewTargetWithBlend(Cam, 0.f);
				ApplyPresentationDefaults(TEXT("AutotestCamera"));
				StartPresentationGuard();
				UE_LOG(LogTemp, Log, TEXT("[KeMuSan] absolute camera activated"));
			}
		}), 12.f, false);
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	RoadBuilder = GetWorld()->SpawnActor<ARoadBuilder>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
	if (RoadBuilder)
	{
		Track = RoadBuilder->GetTrack();
	}

	Traffic = GetWorld()->SpawnActor<ATrafficManager>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
	if (Traffic && Track)
	{
		Traffic->Setup(Track);
	}

	// 信号灯：位于信号路口停止线右侧（坐标转为UE厘米并贴地）
	TrafficLightActor = GetWorld()->SpawnActor<ATrafficLight>(FVector(161.f, 6.9f, RoadLayout::RoadSurfaceZ) * 100.f, FRotator(0.f, 180.f, 0.f), Params);

	Pedestrian = GetWorld()->SpawnActor<APedestrian>(FVector(0.f, 0.f, -100.f), FRotator::ZeroRotator, Params);
	if (Traffic)
	{
		Traffic->SetPedestrian(Pedestrian);
	}
	Beeper = GetWorld()->SpawnActor<ABeeper>(FVector::ZeroVector, FRotator::ZeroRotator, Params);

	BuildLightPool();
	SetupZoneStatuses();

	// 加载历史存档与学员档案
	CachedSaveGame = UExamSaveGame::LoadOrCreateSaveGame();
	ArchiveStatusText = CachedSaveGame ? TEXT("") : TEXT("历史档案无法读取，原文件已保留。请检查存档或恢复备份。");

	SetPhase(EExamPhase::Menu);
	SetPrompt(TEXT(""));
	if (FParse::Param(FCommandLine::Get(), TEXT("test-route-geometry")))
	{
		FTimerHandle TestTimer;
		GetWorld()->GetTimerManager().SetTimer(TestTimer, this, &AExamController::RunRouteGeometryTest, 0.3f, false);
	}
}

void AExamController::BeginExam(bool bIsExam)
{
	bPractice = !bIsExam;
	PlayMode = bIsExam ? EGamePlayMode::SimulatedExam : EGamePlayMode::GuidedPractice;
	Score = 100;
	Deductions.Reset();
	bFailIssued = false;
	ResultLine.Empty();
	ExamStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	ExamDurationSeconds = 0.f;
	ActualDistanceMeters = 0.f;
	LastAnalysisResult = FExamAnalysisResult();
	ArchiveStatusText.Empty();
	bShowingHistoryPanel = false;
	// The editor can re-apply Wireframe/CSG overlays when PIE enters the exam.
	ApplyPresentationDefaults(TEXT("BeginExam"));
	StartPresentationGuard();

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
	bCrosswalkYieldCharged = false;
	bPedStarted = false;

	bMeetingCarSpawned = false;

	bOvertakePassed = false;
	bOvertakeSignalUsed = false;
	OvertakeSignalTime = 0.f;
	bOvertakeObserved = false;
	bOvertakeReturnSignal = false;

	bUTurnEntered = false;
	UTurnYawRef = 180.f;
	UTurnPreviousYaw = 180.f;
	UTurnAccumulatedYaw = 0.f;
	UTurnDeltaMin = 0.f;
	UTurnDeltaMax = 0.f;
	bUTurnSignalUsed = false;
	bUTurnObserved = false;
	bUTurnSpeedCharged = false;
	bUTurnEvaluated = false;
	bUTurnArc1Done = false;
	bUTurnStraightDone = false;

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
	RoadEndTimer = 0.f;

	// 场景复位
	if (TrafficLightActor)
	{
		TrafficLightActor->ResetLight();
	}
	if (Traffic)
	{
		Traffic->SetActive(true);
		Traffic->ResetTraffic();
	}
	if (Pedestrian)
	{
		Pedestrian->StopCrossing();
	}

	// 考试车复位到起点
	if (!Car)
	{
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			Car = Cast<AKeMuSanPawn>(PC->GetPawn());
		}
	}
	if (Car)
	{
		Car->ResetVehicle(StartPose, FRotator(0.f, 0.f, 0.f));
		LastDistanceLocation = Car->GetActorLocation();
	}

	StartS = S_Start;
	CurS = StartS;
	PrevS = StartS;
	CurLat = LaneWidth * 0.5f;
	bCurOnReturn = false;
	bCurAligned = true;

	SetPhase(EExamPhase::Prep);
	{
		const FString ModeText = bIsExam ? FString(TEXT("考试")) : FString(TEXT("练习"));
		UE_LOG(LogTemp, Log, TEXT("[KeMuSan] BeginExam mode=%s score=%d autotest=%d cmdline=%s"),
			*ModeText, Score, bAutoTest ? 1 : 0, FCommandLine::Get());
	}
	if (Beeper)
	{
		Beeper->PlayDing();
	}
}

void AExamController::OnPauseChanged(bool bInPaused)
{
	bPaused = bInPaused;
}

void AExamController::ApplyPresentationDefaults(const TCHAR* Context)
{
	if (!GetWorld())
	{
		return;
	}

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC)
	{
		return;
	}

	// BSP/Brush flags are independent of Wireframe, so turn off every editor
	// diagnostic overlay that can make the playable view look like CSG lines.
	const TCHAR* Commands[] =
	{
		TEXT("viewmode lit"),
		TEXT("showflag.Game 1"),
		TEXT("showflag.Materials 1"),
		TEXT("showflag.Lighting 1"),
		TEXT("showflag.Wireframe 0"),
		TEXT("showflag.BSP 0"),
		TEXT("showflag.BSPTriangles 0"),
		TEXT("showflag.BSPSplit 0"),
		TEXT("showflag.Brushes 0"),
		TEXT("showflag.BuilderBrush 0"),
		TEXT("showflag.MeshEdges 0"),
		TEXT("showflag.Collision 0"),
		TEXT("showflag.CollisionVisibility 0"),
		TEXT("showflag.CollisionPawn 0"),
		TEXT("showflag.Bounds 0"),
		TEXT("showflag.Navigation 0"),
		TEXT("showflag.GameplayDebug 0"),
		TEXT("showflag.ServerDrawDebug 0"),
		TEXT("showflag.ModeWidgets 0"),
		TEXT("showflag.Pivot 0"),
		TEXT("showflag.HitProxies 0"),
		TEXT("showflag.Splines 0"),
		TEXT("showflag.Selection 0"),
		TEXT("showflag.Editor 0")
	};

	for (const TCHAR* Command : Commands)
	{
		PC->ConsoleCommand(Command, true);
	}

	UE_LOG(LogTemp, Log, TEXT("[KeMuSan] presentation defaults applied: %s"), Context ? Context : TEXT("unknown"));
	UE_LOG(LogTemp, Log, TEXT("[KeMuSanTraffic] presentation_state view=Lit Wireframe=0 BSP=0 BSPTriangles=0 BSPSplit=0 Brushes=0 BuilderBrush=0 Collision=0 Bounds=0"));
	bPresentationDefaultsApplied = true;
}

void AExamController::StartPresentationGuard()
{
	if (!GetWorld())
	{
		return;
	}

	ViewModeFixAttempts = 0;
	GetWorld()->GetTimerManager().ClearTimer(ViewModeFixTimer);
	GetWorld()->GetTimerManager().SetTimer(ViewModeFixTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		ApplyPresentationDefaults(TEXT("Guard"));
		++ViewModeFixAttempts;
		if (ViewModeFixAttempts >= 24 && GetWorld())
		{
			GetWorld()->GetTimerManager().ClearTimer(ViewModeFixTimer);
		}
	}), 0.15f, true, 0.05f);
}

float AExamController::GetProgress01() const
{
	if (!Track || Track->TotalLength() <= 1.f)
	{
		return 0.f;
	}
	return FMath::Clamp(CurS / Track->TotalLength(), 0.f, 1.f);
}

void AExamController::Tick(float DeltaSeconds)
{
	if (!bPresentationDefaultsApplied && GetWorld()->GetFirstPlayerController())
	{
		ApplyPresentationDefaults(TEXT("FirstTick"));
		StartPresentationGuard();
	}
	Super::Tick(DeltaSeconds);

	// 诊断：前几帧与每120帧输出一次状态
	{
		static int32 DiagCount = 0;
		++DiagCount;
		if (DiagCount <= 5 || DiagCount % 120 == 0)
		{
			APlayerController* DiagPC = GetWorld()->GetFirstPlayerController();
			UE_LOG(LogTemp, Log, TEXT("[KeMuSan] tick#%d phase=%d paused=%d pc=%d pawn=%d auto=%d track=%d S=%.1f"),
				DiagCount, static_cast<int32>(Phase), bPaused ? 1 : 0,
				DiagPC ? 1 : 0, (DiagPC && DiagPC->GetPawn()) ? 1 : 0,
				bAutoTest ? 1 : 0, Track ? 1 : 0, CurS);
		}
	}

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

	// 记录真实移动距离；位置测试或重置造成的瞬移不计入训练里程。
	const FVector CurrentLocation = Car->GetActorLocation();
	if (Phase == EExamPhase::Ready || Phase == EExamPhase::Driving || Phase == EExamPhase::PullOver)
	{
		const float MovementMeters = FVector::Dist2D(CurrentLocation, LastDistanceLocation) * 0.01f;
		if (MovementMeters <= FMath::Max(0.f, DeltaSeconds) * 70.f + 0.5f)
		{
			ActualDistanceMeters += MovementMeters;
		}
	}
	LastDistanceLocation = CurrentLocation;
	UpdateProjection();
	SyncTrafficState();

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

	if (bAutoTest && Phase != EExamPhase::Menu && Phase != EExamPhase::Finished)
	{
		UpdateAutoDrive(DeltaSeconds);

		// 自动测试状态日志
		static float LogTimer = 0.f;
		LogTimer += DeltaSeconds;
		if (LogTimer > 2.f)
		{
			LogTimer = 0.f;
			const FVector Loc = Car->GetActorLocation();
			UE_LOG(LogTemp, Log, TEXT("[KeMuSan] auto S=%.1f lat=%.2f spd=%.1f pos=(%.0f,%.0f) ret=%d aligned=%d"),
				CurS, CurLat, Car->GetSpeedKmh(), Loc.X, Loc.Y, bCurOnReturn ? 1 : 0, bCurAligned ? 1 : 0);
		}
	}

	PrevS = CurS;
}

// ---------------------------------------------------------------------------
// 阶段流转
// ---------------------------------------------------------------------------
void AExamController::SetPhase(EExamPhase NewPhase)
{
	Phase = NewPhase;
	if (Car)
	{
		const bool bAllowDrive = (NewPhase == EExamPhase::Ready || NewPhase == EExamPhase::Driving || NewPhase == EExamPhase::PullOver);
		Car->SetDrivable(bAllowDrive);
	}
}

void AExamController::SetPrompt(const FString& Text)
{
	CurrentPrompt = Text;
}

void AExamController::AddDeduction(int32 Points, const FString& Reason)
{
	if (!IsExamScoring() || Points <= 0 || Phase == EExamPhase::Menu || Phase == EExamPhase::Finished)
	{
		return;
	}
	Score = FMath::Max(0, Score - Points);
	FDeduction D;
	D.Points = Points;
	D.Reason = Reason;
	D.TimeSeconds = FMath::Max(0.f, GetWorld()->GetTimeSeconds() - ExamStartTime);
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
	if (Phase == EExamPhase::Finished || Phase == EExamPhase::Menu)
	{
		return;
	}
	if (Beeper)
	{
		Beeper->PlayDoubleDing();
	}
	ResultLine = bPractice ? TEXT("练习完成") : (bFailIssued ? TEXT("不合格") : ((Score >= 90) ? TEXT("合格") : TEXT("不合格")));
	if (bPractice)
	{
		SetPrompt(TEXT("练习完成，已记录训练过程。按 Enter 返回菜单"));
	}
	else if (bFailIssued)
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

	const float CurrentTimeSec = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	ExamDurationSeconds = FMath::Max(1.0f, CurrentTimeSec - ExamStartTime);
	const float DistanceMeters = ActualDistanceMeters;

	// 触发全自动错误分析与教练诊断
	LastAnalysisResult = UExamErrorAnalyzer::AnalyzeExamSession(Score, bFailIssued, Deductions, ExamDurationSeconds, DistanceMeters);

	// 归档保存当场记录
	FExamSessionRecord SessionRecord;
	SessionRecord.SessionId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
	SessionRecord.FormattedTime = FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S"));
	SessionRecord.PlayMode = PlayMode;
	SessionRecord.Transmission = Transmission;
	SessionRecord.FinalScore = Score;
	SessionRecord.bPassed = (!bPractice && !bFailIssued && Score >= 90);
	SessionRecord.ResultSummary = ResultLine;
	SessionRecord.DurationSeconds = ExamDurationSeconds;
	SessionRecord.DistanceMeters = DistanceMeters;
	SessionRecord.Deductions = Deductions;
	SessionRecord.ZoneStatuses = ZoneStatuses;
	SessionRecord.PrimaryWeakness = LastAnalysisResult.PrimaryWeakness;
	SessionRecord.PrimaryWeaknessName = LastAnalysisResult.PrimaryWeaknessName;
	SessionRecord.CoachAdvice = (LastAnalysisResult.CoachAdvices.Num() > 0)
		? LastAnalysisResult.CoachAdvices[0]
		: TEXT("本次未记录扣分。继续练习起步、观察和停车流程。");

	const bool bArchived = UExamSaveGame::RecordAndSaveSession(SessionRecord, CachedSaveGame);
	ArchiveStatusText = bArchived ? TEXT("已保存训练记录，按 F3 查看历史与复盘。")
		: (CachedSaveGame ? CachedSaveGame->LastPersistenceMessage : TEXT("档案无法写入，原文件已保留。请检查存档或恢复备份。"));

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
// 路线投影 / 交通状态同步
// ---------------------------------------------------------------------------
void AExamController::SyncTrafficState()
{
	if (!Traffic || !Car)
	{
		return;
	}
	Traffic->UpdatePlayer(
		Car->GetActorLocation() * 0.01f,
		Car->GetActorRotation().Vector(),
		CurS,
		bCurOnReturn,
		CarSpeedKmh(),
		CarSpeedKmh() < 0.8f,
		GetTrafficLightState(),
		GetTrafficLightRemaining(),
		Pedestrian && Pedestrian->IsWaiting(),
		Pedestrian && Pedestrian->IsCrossing());
}

void AExamController::UpdateProjection()
{
	if (!Car || !Track)
	{
		bProjValid = false;
		return;
	}
	const FVector Heading = Car->GetActorRotation().Vector();
	const FVector CarLocM = Car->GetActorLocation() * 0.01f;
	const FRouteTrack::FProjResult P = Track->Project(CarLocM, Heading);
	{
		static int32 ProjDiag = 0;
		if (++ProjDiag <= 8)
		{
			UE_LOG(LogTemp, Log, TEXT("[KeMuSan] proj pos=(%.1f,%.1f) -> S=%.2f lat=%.2f ret=%d aligned=%d samples=%d"),
				CarLocM.X, CarLocM.Y, P.S, P.Lateral, P.bReturn ? 1 : 0, P.bAligned ? 1 : 0, Track->Num());
		}
	}

	PrevS = CurS;
	CurS = P.S;
	CurLat = P.Lateral;
	CurDistSq = P.DistSq;
	bCurOnReturn = P.bReturn;
	bCurAligned = P.bAligned;
	bProjValid = true;
}

// ---------------------------------------------------------------------------
// 各阶段更新
// ---------------------------------------------------------------------------
void AExamController::UpdatePrep(float DT)
{
	MarkZoneByName(TEXT("上车准备"), 1);
	if (Car && Car->IsHandbrakeOn())
	{
		SetPrompt(TEXT("上车准备：按F系安全带 → 按M观察后视镜 → 保持手刹等待系统就绪"));
	}
	else
	{
		SetPrompt(TEXT("上车准备：按F系安全带 → 按M观察后视镜 → 按空格拉紧手刹"));
	}

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
				MarkZoneByName(TEXT("灯光模拟"), 3);
				MarkZoneByName(TEXT("起步"), 1);
				SetPhase(EExamPhase::Ready);
				SetPrompt(TEXT("自由练习：W油门起步，A/D转向，1挂1挡，空格松手刹"));
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
			StartS = S_Start;
			SetPrompt(TEXT("灯光通过！起步：Q左转灯 → M观察 → 按空格松手刹 → 按1挂1挡 → W油门起步"));
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
	MarkZoneByName(TEXT("起步"), 1);
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
	if (bCurAligned && !bCurOnReturn && CurS < StartS - 0.35f)
	{
		FailExam(TEXT("起步时车辆后溜超过30厘米"));
		return;
	}

	if (bCurAligned && CurS > S_ReadyEnd)
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
	TickStraight(DT);
	TickLaneChange(DT);
	TickIntersection(DT);
	TickSchoolBus(DT);
	TickMeeting(DT);
	TickOvertake(DT);
	TickGearShift(DT);
	TickUTurn(DT);

	// 行人触发（接近人行横道时开始过街，触发点距离斑马线约25m）
	if (!bPedStarted && bCurAligned && !bCurOnReturn && CurS > CrosswalkS - 25.f && Pedestrian)
	{
		bPedStarted = true;
		// 先在路缘等待，由 TrafficManager 按安全距离放行。
		const float PedX = 164.f;
		Pedestrian->PrepareCrossing(FVector(PedX, 9.f, 0.f), FVector(PedX, -9.f, 0.f), 1.8f);
		UE_LOG(LogTemp, Log, TEXT("[KeMuSanTraffic] pedestrian_prepare crosswalk_s=%.1f player_s=%.1f"), CrosswalkS, CurS);
		SyncTrafficState();
		SetPrompt(TEXT("前方有行人候过街：减速停车，确认行人通过后再起步"));
	}

	// 红灯等待时放行横向车流
	if (Traffic)
	{
		const bool bApproaching = bCurAligned && !bCurOnReturn && CurS > 148.f && CurS < 178.f;
		Traffic->NotifyLightRed(bApproaching && GetTrafficLightState() == 0);
	}

	// 掉头完成后触发靠边停车
	TickPullOverTrigger(DT);
}

void AExamController::TickStraight(float DT)
{
	if (!bCurAligned || bCurOnReturn || CurS < StraightStartS || CurS > StraightEndS)
	{
		return;
	}

	MarkZone(3, 1);
	if (!bStraightInit)
	{
		bStraightInit = true;
		StraightRefYaw = CarYawDeg();
		StraightRefLat = CurLat;
		SetPrompt(TEXT("直线行驶：保持车辆直线行驶，方向稳定"));
		if (Beeper)
		{
			Beeper->PlayDing();
		}
	}

	const float Dev = FMath::Abs(FMath::UnwindDegrees(CarYawDeg() - StraightRefYaw));
	const float LatDev = FMath::Abs(CurLat - StraightRefLat);
	if (Dev > 3.2f || LatDev > 0.45f)
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
	else if (CurS > StraightEndS - 1.f)
	{
		MarkZone(3, 2);
	}
}

void AExamController::TickLaneChange(float DT)
{
	if (!bCurAligned || bCurOnReturn || CurS < LaneChangeStartS || CurS > LaneChangeEndS)
	{
		return;
	}

	MarkZone(4, 1);

	if (LaneChangeStage == 0 && CurS < LaneChangeMidS)
	{
		SetPrompt(TEXT("变更车道：开启左转向灯（Q），观察（M）后向左变更车道"));
	}
	else if (LaneChangeStage == 0 && CurS >= LaneChangeMidS)
	{
		SetPrompt(TEXT("变更车道：开启右转向灯（E），观察（M）后向右变更回原车道"));
	}

	if (Car->IsLeftSignalOn() || Car->IsRightSignalOn())
	{
		LaneChangeSignalTime += DT;
	}
	else
	{
		LaneChangeSignalTime = 0.f;
	}

	if (LaneChangeStage == 0 && CurLat < -1.0f)
	{
		// 变到左道（车道宽3.5m，右道中心+1.75，左道中心-1.75，进入左道需偏移<-1.0）
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
	else			if (LaneChangeStage == 1 && CurLat > 1.0f && bLaneChangeCrossed)
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

	if (CurS > LaneChangeEndS - 2.f)
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
	if (!bCurAligned || bCurOnReturn)
	{
		return;
	}

	// 停止线判定（红 / 黄灯）
	if (!bStopLineCrossed && PrevS < StopLineS && CurS >= StopLineS)
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
	if (!bIntersectionEntered && PrevS < IntersectionMinS && CurS >= IntersectionMinS)
	{
		bIntersectionEntered = true;
		if (IsExamScoring() && CarSpeedKmh() > IntersectionLimit)
		{
			AddDeduction(10, TEXT("通过路口未减速慢行"));
		}
		MarkZone(5, 2);
	}

	// 人行横道
	if (CurS >= CrosswalkS - 2.5f && CurS <= CrosswalkS + 2.5f)
	{
		MarkZone(6, 1);
		SetPrompt(TEXT("前方通过人行横道：减速慢行，注意让行行人"));
		if (IsExamScoring())
		{
			if (!bCrosswalkYieldCharged && Pedestrian &&
				(Pedestrian->IsWaiting() || Pedestrian->IsCrossing()) &&
				CarSpeedKmh() > 1.2f)
			{
				bCrosswalkYieldCharged = true;
				AddDeduction(20, TEXT("人行横道有行人优先通行时未停车让行"));
			}
			if (!bCrosswalkSpeedCharged && CarSpeedKmh() > ZoneLimit)
			{
				bCrosswalkSpeedCharged = true;
				AddDeduction(10, TEXT("通过人行横道未减速"));
			}
			// 行人在路面上时必须停车让行；仅经过减速但未停稳也记一次。
			if (!bCrosswalkYieldCharged && Pedestrian && Pedestrian->IsOnRoad(164.f, RoadHalfWidth))
			{
				bCrosswalkYieldCharged = true;
				if (CarSpeedKmh() > 1.2f)
				{
					AddDeduction(20, TEXT("人行横道遇行人未停车让行"));
				}
			}
			// 车辆与行人的实际接触仍然是直接不合格。
			if (!bCrosswalkPedFail && Pedestrian && Pedestrian->IsOnRoad(164.f, RoadHalfWidth) && CarSpeedKmh() > 5.f)
			{
				bCrosswalkPedFail = true;
				FailExam(TEXT("人行横道遇行人未停车让行"));
			}
		}
	}
	else if (CurS > CrosswalkS + 2.5f)
	{
		MarkZone(6, 2);
	}
}

void AExamController::TickSchoolBus(float DT)
{
	if (!bCurAligned || bCurOnReturn)
	{
		return;
	}

	if (CurS >= SchoolStartS && CurS <= SchoolEndS)
	{
		MarkZone(7, 1);
		SetPrompt(TEXT("通过学校区域：减速至30km/h以下，注意观察"));
		if (IsExamScoring() && CarSpeedKmh() > ZoneLimit + 1.f)
		{
			FailExam(TEXT("通过学校区域未减速慢行"));
		}
	}
	else if (CurS > SchoolEndS)
	{
		MarkZone(7, 2);
	}

	if (CurS >= BusStartS && CurS <= BusEndS)
	{
		MarkZone(8, 1);
		SetPrompt(TEXT("通过公交车站：减速至30km/h以下，注意观察"));
		if (IsExamScoring() && CarSpeedKmh() > ZoneLimit + 1.f)
		{
			FailExam(TEXT("通过公交车站未减速慢行"));
		}
	}
	else if (CurS > BusEndS)
	{
		MarkZone(8, 2);
	}
}

void AExamController::TickMeeting(float DT)
{
	if (!bCurAligned || bCurOnReturn || CurS < MeetingStartS || CurS > MeetingEndS + 6.f)
	{
		return;
	}

	MarkZone(9, 1);
	SetPrompt(TEXT("会车：前方对向来车，请靠右行驶并注意横向安全距离"));

	// 进入会车区生成对向来车
	if (!bMeetingCarSpawned && Traffic && CurS > MeetingStartS + 4.f)
	{
		bMeetingCarSpawned = true;
		Traffic->SpawnMeetingCar(CurS);
	}

	if (CurS > MeetingEndS + 4.f)
	{
		MarkZone(9, 2);
	}
}

void AExamController::TickOvertake(float DT)
{
	if (!bCurAligned || bCurOnReturn || CurS < OvertakeStartS || CurS > OvertakeEndS)
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

	// 生成前方低速车
	if (Traffic && !Traffic->IsSlowCarActive() && !bOvertakePassed)
	{
		Traffic->SpawnSlowCar(CurS);
	}

	if (Traffic && !bOvertakePassed && Traffic->IsSlowCarActive())
	{
		const float SlowS = Traffic->GetSlowCarS();
		if (SlowS > 0.f && CurS > SlowS + 4.f)
		{
			bOvertakePassed = true;
			if (IsExamScoring())
			{
			if (CurLat > -0.7f)
			{
				FailExam(TEXT("从右侧超车"));
				return;
			}
			if (!bOvertakeSignalUsed)
				{
					AddDeduction(10, TEXT("超车未开启左转向灯"));
				}
				if (!bOvertakeObserved)
				{
					AddDeduction(10, TEXT("超车前未观察后方交通情况"));
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
		if (bOvertakeReturnSignal && CurLat > -0.2f)
		{
			MarkZone(10, 2);
		}
	}

	if (CurS > OvertakeEndS - 1.f)
	{
		if (IsExamScoring() && ZoneStatuses.IsValidIndex(10) && ZoneStatuses[10].State < 2)
		{
			if (!bOvertakePassed)
			{
				AddDeduction(10, TEXT("未按规定完成超车"));
			}
			else if (!bOvertakeReturnSignal)
			{
				AddDeduction(10, TEXT("超车后未开启右转向灯"));
			}
			if (bOvertakePassed && CurLat < -0.2f)
			{
				AddDeduction(10, TEXT("超车后未驶回原车道"));
			}
		}
		MarkZone(10, 2);
	}
}

void AExamController::TickGearShift(float DT)
{
	if (!bCurAligned || bCurOnReturn || CurS < GearStartS || CurS > GearEndS)
	{
		return;
	}

	MarkZone(11, 1);

	const int32 GearIdx = static_cast<int32>(Car->GetGear());

	// 越级换挡检测（使用挡位号：G1=2,G2=3,G3=4,G4=5,G5=6）
	if (GearIdx != LastGearForShift && IsExamScoring() && !bGearJumpCharged && LastGearForShift >= 2)
	{
		const int32 Jump = FMath::Abs(GearIdx - LastGearForShift);
		if (Jump >= 2)
		{
			bGearJumpCharged = true;
			AddDeduction(10, TEXT("越级换挡"));
		}
	}
	LastGearForShift = GearIdx;

	if (GearShiftStage == 0)
	{
		SetPrompt(TEXT("加减挡操作：逐级加挡至4挡"));
		// Skip for auto transmission
		if (IsAutoTransmission())
		{
			GearShiftStage = 2;
			MarkZone(11, 2);
			SetPrompt(TEXT("加减挡操作完成（自动挡）"));
		}
		else if (GearIdx >= static_cast<int32>(EGear::G4)) // 4挡（idx=5）
		{
			GearShiftStage = 1;
			SetPrompt(TEXT("加减挡操作：逐级减挡至2挡"));
		}
	}
	else if (GearShiftStage == 1)
	{
		if (GearIdx <= static_cast<int32>(EGear::G2)) // 2挡（idx=3）
		{
			GearShiftStage = 2;
			MarkZone(11, 2);
			SetPrompt(TEXT("加减挡操作完成"));
		}
	}

	if (CurS > GearEndS - 1.f)
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
	// 掉头入口：提前6米提示，边界与真实几何共用里程。
	if (!bUTurnEntered && bCurAligned && !bCurOnReturn && CurS >= UTurnEntryS - 6.f)
	{
		bUTurnEntered = true;
		UTurnYawRef = CarYawDeg();
		UTurnPreviousYaw = UTurnYawRef;
		UTurnAccumulatedYaw = 0.f;
		UTurnDeltaMin = 0.f;
		UTurnDeltaMax = 0.f;
		SetPrompt(TEXT("掉头：开启左转向灯（Q），观察（M），减速后在掉头区掉头"));
		if (Beeper)
		{
			Beeper->PlayDing();
		}
	}

	if (!bUTurnEntered || bUTurnEvaluated)
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


	if (!bUTurnSpeedCharged && bCurAligned && !bCurOnReturn && CurS < UTurnEntryS + 8.f && CarSpeedKmh() > UTurnLimit + 1.f)
	{
		bUTurnSpeedCharged = true;
		AddDeduction(10, TEXT("掉头前未减速"));
	}

	// 记录转向累计（负值 = 左转）
	const float CurrentYaw = CarYawDeg();
	UTurnAccumulatedYaw += FMath::UnwindDegrees(CurrentYaw - UTurnPreviousYaw);
	UTurnPreviousYaw = CurrentYaw;
	const float Delta = UTurnAccumulatedYaw;
	UTurnDeltaMin = FMath::Min(UTurnDeltaMin, Delta);
	UTurnDeltaMax = FMath::Max(UTurnDeltaMax, Delta);

	auto EvaluateUTurn = [&]()
	{
		bUTurnEvaluated = true;
		if (IsExamScoring())
		{
			if (UTurnDeltaMax > 120.f)
			{
				FailExam(TEXT("未按指定方向掉头（向右掉头）"));
				return;
			}
			if (UTurnDeltaMin > -140.f)
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
		MarkZone(12, 2);
		SetPrompt(TEXT("掉头完成：沿返回车道行驶，准备靠边停车"));
	};
	// 完成必须落在最终回程直道，弧线与中间连接段不能提前触发。
	if (bCurOnReturn && bCurAligned && CurS >= ReturnStartS && FMath::Abs(CurLat) < RoadHalfWidth)
	{
		EvaluateUTurn();
		return;
	}

	// 驶过 U-turn 终点区域仍未掉头
	if (!bUTurnEvaluated && bCurAligned && !bCurOnReturn && CurS > UTurnCompleteS + 40.f)
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
	// 使用 S 坐标判断靠边停车区
	if (bUTurnEvaluated && bCurOnReturn && bCurAligned && CurS >= PullOverEnterS)
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
	// 停车区外继续行驶 -> 未按规定地点停车
	if (IsExamScoring() && !bPullOverStopped && CarSpeedKmh() > 1.f && CurS > PullOverFailS)
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
		if (IsExamScoring())
		{
			if (CurS < PullOverMinS - 2.f || CurS > PullOverMaxS + 2.f)
			{
				FailExam(TEXT("未在规定区域内停车"));
				return;
			}
			// 车身距路缘石距离：gap = 3.7 - 横向位置
			PullOverGap = PullOverGapBase - CurLat;
			if (PullOverGap < -0.02f)
			{
				FailExam(TEXT("靠边停车时车轮压路缘石"));
				return;
			}
			if (PullOverGap > 0.50f)
			{
				FailExam(TEXT("停车后车身距路缘石超过50厘米"));
				return;
			}
			if (PullOverGap > 0.30f)
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
	if (!Car || !Track)
	{
		return;
	}

	if (IsExamScoring())
	{
		const float Spd = CarSpeedKmh();

		// 与机动车、行人或自行车发生碰撞 -> 不合格
		if (Phase == EExamPhase::Driving || Phase == EExamPhase::PullOver)
		{
			const FVector CarLocM = Car->GetActorLocation() * 0.01f;
			if (Traffic && Traffic->HitsPlayer(CarLocM, CarYawDeg()))
			{
				FailExam(TEXT("与机动车发生碰撞"));
				return;
			}
			if (Traffic && Traffic->HitsPedestrian(CarLocM))
			{
				FailExam(TEXT("与过街行人发生碰撞"));
				return;
			}
			if (Traffic && Traffic->HitsBicycle(CarLocM, CarYawDeg()))
			{
				FailExam(TEXT("与非机动车发生碰撞"));
				return;
			}
		}

		// 超速
		if (Spd > GeneralLimit + 0.5f)
		{
			FailExam(TEXT("超过规定时速（60km/h）"));
			return;
		}

		// 骑轧车道中心分界线（变更车道区、转角、掉头区除外）
		const bool bLegalZone =
			(CurS >= LaneChangeStartS && CurS <= LaneChangeEndS + 2.f) ||
			(CurS >= CornerBStartS && CurS <= CornerBEndS) ||
			(CurS >= CornerCStartS && CurS <= CornerCEndS) ||
			(CurS >= UTurnEntryS - 12.f && CurS <= UTurnCompleteS + 20.f) ||
			bCurOnReturn;
		bool bViolate = bCurAligned && !bLegalZone && FMath::Abs(CurLat) < 0.15f;
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

		// 熄火（自动挡不检测）
		if (!IsAutoTransmission())
		{
			if (Car->IsStalled() && !bStallFlagged)
			{
				bStallFlagged = true;
				AddDeduction(10, TEXT("车辆熄火"));
			}
			if (!Car->IsStalled())
			{
				bStallFlagged = false;
			}
		}

		// 驶出路面（离中心线过远或完全脱离路网）
		const bool bOffRoad = CurDistSq > 64.f ||
			(bCurAligned && FMath::Abs(CurLat) > CurbDistance + 1.0f);
		if (bOffRoad)
		{
			FailExam(TEXT("驶出路面"));
			return;
		}

		// 返回段尽头未停车
		if (bCurOnReturn && CurS > RoadEndS)
		{
			RoadEndTimer += DT;
			if (!bRoadEndCharged)
			{
				bRoadEndCharged = true;
				SetPrompt(TEXT("已到达路线尽头！请立即靠边停车，否则判不合格"));
			}
			if (RoadEndTimer > 5.f)
			{
				FailExam(TEXT("未在规定区域内停车"));
				return;
			}
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
// 自动驾驶（仅 -autotest 截图验证使用）
// ---------------------------------------------------------------------------
void AExamController::UpdateAutoDrive(float DT)
{
	if (!Car || !Track)
	{
		return;
	}

	switch (Phase)
	{
	case EExamPhase::Prep:
		if (!Car->IsSeatbeltOn())
		{
			Car->ToggleSeatbelt();
		}
		Car->NotifyHeadCheck();
		break;

	case EExamPhase::LightTest:
	{
		const FLightQuestion* Q = GetCurrentLightQuestion();
		if (Q)
		{
			SubmitLightAnswer(Q->CorrectAnswer);
		}
		break;
	}

	case EExamPhase::Ready:
	case EExamPhase::Driving:
	case EExamPhase::PullOver:
	{
		if (Car->IsHandbrakeOn())
		{
			Car->ToggleHandbrake();
		}
		if (Car->GetGear() == EGear::N)
		{
			Car->SelectGear(static_cast<int32>(EGear::G1));
		}
		if (!Car->IsSeatbeltOn())
		{
			Car->ToggleSeatbelt();
		}

		// 目标：沿主线右侧车道行驶（前视点跟随）
		float TargetS = CurS + 16.f;
		float TargetLat = LaneWidth * 0.5f;
		if (bCurOnReturn)
		{
			TargetS = FMath::Min(CurS + 16.f, Track->TotalLength() - 4.f);
			TargetLat = LaneWidth * 0.5f;
		}

		const FVector TargetPos = Track->LocAtS(TargetS, TargetLat);
		const FVector ToTarget = TargetPos - Car->GetActorLocation() * 0.01f;
		const float TargetYaw = FMath::RadiansToDegrees(FMath::Atan2(ToTarget.Y, ToTarget.X));
		float YawErr = FMath::UnwindDegrees(TargetYaw - CarYawDeg());
		// 横向纠偏
		const float LatErr = TargetLat - CurLat;
		YawErr += FMath::Clamp(LatErr * 8.f, -25.f, 25.f);

		AutoSteerSmooth = FMath::FInterpTo(AutoSteerSmooth, FMath::Clamp(YawErr * 0.05f, -0.65f, 0.65f), DT, 4.f);
		Car->AxisSteer(AutoSteerSmooth);
		const bool bPedestrianYield = Pedestrian &&
			(Pedestrian->IsWaiting() || Pedestrian->IsOnRoad(164.f, RoadHalfWidth)) &&
			!bCurOnReturn && CurS > CrosswalkS - 42.f && CurS < CrosswalkS + 14.f;
		if (bPedestrianYield)
		{
			// 自动测试也遵守“先停车让行，再通过”的规则。
			Car->AxisThrottle(0.f);
			Car->AxisBrake(1.f);
		}
		else
		{
			Car->AxisThrottle(0.55f);
			Car->AxisBrake(FMath::Abs(YawErr) > 55.f ? 0.5f : 0.f);
		}

		// 依速度换挡
		const float Kmh = Car->GetSpeedKmh();
		int32 WantGear;
		// The manual-mode autotest driver must shift below each gear's speed ceiling.
		if (Kmh < 10.f)      WantGear = static_cast<int32>(EGear::G1);
		else if (Kmh < 18.f) WantGear = static_cast<int32>(EGear::G2);
		else if (Kmh < 30.f) WantGear = static_cast<int32>(EGear::G3);
		else if (Kmh < 42.f) WantGear = static_cast<int32>(EGear::G4);
		else                 WantGear = static_cast<int32>(EGear::G5);
		if (static_cast<int32>(Car->GetGear()) != WantGear && Car->GetGear() != EGear::N)
		{
			Car->SelectGear(WantGear);
		}
		break;
	}

	default:
		break;
	}
}

// ---------------------------------------------------------------------------
// 工具
// ---------------------------------------------------------------------------
float AExamController::CarSpeedKmh() const
{
	return Car ? Car->GetSpeedKmh() : 0.f;
}

float AExamController::CarYawDeg() const
{
	return Car ? Car->GetYawDeg() : 0.f;
}

bool AExamController::HeadCheckedRecently(float Seconds) const
{
	if (!Car)
	{
		return false;
	}
	return (GetWorld()->GetTimeSeconds() - Car->GetLastHeadCheckTime()) < Seconds;
}

void AExamController::SetPlayMode(EGamePlayMode InMode)
{
	PlayMode = InMode;
	bPractice = (InMode == EGamePlayMode::GuidedPractice);
}

float AExamController::GetCurrentSpeedLimit() const
{
	if (Phase == EExamPhase::Prep || Phase == EExamPhase::LightTest || Phase == EExamPhase::Ready)
	{
		return RoadLayout::ZoneLimit; // 30
	}
	if (Phase == EExamPhase::PullOver)
	{
		return RoadLayout::PullOverLimit; // 20
	}

	// 掉头区 (UTurnLimit = 25)
	if (CurS >= RoadLayout::UTurnEntryS - 15.f && CurS <= RoadLayout::UTurnCompleteS + 15.f)
	{
		return RoadLayout::UTurnLimit;
	}

	// 学校区域与公交车站 (ZoneLimit = 30)
	if (CurS >= RoadLayout::SchoolStartS - 30.f && CurS <= RoadLayout::SchoolEndS + 10.f)
	{
		return RoadLayout::ZoneLimit;
	}
	if (CurS >= RoadLayout::BusStartS - 25.f && CurS <= RoadLayout::BusEndS + 10.f)
	{
		return RoadLayout::ZoneLimit;
	}

	// 斑马线 / 人行横道 (ZoneLimit = 30)
	if (CurS >= RoadLayout::CrosswalkS - 35.f && CurS <= RoadLayout::CrosswalkS + 15.f)
	{
		return RoadLayout::ZoneLimit;
	}

	// 直行路口 / 信号灯 (IntersectionLimit = 35)
	if (CurS >= RoadLayout::IntersectionMinS - 30.f && CurS <= RoadLayout::IntersectionMaxS + 15.f)
	{
		return RoadLayout::IntersectionLimit;
	}

	// 转角弯道 B、C (ZoneLimit = 30)
	if ((CurS >= RoadLayout::CornerBStartS && CurS <= RoadLayout::CornerBEndS) ||
		(CurS >= RoadLayout::CornerCStartS && CurS <= RoadLayout::CornerCEndS))
	{
		return RoadLayout::ZoneLimit;
	}

	// 一般直线道路最高 60km/h
	return RoadLayout::GeneralLimit;
}

float AExamController::GetCurrentEdgeDistance() const
{
	// 靠边停车基准：直接复用 RoadLayout 共享常量 PullOverGapBase (2.6m)
	float Gap = RoadLayout::PullOverGapBase - CurLat;
	return FMath::Clamp(Gap, -0.2f, 3.5f);
}

FString AExamController::GetCurrentExamItemName() const
{
	switch (Phase)
	{
	case EExamPhase::Menu:      return TEXT("系统准备就绪");
	case EExamPhase::Prep:      return TEXT("上车准备");
	case EExamPhase::LightTest: return TEXT("夜间模拟灯光");
	case EExamPhase::Ready:     return TEXT("起步准备");
	case EExamPhase::PullOver:  return TEXT("靠边停车");
	case EExamPhase::Finished:  return TEXT("考试评判结束");
	default: break;
	}

	if (CurS >= RoadLayout::CrosswalkS - 35.f && CurS <= RoadLayout::CrosswalkS + 15.f)
	{
		return TEXT("通过人行横道");
	}
	if (CurS >= RoadLayout::IntersectionMinS - 30.f && CurS <= RoadLayout::IntersectionMaxS + 15.f)
	{
		return TEXT("直行通过路口");
	}
	if (CurS >= RoadLayout::SchoolStartS - 20.f && CurS <= RoadLayout::SchoolEndS + 10.f)
	{
		return TEXT("通过学校区域");
	}
	if (CurS >= RoadLayout::BusStartS - 20.f && CurS <= RoadLayout::BusEndS + 10.f)
	{
		return TEXT("通过公共汽车站");
	}
	if (CurS >= RoadLayout::LaneChangeStartS && CurS <= RoadLayout::LaneChangeEndS)
	{
		return TEXT("变更车道");
	}
	if (CurS >= RoadLayout::StraightStartS && CurS <= RoadLayout::StraightEndS)
	{
		return TEXT("直线行驶");
	}
	if (CurS >= RoadLayout::MeetingStartS - 10.f && CurS <= RoadLayout::MeetingEndS + 20.f)
	{
		return TEXT("会车");
	}
	if (CurS >= RoadLayout::OvertakeStartS && CurS <= RoadLayout::OvertakeEndS + 20.f)
	{
		return TEXT("超车");
	}
	if (CurS >= RoadLayout::UTurnEntryS - 15.f && CurS <= RoadLayout::UTurnCompleteS + 15.f)
	{
		return TEXT("掉头");
	}
	if (CurS >= RoadLayout::GearStartS && CurS <= RoadLayout::GearEndS)
	{
		return TEXT("加减挡位操作");
	}

	return TEXT("道路安全驾驶");
}

void AExamController::ToggleHistoryPanel()
{
	bShowingHistoryPanel = !bShowingHistoryPanel;
}

void AExamController::RefreshArchive()
{
	// 保存失败的本场仍在缓存中；打开档案时尝试补存，不能用旧记录覆盖它。
	if (CachedSaveGame && !CachedSaveGame->bLastBinarySaveSucceeded && !CachedSaveGame->LastPersistenceMessage.IsEmpty())
	{
		if (!CachedSaveGame->HistorySessions.IsEmpty())
		{
			const FExamSessionRecord Pending = CachedSaveGame->HistorySessions[0];
			UExamSaveGame::RecordAndSaveSession(Pending, CachedSaveGame);
		}
		ArchiveStatusText = CachedSaveGame->LastPersistenceMessage;
		return;
	}
	CachedSaveGame = UExamSaveGame::LoadOrCreateSaveGame();
	if (!CachedSaveGame)
	{
		ArchiveStatusText = TEXT("历史档案无法读取，原文件已保留。请检查存档或恢复备份。");
	}
}

// 独立几何/考官回归：采样与测试姿态不能视作完整道路驾驶。
void AExamController::RunRouteGeometryTest()
{
	int32 Failures = 0;
	auto Check = [&](const TCHAR* Name, bool bOk)
	{
		if (!bOk) ++Failures;
		UE_LOG(LogTemp, Log, TEXT("[KeMuSanRouteTest] %s %s"), Name, bOk ? TEXT("PASS") : TEXT("FAIL"));
	};
	if (!Track || !GetWorld()->GetFirstPlayerController())
	{
		Check(TEXT("track_ready"), false);
		FGenericPlatformMisc::RequestExit(false);
		return;
	}
	FRouteTrack Rebuilt;
	Rebuilt.Build();
	const float FirstLength = Rebuilt.TotalLength();
	Rebuilt.Build();
	Check(TEXT("rebuild_resets_length"), FMath::IsNearlyEqual(FirstLength, Rebuilt.TotalLength(), 0.01f));
	Check(TEXT("computed_mileage"), FMath::IsNearlyEqual(Track->GetMainLength(), UTurnEntryS, 0.05f)
		&& FMath::IsNearlyEqual(Track->TotalLength(), ReturnStartS + FVector::Dist(ReturnStart, ReturnEnd), 0.05f));
	bool bConnected = true;
	bool bTangentsSmooth = true;
	bool bProjectionMatches = true;
	bool bReturnOnlyAfterTurn = true;
	float MaxJoinDistance = 0.f;
	for (int32 i = 0; i < Track->Num(); ++i)
	{
		const FRouteTrack::FSample& Sample = Track->GetSample(i);
		if (i > 0)
		{
			const FRouteTrack::FSample& Previous = Track->GetSample(i - 1);
			const float Distance = FVector::Dist(Sample.Pos, Previous.Pos);
			MaxJoinDistance = FMath::Max(MaxJoinDistance, Distance);
			bConnected &= Distance <= 1.1f && Sample.S > Previous.S;
			bTangentsSmooth &= FVector::DotProduct(Sample.Tangent, Previous.Tangent) >= 0.99f;
		}
		const FRouteTrack::FProjResult Projection = Track->Project(Sample.Pos, Sample.Tangent);
		bProjectionMatches &= FMath::Abs(Projection.S - Sample.S) < 1.1f && Projection.bAligned && Projection.bReturn == Sample.bReturn;
		bReturnOnlyAfterTurn &= Sample.bReturn == (Sample.S >= ReturnStartS - 0.01f);
	}
	Check(TEXT("samples_connected"), bConnected);
	Check(TEXT("tangents_continuous"), bTangentsSmooth);
	Check(TEXT("projection_round_trip"), bProjectionMatches);
	Check(TEXT("return_flag_after_turn"), bReturnOnlyAfterTurn);
	UE_LOG(LogTemp, Log, TEXT("[KeMuSanRouteTest] geometry samples=%d length=%.3f max_join=%.3f"), Track->Num(), Track->TotalLength(), MaxJoinDistance);
	BeginExam(true);
	Check(TEXT("pawn_ready"), Car != nullptr);
	if (Car)
	{
		SetPhase(EExamPhase::Driving);
		Car->ToggleLeftSignal();
		Car->NotifyHeadCheck();
		bool bPremature = false;
		for (float S = UTurnEntryS - 5.f; S <= ReturnStartS + 1.f; S += 0.25f)
		{
			FVector Location = Track->LocAtS(S, 0.f);
			Location.Z = RoadSurfaceZ;
			const FVector Tangent = Track->TangentAtS(S);
			Car->SetTestPose(Location * 100.f, Tangent.Rotation());
			UpdateProjection();
			TickUTurn(0.016f);
			if (CurS < ReturnStartS - 0.01f && bUTurnEvaluated) bPremature = true;
		}
		Check(TEXT("no_premature_turn_completion"), bUTurnEntered && !bPremature);
		Check(TEXT("left_turn_angle_unwrapped"), UTurnDeltaMin < -175.f && UTurnDeltaMax < 5.f);
		Check(TEXT("turn_completes_on_return"), bUTurnEvaluated && bCurOnReturn && Score == 100 && !bFailIssued);
	}
	UE_LOG(LogTemp, Log, TEXT("[KeMuSanRouteTest] complete %s failures=%d scope=geometry_and_exam_fixtures"), Failures == 0 ? TEXT("PASS") : TEXT("FAIL"), Failures);
	// 夹具验收已结束，停止考试判罚，避免拍图等待期间新增无关结果。
	SetPhase(EExamPhase::Menu);
	FString ScreenshotPath;
	if (Failures == 0 && FParse::Value(FCommandLine::Get(), TEXT("route-screenshot="), ScreenshotPath))
	{
		// 静态俯视图展示新道路，不能用于证明真实驾驶完成。
		APlayerController* PC = GetWorld()->GetFirstPlayerController();
		const FVector CameraLocation(40000.f, 37800.f, 5000.f);
		const FVector TargetLocation(38900.f, 33800.f, 0.f);
		FActorSpawnParameters Params;
		ACameraActor* Camera = GetWorld()->SpawnActor<ACameraActor>(CameraLocation, (TargetLocation - CameraLocation).Rotation(), Params);
		if (Camera && PC)
		{
			PC->SetViewTarget(Camera);
			if (PC->GetHUD()) PC->GetHUD()->bShowHUD = false;
			FTimerHandle ScreenshotTimer;
			GetWorld()->GetTimerManager().SetTimer(ScreenshotTimer, FTimerDelegate::CreateWeakLambda(this, [ScreenshotPath]()
			{
				FScreenshotRequest::RequestScreenshot(ScreenshotPath, false, false);
			}), 0.7f, false);
			FTimerHandle ExitTimer;
			GetWorld()->GetTimerManager().SetTimer(ExitTimer, FTimerDelegate::CreateWeakLambda(this, []()
			{
				FGenericPlatformMisc::RequestExit(false);
			}), 2.f, false);
			return;
		}
	}
	FGenericPlatformMisc::RequestExit(false);
}
