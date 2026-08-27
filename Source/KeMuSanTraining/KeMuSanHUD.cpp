#include "KeMuSanHUD.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

#include "KeMuSanGameMode.h"
#include "KeMuSanPawn.h"
#include "ExamController.h"
#include "ExamTypes.h"

namespace
{
	const FLinearColor ColWhite(1.f, 1.f, 1.f);
	const FLinearColor ColYellow(1.f, 0.85f, 0.2f);
	const FLinearColor ColRed(1.f, 0.25f, 0.2f);
	const FLinearColor ColGreen(0.3f, 1.f, 0.4f);
	const FLinearColor ColGray(0.6f, 0.6f, 0.65f);
	const FLinearColor ColCyan(0.35f, 0.85f, 1.f);

	// Filled rectangle via texture-less FCanvasTileItem; the engine provides the
	// white tile itself, so this links without GWhiteTexture and renders in PIE.
	void DrawFilledRect(UCanvas* Canvas, float X, float Y, float W, float H, const FLinearColor& Color)
	{
		if (!Canvas) return;
		FCanvasTileItem Item(FVector2D(X, Y), FVector2D(W, H), Color);
		Item.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Item);
	}
}

AKeMuSanGameMode* AKeMuSanHUD::GetGameMode() const
{
	return GetWorld() ? GetWorld()->GetAuthGameMode<AKeMuSanGameMode>() : nullptr;
}

AExamController* AKeMuSanHUD::GetExamController() const
{
	AKeMuSanGameMode* GM = GetGameMode();
	return GM ? GM->GetExamController() : nullptr;
}

AKeMuSanPawn* AKeMuSanHUD::GetCar() const
{
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		return Cast<AKeMuSanPawn>(PC->GetPawn());
	}
	return nullptr;
}

void AKeMuSanHUD::DrawTextShadowed(const FString& Text, float X, float Y, float Scale, const FLinearColor& Color, const UFont* Font)
{
	if (!Canvas || !Font)
	{
		return;
	}
	// Draw shadow: dark, slight offset, translucent
	Canvas->SetLinearDrawColor(FLinearColor(0.f, 0.f, 0.f, 0.7f));
	Canvas->DrawText(Font, *Text, X + 1.f, Y + 1.f, Scale, Scale, FFontRenderInfo());
	// Draw foreground
	Canvas->SetLinearDrawColor(Color);
	Canvas->DrawText(Font, *Text, X, Y, Scale, Scale, FFontRenderInfo());
}

void AKeMuSanHUD::DrawHUD()
{
	Super::DrawHUD();

	AKeMuSanGameMode* GM = GetGameMode();
	AExamController* EC = GetExamController();
	if (!GM || !EC)
	{
		return;
	}

	if (!GM->IsGameStarted())
	{
		DrawMenu();
		return;
	}

	const EExamPhase P = EC->GetPhase();
	DrawTopPrompt(EC);
	DrawProgressBar(EC);
	DrawScorePanel(EC);
	DrawProgressList(EC);

	if (P == EExamPhase::Driving || P == EExamPhase::PullOver || P == EExamPhase::Ready)
	{
		DrawMiniMap(EC, GetCar());
	}

	if (P != EExamPhase::Finished)
	{
		DrawVehiclePanel(GetCar(), EC);
	}

	if (P == EExamPhase::LightTest)
	{
		DrawLightTestPanel(EC);
	}

	if (P == EExamPhase::Finished)
	{
		DrawResultPanel(EC, GM);
	}

	DrawKeyHelp(EC);

	if (GM->bPaused)
	{
		DrawPauseOverlay(GM);
	}
}

void AKeMuSanHUD::DrawProgressBar(AExamController* EC)
{
	if (!EC)
	{
		return;
	}
	const float W = Canvas->SizeX * 0.42f;
	const float H = 10.f;
	const float X = (Canvas->SizeX - W) * 0.5f;
	const float Y = Canvas->SizeY - 52.f;

	DrawFilledRect(Canvas, X - 2.f, Y - 2.f, W + 4.f, H + 4.f, FLinearColor(0.f, 0.f, 0.f, 0.55f));
	DrawFilledRect(Canvas, X, Y, W * EC->GetProgress01(), H, FLinearColor(0.25f, 0.75f, 1.f, 0.85f));
	DrawTextShadowed(TEXT("考试路线进度"), X, Y - 22.f, 0.8f, ColGray, GEngine->GetMediumFont());
}

