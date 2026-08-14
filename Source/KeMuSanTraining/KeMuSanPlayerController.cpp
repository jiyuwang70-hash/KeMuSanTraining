#include "KeMuSanPlayerController.h"

#include "Components/InputComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

#include "KeMuSanGameMode.h"
#include "KeMuSanPawn.h"
#include "ExamController.h"

AKeMuSanPlayerController::AKeMuSanPlayerController()
{
	bShowMouseCursor = false;
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

	// 全局按键
	InputComponent->BindAction(TEXT("Confirm"), IE_Pressed, this, &AKeMuSanPlayerController::ConfirmPressed);
	InputComponent->BindAction(TEXT("Pause"), IE_Pressed, this, &AKeMuSanPlayerController::PausePressed);
	InputComponent->BindAction(TEXT("FreePractice"), IE_Pressed, this, &AKeMuSanPlayerController::FreePracticePressed);
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
	if (APawn* P = GetPawn())
	{
		if (AKeMuSanPawn* Car = Cast<AKeMuSanPawn>(P))
		{
			Car->SelectGear(GearIndex);
			LastGearValue = GearIndex;
		}
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
	if (!GM)
	{
		return;
	}
	AExamController* EC = GM->GetExamController();

	if (!GM->IsGameStarted())
	{
		GM->StartExam();
		return;
	}

	if (GM->bPaused)
	{
		GM->TogglePause();
		return;
	}

	if (EC && EC->GetPhase() == EExamPhase::Finished)
	{
		GM->RestartGame();
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
