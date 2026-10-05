#include "KeMuSanPlayerController.h"

#include "Components/InputComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerInput.h"
#include "Misc/CommandLine.h"
#include "GenericPlatform/GenericPlatformMisc.h"

#include "KeMuSanGameMode.h"
#include "KeMuSanPawn.h"
#include "ExamController.h"

AKeMuSanPlayerController::AKeMuSanPlayerController()
{
	bShowMouseCursor = false;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
}

void AKeMuSanPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (FParse::Param(FCommandLine::Get(), TEXT("test-input-chain")))
	{
		bInputChainTesting = true;
		InputChainTimer = 0.f;
		InputChainStep = 0;
		InputChainSubStep = 0;
		InputChainFailures = 0;
		UE_LOG(LogTemp, Log, TEXT("[KeMuSanInputChain] started automated input chain verification via native PlayerInput/InputKey"));
	}

	FString ExamMode;
	if (FParse::Value(FCommandLine::Get(), TEXT("test-exam-start="), ExamMode))
	{
		bExamStartTesting = true;
		ExamStartTarget = ExamMode.ToLower();
		ExamStartStep = 0;
		ExamStartTimer = 0.f;
		UE_LOG(LogTemp, Log, TEXT("[KeMuSanExamStart] started exam start verification for target: %s"), *ExamStartTarget);
	}
}

void AKeMuSanPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bInputChainTesting)
	{
		TickInputChainTest(DeltaSeconds);
	}
	if (bExamStartTesting)
	{
		TickExamStartTest(DeltaSeconds);
	}
}

void AKeMuSanPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (!InputComponent)
	{
		return;
	}

	// 挡位（数字键 1-5、N、R）
	InputComponent->BindAction(TEXT("Gear1"), IE_Pressed, this, &AKeMuSanPlayerController::HandleGearKey);
	InputComponent->BindAction(TEXT("Gear2"), IE_Pressed, this, &AKeMuSanPlayerController::HandleGearKey);
	InputComponent->BindAction(TEXT("Gear3"), IE_Pressed, this, &AKeMuSanPlayerController::HandleGearKey);
	InputComponent->BindAction(TEXT("Gear4"), IE_Pressed, this, &AKeMuSanPlayerController::HandleGearKey);
	InputComponent->BindAction(TEXT("Gear5"), IE_Pressed, this, &AKeMuSanPlayerController::HandleGearKey);
	InputComponent->BindAction(TEXT("GearN"), IE_Pressed, this, &AKeMuSanPlayerController::HandleGearKey);
	InputComponent->BindAction(TEXT("GearR"), IE_Pressed, this, &AKeMuSanPlayerController::HandleGearKey);

	// 灯光模拟答案（与挡位共用数字键，按阶段分流）
	InputComponent->BindAction(TEXT("Answer1"), IE_Pressed, this, &AKeMuSanPlayerController::HandleAnswerKey);
	InputComponent->BindAction(TEXT("Answer2"), IE_Pressed, this, &AKeMuSanPlayerController::HandleAnswerKey);
	InputComponent->BindAction(TEXT("Answer3"), IE_Pressed, this, &AKeMuSanPlayerController::HandleAnswerKey);
	InputComponent->BindAction(TEXT("Answer4"), IE_Pressed, this, &AKeMuSanPlayerController::HandleAnswerKey);
	InputComponent->BindAction(TEXT("Answer5"), IE_Pressed, this, &AKeMuSanPlayerController::HandleAnswerKey);

	// 全局按键（单次绑定，允许在暂停时执行）
	FInputActionBinding& ConfirmBinding = InputComponent->BindAction(TEXT("Confirm"), IE_Pressed, this, &AKeMuSanPlayerController::ConfirmPressed);
	ConfirmBinding.bExecuteWhenPaused = true;

	FInputActionBinding& PauseBinding = InputComponent->BindAction(TEXT("Pause"), IE_Pressed, this, &AKeMuSanPlayerController::PausePressed);
	PauseBinding.bExecuteWhenPaused = true;

	InputComponent->BindAction(TEXT("FreePractice"), IE_Pressed, this, &AKeMuSanPlayerController::FreePracticePressed);
	InputComponent->BindAction(TEXT("ManualExam"), IE_Pressed, this, &AKeMuSanPlayerController::ManualExamPressed);
	InputComponent->BindAction(TEXT("AutoExam"), IE_Pressed, this, &AKeMuSanPlayerController::AutoExamPressed);

	// 调镜与视角按键
	InputComponent->BindAction(TEXT("CycleGearAuto"), IE_Pressed, this, &AKeMuSanPlayerController::CycleGearOrMirror);
	InputComponent->BindAction(TEXT("ToggleCamera"), IE_Pressed, this, &AKeMuSanPlayerController::ToggleCameraPressed);
	InputComponent->BindAction(TEXT("ToggleMirrorMode"), IE_Pressed, this, &AKeMuSanPlayerController::ToggleMirrorModePressed);
	InputComponent->BindAction(TEXT("MirrorUp"), IE_Pressed, this, &AKeMuSanPlayerController::MirrorUp);
	InputComponent->BindAction(TEXT("MirrorDown"), IE_Pressed, this, &AKeMuSanPlayerController::MirrorDown);
	InputComponent->BindAction(TEXT("MirrorLeft"), IE_Pressed, this, &AKeMuSanPlayerController::MirrorLeft);
	InputComponent->BindAction(TEXT("MirrorRight"), IE_Pressed, this, &AKeMuSanPlayerController::MirrorRight);
}