void AKeMuSanHUD::DrawMenu()
{
	UFont* BigFont = GEngine->GetLargeFont();
	UFont* SmallFont = GEngine->GetMediumFont();
	const float CX = Canvas->SizeX * 0.5f;
	const float CY = Canvas->SizeY * 0.5f;

	const FString Title1 = TEXT("科目三路考模拟");
	const FString Title2 = TEXT("城市道路驾驶技能考试 · 全长1.5km真实路线");
	DrawTextShadowed(Title1, CX - BigFont->GetStringSize(*Title1) * 3.0f * 0.5f, CY - 320.f, 3.0f, ColYellow, BigFont);
	DrawTextShadowed(Title2, CX - BigFont->GetStringSize(*Title2) * 1.3f * 0.5f, CY - 230.f, 1.3f, ColWhite, BigFont);

	const FString Tips[] =
	{
		TEXT("F1  手动挡考试（100分制，90分合格）"),
		TEXT("F2  自动挡考试（100分制，90分合格）"),
		TEXT("F9  自由练习（不计分）"),
		TEXT(""),
		TEXT("W/S 油门/刹车    A/D 转向    空格手刹    F 安全带"),
		TEXT("Q 左转向灯    E 右转向灯    L 灯光循环    J 远近交替"),
		TEXT("H 双闪    K 雾灯    B 喇叭    M 观察"),
		TEXT("手动挡：1-5换挡 / N空挡 / R倒挡   自动挡：Tab P/R/N/D"),
		TEXT("Esc 暂停"),
		TEXT(""),
		TEXT("考试流程：上车准备 → 灯光模拟 → 起步 → 直线行驶 → 变更车道"),
		TEXT("→ 通过路口/人行横道 → 学校/公交站 → 会车 → 超车 → 加减挡"),
		TEXT("→ 掉头返回 → 靠边停车"),
		TEXT(""),
		TEXT("注意：路网上有社会车辆通行，发生碰撞将判不合格！")
	};

	float Y = CY - 150.f;
	for (const FString& Tip : Tips)
	{
		if (!Tip.IsEmpty())
		{
			DrawTextShadowed(Tip, CX - 350.f, Y, 1.15f, Tip.Contains(TEXT("!")) ? ColRed : ColCyan, SmallFont);
		}
		Y += 32.f;
	}

	// 闪烁提示
	const float T = GetWorld()->GetTimeSeconds();
	if (FMath::Fmod(T, 1.2f) < 0.7f)
	{
		const FString Go = TEXT("▶ 按 Enter 开始考试 ◀");
		DrawTextShadowed(Go, CX - BigFont->GetStringSize(*Go) * 1.6f * 0.5f, CY + 260.f, 1.6f, ColGreen, BigFont);
	}
}

void AKeMuSanHUD::DrawTopPrompt(AExamController* EC)
{
	UFont* Font = GEngine->GetLargeFont();
	const float W = Canvas->SizeX * 0.66f;
	const float H = 84.f;
	const float X = (Canvas->SizeX - W) * 0.5f;
	const float Y = 10.f;

	DrawFilledRect(Canvas, X, Y, W, H, FLinearColor(0.f, 0.f, 0.f, 0.55f));

	const FString Text = EC->GetPrompt();
	const int32 TW = Font->GetStringSize(*Text);
	const float TH = Font->GetMaxCharHeight();
	const float Scale = (TW * 1.1f > W) ? (W / (TW * 1.1f)) : 1.1f;
	DrawTextShadowed(Text, X + (W - TW * Scale) * 0.5f, Y + (H - TH * Scale) * 0.5f, Scale, ColYellow, Font);
}

void AKeMuSanHUD::DrawScorePanel(AExamController* EC)
{
	UFont* Font = GEngine->GetLargeFont();
	const float X = Canvas->SizeX - 260.f;
	const float Y = 16.f;

	DrawTextShadowed(FString::Printf(TEXT("分数：%d"), EC->GetScore()), X, Y, 1.5f, EC->GetScore() >= 90 ? ColGreen : ColYellow, Font);

	if (EC->IsPractice())
	{
		DrawTextShadowed(TEXT("自由练习模式"), X, Y + 42.f, 1.0f, ColCyan, Font);
	}

	// 最近扣分
	const TArray<FDeduction>& Deds = EC->GetDeductions();
	const int32 Show = FMath::Min(Deds.Num(), 6);
	for (int32 i = 0; i < Show; ++i)
	{
		const FDeduction& D = Deds[Deds.Num() - 1 - i];
		DrawTextShadowed(FString::Printf(TEXT("-%d  %s"), D.Points, *D.Reason), X - 120.f, Y + 82.f + i * 26.f, 0.85f, ColRed, Font);
	}
}