void AKeMuSanPlayerController::HandleGearKey(FKey Key)
{
	int32 Idx = -1;
	if (Key == EKeys::One) { Idx = 2; }
	else if (Key == EKeys::Two) { Idx = 3; }
	else if (Key == EKeys::Three) { Idx = 4; }
	else if (Key == EKeys::Four) { Idx = 5; }
	else if (Key == EKeys::Five) { Idx = 6; }
	else if (Key == EKeys::N) { Idx = 0; }
	else if (Key == EKeys::R) { Idx = 1; }
	if (Idx >= 0)
	{
		GearKey(Idx);
	}
}

void AKeMuSanPlayerController::HandleAnswerKey(FKey Key)
{
	int32 Ans = 0;
	if (Key == EKeys::One) { Ans = 1; }
	else if (Key == EKeys::Two) { Ans = 2; }
	else if (Key == EKeys::Three) { Ans = 3; }
	else if (Key == EKeys::Four) { Ans = 4; }
	else if (Key == EKeys::Five) { Ans = 5; }
	if (Ans > 0)
	{
		AnswerKey(Ans);
	}
}

void AKeMuSanPlayerController::GearKey(int32 GearIndex)
{
	AExamController* EC = GetExamController();
	if (EC && EC->GetPhase() == EExamPhase::LightTest)
	{
		// 灯光考试阶段：数字键用于答题，不换挡
		return;
	}
	if (AKeMuSanPawn* Car = GetTrainingPawn())
	{
		if (Car->IsMirrorAdjustMode() && GearIndex == 1)
		{
			// 调镜模式下按 R 键：重置后视镜为标准镜位，严禁触发挂倒挡！
			Car->ResetActiveMirrorToStandard();
			UE_LOG(LogTemp, Log, TEXT("[KeMuSanInput] Mirror reset to standard via R key (no reverse gear)"));
			return;
		}
		Car->SelectGear(GearIndex);
		LastGearValue = GearIndex;
	}
}

void AKeMuSanPlayerController::AnswerKey(int32 Answer)
{
	AExamController* EC = GetExamController();
	if (!EC || EC->GetPhase() != EExamPhase::LightTest)
	{
		return;
	}
	EC->SubmitLightAnswer(Answer);
}

void AKeMuSanPlayerController::ConfirmPressed()
{
	AKeMuSanGameMode* GM = GetGameMode();
	if (!GM) return;

	if (GM->bPaused)
	{
		GM->TogglePause();
		return;
	}

	if (!GM->IsGameStarted())
	{
		// 首屏回车：开始推荐的【引导练习】（手动挡）
		GM->StartFreePractice(ETransmissionType::Manual);
		return;
	}

	AExamController* EC = GM->GetExamController();
	if (EC && EC->GetPhase() == EExamPhase::Finished)
	{
		GM->RestartGame();
	}
}

void AKeMuSanPlayerController::ManualExamPressed()
{
	AKeMuSanGameMode* GM = GetGameMode();
	if (GM && !GM->IsGameStarted())
	{
		GM->StartExam(ETransmissionType::Manual);
	}
}

void AKeMuSanPlayerController::AutoExamPressed()
{
	AKeMuSanGameMode* GM = GetGameMode();
	if (GM && !GM->IsGameStarted())
	{
		GM->StartExam(ETransmissionType::Auto);
	}
}

void AKeMuSanPlayerController::CycleGearOrMirror()
{
	if (AKeMuSanPawn* Car = GetTrainingPawn())
	{
		if (Car->IsMirrorAdjustMode())
		{
			// 调镜模式下 Tab 切换当前调节的镜面（左 -> 内 -> 右 -> 左）
			Car->CycleActiveMirror();
			UE_LOG(LogTemp, Log, TEXT("[KeMuSanInput] Tab switched active mirror: %d"), static_cast<int32>(Car->GetActiveMirror()));
			return;
		}

		if (Car->GetTransmissionType() == ETransmissionType::Auto)
		{
			Car->CycleGearAuto();
		}
	}
}

void AKeMuSanPlayerController::ToggleCameraPressed()
{
	if (AKeMuSanPawn* Car = GetTrainingPawn())
	{
		Car->ToggleCameraView();
		UE_LOG(LogTemp, Log, TEXT("[KeMuSanInput] Camera view toggled (Cockpit=%d)"), Car->IsCockpitView() ? 1 : 0);
	}
}

void AKeMuSanPlayerController::ToggleMirrorModePressed()
{
	if (AKeMuSanPawn* Car = GetTrainingPawn())
	{
		Car->ToggleMirrorAdjustMode();
		UE_LOG(LogTemp, Log, TEXT("[KeMuSanInput] Mirror adjust mode toggled (%d)"), Car->IsMirrorAdjustMode() ? 1 : 0);
	}
}

void AKeMuSanPlayerController::MirrorUp()
{
	if (AKeMuSanPawn* Car = GetTrainingPawn())
	{
		if (Car->IsMirrorAdjustMode())
		{
			Car->AdjustActiveMirror(0.5f, 0.0f);
		}
	}
}

void AKeMuSanPlayerController::MirrorDown()
{
	if (AKeMuSanPawn* Car = GetTrainingPawn())
	{
		if (Car->IsMirrorAdjustMode())
		{
			Car->AdjustActiveMirror(-0.5f, 0.0f);
		}
	}
}

void AKeMuSanPlayerController::MirrorLeft()
{
	if (AKeMuSanPawn* Car = GetTrainingPawn())
	{
		if (Car->IsMirrorAdjustMode())
		{
			Car->AdjustActiveMirror(0.0f, -0.5f);
		}
	}
}

void AKeMuSanPlayerController::MirrorRight()
{
	if (AKeMuSanPawn* Car = GetTrainingPawn())
	{
		if (Car->IsMirrorAdjustMode())
		{
			Car->AdjustActiveMirror(0.0f, 0.5f);
		}
	}
}