void AKeMuSanHUD::DrawProgressList(AExamController* EC)
{
	UFont* Font = GEngine->GetMediumFont();
	const TArray<FZoneStatus>& Zones = EC->GetZoneStatuses();

	// 背景
	DrawFilledRect(Canvas, 10.f, 10.f, 210.f, Zones.Num() * 24.f + 20.f, FLinearColor(0.f, 0.f, 0.f, 0.45f));

	float Y = 24.f;
	for (const FZoneStatus& Z : Zones)
	{
		FString Mark;
		FLinearColor Color;
		switch (Z.State)
		{
		case 1: Mark = TEXT("▶"); Color = ColYellow; break;
		case 2: Mark = TEXT("✓"); Color = ColGreen; break;
		default: Mark = TEXT("·"); Color = ColGray; break;
		}
		DrawTextShadowed(FString::Printf(TEXT("%s %s"), *Mark, *Z.Name), 22.f, Y, 0.95f, Color, Font);
		Y += 24.f;
	}
}

FString AKeMuSanHUD::GetLightStateText(const AKeMuSanPawn* Car) const
{
	if (!Car)
	{
		return TEXT("");
	}
	FString S;
	if (Car->IsOutlineOn()) S += TEXT("示廓 ");
	if (Car->IsLowBeamOn()) S += TEXT("近光 ");
	if (Car->IsHighBeamOn()) S += TEXT("远光 ");
	if (Car->IsFogLampOn()) S += TEXT("雾灯 ");
	if (Car->IsHazardOn()) S += TEXT("双闪 ");
	if (Car->IsLeftSignalOn()) S += TEXT("←左转 ");
	if (Car->IsRightSignalOn()) S += TEXT("右转→ ");
	return S.TrimEnd();
}

void AKeMuSanHUD::DrawVehiclePanel(AKeMuSanPawn* Car, AExamController* EC)
{
	UFont* Font = GEngine->GetLargeFont();
	UFont* SmallFont = GEngine->GetMediumFont();
	const float X = 20.f;
	const float Y = Canvas->SizeY - 190.f;

	if (!Car)
	{
		return;
	}

	// 速度
	DrawTextShadowed(FString::Printf(TEXT("%.0f"), Car->GetSpeedKmh()), X, Y, 3.2f, ColWhite, Font);
	DrawTextShadowed(TEXT("km/h"), X + 150.f, Y + 40.f, 1.0f, ColGray, Font);

	// 挡位
	FString GearText;
	if (Car->GetTransmissionType() == ETransmissionType::Auto)
	{
		switch (Car->GetGear())
		{
		case EGear::N: GearText = TEXT("N"); break;
		case EGear::R: GearText = TEXT("R"); break;
		case EGear::G1: case EGear::G2: case EGear::G3: case EGear::G4: case EGear::G5:
			GearText = TEXT("D"); break;
		}
	}
	else
	{
		switch (Car->GetGear())
		{
		case EGear::N: GearText = TEXT("N"); break;
		case EGear::R: GearText = TEXT("R"); break;
		case EGear::G1: GearText = TEXT("1"); break;
		case EGear::G2: GearText = TEXT("2"); break;
		case EGear::G3: GearText = TEXT("3"); break;
		case EGear::G4: GearText = TEXT("4"); break;
		case EGear::G5: GearText = TEXT("5"); break;
		}
	}
	DrawTextShadowed(GearText, X + 60.f, Y + 90.f, 1.8f, Car->IsStalled() ? ColRed : ColYellow, Font);

	// 转速条
	const float RpmRatio = FMath::Clamp(Car->GetEngineRpm() / 6200.f, 0.f, 1.f);
	const float BarX = X + 2.f;
	const float BarY = Y + 136.f;
	DrawFilledRect(Canvas, BarX, BarY, 200.f, 12.f, FLinearColor(0.1f, 0.1f, 0.1f, 0.7f));
	DrawFilledRect(Canvas, BarX, BarY, 200.f * RpmRatio, 12.f, FLinearColor(0.9f, 0.6f, 0.1f, 0.9f));

	// 车辆状态
	FString State = GetLightStateText(Car);
	if (Car->IsSeatbeltOn()) State += TEXT("安全带 ");
	if (Car->IsHandbrakeOn()) State += TEXT("手刹 ");
	if (Car->IsStalled()) State += TEXT("熄火！ ");
	DrawTextShadowed(State.IsEmpty() ? TEXT("灯光关闭") : State, X, Y + 160.f, 0.85f, ColCyan, SmallFont);

	// 红绿灯状态（靠近路口时）
	if (EC && EC->GetPhase() == EExamPhase::Driving)
	{
		const int32 S = EC->GetTrafficLightState();
		if (S == 0)
		{
			DrawTextShadowed(FString::Printf(TEXT("红灯 %.0fs"), EC->GetTrafficLightRemaining()), X + 260.f, Y + 4.f, 1.1f, ColRed, Font);
		}
		else if (S == 1)
		{
			DrawTextShadowed(FString::Printf(TEXT("黄灯 %.0fs"), EC->GetTrafficLightRemaining()), X + 260.f, Y + 4.f, 1.1f, ColYellow, Font);
		}
		else
		{
			DrawTextShadowed(FString::Printf(TEXT("绿灯 %.0fs"), EC->GetTrafficLightRemaining()), X + 260.f, Y + 4.f, 1.1f, ColGreen, Font);
		}
	}
}

void AKeMuSanHUD::DrawLightTestPanel(AExamController* EC)
{
	UFont* Font = GEngine->GetLargeFont();
	UFont* SmallFont = GEngine->GetMediumFont();

	const float W = Canvas->SizeX * 0.72f;
	const float H = 340.f;
	const float X = (Canvas->SizeX - W) * 0.5f;
	const float Y = Canvas->SizeY * 0.5f - 100.f;

	DrawFilledRect(Canvas, X, Y, W, H, FLinearColor(0.f, 0.f, 0.f, 0.72f));

	const FLightQuestion* Q = EC->GetCurrentLightQuestion();
	if (Q)
	{
		DrawTextShadowed(Q->Text, X + 30.f, Y + 20.f, 1.4f, ColYellow, Font);
	}
	else
	{
		DrawTextShadowed(TEXT("准备下一题…"), X + 30.f, Y + 20.f, 1.4f, ColYellow, Font);
	}

	const FString Options[] =
	{
		TEXT("1  近光灯"),
		TEXT("2  远光灯"),
		TEXT("3  远近交替"),
		TEXT("4  示廓灯 + 危险报警闪光灯"),
		TEXT("5  雾灯 + 危险报警闪光灯")
	};
	for (int32 i = 0; i < 5; ++i)
	{
		DrawTextShadowed(Options[i], X + 60.f, Y + 90.f + i * 34.f, 1.1f, ColWhite, SmallFont);
	}

	DrawTextShadowed(FString::Printf(TEXT("剩余时间：%.1f 秒"), FMath::Max(0.f, EC->GetLightCountdown())), X + W - 300.f, Y + H - 46.f, 1.1f, ColCyan, Font);
	DrawTextShadowed(FString::Printf(TEXT("第 %d / %d 题"), EC->GetLightQuestionIndex(), EC->GetLightQuestionTotal()), X + 60.f, Y + H - 46.f, 1.0f, ColGray, Font);
}