void AKeMuSanPlayerController::PausePressed()
{
	if (AKeMuSanGameMode* GM = GetGameMode())
	{
		if (GM->IsGameStarted())
		{
			GM->TogglePause();
		}
	}
}

void AKeMuSanPlayerController::FreePracticePressed()
{
	if (AKeMuSanGameMode* GM = GetGameMode())
	{
		if (!GM->IsGameStarted())
		{
			GM->StartFreePractice();
		}
	}
}

AKeMuSanGameMode* AKeMuSanPlayerController::GetGameMode() const
{
	return GetWorld() ? GetWorld()->GetAuthGameMode<AKeMuSanGameMode>() : nullptr;
}

AExamController* AKeMuSanPlayerController::GetExamController() const
{
	AKeMuSanGameMode* GM = GetGameMode();
	return GM ? GM->GetExamController() : nullptr;
}

AKeMuSanPawn* AKeMuSanPlayerController::GetTrainingPawn() const
{
	return Cast<AKeMuSanPawn>(GetPawn());
}

void AKeMuSanPlayerController::TickInputChainTest(float DeltaSeconds)
{
	InputChainTimer += DeltaSeconds;
	const bool bContinuousStep = (InputChainStep == 19 || (InputChainStep == 20 && InputChainSubStep == 2) || InputChainStep == 22 || InputChainStep == 23);
	if (!bContinuousStep && InputChainTimer < 0.10f)
	{
		return;
	}
	InputChainTimer = 0.f;

	AKeMuSanGameMode* GM = GetGameMode();
	AExamController* EC = GetExamController();
	AKeMuSanPawn* Car = GetTrainingPawn();

	switch (InputChainStep)
	{
	case 0: // Step 0: Enter 键启动推荐练习
		if (InputChainSubStep == 0)
		{
			InputKey(FInputKeyParams(EKeys::Enter, IE_Pressed, 1.0));
			InputChainSubStep = 1;
		}
		else if (InputChainSubStep == 1)
		{
			InputKey(FInputKeyParams(EKeys::Enter, IE_Released, 0.0));
			InputChainSubStep = 2;
		}
		else
		{
			const bool bOk = GM && GM->IsGameStarted();
			if (!bOk) { InputChainFailures++; }
			UE_LOG(LogTemp, Log, TEXT("[KeMuSanInputChain] step0_enter_start_game %s"), bOk ? TEXT("PASS") : TEXT("FAIL"));
			InputChainStep = 1;
			InputChainSubStep = 0;
		}
		break;

	case 1: // Step 1: SpaceBar 键松开手刹
		if (InputChainSubStep == 0)
		{
			InputKey(FInputKeyParams(EKeys::SpaceBar, IE_Pressed, 1.0));
			InputChainSubStep = 1;
		}
		else if (InputChainSubStep == 1)
		{
			InputKey(FInputKeyParams(EKeys::SpaceBar, IE_Released, 0.0));
			InputChainSubStep = 2;
		}
		else
		{
			const bool bOk = Car && !Car->IsHandbrakeEngaged();
			if (!bOk) { InputChainFailures++; }
			UE_LOG(LogTemp, Log, TEXT("[KeMuSanInputChain] step1_space_handbrake %s"), bOk ? TEXT("PASS") : TEXT("FAIL"));
			InputChainStep = 2;
			InputChainSubStep = 0;
		}
		break;

	case 2: // Step 2: One 键挂入 1 挡
		if (InputChainSubStep == 0)
		{
			InputKey(FInputKeyParams(EKeys::One, IE_Pressed, 1.0));
			InputChainSubStep = 1;
		}
		else if (InputChainSubStep == 1)
		{
			InputKey(FInputKeyParams(EKeys::One, IE_Released, 0.0));
			InputChainSubStep = 2;
		}
		else
		{
			const bool bOk = Car && (Car->GetGear() == EGear::G1);
			if (!bOk) { InputChainFailures++; }
			UE_LOG(LogTemp, Log, TEXT("[KeMuSanInputChain] step2_one_gear1 %s"), bOk ? TEXT("PASS") : TEXT("FAIL"));
			InputChainStep = 3;
			InputChainSubStep = 0;
		}
		break;

	case 3: // Step 3: W 键油门轴（按压响应与松开归零）
		if (InputChainSubStep == 0)
		{
			InputKey(FInputKeyParams(EKeys::W, IE_Pressed, 1.0));
			InputChainSubStep = 1;
		}
		else if (InputChainSubStep == 1)
		{
			const bool bPressedOk = Car && (Car->GetThrottle() > 0.05f);
			if (!bPressedOk) { InputChainFailures++; }
			UE_LOG(LogTemp, Log, TEXT("[KeMuSanInputChain] step3_w_throttle_pressed %s (Throttle=%.2f)"),
				bPressedOk ? TEXT("PASS") : TEXT("FAIL"), Car ? Car->GetThrottle() : 0.f);
			InputKey(FInputKeyParams(EKeys::W, IE_Released, 0.0));
			InputChainSubStep = 2;
		}
		else
		{
			const bool bReleasedOk = Car && (Car->GetThrottle() <= 0.05f);
			if (!bReleasedOk) { InputChainFailures++; }
			UE_LOG(LogTemp, Log, TEXT("[KeMuSanInputChain] step3_w_throttle_released %s"), bReleasedOk ? TEXT("PASS") : TEXT("FAIL"));
			InputChainStep = 4;
			InputChainSubStep = 0;
		}
		break;

	case 4: // Step 4: A 键转向轴（按压左转与松开回正）
		if (InputChainSubStep == 0)
		{
			InputKey(FInputKeyParams(EKeys::A, IE_Pressed, 1.0));
			InputChainSubStep = 1;
		}
		else if (InputChainSubStep == 1)
		{
			const bool bPressedOk = Car && (Car->GetSteeringInput() < -0.05f);
			if (!bPressedOk) { InputChainFailures++; }
			UE_LOG(LogTemp, Log, TEXT("[KeMuSanInputChain] step4_a_steer_pressed %s (Steer=%.2f)"),
				bPressedOk ? TEXT("PASS") : TEXT("FAIL"), Car ? Car->GetSteeringInput() : 0.f);
			InputKey(FInputKeyParams(EKeys::A, IE_Released, 0.0));
			InputChainSubStep = 2;
		}
		else
		{
			const bool bReleasedOk = Car && (FMath::Abs(Car->GetSteeringInput()) <= 0.05f);
			if (!bReleasedOk) { InputChainFailures++; }
			UE_LOG(LogTemp, Log, TEXT("[KeMuSanInputChain] step4_a_steer_released %s"), bReleasedOk ? TEXT("PASS") : TEXT("FAIL"));
			InputChainStep = 5;
			InputChainSubStep = 0;
		}
		break;

	case 5: // Step 5: Q 键左转向灯
		if (InputChainSubStep == 0)
		{
			InputKey(FInputKeyParams(EKeys::Q, IE_Pressed, 1.0));
			InputChainSubStep = 1;
		}
		else if (InputChainSubStep == 1)
		{
			InputKey(FInputKeyParams(EKeys::Q, IE_Released, 0.0));
			InputChainSubStep = 2;
		}
		else
		{
			const bool bOk = Car && Car->IsLeftSignalOn();
			if (!bOk) { InputChainFailures++; }
			UE_LOG(LogTemp, Log, TEXT("[KeMuSanInputChain] step5_q_left_signal %s"), bOk ? TEXT("PASS") : TEXT("FAIL"));
			InputChainStep = 6;
			InputChainSubStep = 0;
		}
		break;

	case 6: // Step 6: B 键鸣笛（按压鸣响与松开停止）
		if (InputChainSubStep == 0)
		{
			InputKey(FInputKeyParams(EKeys::B, IE_Pressed, 1.0));
			InputChainSubStep = 1;
		}
		else if (InputChainSubStep == 1)
		{
			const bool bHeldOk = Car && Car->IsHornHeld();
			if (!bHeldOk) { InputChainFailures++; }
			UE_LOG(LogTemp, Log, TEXT("[KeMuSanInputChain] step6_b_horn_held %s"), bHeldOk ? TEXT("PASS") : TEXT("FAIL"));
			InputKey(FInputKeyParams(EKeys::B, IE_Released, 0.0));
			InputChainSubStep = 2;
		}
		else
		{
			const bool bReleasedOk = Car && !Car->IsHornHeld();
			if (!bReleasedOk) { InputChainFailures++; }
			UE_LOG(LogTemp, Log, TEXT("[KeMuSanInputChain] step6_b_horn_released %s"), bReleasedOk ? TEXT("PASS") : TEXT("FAIL"));
			InputChainStep = 7;
			InputChainSubStep = 0;
		}
		break;

	case 7: // Step 7: M 键侧头观察
		if (InputChainSubStep == 0)
		{
			InputKey(FInputKeyParams(EKeys::M, IE_Pressed, 1.0));
			InputChainSubStep = 1;
		}
		else if (InputChainSubStep == 1)
		{
			InputKey(FInputKeyParams(EKeys::M, IE_Released, 0.0));
			InputChainSubStep = 2;
		}
		else
		{
			const bool bOk = Car && (Car->GetHeadCheckTimer() > 0.f);
			if (!bOk) { InputChainFailures++; }
			UE_LOG(LogTemp, Log, TEXT("[KeMuSanInputChain] step7_m_head_check %s"), bOk ? TEXT("PASS") : TEXT("FAIL"));
			InputChainStep = 8;
			InputChainSubStep = 0;
		}
		break;

	case 8: // Step 8: T 键开启调镜模式
		if (InputChainSubStep == 0)
		{
			InputKey(FInputKeyParams(EKeys::T, IE_Pressed, 1.0));
			InputChainSubStep = 1;
		}
		else if (InputChainSubStep == 1)
		{
			InputKey(FInputKeyParams(EKeys::T, IE_Released, 0.0));
			InputChainSubStep = 2;
		}
		else
		{
			const bool bOk = Car && Car->IsMirrorAdjustMode();
			if (!bOk) { InputChainFailures++; }
			UE_LOG(LogTemp, Log, TEXT("[KeMuSanInputChain] step8_t_mirror_mode %s"), bOk ? TEXT("PASS") : TEXT("FAIL"));
			InputChainStep = 9;
			InputChainSubStep = 0;
		}
		break;

	case 9: // Step 9: Tab 键切换镜面（从 Left 切到 Interior）
		if (InputChainSubStep == 0)
		{
			InputKey(FInputKeyParams(EKeys::Tab, IE_Pressed, 1.0));
			InputChainSubStep = 1;
		}
		else if (InputChainSubStep == 1)
		{
			InputKey(FInputKeyParams(EKeys::Tab, IE_Released, 0.0));
			InputChainSubStep = 2;
		}
		else
		{
			const bool bOk = Car && (Car->GetActiveMirror() != EMirrorType::Left);
			if (!bOk) { InputChainFailures++; }
			UE_LOG(LogTemp, Log, TEXT("[KeMuSanInputChain] step9_tab_cycle_mirror %s (ActiveMirror=%d)"),
				bOk ? TEXT("PASS") : TEXT("FAIL"), Car ? static_cast<int32>(Car->GetActiveMirror()) : -1);
			InputChainStep = 10;
			InputChainSubStep = 0;
		}
		break;

	case 10: // Step 10: 方向键微调镜面
		if (InputChainSubStep == 0)
		{
			InputKey(FInputKeyParams(EKeys::Up, IE_Pressed, 1.0));
			InputChainSubStep = 1;
		}
		else if (InputChainSubStep == 1)
		{
			InputKey(FInputKeyParams(EKeys::Up, IE_Released, 0.0));
			InputKey(FInputKeyParams(EKeys::Left, IE_Pressed, 1.0));
			InputChainSubStep = 2;
		}
		else if (InputChainSubStep == 2)
		{
			InputKey(FInputKeyParams(EKeys::Left, IE_Released, 0.0));
			InputChainSubStep = 3;
		}
		else
		{
			const EMirrorType CurMirror = Car ? Car->GetActiveMirror() : EMirrorType::Interior;
			const bool bOk = Car && (FMath::Abs(Car->GetMirrorState(CurMirror).Pitch) > 0.01f || FMath::Abs(Car->GetMirrorState(CurMirror).Yaw) > 0.01f);
			if (!bOk) { InputChainFailures++; }
			UE_LOG(LogTemp, Log, TEXT("[KeMuSanInputChain] step10_arrow_keys_adjust %s (Pitch=%.2f, Yaw=%.2f)"),
				bOk ? TEXT("PASS") : TEXT("FAIL"), Car ? Car->GetMirrorState(CurMirror).Pitch : 0.f, Car ? Car->GetMirrorState(CurMirror).Yaw : 0.f);
			InputChainStep = 11;
			InputChainSubStep = 0;
		}
		break;

	case 11: // Step 11: 调镜模式下按 R 键（核心安全机制：镜面重置为标准，严禁挂入倒挡！）
		if (InputChainSubStep == 0)
		{
			InputKey(FInputKeyParams(EKeys::R, IE_Pressed, 1.0));
			InputChainSubStep = 1;
		}
		else if (InputChainSubStep == 1)
		{
			InputKey(FInputKeyParams(EKeys::R, IE_Released, 0.0));
			InputChainSubStep = 2;
		}
		else
		{
			const bool bNotReverse = Car && (Car->GetGear() != EGear::R);
			const bool bStandard = Car && Car->GetMirrorState(Car->GetActiveMirror()).bStandardAdjusted;
			const bool bOk = bNotReverse && bStandard;
			if (!bOk) { InputChainFailures++; }
			UE_LOG(LogTemp, Log, TEXT("[KeMuSanInputChain] step11_r_mirror_reset_no_reverse %s (Gear=%d, Standard=%d)"),
				bOk ? TEXT("PASS") : TEXT("FAIL"), Car ? static_cast<int32>(Car->GetGear()) : -1, bStandard ? 1 : 0);
			InputChainStep = 12;
			InputChainSubStep = 0;
		}
		break;

	case 12: // Step 12: T 键退出调镜模式
		if (InputChainSubStep == 0)
		{
			InputKey(FInputKeyParams(EKeys::T, IE_Pressed, 1.0));
			InputChainSubStep = 1;
		}
		else if (InputChainSubStep == 1)
		{
			InputKey(FInputKeyParams(EKeys::T, IE_Released, 0.0));
			InputChainSubStep = 2;
		}
		else
		{
			const bool bOk = Car && !Car->IsMirrorAdjustMode();
			if (!bOk) { InputChainFailures++; }
			UE_LOG(LogTemp, Log, TEXT("[KeMuSanInputChain] step12_t_exit_mirror_mode %s"), bOk ? TEXT("PASS") : TEXT("FAIL"));
			InputChainStep = 13;
			InputChainSubStep = 0;
		}
		break;

	case 13: // Step 13: V 键切换座舱第一人称视角
		if (InputChainSubStep == 0)
		{
			InputKey(FInputKeyParams(EKeys::V, IE_Pressed, 1.0));
			InputChainSubStep = 1;
		}
		else if (InputChainSubStep == 1)
		{
			InputKey(FInputKeyParams(EKeys::V, IE_Released, 0.0));
			InputChainSubStep = 2;
		}
		else
		{
			const bool bOk = Car && Car->IsCockpitView();
			if (!bOk) { InputChainFailures++; }
			UE_LOG(LogTemp, Log, TEXT("[KeMuSanInputChain] step13_v_toggle_cockpit %s"), bOk ? TEXT("PASS") : TEXT("FAIL"));
			InputChainStep = 14;
			InputChainSubStep = 0;
		}
		break;

	case 14: // Step 14: Escape 键暂停游戏
		if (InputChainSubStep == 0)
		{
			InputKey(FInputKeyParams(EKeys::Escape, IE_Pressed, 1.0));
			InputChainSubStep = 1;
		}
		else if (InputChainSubStep == 1)
		{
			InputKey(FInputKeyParams(EKeys::Escape, IE_Released, 0.0));
			InputChainSubStep = 2;
		}
		else
		{
			const bool bOk = GM && GM->bPaused;
			if (!bOk) { InputChainFailures++; }
			UE_LOG(LogTemp, Log, TEXT("[KeMuSanInputChain] step14_escape_pause %s"), bOk ? TEXT("PASS") : TEXT("FAIL"));
			InputChainStep = 15;
			InputChainSubStep = 0;
		}
		break;

	case 15: // Step 15: 暂停中按 Escape 键恢复运行（验证 bExecuteWhenPaused 生效）
		if (InputChainSubStep == 0)
		{
			InputKey(FInputKeyParams(EKeys::Escape, IE_Pressed, 1.0));
			InputChainSubStep = 1;
		}
		else if (InputChainSubStep == 1)
		{
			InputKey(FInputKeyParams(EKeys::Escape, IE_Released, 0.0));
			InputChainSubStep = 2;
		}
		else
		{
			const bool bOk = GM && !GM->bPaused;
			if (!bOk) { InputChainFailures++; }
			UE_LOG(LogTemp, Log, TEXT("[KeMuSanInputChain] step15_escape_unpause_in_paused_state %s"), bOk ? TEXT("PASS") : TEXT("FAIL"));
			InputChainStep = 16;
			InputChainSubStep = 0;
		}
		break;

	case 16: // Step 16: 保持/拉紧手刹（确认进入标准驻车准备状态）
		if (InputChainSubStep == 0)
		{
			if (Car && Car->IsHandbrakeEngaged())
			{
				UE_LOG(LogTemp, Log, TEXT("[KeMuSanInputChain] step16_ensure_handbrake_engaged PASS (AlreadyEngaged)"));
				InputChainStep = 17;
				InputChainSubStep = 0;
				InputChainJourneyTimer = 0.f;
			}
			else
			{
				InputKey(FInputKeyParams(EKeys::SpaceBar, IE_Pressed, 1.0));
				InputChainSubStep = 1;
			}
		}
		else if (InputChainSubStep == 1)
		{
			InputKey(FInputKeyParams(EKeys::SpaceBar, IE_Released, 0.0));
			InputChainSubStep = 2;
		}
		else
		{
			const bool bOk = Car && Car->IsHandbrakeEngaged();
			if (!bOk) { InputChainFailures++; }
			UE_LOG(LogTemp, Log, TEXT("[KeMuSanInputChain] step16_ensure_handbrake_engaged %s"), bOk ? TEXT("PASS") : TEXT("FAIL"));
			InputChainStep = 17;
			InputChainSubStep = 0;
			InputChainJourneyTimer = 0.f;
		}
		break;

	case 17: // Step 17: F 键系安全带（上车准备核心考点）
		if (InputChainSubStep == 0)
		{
			InputKey(FInputKeyParams(EKeys::F, IE_Pressed, 1.0));
			InputChainSubStep = 1;
		}
		else if (InputChainSubStep == 1)
		{
			InputKey(FInputKeyParams(EKeys::F, IE_Released, 0.0));
			InputChainSubStep = 2;
		}
		else
		{
			const bool bOk = Car && Car->IsSeatbeltOn();
			if (!bOk) { InputChainFailures++; }
			UE_LOG(LogTemp, Log, TEXT("[KeMuSanInputChain] step17_f_seatbelt %s"), bOk ? TEXT("PASS") : TEXT("FAIL"));
			InputChainStep = 18;
			InputChainSubStep = 0;
		}
		break;

	case 18: // Step 18: M 键侧头观察（上车准备核心考点）
		if (InputChainSubStep == 0)
		{
			InputKey(FInputKeyParams(EKeys::M, IE_Pressed, 1.0));
			InputChainSubStep = 1;
		}
		else if (InputChainSubStep == 1)
		{
			InputKey(FInputKeyParams(EKeys::M, IE_Released, 0.0));
			InputChainSubStep = 2;
		}
		else
		{
			const bool bOk = Car && (Car->GetHeadCheckTimer() > 0.f);
			if (!bOk) { InputChainFailures++; }
			UE_LOG(LogTemp, Log, TEXT("[KeMuSanInputChain] step18_m_headcheck %s"), bOk ? TEXT("PASS") : TEXT("FAIL"));
			InputChainStep = 19;
			InputChainSubStep = 0;
			InputChainJourneyTimer = 0.f;
		}
		break;

	case 19: // Step 19: 保持手刹拉紧等待 Prep 转 Ready（EC 累计 0.8s 自动切入 Ready）
		InputChainJourneyTimer += DeltaSeconds;
		if (EC && EC->GetPhase() == EExamPhase::Ready)
		{
			const bool bHandbrakeHeld = Car && Car->IsHandbrakeEngaged();
			const bool bOk = bHandbrakeHeld;
			if (!bOk) { InputChainFailures++; }
			UE_LOG(LogTemp, Log, TEXT("[KeMuSanInputChain] step19_prep_to_ready %s (WaitTime=%.2fs, Handbrake=%d)"),
				bOk ? TEXT("PASS") : TEXT("FAIL"), InputChainJourneyTimer, bHandbrakeHeld ? 1 : 0);
			InputChainStep = 20;
			InputChainSubStep = 0;
			InputChainJourneyTimer = 0.f;
		}
		else if (InputChainJourneyTimer > 3.0f)
		{
			InputChainFailures++;
			UE_LOG(LogTemp, Error, TEXT("[KeMuSanInputChain] step19_prep_to_ready FAIL (Timeout, Phase=%d)"),
				EC ? static_cast<int32>(EC->GetPhase()) : -1);
			InputChainStep = 20;
			InputChainSubStep = 0;
			InputChainJourneyTimer = 0.f;
		}
		break;

	case 20: // Step 20: Q 键左转向灯并等待 >= 3.0 秒（起步前打灯满3秒国标）
		if (InputChainSubStep == 0)
		{
			if (Car && Car->IsLeftSignalOn())
			{
				InputChainSubStep = 2;
				InputChainJourneyTimer = 0.f;
			}
			else
			{
				InputKey(FInputKeyParams(EKeys::Q, IE_Pressed, 1.0));
				InputChainSubStep = 1;
			}
		}
		else if (InputChainSubStep == 1)
		{
			InputKey(FInputKeyParams(EKeys::Q, IE_Released, 0.0));
			InputChainSubStep = 2;
			InputChainJourneyTimer = 0.f;
		}
		else
		{
			InputChainJourneyTimer += DeltaSeconds;
			if (InputChainJourneyTimer >= 3.1f)
			{
				const bool bSignalOn = Car && Car->IsLeftSignalOn();
				if (!bSignalOn) { InputChainFailures++; }
				UE_LOG(LogTemp, Log, TEXT("[KeMuSanInputChain] step20_q_signal_wait3s %s (SignalTime=%.2fs, LeftSignal=%d)"),
					bSignalOn ? TEXT("PASS") : TEXT("FAIL"), InputChainJourneyTimer, bSignalOn ? 1 : 0);
				InputChainStep = 21;
				InputChainSubStep = 0;
				InputChainJourneyTimer = 0.f;
			}
		}
		break;

	case 21: // Step 21: 按 1 挂入 1 挡，按 SpaceBar 松开手刹
		if (InputChainSubStep == 0)
		{
			InputKey(FInputKeyParams(EKeys::One, IE_Pressed, 1.0));
			InputChainSubStep = 1;
		}
		else if (InputChainSubStep == 1)
		{
			InputKey(FInputKeyParams(EKeys::One, IE_Released, 0.0));
			InputChainSubStep = 2;
		}
		else if (InputChainSubStep == 2)
		{
			InputKey(FInputKeyParams(EKeys::SpaceBar, IE_Pressed, 1.0));
			InputChainSubStep = 3;
		}
		else if (InputChainSubStep == 3)
		{
			InputKey(FInputKeyParams(EKeys::SpaceBar, IE_Released, 0.0));
			InputChainSubStep = 4;
		}
		else
		{
			const bool bGearOk = Car && (Car->GetGear() == EGear::G1);
			const bool bHandbrakeReleased = Car && !Car->IsHandbrakeEngaged();
			const bool bOk = bGearOk && bHandbrakeReleased;
			if (!bOk) { InputChainFailures++; }
			if (Car)
			{
				InputChainStartLocation = Car->GetActorLocation();
			}
			UE_LOG(LogTemp, Log, TEXT("[KeMuSanInputChain] step21_gear1_release_handbrake %s (Gear=%d, Handbrake=%d)"),
				bOk ? TEXT("PASS") : TEXT("FAIL"), Car ? static_cast<int32>(Car->GetGear()) : -1, bHandbrakeReleased ? 0 : 1);
			InputChainStep = 22;
			InputChainSubStep = 0;
			InputChainJourneyTimer = 0.f;
		}
		break;

	case 22: // Step 22: 持续输入 W（真实油门），平滑起步不熄火，进入 Driving 阶段并沿路线位移 >= 14 米
		InputChainJourneyTimer += DeltaSeconds;
		InputKey(FInputKeyParams(EKeys::W, IE_Pressed, 1.0));
		{
			const float DistMoved = Car ? (FVector::Dist(Car->GetActorLocation(), InputChainStartLocation) * 0.01f) : 0.f;
			const float CurS = EC ? EC->GetCurS() : 0.f;
			const float CurSpeedKmh = Car ? Car->GetSpeedKmh() : 0.f;
			const bool bIsRealDriving = EC && (EC->GetPhase() == EExamPhase::Driving);
			const bool bNotStalled = Car && (!Car->IsStalled());

			// 严格判定：真实进入 Driving 阶段、路线里程 S > 34m、位移 >= 14m、速度 > 5km/h 且未熄火
			if (DistMoved >= 14.0f && CurS > 34.0f && bIsRealDriving && (CurSpeedKmh > 5.0f) && bNotStalled)
			{
				InputKey(FInputKeyParams(EKeys::W, IE_Released, 0.0));
				UE_LOG(LogTemp, Log, TEXT("[KeMuSanInputChain] step22_w_driving_distance PASS (Dist=%.1fm, CurS=%.1f, Speed=%.1fkm/h, Driving=1, NotStalled=1, Time=%.2fs)"),
					DistMoved, CurS, CurSpeedKmh, InputChainJourneyTimer);
				InputChainStep = 23;
				InputChainSubStep = 0;
				InputChainJourneyTimer = 0.f;
			}
			else if (InputChainJourneyTimer > 12.0f)
			{
				InputKey(FInputKeyParams(EKeys::W, IE_Released, 0.0));
				InputChainFailures++;
				UE_LOG(LogTemp, Error, TEXT("[KeMuSanInputChain] step22_w_driving_distance FAIL (Timeout, Dist=%.1fm, CurS=%.1f, Speed=%.1fkm/h, Phase=%d, Stalled=%d)"),
					DistMoved, CurS, CurSpeedKmh, EC ? static_cast<int32>(EC->GetPhase()) : -1, Car ? (Car->IsStalled() ? 1 : 0) : -1);
				InputChainStep = 23;
				InputChainSubStep = 0;
				InputChainJourneyTimer = 0.f;
			}
		}
		break;

	case 23: // Step 23: S 键刹车停住（速度降至 < 0.5km/h 刹停）
		InputChainJourneyTimer += DeltaSeconds;
		InputKey(FInputKeyParams(EKeys::S, IE_Pressed, 1.0));
		{
			const float CurSpeedKmh = Car ? Car->GetSpeedKmh() : 0.f;
			if (CurSpeedKmh < 0.5f)
			{
				InputKey(FInputKeyParams(EKeys::S, IE_Released, 0.0));
				UE_LOG(LogTemp, Log, TEXT("[KeMuSanInputChain] step23_s_brake_stop PASS (FinalSpeed=%.2fkm/h, StopTime=%.2fs)"),
					CurSpeedKmh, InputChainJourneyTimer);
				InputChainStep = 24;
				InputChainSubStep = 0;
				InputChainJourneyTimer = 0.f;
			}
			else if (InputChainJourneyTimer > 6.0f)
			{
				InputKey(FInputKeyParams(EKeys::S, IE_Released, 0.0));
				InputChainFailures++;
				UE_LOG(LogTemp, Error, TEXT("[KeMuSanInputChain] step23_s_brake_stop FAIL (Timeout, FinalSpeed=%.2fkm/h)"), CurSpeedKmh);
				InputChainStep = 24;
				InputChainSubStep = 0;
				InputChainJourneyTimer = 0.f;
			}
		}
		break;

	case 24: // Step 24: 练习模式设计契约一致性核验（验证处于练习模式且游戏已启动）
		{
			const bool bPracticeOk = EC && EC->IsPractice();
			const bool bGameStarted = GM && GM->IsGameStarted();
			const bool bOk = bPracticeOk && bGameStarted;
			if (!bOk) { InputChainFailures++; }
			UE_LOG(LogTemp, Log, TEXT("[KeMuSanInputChain] step24_practice_mode_consistency %s (IsPractice=%d, GameStarted=%d)"),
				bOk ? TEXT("PASS") : TEXT("FAIL"), bPracticeOk ? 1 : 0, bGameStarted ? 1 : 0);
			InputChainStep = 25;
			InputChainSubStep = 0;
		}
		break;

	case 25: // Step 25: 汇总结果与判定
		if (InputChainFailures == 0)
		{
			UE_LOG(LogTemp, Log, TEXT("[KeMuSanInputChain] test_complete PASS"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[KeMuSanInputChain] test_complete FAIL (Failures=%d)"), InputChainFailures);
		}
		bInputChainTesting = false;
		InputChainStep = 26;
		if (FParse::Param(FCommandLine::Get(), TEXT("test-input-chain-exit")))
		{
			FGenericPlatformMisc::RequestExit(false);
		}
		break;

	default:
		break;
	}
}

void AKeMuSanPlayerController::TickExamStartTest(float DeltaSeconds)
{
	ExamStartTimer += DeltaSeconds;
	AExamController* EC = GetExamController();
	AKeMuSanPawn* Car = GetTrainingPawn();

	switch (ExamStartStep)
	{
	case 0: // Step 0: 等待游戏初始稳定（0.3s），然后真实按下 F1 或 F2
		if (ExamStartTimer >= 0.3f)
		{
			const FKey TargetKey = (ExamStartTarget == TEXT("auto")) ? EKeys::F2 : EKeys::F1;
			InputKey(FInputKeyParams(TargetKey, IE_Pressed, 1.0));
			ExamStartStep = 1;
			ExamStartTimer = 0.f;
		}
		break;

	case 1: // Step 1: 下一帧释放按键
		{
			const FKey TargetKey = (ExamStartTarget == TEXT("auto")) ? EKeys::F2 : EKeys::F1;
			InputKey(FInputKeyParams(TargetKey, IE_Released, 0.0));
			ExamStartStep = 2;
			ExamStartTimer = 0.f;
		}
		break;

	case 2: // Step 2: 等待 0.3s 让控制器完成初始化并执行断言
		if (ExamStartTimer >= 0.3f)
		{
			AKeMuSanGameMode* GM = Cast<AKeMuSanGameMode>(GetWorld()->GetAuthGameMode());
			const bool bGMStarted = GM && GM->IsGameStarted() && GM->bExamMode;
			const bool bNotPractice = EC && (!EC->IsPractice());
			const bool bSimulatedExam = EC && (EC->GetPlayMode() == EGamePlayMode::SimulatedExam);
			const bool bPrepPhase = EC && (EC->GetPhase() == EExamPhase::Prep);
			const bool bECTransMatch = EC && (ExamStartTarget == TEXT("auto") ? (EC->GetTransmission() == ETransmissionType::Auto) : (EC->GetTransmission() == ETransmissionType::Manual));
			const bool bPawnTransMatch = Car && (ExamStartTarget == TEXT("auto") ? (Car->GetTransmissionType() == ETransmissionType::Auto) : (Car->GetTransmissionType() == ETransmissionType::Manual));
			const bool bNoAutoSeatbelt = Car && (!Car->IsSeatbeltOn());
			const bool bHandbrakeEngaged = Car && Car->IsHandbrakeEngaged();
			const bool bPrepNotDrivable = Car && (!Car->IsDrivable());

			const bool bAllOk = bGMStarted && bNotPractice && bSimulatedExam && bPrepPhase && bECTransMatch && bPawnTransMatch && bNoAutoSeatbelt && bHandbrakeEngaged && bPrepNotDrivable;

			if (bAllOk)
			{
				UE_LOG(LogTemp, Log, TEXT("[KeMuSanExamStart] %s PASS (GMStarted=%d, NotPractice=%d, SimExam=%d, PrepPhase=%d, ECTrans=%d, PawnTrans=%d, NoSeatbelt=%d, Handbrake=%d, NotDrivable=%d)"),
					*ExamStartTarget, bGMStarted ? 1 : 0, bNotPractice ? 1 : 0, bSimulatedExam ? 1 : 0, bPrepPhase ? 1 : 0, bECTransMatch ? 1 : 0, bPawnTransMatch ? 1 : 0, bNoAutoSeatbelt ? 1 : 0, bHandbrakeEngaged ? 1 : 0, bPrepNotDrivable ? 1 : 0);
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[KeMuSanExamStart] %s FAIL (GMStarted=%d, NotPractice=%d, SimExam=%d, PrepPhase=%d, ECTrans=%d, PawnTrans=%d, NoSeatbelt=%d, Handbrake=%d, NotDrivable=%d)"),
					*ExamStartTarget, bGMStarted ? 1 : 0, bNotPractice ? 1 : 0, bSimulatedExam ? 1 : 0, bPrepPhase ? 1 : 0, bECTransMatch ? 1 : 0, bPawnTransMatch ? 1 : 0, bNoAutoSeatbelt ? 1 : 0, bHandbrakeEngaged ? 1 : 0, bPrepNotDrivable ? 1 : 0);
			}

			bExamStartTesting = false;
			ExamStartStep = 3;
			FGenericPlatformMisc::RequestExit(false);
		}
		break;

	default:
		break;
	}
}