void AKeMuSanHUD::DrawResultPanel(AExamController* EC, AKeMuSanGameMode* GM)
{
	UFont* Font = GEngine->GetLargeFont();
	UFont* SmallFont = GEngine->GetMediumFont();

	const float W = Canvas->SizeX * 0.62f;
	const float H = 500.f;
	const float X = (Canvas->SizeX - W) * 0.5f;
	const float Y = (Canvas->SizeY - H) * 0.5f - 20.f;

	DrawFilledRect(Canvas, X, Y, W, H, FLinearColor(0.f, 0.f, 0.f, 0.82f));

	const bool bPass = !EC->IsFailIssued() && EC->GetScore() >= 90;
	{
		const FString T1 = TEXT("考试结束");
		const FString T2 = bPass ? TEXT("合 格") : TEXT("不合格");
		DrawTextShadowed(T1, X + (W - Font->GetStringSize(*T1) * 1.5f) * 0.5f, Y + 18.f, 1.5f, ColWhite, Font);
		DrawTextShadowed(T2, X + (W - Font->GetStringSize(*T2) * 2.6f) * 0.5f, Y + 70.f, 2.6f, bPass ? ColGreen : ColRed, Font);
	}
	{
		const FString T3 = FString::Printf(TEXT("最终得分：%d 分"), EC->GetScore());
		DrawTextShadowed(T3, X + (W - Font->GetStringSize(*T3) * 1.4f) * 0.5f, Y + 185.f, 1.4f, ColYellow, Font);
	}

	// 扣分明细
	const TArray<FDeduction>& Deds = EC->GetDeductions();
	{
		const FString T4 = TEXT("—— 扣分明细 ——");
		DrawTextShadowed(T4, X + (W - Font->GetStringSize(*T4) * 1.0f) * 0.5f, Y + 245.f, 1.0f, ColWhite, Font);
	}
	const int32 Show = FMath::Min(Deds.Num(), 6);
	for (int32 i = 0; i < Show; ++i)
	{
		const FDeduction& D = Deds[Deds.Num() - 1 - i];
		DrawTextShadowed(FString::Printf(TEXT("-%d  %s"), D.Points, *D.Reason), X + 40.f, Y + 285.f + i * 26.f, 0.9f, ColRed, SmallFont);
	}
	if (Deds.Num() == 0)
	{
		DrawTextShadowed(TEXT("满分！没有任何扣分"), X + 40.f, Y + 285.f, 0.9f, ColGreen, SmallFont);
	}

	const float T = GetWorld()->GetTimeSeconds();
	if (FMath::Fmod(T, 1.2f) < 0.7f)
	{
		const FString T5 = TEXT("按 Enter 重新开始");
		DrawTextShadowed(T5, X + (W - Font->GetStringSize(*T5) * 1.3f) * 0.5f, Y + H - 56.f, 1.3f, ColYellow, Font);
	}

	if (GM && GM->bExamMode == false)
	{
		DrawTextShadowed(TEXT("（自由练习无评分）"), X + W - 280.f, Y + 24.f, 1.0f, ColCyan, SmallFont);
	}
}

void AKeMuSanHUD::DrawPauseOverlay(AKeMuSanGameMode* GM)
{
	UFont* Font = GEngine->GetLargeFont();
	const float CX = Canvas->SizeX * 0.5f;
	DrawFilledRect(Canvas, 0.f, 0.f, Canvas->SizeX, Canvas->SizeY, FLinearColor(0.f, 0.f, 0.f, 0.6f));
	{
		const FString T1 = TEXT("已暂停");
		const FString T2 = TEXT("按 Enter 或 Esc 继续游戏");
		DrawTextShadowed(T1, CX - Font->GetStringSize(*T1) * 2.4f * 0.5f, Canvas->SizeY * 0.5f - 60.f, 2.4f, ColYellow, Font);
		DrawTextShadowed(T2, CX - Font->GetStringSize(*T2) * 1.2f * 0.5f, Canvas->SizeY * 0.5f + 10.f, 1.2f, ColWhite, Font);
	}
}

void AKeMuSanHUD::DrawKeyHelp(AExamController* EC)
{
	UFont* Font = GEngine->GetMediumFont();
	const FString Help = TEXT("W/S油门刹车  A/D转向  Q/E转向灯  L灯光  J交替  空格手刹  F安全带  B喇叭  M观察  Esc暂停");
	const float Y = Canvas->SizeY - 30.f;
	DrawTextShadowed(Help, 12.f, Y, 0.8f, ColGray, Font);

	if (EC && EC->GetPhase() == EExamPhase::Finished)
	{
		return;
	}
}

void AKeMuSanHUD::DrawGearDisplay(AKeMuSanPawn* Car, const UFont* Font, float X, float Y)
{
	if (!Car) return;
	FString G;
	if (Car->GetTransmissionType() == ETransmissionType::Auto)
	{
		switch (Car->GetGear())
		{
		case EGear::N: G = TEXT("N"); break;
		case EGear::R: G = TEXT("R"); break;
		case EGear::G1: case EGear::G2: case EGear::G3: case EGear::G4: case EGear::G5:
			G = TEXT("D"); break;
		}
	}
	else
	{
		switch (Car->GetGear())
		{
		case EGear::N: G = TEXT("N"); break;
		case EGear::R: G = TEXT("R"); break;
		case EGear::G1: G = TEXT("1"); break;
		case EGear::G2: G = TEXT("2"); break;
		case EGear::G3: G = TEXT("3"); break;
		case EGear::G4: G = TEXT("4"); break;
		case EGear::G5: G = TEXT("5"); break;
		}
	}
	DrawTextShadowed(G, X, Y, 1.8f, ColYellow, Font);
}

void AKeMuSanHUD::DrawMiniMap(AExamController* EC, AKeMuSanPawn* Car)
{
	if (!EC || !Canvas) return;
	UFont* Font = GEngine->GetMediumFont();

	// Small map box, top-right
	const float MapW = 180.f;
	const float MapH = 130.f;
	const float MapX = Canvas->SizeX - MapW - 18.f;
	const float MapY = Canvas->SizeY - MapH - 90.f;

	DrawFilledRect(Canvas, MapX - 2.f, MapY - 2.f, MapW + 4.f, MapH + 4.f, FLinearColor(0.f, 0.f, 0.f, 0.6f));
	DrawTextShadowed(TEXT("路线图"), MapX + 4.f, MapY + 2.f, 0.7f, ColCyan, Font);

	// Draw simplified route topology: 东→北→西→掉头→返回
	// 东段 (horizontal right): x=MapX+10..MapX+170, y=MapY+75
	// 北段 (vertical up): x=MapX+155, y=MapY+18..MapY+75
	// 西段 (horizontal left): x=MapX+10..MapX+158, y=MapY+18
	// 返回段 (horizontal right, below): x=MapX+70..MapX+170, y=MapY+98
	const float EastX1 = MapX + 12.f, EastX2 = MapX + MapW - 12.f;
	const float WestY = MapY + 22.f;
	const float EastY = MapY + 62.f;
	const float NorthX = MapX + MapW - 28.f;
	const float RetY = MapY + 90.f;
	const float RetX1 = MapX + 68.f;

	// Road lines
	DrawFilledRect(Canvas, EastX1, EastY - 1.f, EastX2 - EastX1, 2.f, FLinearColor(0.5f, 0.5f, 0.55f, 0.9f));             // 东段
	DrawFilledRect(Canvas, NorthX - 1.f, WestY, 2.f, EastY - WestY, FLinearColor(0.5f, 0.5f, 0.55f, 0.9f));                // 北段
	DrawFilledRect(Canvas, EastX1, WestY - 1.f, NorthX - EastX1, 2.f, FLinearColor(0.5f, 0.5f, 0.55f, 0.9f));              // 西段
	DrawFilledRect(Canvas, RetX1, RetY - 1.f, EastX2 - RetX1, 2.f, FLinearColor(0.55f, 0.5f, 0.5f, 0.9f));                // 返回段

	// U-turn loop (small arc at left side)
	DrawFilledRect(Canvas, EastX1 + 50.f, WestY + 35.f, 20.f, 20.f, FLinearColor(0.5f, 0.3f, 0.3f, 0.6f));

	// Player position dot
	const float Prog = EC->GetProgress01();
	float DotX, DotY;
	if (Prog < 0.35f)
	{
		// 东段
		const float Frac = Prog / 0.35f;
		DotX = EastX1 + (EastX2 - EastX1) * Frac;
		DotY = EastY;
	}
	else if (Prog < 0.65f)
	{
		// 北段
		const float Frac = (Prog - 0.35f) / 0.30f;
		DotX = NorthX;
		DotY = EastY - (EastY - WestY) * Frac;
	}
	else if (Prog < 0.82f)
	{
		// 西段
		const float Frac = (Prog - 0.65f) / 0.17f;
		DotX = NorthX - (NorthX - EastX1) * Frac;
		DotY = WestY;
	}
	else
	{
		// 返回段
		const float Frac = FMath::Clamp((Prog - 0.82f) / 0.18f, 0.f, 1.f);
		DotX = RetX1 + (EastX2 - RetX1) * Frac;
		DotY = RetY;
	}
	DrawFilledRect(Canvas, DotX - 3.f, DotY - 3.f, 6.f, 6.f, ColGreen);

	// Next zone label
	const TArray<FZoneStatus>& Zones = EC->GetZoneStatuses();
	FString NextZone;
	for (const FZoneStatus& Z : Zones)
	{
		if (Z.State == 0) { NextZone = Z.Name; break; }
	}
	if (!NextZone.IsEmpty())
	{
		DrawTextShadowed(FString::Printf(TEXT("▶ %s"), *NextZone), MapX + 4.f, RetY + 14.f, 0.65f, ColYellow, Font);
	}
}
