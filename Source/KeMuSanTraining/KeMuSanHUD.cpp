#include "KeMuSanHUD.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/PlayerController.h"
#include "Fonts/SlateFontInfo.h"

#include "KeMuSanGameMode.h"
#include "KeMuSanPawn.h"
#include "KeMuSanPlayerController.h"
#include "ExamController.h"
#include "ExamTypes.h"
#include "RoadLayout.h"
#include "ExamErrorAnalyzer.h"
#include "ExamSaveGame.h"

namespace
{
	const FLinearColor ColWhite(1.f, 1.f, 1.f);
	const FLinearColor ColYellow(1.f, 0.85f, 0.15f);
	const FLinearColor ColGold(1.f, 0.75f, 0.18f);
	const FLinearColor ColRed(1.f, 0.22f, 0.18f);
	const FLinearColor ColGreen(0.25f, 0.95f, 0.38f);
	const FLinearColor ColGray(0.65f, 0.68f, 0.72f);
	const FLinearColor ColDarkGray(0.25f, 0.28f, 0.32f);
	const FLinearColor ColCyan(0.28f, 0.82f, 1.f);
	const FLinearColor ColOrange(1.f, 0.55f, 0.12f);

	void DrawFilledRect(UCanvas* Canvas, float X, float Y, float W, float H, const FLinearColor& Color)
	{
		if (!Canvas || W <= 0.f || H <= 0.f) return;
		FCanvasTileItem Item(FVector2D(X, Y), FVector2D(W, H), Color);
		Item.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Item);
	}

	void DrawHollowRect(UCanvas* Canvas, float X, float Y, float W, float H, float Thickness, const FLinearColor& Color)
	{
		if (!Canvas || W <= 0.f || H <= 0.f) return;
		DrawFilledRect(Canvas, X, Y, W, Thickness, Color);
		DrawFilledRect(Canvas, X, Y + H - Thickness, W, Thickness, Color);
		DrawFilledRect(Canvas, X, Y + Thickness, Thickness, H - Thickness * 2.f, Color);
		DrawFilledRect(Canvas, X + W - Thickness, Y + Thickness, Thickness, H - Thickness * 2.f, Color);
	}

	void DrawRoundedCard(UCanvas* Canvas, float X, float Y, float W, float H, const FLinearColor& BgColor, const FLinearColor& BorderColor, float BorderThickness = 1.5f)
	{
		DrawFilledRect(Canvas, X, Y, W, H, BgColor);
		DrawHollowRect(Canvas, X, Y, W, H, BorderThickness, BorderColor);
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

void AKeMuSanHUD::DrawTextPixel(const FString& Text, float X, float Y, const FLinearColor& Color, const UFont* Font)
{
	if (!Canvas || !Font || Text.IsEmpty())
	{
		return;
	}
	// Pixel-Perfect 纯净渲染：严格 1:1 像素映射（Scale=1.0f），无阴影渗透，字体笔画清晰锐利如刀刻
	Canvas->SetLinearDrawColor(Color);
	Canvas->DrawText(Font, *Text, FMath::RoundToFloat(X), FMath::RoundToFloat(Y), 1.0f, 1.0f, FFontRenderInfo());
}

void AKeMuSanHUD::DrawTextWrapped(const FString& Text, float X, float Y, float MaxWidth, int32 MaxLines, const FLinearColor& Color, const UFont* Font, float LineHeight)
{
	if (!Canvas || !Font || MaxWidth <= 0.f || MaxLines <= 0) return;
	TArray<FString> Lines;
	FString Line;
	for (const TCHAR Ch : Text)
	{
		if (Ch == TEXT('\r')) continue;
		const FString Candidate = Line + FString::Chr(Ch);
		if (Ch == TEXT('\n') || (!Line.IsEmpty() && Font->GetStringSize(*Candidate) > MaxWidth))
		{
			Lines.Add(Line);
			Line.Empty();
			if (Ch == TEXT('\n')) continue;
		}
		Line.AppendChar(Ch);
	}
	if (!Line.IsEmpty()) Lines.Add(Line);
	const int32 Count = FMath::Min(MaxLines, Lines.Num());
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FString Visible = Lines[Index];
		if (Index == Count - 1 && Lines.Num() > MaxLines)
		{
			while (!Visible.IsEmpty() && Font->GetStringSize(*(Visible + TEXT("…"))) > MaxWidth) Visible.LeftChopInline(1);
			Visible += TEXT("…");
		}
		DrawTextPixel(Visible, X, Y + Index * LineHeight, Color, Font);
	}
}

void AKeMuSanHUD::DrawTextSlateLarge(const FString& Text, float X, float Y, const FLinearColor& Color, const UFont* Font, int32 PointSize)
{
	if (!Canvas || !Font || Text.IsEmpty())
	{
		return;
	}
	// 基于 FSlateFontInfo 重新栅格化矢量字体（如 Size=26~28px），Scale=1，无双层黑影模糊，笔画极致锐利
	FSlateFontInfo FontInfo = Font->GetLegacySlateFontInfo();
	FontInfo.Size = PointSize;
	FCanvasTextItem Item(FVector2D(FMath::RoundToFloat(X), FMath::RoundToFloat(Y)), FText::FromString(Text), FontInfo, Color);
	Item.Scale = FVector2D(1.0f, 1.0f);
	Item.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Item);
}

float AKeMuSanHUD::MeasureTextSlate(const FString& Text, const UFont* Font, int32 PointSize)
{
	if (Text.IsEmpty())
	{
		return 0.f;
	}
	float Width = 0.f;
	for (const TCHAR Ch : Text)
	{
		if (Ch > 127)
		{
			Width += static_cast<float>(PointSize) * 1.02f;
		}
		else
		{
			Width += static_cast<float>(PointSize) * 0.56f;
		}
	}
	return Width;
}

void AKeMuSanHUD::DrawTextBig(const FString& Text, float X, float Y, const FLinearColor& Color, const UFont* Font, int32 IntScale)
{
	if (!Canvas || !Font || Text.IsEmpty())
	{
		return;
	}
	const float S = static_cast<float>(FMath::Max(1, IntScale));
	const float RX = FMath::RoundToFloat(X);
	const float RY = FMath::RoundToFloat(Y);
	// 严格整数倍率采样，搭配整数像素阴影，消除小数缩放导致的笔画粘连模糊
	Canvas->SetLinearDrawColor(FLinearColor(0.f, 0.f, 0.f, 0.95f));
	Canvas->DrawText(Font, *Text, RX + 2.f, RY + 2.f, S, S, FFontRenderInfo());
	Canvas->SetLinearDrawColor(Color);
	Canvas->DrawText(Font, *Text, RX, RY, S, S, FFontRenderInfo());
}

void AKeMuSanHUD::DrawTextShadowedPixel(const FString& Text, float X, float Y, const FLinearColor& Color, const UFont* Font)
{
	if (!Canvas || !Font || Text.IsEmpty())
	{
		return;
	}
	const float RX = FMath::RoundToFloat(X);
	const float RY = FMath::RoundToFloat(Y);
	// 精确 1 像素整数投影，杜绝浮点亚像素边缘双线性抗锯齿模糊
	Canvas->SetLinearDrawColor(FLinearColor(0.f, 0.f, 0.f, 0.92f));
	Canvas->DrawText(Font, *Text, RX + 1.f, RY + 1.f, 1.0f, 1.0f, FFontRenderInfo());
	Canvas->SetLinearDrawColor(Color);
	Canvas->DrawText(Font, *Text, RX, RY, 1.0f, 1.0f, FFontRenderInfo());
}

void AKeMuSanHUD::DrawTextShadowed(const FString& Text, float X, float Y, float Scale, const FLinearColor& Color, const UFont* Font)
{
	if (!Canvas || !Font || Text.IsEmpty())
	{
		return;
	}
	const float RX = FMath::RoundToFloat(X);
	const float RY = FMath::RoundToFloat(Y);
	const float S = (FMath::Abs(Scale - 1.0f) < 0.25f) ? 1.0f : Scale;
	const float Offset = (S >= 2.0f) ? 2.0f : 1.0f;
	Canvas->SetLinearDrawColor(FLinearColor(0.f, 0.f, 0.f, 0.90f));
	Canvas->DrawText(Font, *Text, RX + Offset, RY + Offset, S, S, FFontRenderInfo());
	Canvas->SetLinearDrawColor(Color);
	Canvas->DrawText(Font, *Text, RX, RY, S, S, FFontRenderInfo());
}

void AKeMuSanHUD::DrawHUD()
{
	Super::DrawHUD();

	AKeMuSanGameMode* GM = GetGameMode();
	AExamController* EC = GetExamController();
	AKeMuSanPawn* Car = GetCar();

	if (!GM || !EC)
	{
		return;
	}

	// 1. 首屏界面
	if (!GM->IsGameStarted())
	{
		DrawMenu();
		if (EC->IsShowingHistoryPanel()) DrawHistoryAnalysisPanel(EC);
		return;
	}

	const EExamPhase P = EC->GetPhase();

	// 2. 真实光学后视镜渲染（双侧镜 + 车内中央后视镜）
	DrawMirrors(Car);

	// 3. 顶部考点、动态限速牌与超速警示
	DrawTopHeader(EC, Car);

	// 4. 起步步骤向导条（仅在练习模式起步阶段推进展示）
	if (EC->IsPractice() && P == EExamPhase::Ready)
	{
		DrawStepGuide(EC, Car);
	}

	// 5. 路线考点列表与得分面板
	DrawProgressList(EC);
	DrawScorePanel(EC);

	// 6. 靠边停车 30cm 真实标尺雷达（仅在靠边停车阶段激活）
	if (P == EExamPhase::PullOver)
	{
		DrawPullOverRadar(EC);
	}

	// 7. 路线小地图（行车阶段显示）
	if (P == EExamPhase::Driving || P == EExamPhase::PullOver || P == EExamPhase::Ready)
	{
		DrawMiniMap(EC, Car);
	}

	// 8. 现代风格集成驾驶仪表盘（车速、挡位、转速、踏板、指示灯）
	if (P != EExamPhase::Finished)
	{
		DrawModernDashboard(Car, EC);
	}

	// 9. 灯光模拟答题卡片
	if (P == EExamPhase::LightTest)
	{
		DrawLightTestPanel(EC);
	}

	// 10. 成绩结算报告
	if (P == EExamPhase::Finished)
	{
		DrawResultPanel(EC, GM);
	}

	// 11. 调镜模式工作台悬浮提示
	if (Car && Car->IsMirrorAdjustMode())
	{
		DrawMirrorAdjustOverlay(Car);
	}

	// 12. 底部轻量快捷键帮助
	DrawKeyHelp(EC, Car);

	// 13. 暂停蒙版
	if (GM->bPaused)
	{
		DrawPauseOverlay(GM);
	}
	// The archive is modal: render it after mirrors, hints and the existing pause overlay.
	if (EC->IsShowingHistoryPanel()) DrawHistoryAnalysisPanel(EC);
}

void AKeMuSanHUD::DrawMenu()
{
	UFont* BigFont = GEngine->GetLargeFont();
	UFont* SmallFont = GEngine->GetMediumFont();
	const float CX = Canvas->SizeX * 0.5f;
	const float CY = Canvas->SizeY * 0.5f;

	// 全屏深色毛玻璃背景
	DrawFilledRect(Canvas, 0.f, 0.f, Canvas->SizeX, Canvas->SizeY, FLinearColor(0.04f, 0.06f, 0.09f, 0.90f));

	// 顶部大标题：专用高对比度深曜石托板（Title Plaque），彻底隔绝背景杂色干扰
	const FString Title = TEXT("科目三驾驶训练");
	const FString Subtitle = TEXT("全长约 1.2km 城市考道 · 动态社会交通流 · 真实光学后视镜 · 14项训练评判");

	// 标题短化并置于 680px 宽裕底板正中，彻底杜绝任何字体 fallback 导致的边缘溢出
	const float TitleW = MeasureTextSlate(Title, BigFont, 26);
	const float TitlePlaqueW = 680.f;
	const float TitlePlaqueH = 52.f;
	const float TitlePlaqueX = CX - TitlePlaqueW * 0.5f;
	const float TitlePlaqueY = CY - 295.f;

	// 高质感深底衬 + 雅金细边框 + 顶沿微高光
	DrawRoundedCard(Canvas, TitlePlaqueX, TitlePlaqueY, TitlePlaqueW, TitlePlaqueH, FLinearColor(0.02f, 0.04f, 0.07f, 0.96f), FLinearColor(0.85f, 0.72f, 0.38f, 0.90f), 1.5f);
	DrawFilledRect(Canvas, TitlePlaqueX + 3.f, TitlePlaqueY + 2.f, TitlePlaqueW - 6.f, 2.f, FLinearColor(1.0f, 0.92f, 0.55f, 0.50f));

	DrawTextSlateLarge(Title, CX - TitleW * 0.5f, TitlePlaqueY + 12.f, FLinearColor(1.0f, 0.97f, 0.90f, 1.0f), BigFont, 26);

	// 副标题：冰青白高对比排布在托板下方
	const float SubW = SmallFont->GetStringSize(*Subtitle);
	DrawTextPixel(Subtitle, CX - SubW * 0.5f, TitlePlaqueY + TitlePlaqueH + 12.f, FLinearColor(0.78f, 0.85f, 0.95f, 0.95f), SmallFont);

	// 模式选择卡片区（双卡片并排）
	const float CardW = 440.f;
	const float CardH = 280.f;
	const float CardY = CY - 190.f;
	const float CardLeftX = CX - CardW - 25.f;
	const float CardRightX = CX + 25.f;

	// 左卡片：【引导练习模式】（新手推荐）
	DrawRoundedCard(Canvas, CardLeftX, CardY, CardW, CardH, FLinearColor(0.08f, 0.16f, 0.12f, 0.88f), ColGreen, 2.5f);
	DrawFilledRect(Canvas, CardLeftX, CardY, CardW, 38.f, FLinearColor(0.12f, 0.32f, 0.18f, 0.95f));
	DrawTextPixel(TEXT("★ 推荐新手：引导练习模式"), CardLeftX + 16.f, CardY + 8.f, ColWhite, BigFont);

	const FString PracticeHighlights[] =
	{
		TEXT("• 实时考点步骤与提示音向导，轻松掌握考规流程"),
		TEXT("• 动态道路限速智能预警，靠边停车30cm实时标尺"),
		TEXT("• 真实操作起步顺序与观察动作，不计扣分失败"),
		TEXT("• 支持随时按 T 校准后视镜，按 V 体验座舱第一人称")
	};
	float TextY = CardY + 54.f;
	for (const FString& Line : PracticeHighlights)
	{
		DrawTextPixel(Line, CardLeftX + 18.f, TextY, ColWhite, SmallFont);
		TextY += 32.f;
	}
	DrawRoundedCard(Canvas, CardLeftX + 24.f, CardY + CardH - 52.f, CardW - 48.f, 38.f, FLinearColor(0.18f, 0.52f, 0.25f, 0.95f), ColGreen, 1.5f);
	const FString StartPracticeBtn = TEXT("▶ 按 [Enter] 回车键立即开始推荐练习");
	DrawTextPixel(StartPracticeBtn, CardLeftX + (CardW - SmallFont->GetStringSize(*StartPracticeBtn)) * 0.5f, CardY + CardH - 43.f, ColWhite, SmallFont);

	// 右卡片：【模拟考试模式】（严苛评判）
	DrawRoundedCard(Canvas, CardRightX, CardY, CardW, CardH, FLinearColor(0.12f, 0.12f, 0.16f, 0.88f), ColCyan, 2.0f);
	DrawFilledRect(Canvas, CardRightX, CardY, CardW, 38.f, FLinearColor(0.18f, 0.22f, 0.32f, 0.95f));
	DrawTextPixel(TEXT("⚔ 全真模考：机动车驾驶人考场"), CardRightX + 16.f, CardY + 8.f, ColWhite, BigFont);

	const FString ExamHighlights[] =
	{
		TEXT("• 真实夜间灯光模拟随机抽考（8道必考题，文字与提示音）"),
		TEXT("• 完整14项必考评判严苛触发，真实社会车流动态交互"),
		TEXT("• 100分制严格评分，90分及格，重大违规直接扣100分"),
		TEXT("• 考验真实独立应变与驾驶习惯，真实还原考场压力")
	};
	TextY = CardY + 54.f;
	for (const FString& Line : ExamHighlights)
	{
		DrawTextPixel(Line, CardRightX + 18.f, TextY, ColGray, SmallFont);
		TextY += 32.f;
	}
	DrawRoundedCard(Canvas, CardRightX + 24.f, CardY + CardH - 52.f, CardW - 48.f, 38.f, FLinearColor(0.22f, 0.28f, 0.42f, 0.95f), ColCyan, 1.5f);
	const FString StartExamBtn = TEXT("按 [F1] 手动挡C1考试  /  [F2] 自动挡C2考试");
	DrawTextPixel(StartExamBtn, CardRightX + (CardW - SmallFont->GetStringSize(*StartExamBtn)) * 0.5f, CardY + CardH - 43.f, ColYellow, SmallFont);

	// 下方控制按键简明导览（紧凑高对比底栏）
	const float InfoW = 920.f;
	const float InfoH = 112.f;
	const float InfoX = CX - InfoW * 0.5f;
	const float InfoY = CY + 105.f;
	DrawRoundedCard(Canvas, InfoX, InfoY, InfoW, InfoH, FLinearColor(0.06f, 0.08f, 0.12f, 0.88f), ColDarkGray);

	DrawTextPixel(TEXT("【驾驶操纵】 W/S 踏板(油门/刹车)  |  A/D 转向方向盘  |  空格 手刹  |  F 安全带  |  B 鸣笛  |  M 侧头观察"), InfoX + 20.f, InfoY + 10.f, ColWhite, SmallFont);
	DrawTextPixel(TEXT("【灯光控制】 Q 左转灯  |  E 右转灯  |  L 大灯(示廓/近光/远光)  |  J 远近闪光交替  |  H 危险双闪  |  K 雾灯"), InfoX + 20.f, InfoY + 34.f, ColCyan, SmallFont);
	DrawTextPixel(TEXT("【教学辅助】 V 切换座舱/追尾视角  |  T 开启后视镜校准(Tab切镜/方向键微调/R重置)  |  Esc 暂停与恢复"), InfoX + 20.f, InfoY + 58.f, ColYellow, SmallFont);

	AExamController* EC = GetExamController();
	const UExamSaveGame* SaveGame = EC ? EC->GetSaveGame() : nullptr;
	if (SaveGame && SaveGame->GetSessionCount() > 0)
	{
		const double PassRate = SaveGame->TotalExamsCount > 0 ? (static_cast<double>(SaveGame->PassedExamsCount) / SaveGame->TotalExamsCount * 100.0) : 0.0;
		const FString ArchiveText = FString::Printf(TEXT("【学员档案】模拟考试 %d 场 · 合格率 %.1f%% · 最高 %d 分 · F3 查看训练记录与错题分析"),
			SaveGame->TotalExamsCount, PassRate, SaveGame->BestScore);
		DrawTextPixel(ArchiveText, InfoX + 20.f, InfoY + 82.f, ColGreen, SmallFont);
	}
	else
	{
		DrawTextPixel(TEXT("【学员档案】暂无历史考核记录 · 首次训练完成后将自动归档 · 按 [F3] 随时查看档案面板"), InfoX + 20.f, InfoY + 82.f, ColGray, SmallFont);
	}

	// 底部脉冲提示文字
	const float TimeSec = GetWorld()->GetTimeSeconds();
	const bool bPulse = (FMath::Fmod(TimeSec, 1.1f) < 0.65f);
	const FString BottomPrompt = TEXT("▶ 按 Enter 回车键立即开始【引导练习模式】（按 F1/F2 开启全真考试） ◀");
	DrawTextPixel(BottomPrompt, CX - SmallFont->GetStringSize(*BottomPrompt) * 0.5f, CY + 230.f, bPulse ? ColGreen : ColYellow, SmallFont);
}

void AKeMuSanHUD::DrawTopHeader(AExamController* EC, AKeMuSanPawn* Car)
{
	UFont* BigFont = GEngine->GetLargeFont();
	UFont* SmallFont = GEngine->GetMediumFont();

	const float BarW = FMath::Min(920.f, Canvas->SizeX * 0.70f);
	const float BarH = 72.f;
	const float BarX = (Canvas->SizeX - BarW) * 0.5f;
	const float BarY = 12.f;

	// 顶部条半透明底板
	DrawRoundedCard(Canvas, BarX, BarY, BarW, BarH, FLinearColor(0.04f, 0.06f, 0.09f, 0.88f), ColDarkGray);

	// 左侧考点名勋章
	const FString ItemName = EC->GetCurrentExamItemName();
	DrawRoundedCard(Canvas, BarX + 10.f, BarY + 10.f, 190.f, BarH - 20.f, FLinearColor(0.12f, 0.22f, 0.35f, 0.9f), ColCyan);
	DrawTextPixel(TEXT("当前考点"), BarX + 20.f, BarY + 13.f, ColGray, SmallFont);
	DrawTextPixel(ItemName, BarX + 20.f, BarY + 32.f, ColWhite, BigFont);

	// 右侧真实限速标牌数据与超速状态
	const float SpeedLimit = EC->GetCurrentSpeedLimit();
	const float CarSpeed = Car ? Car->GetSpeedKmh() : 0.f;
	const bool bOverSpeed = (CarSpeed > SpeedLimit + 0.5f);

	// 中间指导提示文本
	const float PromptX = BarX + 215.f;
	if (bOverSpeed)
	{
		const bool bRedPulse = (FMath::Fmod(GetWorld()->GetTimeSeconds(), 0.5f) < 0.25f);
		const FString Warning = FString::Printf(TEXT("【超速警告】车速 %.0f km/h 已超限速 (%.0f km/h)！请减速！"), CarSpeed, SpeedLimit);
		DrawTextPixel(Warning, PromptX, BarY + 24.f, bRedPulse ? ColRed : ColYellow, BigFont);
	}
	else
	{
		const FString Prompt = EC->GetPrompt();
		DrawTextPixel(Prompt, PromptX, BarY + 24.f, ColYellow, BigFont);
	}

	const float SignX = BarX + BarW - 90.f;
	const float SignY = BarY + 8.f;
	const float SignR = 56.f;

	const FLinearColor SignBg = bOverSpeed ? ((FMath::Fmod(GetWorld()->GetTimeSeconds(), 0.5f) < 0.25f) ? ColRed : ColWhite) : ColWhite;
	const FLinearColor SignBorder = ColRed;
	DrawRoundedCard(Canvas, SignX, SignY, SignR, SignR, SignBg, SignBorder, 3.5f);

	const FString SpdStr = FString::Printf(TEXT("%.0f"), SpeedLimit);
	DrawTextPixel(SpdStr, SignX + (SignR - BigFont->GetStringSize(*SpdStr)) * 0.5f, SignY + 12.f, bOverSpeed ? ColWhite : FLinearColor(0.1f, 0.1f, 0.1f), BigFont);
	DrawTextPixel(TEXT("限速"), SignX + 16.f, SignY + 36.f, bOverSpeed ? ColWhite : ColRed, SmallFont);
}

void AKeMuSanHUD::DrawStepGuide(AExamController* EC, AKeMuSanPawn* Car)
{
	if (!Car || !EC) return;
	UFont* SmallFont = GEngine->GetMediumFont();

	// 单向状态推进检测
	if (Car->IsHornHeld())
	{
		bRecordedHorn = true;
	}

	const bool bBelt = Car->IsSeatbeltFastened();
	const bool bGear = (Car->GetGear() != EGear::N && Car->GetGear() != EGear::R);
	const bool bSignal = Car->IsLeftSignalOn();
	const bool bHorn = bRecordedHorn;
	const bool bObserve = (Car->GetHeadCheckTimer() > 0.f || Car->GetLastHeadCheckTime() > 0.f);
	const bool bHandbrake = !Car->IsHandbrakeEngaged();

	struct FStepItem
	{
		FString Name;
		FString KeyHint;
		bool bDone;
	};

	FStepItem Steps[6] =
	{
		{ TEXT("1.系安全带"), TEXT("[F]"), bBelt },
		{ TEXT("2.点火挂挡"), TEXT("[1/Tab]"), bGear },
		{ TEXT("3.打左转灯"), TEXT("[Q]"), bSignal },
		{ TEXT("4.鸣喇叭"), TEXT("[B]"), bHorn },
		{ TEXT("5.侧头观察"), TEXT("[M]"), bObserve },
		{ TEXT("6.松手刹起步"), TEXT("[空格]"), bHandbrake }
	};

	const float TotalW = 840.f;
	const float X0 = (Canvas->SizeX - TotalW) * 0.5f;
	// 独立放置在 y=205.f 区域，彻底避开上方中央后视镜 (y:92~172)，保证中央镜视界完全通透
	const float Y0 = 205.f;
	const float StepW = TotalW / 6.f;

	DrawRoundedCard(Canvas, X0, Y0, TotalW, 36.f, FLinearColor(0.04f, 0.06f, 0.09f, 0.88f), ColDarkGray);

	int32 ActiveIndex = -1;
	for (int32 i = 0; i < 6; ++i)
	{
		if (!Steps[i].bDone)
		{
			ActiveIndex = i;
			break;
		}
	}

	for (int32 i = 0; i < 6; ++i)
	{
		const float ItemX = X0 + i * StepW;
		const bool bCurrent = (i == ActiveIndex);
		if (bCurrent)
		{
			DrawFilledRect(Canvas, ItemX + 2.f, Y0 + 2.f, StepW - 4.f, 32.f, FLinearColor(0.18f, 0.42f, 0.22f, 0.85f));
			DrawHollowRect(Canvas, ItemX + 2.f, Y0 + 2.f, StepW - 4.f, 32.f, 1.5f, ColGreen);
		}

		FString Text = Steps[i].bDone ? FString::Printf(TEXT("✓ %s"), *Steps[i].Name) : Steps[i].Name;
		FLinearColor Color = Steps[i].bDone ? ColGreen : (bCurrent ? ColYellow : ColGray);
		DrawTextPixel(Text, ItemX + 6.f, Y0 + 4.f, Color, SmallFont);
		if (bCurrent)
		{
			DrawTextPixel(Steps[i].KeyHint, ItemX + 6.f, Y0 + 19.f, ColCyan, SmallFont);
		}
	}
}

void AKeMuSanHUD::DrawMirrors(AKeMuSanPawn* Car)
{
	if (!Car) return;

	const float ScreenW = Canvas->SizeX;
	const float MirrorW = 210.f;
	const float MirrorH = 105.f;
	const float MirrorY = 88.f;

	// 1. 左后视镜（位于屏幕左上区域）
	const float LeftX = 22.f;
	DrawMirrorFrame(LeftX, MirrorY, MirrorW, MirrorH, TEXT("左后视镜 (Tab选择)"),
		Car->GetMirrorRenderTarget(EMirrorType::Left),
		Car->IsMirrorAdjustMode() && Car->GetActiveMirror() == EMirrorType::Left,
		Car->GetMirrorState(EMirrorType::Left));

	// 2. 车内中央后视镜（位于屏幕正中偏上）
	const float CenterW = 260.f;
	const float CenterH = 80.f;
	const float CenterX = (ScreenW - CenterW) * 0.5f;
	DrawMirrorFrame(CenterX, MirrorY + 4.f, CenterW, CenterH, TEXT("车内中央后视镜"),
		Car->GetMirrorRenderTarget(EMirrorType::Interior),
		Car->IsMirrorAdjustMode() && Car->GetActiveMirror() == EMirrorType::Interior,
		Car->GetMirrorState(EMirrorType::Interior));

	// 3. 右后视镜（位于屏幕右上区域）
	const float RightX = ScreenW - MirrorW - 22.f;
	DrawMirrorFrame(RightX, MirrorY, MirrorW, MirrorH, TEXT("右后视镜 (Tab选择)"),
		Car->GetMirrorRenderTarget(EMirrorType::Right),
		Car->IsMirrorAdjustMode() && Car->GetActiveMirror() == EMirrorType::Right,
		Car->GetMirrorState(EMirrorType::Right));
}

void AKeMuSanHUD::DrawMirrorFrame(float X, float Y, float W, float H, const FString& Label, UTextureRenderTarget2D* Target, bool bActive, const FMirrorOpticalState& State)
{
	UFont* SmallFont = GEngine->GetMediumFont();

	// 底板背景
	const FLinearColor BorderCol = bActive ? ColGold : (State.bStandardAdjusted ? ColDarkGray : ColOrange);
	const float BorderThick = bActive ? 3.0f : 1.5f;
	DrawRoundedCard(Canvas, X, Y, W, H, FLinearColor(0.02f, 0.03f, 0.05f, 0.95f), BorderCol, BorderThick);

	// 真实光学 Texture 绘制（水平镜像翻转）
	if (Target && Target->GetResource())
	{
		const float ImgX = X + 2.f;
		const float ImgY = Y + 20.f;
		const float ImgW = W - 4.f;
		const float ImgH = H - 22.f;

		FCanvasTileItem Tile(FVector2D(ImgX, ImgY), Target->GetResource(), FVector2D(ImgW, ImgH), FLinearColor::White);
		Tile.BlendMode = SE_BLEND_Opaque;
		// 水平镜像翻转：左对应右
		Tile.UV0 = FVector2D(1.f, 0.f);
		Tile.UV1 = FVector2D(0.f, 1.f);
		Canvas->DrawItem(Tile);
	}
	else
	{
		DrawTextPixel(TEXT("[光学捕获就绪]"), X + 35.f, Y + H * 0.5f, ColGray, SmallFont);
	}

	// 顶部标签条
	DrawFilledRect(Canvas, X + 2.f, Y + 2.f, W - 4.f, 18.f, FLinearColor(0.06f, 0.08f, 0.12f, 0.90f));
	DrawTextPixel(Label, X + 6.f, Y + 2.f, bActive ? ColGold : ColWhite, SmallFont);

	// 状态角标（右侧）
	if (State.bStandardAdjusted)
	{
		DrawTextPixel(TEXT("✓ 标准到位"), X + W - 76.f, Y + 2.f, ColGreen, SmallFont);
	}
	else
	{
		DrawTextPixel(TEXT("! 需调整"), X + W - 62.f, Y + 2.f, ColOrange, SmallFont);
	}

	// 激活时的视线十字标尺
	if (bActive)
	{
		const float MidX = X + W * 0.5f;
		const float MidY = Y + H * 0.5f;
		DrawFilledRect(Canvas, MidX - 12.f, MidY, 24.f, 1.f, FLinearColor(1.f, 0.85f, 0.15f, 0.8f));
		DrawFilledRect(Canvas, MidX, MidY - 12.f, 1.f, 24.f, FLinearColor(1.f, 0.85f, 0.15f, 0.8f));
	}
}

void AKeMuSanHUD::DrawMirrorAdjustOverlay(AKeMuSanPawn* Car)
{
	if (!Car) return;
	UFont* BigFont = GEngine->GetLargeFont();
	UFont* SmallFont = GEngine->GetMediumFont();

	const float PanelW = 720.f;
	const float PanelH = 115.f;
	const float PanelX = (Canvas->SizeX - PanelW) * 0.5f;
	const float PanelY = Canvas->SizeY - 315.f;

	DrawRoundedCard(Canvas, PanelX, PanelY, PanelW, PanelH, FLinearColor(0.05f, 0.08f, 0.14f, 0.95f), ColGold, 2.0f);

	const FMirrorOpticalState& State = Car->GetMirrorState(Car->GetActiveMirror());
	FString MirrorName;
	switch (Car->GetActiveMirror())
	{
	case EMirrorType::Left: MirrorName = TEXT("左侧后视镜"); break;
	case EMirrorType::Right: MirrorName = TEXT("右侧后视镜"); break;
	default: MirrorName = TEXT("车内后视镜"); break;
	}

	DrawTextPixel(FString::Printf(TEXT("【后视镜校准工作台】正在调整【%s】"), *MirrorName), PanelX + 16.f, PanelY + 12.f, ColGold, BigFont);
	DrawTextPixel(FString::Printf(TEXT("规范指引：%s"), *State.Tip), PanelX + 16.f, PanelY + 44.f, State.bStandardAdjusted ? ColGreen : ColYellow, SmallFont);

	const FString ShortHints = TEXT("快捷操控：[Tab] 切换镜面  |  [↑↓←→] 方向键微调  |  [R] 一键恢复标准视角  |  [T] 退出保存");
	DrawTextPixel(ShortHints, PanelX + 16.f, PanelY + 80.f, ColWhite, SmallFont);
}

void AKeMuSanHUD::DrawModernDashboard(AKeMuSanPawn* Car, AExamController* EC)
{
	if (!Car) return;
	UFont* BigFont = GEngine->GetLargeFont();
	UFont* SmallFont = GEngine->GetMediumFont();

	const float DashW = 460.f;
	const float DashH = 165.f;
	const float DashX = 18.f;
	const float DashY = Canvas->SizeY - DashH - 28.f;

	// 高质感深黑半透明底板
	DrawRoundedCard(Canvas, DashX, DashY, DashW, DashH, FLinearColor(0.04f, 0.06f, 0.08f, 0.92f), ColDarkGray);

	// 1. 大号数字车速（纯白高亮，卡片底板内杜绝黑影扩散发虚）
	const float Speed = Car->GetSpeedKmh();
	Canvas->SetLinearDrawColor(ColWhite);
	Canvas->DrawText(BigFont, *FString::Printf(TEXT("%.0f"), Speed), DashX + 18.f, DashY + 10.f, 2.0f, 2.0f, FFontRenderInfo());
	DrawTextPixel(TEXT("km/h"), DashX + 120.f, DashY + 48.f, ColGray, SmallFont);

	// 2. 挡位方块
	FString GearStr;
	if (Car->GetTransmissionType() == ETransmissionType::Auto)
	{
		switch (Car->GetGear())
		{
		case EGear::N: GearStr = TEXT("N"); break;
		case EGear::R: GearStr = TEXT("R"); break;
		default: GearStr = TEXT("D"); break;
		}
	}
	else
	{
		switch (Car->GetGear())
		{
		case EGear::N: GearStr = TEXT("N"); break;
		case EGear::R: GearStr = TEXT("R"); break;
		case EGear::G1: GearStr = TEXT("1"); break;
		case EGear::G2: GearStr = TEXT("2"); break;
		case EGear::G3: GearStr = TEXT("3"); break;
		case EGear::G4: GearStr = TEXT("4"); break;
		case EGear::G5: GearStr = TEXT("5"); break;
		}
	}

	const float GearBoxX = DashX + 205.f;
	const float GearBoxY = DashY + 14.f;
	const float GearBoxS = 54.f;
	const bool bStall = Car->IsStalled();
	DrawRoundedCard(Canvas, GearBoxX, GearBoxY, GearBoxS, GearBoxS, bStall ? FLinearColor(0.5f, 0.08f, 0.08f, 0.95f) : FLinearColor(0.12f, 0.16f, 0.22f, 0.9f), bStall ? ColRed : ColGold);
	Canvas->SetLinearDrawColor(bStall ? ColWhite : ColYellow);
	Canvas->DrawText(BigFont, *GearStr, GearBoxX + 18.f, GearBoxY + 10.f, 1.6f, 1.6f, FFontRenderInfo());
	if (bStall)
	{
		DrawTextPixel(TEXT("熄火!"), GearBoxX + 8.f, GearBoxY + 35.f, ColWhite, SmallFont);
	}

	// 3. 转速条与踏板开度
	const float Rpm = Car->GetEngineRpm();
	const float RpmRatio = FMath::Clamp(Rpm / 6000.f, 0.f, 1.f);
	const float BarX = DashX + 275.f;
	const float BarY = DashY + 16.f;
	const float BarW = 165.f;

	DrawTextPixel(FString::Printf(TEXT("转速: %.0f RPM"), Rpm), BarX, BarY, ColGray, SmallFont);
	DrawFilledRect(Canvas, BarX, BarY + 18.f, BarW, 8.f, FLinearColor(0.15f, 0.18f, 0.22f, 0.85f));
	DrawFilledRect(Canvas, BarX, BarY + 18.f, BarW * RpmRatio, 8.f, (Rpm > 4500.f) ? ColRed : ColCyan);

	// 油门与刹车踏板指示条
	const float Thr = Car->GetThrottleInput();
	const float Brk = Car->GetBrakeInput();
	DrawFilledRect(Canvas, BarX, BarY + 34.f, BarW * 0.48f, 6.f, FLinearColor(0.15f, 0.18f, 0.22f, 0.85f));
	DrawFilledRect(Canvas, BarX, BarY + 34.f, BarW * 0.48f * Thr, 6.f, ColGreen);
	DrawFilledRect(Canvas, BarX + BarW * 0.52f, BarY + 34.f, BarW * 0.48f, 6.f, FLinearColor(0.15f, 0.18f, 0.22f, 0.85f));
	DrawFilledRect(Canvas, BarX + BarW * 0.52f, BarY + 34.f, BarW * 0.48f * Brk, 6.f, ColRed);
	DrawTextPixel(TEXT("油门"), BarX, BarY + 43.f, ColGray, SmallFont);
	DrawTextPixel(TEXT("刹车"), BarX + BarW * 0.52f, BarY + 43.f, ColGray, SmallFont);

	// 4. 底部车辆状态指示灯排
	const float IndY = DashY + 88.f;
	const float IndH = 28.f;
	const float IndGap = 6.f;

	auto DrawIndicator = [&](float InX, float InW, const FString& Label, bool bLit, const FLinearColor& LitColor)
	{
		DrawRoundedCard(Canvas, InX, IndY, InW, IndH, bLit ? LitColor : FLinearColor(0.08f, 0.10f, 0.14f, 0.7f), bLit ? LitColor : ColDarkGray, 1.0f);
		DrawTextPixel(Label, InX + 6.f, IndY + 5.f, bLit ? ColWhite : ColGray, SmallFont);
	};

	float CurrentIndX = DashX + 16.f;
	DrawIndicator(CurrentIndX, 64.f, TEXT("安全带"), Car->IsSeatbeltFastened(), ColGreen); CurrentIndX += 64.f + IndGap;
	DrawIndicator(CurrentIndX, 50.f, TEXT("手刹"), Car->IsHandbrakeEngaged(), ColRed); CurrentIndX += 50.f + IndGap;
	DrawIndicator(CurrentIndX, 50.f, TEXT("近光"), Car->IsLowBeamOn(), ColCyan); CurrentIndX += 50.f + IndGap;
	DrawIndicator(CurrentIndX, 50.f, TEXT("远光"), Car->IsHighBeamOn(), FLinearColor(0.3f, 0.5f, 1.0f)); CurrentIndX += 50.f + IndGap;
	DrawIndicator(CurrentIndX, 50.f, TEXT("雾灯"), Car->IsFogLampOn(), ColOrange); CurrentIndX += 50.f + IndGap;

	const bool bBlinkOn = (FMath::Fmod(GetWorld()->GetTimeSeconds(), 0.7f) < 0.35f);
	DrawIndicator(CurrentIndX, 60.f, TEXT("←左转"), Car->IsLeftSignalOn() && bBlinkOn, ColYellow); CurrentIndX += 60.f + IndGap;
	DrawIndicator(CurrentIndX, 60.f, TEXT("右转→"), Car->IsRightSignalOn() && bBlinkOn, ColYellow);

	// 5. 视角指示条
	const FString ViewStr = Car->IsCockpitView() ? TEXT("【当前：第一人称座舱 (按V切换)】") : TEXT("【当前：第三人称追尾 (按V切换)】");
	DrawTextPixel(ViewStr, DashX + 16.f, DashY + DashH - 24.f, ColGray, SmallFont);
}

void AKeMuSanHUD::DrawPullOverRadar(AExamController* EC)
{
	if (!EC) return;
	UFont* BigFont = GEngine->GetLargeFont();
	UFont* SmallFont = GEngine->GetMediumFont();

	const float RadarW = 500.f;
	const float RadarH = 92.f;
	const float RadarX = (Canvas->SizeX - RadarW) * 0.5f;
	const float RadarY = Canvas->SizeY - 280.f;

	DrawRoundedCard(Canvas, RadarX, RadarY, RadarW, RadarH, FLinearColor(0.04f, 0.06f, 0.10f, 0.94f), ColGold, 2.0f);

	const float EdgeDistM = EC->GetCurrentEdgeDistance();
	const float EdgeDistCm = EdgeDistM * 100.f;

	FString StatusText;
	FLinearColor StatusColor;
	if (EdgeDistCm < 0.f)
	{
		StatusText = TEXT("压道路边缘线！[扣100分]");
		StatusColor = ColRed;
	}
	else if (EdgeDistCm <= 30.f)
	{
		StatusText = TEXT("标准到位：距边缘 ≤ 30cm [合格满分] ✓");
		StatusColor = ColGreen;
	}
	else if (EdgeDistCm <= 50.f)
	{
		StatusText = TEXT("距边缘 30~50cm [扣10分]");
		StatusColor = ColYellow;
	}
	else
	{
		StatusText = TEXT("超距：距边缘 > 50cm [扣100分]");
		StatusColor = ColRed;
	}

	DrawTextPixel(TEXT("靠边停车路缘标尺监控（国标要求：右侧车轮距路沿 ≤ 30cm）"), RadarX + 14.f, RadarY + 8.f, ColWhite, SmallFont);
	DrawTextPixel(FString::Printf(TEXT("实测边距: %.1f cm   %s"), EdgeDistCm, *StatusText), RadarX + 14.f, RadarY + 32.f, StatusColor, BigFont);

	// 渐变刻度条（范围 0 ~ 80 cm）
	const float SlotX = RadarX + 14.f;
	const float SlotY = RadarY + 64.f;
	const float SlotW = RadarW - 28.f;
	const float SlotH = 14.f;

	DrawFilledRect(Canvas, SlotX, SlotY, SlotW, SlotH, FLinearColor(0.12f, 0.14f, 0.18f, 0.85f));
	// 0 ~ 30cm 绿色标尺
	DrawFilledRect(Canvas, SlotX, SlotY, SlotW * (30.f / 80.f), SlotH, FLinearColor(0.2f, 0.8f, 0.3f, 0.6f));
	// 30 ~ 50cm 黄色标尺
	DrawFilledRect(Canvas, SlotX + SlotW * (30.f / 80.f), SlotY, SlotW * (20.f / 80.f), SlotH, FLinearColor(0.9f, 0.7f, 0.1f, 0.6f));
	// 50 ~ 80cm 红色标尺
	DrawFilledRect(Canvas, SlotX + SlotW * (50.f / 80.f), SlotY, SlotW * (30.f / 80.f), SlotH, FLinearColor(0.8f, 0.2f, 0.2f, 0.6f));

	// 当前指针滑块
	const float PointerRatio = FMath::Clamp(EdgeDistCm / 80.f, 0.f, 1.f);
	const float PointerX = SlotX + SlotW * PointerRatio;
	DrawFilledRect(Canvas, PointerX - 2.f, SlotY - 3.f, 5.f, SlotH + 6.f, ColWhite);
}

void AKeMuSanHUD::DrawScorePanel(AExamController* EC)
{
	UFont* BigFont = GEngine->GetLargeFont();
	UFont* SmallFont = GEngine->GetMediumFont();

	const float PanelW = 210.f;
	const float PanelX = Canvas->SizeX - PanelW - 18.f;
	const float PanelY = 205.f;

	const int32 Score = EC->GetScore();
	const bool bPass = (Score >= 90);

	DrawRoundedCard(Canvas, PanelX, PanelY, PanelW, 115.f, FLinearColor(0.04f, 0.06f, 0.09f, 0.88f), ColDarkGray);

	DrawTextPixel(TEXT("考试成绩"), PanelX + 14.f, PanelY + 10.f, ColGray, SmallFont);
	Canvas->SetLinearDrawColor(bPass ? ColGreen : ColRed);
	Canvas->DrawText(BigFont, *FString::Printf(TEXT("%d"), Score), PanelX + 14.f, PanelY + 28.f, 1.8f, 1.8f, FFontRenderInfo());
	DrawTextPixel(TEXT("分"), PanelX + 90.f, PanelY + 48.f, ColGray, SmallFont);

	if (EC->IsPractice())
	{
		DrawTextPixel(TEXT("【引导练习 · 不记败】"), PanelX + 14.f, PanelY + 86.f, ColCyan, SmallFont);
	}
	else
	{
		DrawTextPixel(bPass ? TEXT("【当前成绩合格】") : TEXT("【当前成绩不合格】"), PanelX + 14.f, PanelY + 86.f, bPass ? ColGreen : ColRed, SmallFont);
	}

	// 最近扣分浮动日志
	const TArray<FDeduction>& Deds = EC->GetDeductions();
	const int32 NumToShow = FMath::Min(Deds.Num(), 4);
	if (NumToShow > 0)
	{
		float DedY = PanelY + 125.f;
		for (int32 i = 0; i < NumToShow; ++i)
		{
			const FDeduction& D = Deds[Deds.Num() - 1 - i];
			DrawRoundedCard(Canvas, PanelX - 80.f, DedY, PanelW + 80.f, 28.f, FLinearColor(0.45f, 0.08f, 0.08f, 0.90f), ColRed);
			DrawTextPixel(FString::Printf(TEXT("-%d分  %s"), D.Points, *D.Reason), PanelX - 74.f, DedY + 4.f, ColWhite, SmallFont);
			DedY += 32.f;
		}
	}
}

void AKeMuSanHUD::DrawProgressList(AExamController* EC)
{
	if (!EC) return;
	UFont* SmallFont = GEngine->GetMediumFont();
	const TArray<FZoneStatus>& Zones = EC->GetZoneStatuses();
	const int32 TotalZones = Zones.Num();
	if (TotalZones == 0) return;

	// 统计完成与跳过项
	int32 CompletedCount = 0;
	int32 SkippedCount = 0;
	int32 ActiveIndex = INDEX_NONE;

	// 第一优先级：查找实际正在考核中 (State == 1) 的考点
	for (int32 i = 0; i < TotalZones; ++i)
	{
		if (Zones[i].State == 2)
		{
			CompletedCount++;
		}
		else if (Zones[i].State == 3)
		{
			SkippedCount++;
		}
		else if (Zones[i].State == 1 && ActiveIndex == INDEX_NONE)
		{
			ActiveIndex = i;
		}
	}

	// 第二优先级：若无进行中项目，查找首个适用且未完成 (State == 0) 的考点
	if (ActiveIndex == INDEX_NONE)
	{
		for (int32 i = 0; i < TotalZones; ++i)
		{
			if (Zones[i].State == 0)
			{
				ActiveIndex = i;
				break;
			}
		}
	}

	const float ListW = 200.f;
	const float ListH = 116.f;
	const float ListX = 18.f;
	const float ListY = 205.f;

	// 紧凑高质感底板，高度仅116px，消除遮挡
	DrawRoundedCard(Canvas, ListX, ListY, ListW, ListH, FLinearColor(0.04f, 0.06f, 0.09f, 0.90f), ColDarkGray);

	// 顶部：进度概览与细微进度条（明确标识跳过项，绝不冒充合格通过）
	FString ProgressHeader;
	if (SkippedCount > 0)
	{
		ProgressHeader = FString::Printf(TEXT("考核进度: %d/%d (跳过%d)"), CompletedCount, TotalZones, SkippedCount);
	}
	else
	{
		ProgressHeader = FString::Printf(TEXT("考核进度: %d / %d 项"), CompletedCount, TotalZones);
	}
	DrawTextPixel(ProgressHeader, ListX + 12.f, ListY + 8.f, ColCyan, SmallFont);

	const float BarW = ListW - 24.f;
	DrawFilledRect(Canvas, ListX + 12.f, ListY + 28.f, BarW, 4.f, FLinearColor(0.15f, 0.18f, 0.22f, 0.85f));
	const float Ratio = FMath::Clamp(static_cast<float>(CompletedCount) / static_cast<float>(TotalZones), 0.f, 1.f);
	DrawFilledRect(Canvas, ListX + 12.f, ListY + 28.f, BarW * Ratio, 4.f, ColGreen);

	// 中部：当前进行中考点（高亮醒目黄/绿）
	DrawTextPixel(TEXT("当前:"), ListX + 12.f, ListY + 40.f, ColGray, SmallFont);
	if (ActiveIndex != INDEX_NONE && Zones.IsValidIndex(ActiveIndex))
	{
		const FString CurName = Zones[ActiveIndex].Name;
		FString CurStateStr;
		FLinearColor StateCol = ColYellow;
		if (Zones[ActiveIndex].State == 1)
		{
			CurStateStr = TEXT("[考核中]");
			StateCol = ColGreen;
		}
		else if (Zones[ActiveIndex].State == 3)
		{
			CurStateStr = TEXT("[练习跳过]");
			StateCol = ColGray;
		}
		else
		{
			CurStateStr = TEXT("[待到达]");
			StateCol = ColYellow;
		}
		DrawTextPixel(FString::Printf(TEXT("▶ %d.%s %s"), ActiveIndex + 1, *CurName, *CurStateStr), ListX + 12.f, ListY + 58.f, StateCol, SmallFont);

		// 下部：查找后续首个未完成且未跳过的项目作为预告
		int32 NextIndex = INDEX_NONE;
		for (int32 j = ActiveIndex + 1; j < TotalZones; ++j)
		{
			if (Zones[j].State != 2 && Zones[j].State != 3)
			{
				NextIndex = j;
				break;
			}
		}

		if (NextIndex != INDEX_NONE && Zones.IsValidIndex(NextIndex))
		{
			DrawTextPixel(FString::Printf(TEXT("下一项: %d.%s"), NextIndex + 1, *Zones[NextIndex].Name), ListX + 12.f, ListY + 86.f, ColGray, SmallFont);
		}
		else
		{
			DrawTextPixel(TEXT("路线终点：按要求平稳靠边停直"), ListX + 12.f, ListY + 86.f, ColGreen, SmallFont);
		}
	}
	else
	{
		// 全部完成
		DrawTextPixel(TEXT("▶ 全部训练项目已完成"), ListX + 12.f, ListY + 58.f, ColGreen, SmallFont);
		DrawTextPixel(TEXT("路线终点：按要求平稳靠边停直"), ListX + 12.f, ListY + 86.f, ColGreen, SmallFont);
	}
}

void AKeMuSanHUD::DrawMiniMap(AExamController* EC, AKeMuSanPawn* Car)
{
	if (!EC || !Canvas || !Car) return;
	UFont* Font = GEngine->GetMediumFont();
	const float W = 180.f;
	const float H = 120.f;
	const float X = Canvas->SizeX - W - 18.f;
	const float Y = Canvas->SizeY - H - 30.f;
	DrawRoundedCard(Canvas, X, Y, W, H, FLinearColor(0.04f, 0.06f, 0.09f, 0.92f), ColDarkGray);
	DrawTextPixel(TEXT("考道 · 实际位置"), X + 10.f, Y + 6.f, ColCyan, Font);
	// Use the same sampled geometry as the examiner, including both U-turn arcs and the return lane.
	static const FRouteTrack MapTrack = []() { FRouteTrack Built; Built.Build(); return Built; }();
	FVector2D Min(TNumericLimits<float>::Max(), TNumericLimits<float>::Max());
	FVector2D Max(TNumericLimits<float>::Lowest(), TNumericLimits<float>::Lowest());
	for (int32 Index = 0; Index < MapTrack.Num(); ++Index)
	{
		const FVector& P = MapTrack.GetSample(Index).Pos;
		Min.X = FMath::Min(Min.X, P.X); Min.Y = FMath::Min(Min.Y, P.Y);
		Max.X = FMath::Max(Max.X, P.X); Max.Y = FMath::Max(Max.Y, P.Y);
	}
	const float Scale = FMath::Min((W - 28.f) / FMath::Max(1.f, Max.X - Min.X), (H - 44.f) / FMath::Max(1.f, Max.Y - Min.Y));
	const FVector2D Origin(X + (W - (Max.X - Min.X) * Scale) * 0.5f, Y + 30.f + (H - 44.f - (Max.Y - Min.Y) * Scale) * 0.5f);
	auto ToMap = [&](const FVector& P) { return Origin + FVector2D((P.X - Min.X) * Scale, (Max.Y - P.Y) * Scale); };
	for (int32 Index = 0; Index + 1 < MapTrack.Num(); Index += 8)
	{
		const int32 Next = FMath::Min(Index + 8, MapTrack.Num() - 1);
		FCanvasLineItem Line(ToMap(MapTrack.GetSample(Index).Pos), ToMap(MapTrack.GetSample(Next).Pos));
		Line.SetColor(MapTrack.GetSample(Index).bReturn ? ColOrange : ColGray);
		Line.LineThickness = 2.f;
		Canvas->DrawItem(Line);
	}
	FVector2D Dot = ToMap(Car->GetActorLocation() / 100.f);
	Dot.X = FMath::Clamp(Dot.X, X + 5.f, X + W - 5.f);
	Dot.Y = FMath::Clamp(Dot.Y, Y + 28.f, Y + H - 5.f);
	DrawFilledRect(Canvas, Dot.X - 3.f, Dot.Y - 3.f, 6.f, 6.f, ColGreen);
	const FVector Forward = Car->GetActorForwardVector();
	FCanvasLineItem Heading(Dot, Dot + FVector2D(Forward.X, -Forward.Y) * 9.f);
	Heading.SetColor(ColGreen);
	Heading.LineThickness = 2.f;
	Canvas->DrawItem(Heading);
}

void AKeMuSanHUD::DrawLightTestPanel(AExamController* EC)
{
	UFont* BigFont = GEngine->GetLargeFont();
	UFont* SmallFont = GEngine->GetMediumFont();

	const float W = Canvas->SizeX * 0.68f;
	const float H = 320.f;
	const float X = (Canvas->SizeX - W) * 0.5f;
	const float Y = Canvas->SizeY * 0.5f - 110.f;

	DrawRoundedCard(Canvas, X, Y, W, H, FLinearColor(0.04f, 0.06f, 0.10f, 0.95f), ColGold, 2.5f);

	const FLightQuestion* Q = EC->GetCurrentLightQuestion();
	if (Q)
	{
		DrawTextPixel(FString::Printf(TEXT("夜间模拟灯光考试 · 第 %d / %d 题"), EC->GetLightQuestionIndex(), EC->GetLightQuestionTotal()), X + 28.f, Y + 16.f, ColGray, SmallFont);
		DrawTextPixel(Q->Text, X + 28.f, Y + 46.f, ColYellow, BigFont);
	}
	else
	{
		DrawTextPixel(TEXT("正在调取下一道夜间灯光指令…"), X + 28.f, Y + 46.f, ColYellow, BigFont);
	}

	const FString Options[] =
	{
		TEXT("[1] 开启近光灯"),
		TEXT("[2] 开启远光灯"),
		TEXT("[3] 远近光灯交替闪烁两次"),
		TEXT("[4] 开启示廓灯 + 危险报警闪光灯"),
		TEXT("[5] 开启雾灯 + 危险报警闪光灯")
	};
	for (int32 i = 0; i < 5; ++i)
	{
		DrawTextPixel(Options[i], X + 48.f, Y + 96.f + i * 36.f, ColWhite, SmallFont);
	}

	DrawTextPixel(FString::Printf(TEXT("作答应变倒计时：%.1f 秒"), FMath::Max(0.f, EC->GetLightCountdown())), X + W - 280.f, Y + H - 42.f, ColCyan, BigFont);
}

void AKeMuSanHUD::DrawResultPanel(AExamController* EC, AKeMuSanGameMode* GM)
{
	if (!Canvas || !EC) return;
	UFont* Font = GEngine->GetMediumFont();
	UFont* TitleFont = GEngine->GetLargeFont();
	const bool bPractice = EC->IsPractice();
	const bool bPass = !bPractice && !EC->IsFailIssued() && EC->GetScore() >= 90;
	const FLinearColor Accent = bPractice ? ColCyan : (bPass ? ColGreen : ColRed);
	const float W = FMath::Min(1060.f, Canvas->SizeX - 40.f);
	const float H = FMath::Min(610.f, Canvas->SizeY - 40.f);
	const float X = (Canvas->SizeX - W) * 0.5f;
	const float Y = (Canvas->SizeY - H) * 0.5f;
	const float ColW = (W - 54.f) * 0.5f;
	const float LX = X + 18.f;
	const float RX = LX + ColW + 18.f;
	const float CY = Y + 100.f;
	const float CH = H - 192.f;
	DrawRoundedCard(Canvas, X, Y, W, H, FLinearColor(0.04f, 0.06f, 0.09f, 0.97f), Accent, 2.f);
	const FString Title = bPractice ? TEXT("引导练习完成 · 训练复盘") : (bPass ? TEXT("模拟考试合格 · 训练复盘") : TEXT("模拟考试未合格 · 训练复盘"));
	DrawTextSlateLarge(Title, X + 24.f, Y + 18.f, Accent, TitleFont, 26);
	const FExamAnalysisResult& Analysis = EC->GetLastAnalysisResult();
	DrawTextWrapped(bPractice ? TEXT("练习仅记录训练过程；不计算模拟考试得分、合格率或最高成绩。") : Analysis.PerformanceRating,
		X + 24.f, Y + 60.f, W - 48.f, 1, ColGray, Font);
	DrawRoundedCard(Canvas, LX, CY, ColW, CH, FLinearColor(0.07f, 0.09f, 0.14f, 0.95f), ColDarkGray);
	DrawRoundedCard(Canvas, RX, CY, ColW, CH, FLinearColor(0.07f, 0.09f, 0.14f, 0.95f), ColGold);
	DrawTextPixel(TEXT("失误分布与训练重点"), LX + 16.f, CY + 12.f, ColCyan, Font);
	DrawTextSlateLarge(bPractice ? TEXT("练习记录") : FString::Printf(TEXT("%d 分 / 100 分"), EC->GetScore()), LX + 20.f, CY + 40.f, Accent, TitleFont, 28);
	auto DrawCategory = [&](float RowY, const FString& Name, int32 Points, int32 Count, const FLinearColor& Color)
	{
		DrawTextPixel(FString::Printf(TEXT("%s  ·  -%d 分 / %d 次"), *Name, Points, Count), LX + 20.f, RowY, Points > 0 ? Color : ColGray, Font);
		DrawFilledRect(Canvas, LX + 20.f, RowY + 23.f, ColW - 40.f, 5.f, FLinearColor(0.16f, 0.18f, 0.23f));
		if (Points > 0) DrawFilledRect(Canvas, LX + 20.f, RowY + 23.f, (ColW - 40.f) * FMath::Clamp(Points / 100.f, 0.f, 1.f), 5.f, Color);
	};
	float RowY = CY + 90.f;
	DrawCategory(RowY, TEXT("安全观察"), Analysis.ObservationDeductions, Analysis.ObservationCount, ColRed);
	DrawCategory(RowY + 41.f, TEXT("灯光信号"), Analysis.LightingDeductions, Analysis.LightingCount, ColYellow);
	DrawCategory(RowY + 82.f, TEXT("车辆操纵"), Analysis.VehicleControlDeductions, Analysis.VehicleControlCount, ColCyan);
	DrawCategory(RowY + 123.f, TEXT("路权规范"), Analysis.RulesAndWayDeductions, Analysis.RulesAndWayCount, ColOrange);
	DrawTextWrapped(FString::Printf(TEXT("训练重点：%s"), *Analysis.PrimaryWeaknessName), LX + 20.f, CY + CH - 98.f, ColW - 40.f, 2, ColYellow, Font);
	if (Analysis.OtherDeductions > 0) DrawTextPixel(FString::Printf(TEXT("其他综合失误：-%d 分"), Analysis.OtherDeductions), LX + 20.f, CY + CH - 44.f, ColRed, Font);
	else DrawTextWrapped(bPractice ? TEXT("按 F3 可查看本场用时、实际行驶距离与历史记录。") : TEXT("按 F3 选择本场记录，查看全部扣分与对应时间。"), LX + 20.f, CY + CH - 46.f, ColW - 40.f, 2, ColGray, Font);

	DrawTextPixel(TEXT("教练复盘与扣分明细"), RX + 16.f, CY + 12.f, ColGold, Font);
	float AdviceY = CY + 40.f;
	const int32 NumAdvices = FMath::Min(Analysis.CoachAdvices.Num(), 2);
	for (int32 Index = 0; Index < NumAdvices; ++Index)
	{
		DrawRoundedCard(Canvas, RX + 14.f, AdviceY, ColW - 28.f, 82.f, FLinearColor(0.10f, 0.13f, 0.19f), ColDarkGray);
		DrawTextWrapped(Analysis.CoachAdvices[Index], RX + 24.f, AdviceY + 9.f, ColW - 48.f, 3, ColWhite, Font);
		AdviceY += 90.f;
	}
	DrawTextPixel(TEXT("本场最近扣分"), RX + 18.f, AdviceY + 3.f, ColGray, Font);
	const TArray<FDeduction>& Deds = EC->GetDeductions();
	float ItemY = AdviceY + 28.f;
	if (Deds.IsEmpty())
	{
		DrawTextWrapped(bPractice ? TEXT("引导练习不执行考试扣分，完成训练后可进入模拟考试检验。") : TEXT("本场无扣分记录。"), RX + 20.f, ItemY, ColW - 40.f, 2, bPractice ? ColGray : ColGreen, Font);
	}
	else
	{
		const int32 AvailableRows = FMath::Max(0, FMath::FloorToInt((CY + CH - 14.f - ItemY) / 42.f));
		for (int32 Index = 0; Index < FMath::Min(Deds.Num(), AvailableRows); ++Index)
		{
			const FDeduction& D = Deds[Deds.Num() - 1 - Index];
			DrawTextWrapped(FString::Printf(TEXT("- %d 分 · %.1f 秒  %s"), D.Points, D.TimeSeconds, *D.Reason), RX + 20.f, ItemY, ColW - 40.f, 2, ColRed, Font, 20.f);
			ItemY += 42.f;
		}
	}
	DrawTextWrapped(EC->GetArchiveStatusText(), X + 24.f, Y + H - 76.f, W - 48.f, 1, ColGray, Font);
	DrawTextWrapped(TEXT("Enter 重新开始    F3 学员档案 / 完整扣分详情"), X + 24.f, Y + H - 42.f, W - 48.f, 1, ColYellow, Font);
}

void AKeMuSanHUD::DrawHistoryAnalysisPanel(AExamController* EC)
{
	if (!Canvas || !EC) return;
	AKeMuSanPlayerController* PC = Cast<AKeMuSanPlayerController>(GetWorld()->GetFirstPlayerController());
	if (PC && PC->IsShowingHistoryDetails())
	{
		DrawHistorySessionDetails(EC);
		return;
	}
	UFont* Font = GEngine->GetMediumFont();
	UFont* TitleFont = GEngine->GetLargeFont();
	DrawFilledRect(Canvas, 0.f, 0.f, Canvas->SizeX, Canvas->SizeY, FLinearColor(0.02f, 0.03f, 0.05f, 0.92f));
	const float W = FMath::Min(1060.f, Canvas->SizeX - 40.f);
	const float H = FMath::Min(620.f, Canvas->SizeY - 40.f);
	const float X = (Canvas->SizeX - W) * 0.5f;
	const float Y = (Canvas->SizeY - H) * 0.5f;
	DrawRoundedCard(Canvas, X, Y, W, H, FLinearColor(0.05f, 0.07f, 0.11f, 0.99f), ColCyan, 2.f);
	DrawTextSlateLarge(TEXT("学员档案 · 每一次练习都有记录"), X + 22.f, Y + 18.f, ColCyan, TitleFont, 26);
	const UExamSaveGame* Save = EC->GetSaveGame();
	if (!Save || Save->GetSessionCount() == 0)
	{
		DrawTextWrapped(Save ? TEXT("暂无训练记录。完成练习或模拟考试后会自动保存，重新打开项目也能继续查看。") : EC->GetArchiveStatusText(),
			X + 30.f, Y + 116.f, W - 60.f, 3, ColGray, Font);
		DrawTextPixel(TEXT("F3 / Esc / Enter 关闭档案"), X + 30.f, Y + H - 42.f, ColYellow, Font);
		return;
	}
	const float CardW = (W - 61.f) / 4.f;
	const float StatsY = Y + 68.f;
	auto Stat = [&](int32 Index, const FString& Label, const FString& Value, const FLinearColor& ValueColor)
	{
		const float SX = X + 20.f + Index * (CardW + 7.f);
		DrawRoundedCard(Canvas, SX, StatsY, CardW, 68.f, FLinearColor(0.09f, 0.12f, 0.18f), ColDarkGray);
		DrawTextPixel(Label, SX + 12.f, StatsY + 8.f, ColGray, Font);
		DrawTextSlateLarge(Value, SX + 12.f, StatsY + 30.f, ValueColor, TitleFont, 24);
	};
	const float PassRate = Save->TotalExamsCount > 0 ? 100.f * Save->PassedExamsCount / Save->TotalExamsCount : 0.f;
	Stat(0, TEXT("训练总场次（考试 + 练习）"), FString::Printf(TEXT("%d 场"), Save->GetSessionCount()), ColWhite);
	Stat(1, TEXT("模拟考试合格 / 已考"), FString::Printf(TEXT("%d / %d"), Save->PassedExamsCount, Save->TotalExamsCount), ColGreen);
	Stat(2, TEXT("模拟考试合格率"), Save->TotalExamsCount > 0 ? FString::Printf(TEXT("%.1f%%"), PassRate) : TEXT("尚未考试"), ColYellow);
	Stat(3, TEXT("模拟考试最高成绩"), Save->TotalExamsCount > 0 ? FString::Printf(TEXT("%d 分"), Save->BestScore) : TEXT("尚未考试"), ColGold);
	const float CW = (W - 54.f) * 0.5f;
	const float LX = X + 18.f;
	const float RX = LX + CW + 18.f;
	const float CY = StatsY + 84.f;
	const float CH = H - 258.f;
	DrawRoundedCard(Canvas, LX, CY, CW, CH, FLinearColor(0.08f, 0.10f, 0.15f), ColDarkGray);
	DrawRoundedCard(Canvas, RX, CY, CW, CH, FLinearColor(0.08f, 0.10f, 0.15f), ColDarkGray);
	DrawTextPixel(TEXT("高频错题 · 优先练习薄弱项"), LX + 16.f, CY + 12.f, ColYellow, Font);
	const TArray<FErrorFrequencyItem> Errors = Save->GetTopFrequentErrors(4);
	if (Errors.IsEmpty()) DrawTextWrapped(TEXT("暂无模拟考试扣分记录。练习记录不计入考试扣分排行榜。"), LX + 20.f, CY + 50.f, CW - 40.f, 3, ColGray, Font);
	for (int32 Index = 0; Index < Errors.Num(); ++Index)
	{
		const float EY = CY + 40.f + Index * 54.f;
		DrawRoundedCard(Canvas, LX + 14.f, EY, CW - 28.f, 48.f, FLinearColor(0.11f, 0.13f, 0.19f), Index == 0 ? ColRed : ColDarkGray);
		DrawTextWrapped(FString::Printf(TEXT("%d. %s · %d 次"), Index + 1, *Errors[Index].CategoryName, Errors[Index].Count), LX + 22.f, EY + 5.f, CW - 44.f, 1, ColYellow, Font, 20.f);
		DrawTextWrapped(Errors[Index].Reason, LX + 22.f, EY + 26.f, CW - 44.f, 1, ColWhite, Font, 20.f);
	}
	if (!Errors.IsEmpty())
	{
		const float AY = CY + CH - 101.f;
		DrawRoundedCard(Canvas, LX + 14.f, AY, CW - 28.f, 88.f, FLinearColor(0.12f, 0.15f, 0.21f), ColGold);
		DrawTextWrapped(UExamErrorAnalyzer::GetCoachAdviceForReason(Errors[0].Reason), LX + 24.f, AY + 10.f, CW - 48.f, 3, ColWhite, Font);
	}
	const int32 Selected = PC ? FMath::Clamp(PC->GetHistorySelectedIndex(), 0, Save->HistorySessions.Num() - 1) : 0;
	const int32 Page = Selected / 5;
	const int32 PageCount = FMath::Max(1, (Save->HistorySessions.Num() + 4) / 5);
	DrawTextPixel(FString::Printf(TEXT("最近训练 · 第 %d / %d 页（保留 20 场）"), Page + 1, PageCount), RX + 16.f, CY + 12.f, ColCyan, Font);
	for (int32 Row = 0; Row < 5; ++Row)
	{
		const int32 Index = Page * 5 + Row;
		if (!Save->HistorySessions.IsValidIndex(Index)) break;
		const FExamSessionRecord& Session = Save->HistorySessions[Index];
		const bool bPractice = Session.PlayMode == EGamePlayMode::GuidedPractice;
		const bool bSelected = Index == Selected;
		const float SY = CY + 40.f + Row * 56.f;
		const FLinearColor Accent = bPractice ? ColCyan : (Session.bPassed ? ColGreen : ColRed);
		DrawRoundedCard(Canvas, RX + 14.f, SY, CW - 28.f, 50.f, bSelected ? FLinearColor(0.15f, 0.18f, 0.26f) : FLinearColor(0.10f, 0.12f, 0.18f), bSelected ? ColGold : ColDarkGray, bSelected ? 2.f : 1.f);
		const FString Transmission = Session.Transmission == ETransmissionType::Auto ? TEXT("C2") : TEXT("C1");
		const FString Outcome = bPractice ? TEXT("练习") : FString::Printf(TEXT("%d分 · %s"), Session.FinalScore, Session.bPassed ? TEXT("合格") : TEXT("未合格"));
		DrawTextWrapped(FString::Printf(TEXT("%s %s · %s · %s"), bSelected ? TEXT("▶") : TEXT(" "), *Session.FormattedTime.Left(16), *Transmission, *Outcome), RX + 22.f, SY + 5.f, CW - 44.f, 1, Accent, Font);
		DrawTextWrapped(FString::Printf(TEXT("%.0f秒 / %.1f米 · %s"), Session.DurationSeconds, Session.DistanceMeters,
			Session.Deductions.IsEmpty() ? (bPractice ? TEXT("引导练习，未进行考试评分") : TEXT("无扣分记录")) : *Session.Deductions[0].Reason), RX + 22.f, SY + 28.f, CW - 44.f, 1, ColGray, Font);
	}
	DrawTextWrapped(EC->GetArchiveStatusText(), X + 24.f, Y + H - 80.f, W - 48.f, 1, ColGray, Font);
	DrawTextWrapped(TEXT("↑↓ 选择记录    PageUp / PageDown 翻页    Enter 查看详情    F3 / Esc 关闭"), X + 24.f, Y + H - 46.f, W - 48.f, 1, ColYellow, Font);
}

void AKeMuSanHUD::DrawHistorySessionDetails(AExamController* EC)
{
	AKeMuSanPlayerController* PC = Cast<AKeMuSanPlayerController>(GetWorld()->GetFirstPlayerController());
	const UExamSaveGame* Save = EC->GetSaveGame();
	if (!Canvas || !PC || !Save || !Save->HistorySessions.IsValidIndex(PC->GetHistorySelectedIndex())) return;
	const FExamSessionRecord& Session = Save->HistorySessions[PC->GetHistorySelectedIndex()];
	const bool bPractice = Session.PlayMode == EGamePlayMode::GuidedPractice;
	const FExamAnalysisResult Analysis = UExamErrorAnalyzer::AnalyzeExamSession(Session.FinalScore, !Session.bPassed, Session.Deductions, Session.DurationSeconds, Session.DistanceMeters);
	UFont* Font = GEngine->GetMediumFont();
	UFont* TitleFont = GEngine->GetLargeFont();
	DrawFilledRect(Canvas, 0.f, 0.f, Canvas->SizeX, Canvas->SizeY, FLinearColor(0.02f, 0.03f, 0.05f, 0.94f));
	const float W = FMath::Min(1060.f, Canvas->SizeX - 40.f);
	const float H = FMath::Min(620.f, Canvas->SizeY - 40.f);
	const float X = (Canvas->SizeX - W) * 0.5f;
	const float Y = (Canvas->SizeY - H) * 0.5f;
	const float CW = (W - 54.f) * 0.5f;
	const float LX = X + 18.f;
	const float RX = LX + CW + 18.f;
	const float CY = Y + 100.f;
	const float CH = H - 180.f;
	DrawRoundedCard(Canvas, X, Y, W, H, FLinearColor(0.05f, 0.07f, 0.11f, 0.99f), ColCyan, 2.f);
	DrawTextSlateLarge(TEXT("单场复盘 · 找到下一次进步的方向"), X + 22.f, Y + 18.f, ColCyan, TitleFont, 26);
	DrawTextWrapped(FString::Printf(TEXT("%s · %s · %s"), *Session.FormattedTime,
		Session.Transmission == ETransmissionType::Auto ? TEXT("C2 自动挡") : TEXT("C1 手动挡"), bPractice ? TEXT("引导练习") : TEXT("模拟考试")), X + 24.f, Y + 60.f, W - 48.f, 1, ColGray, Font);
	DrawRoundedCard(Canvas, LX, CY, CW, CH, FLinearColor(0.08f, 0.10f, 0.15f), ColDarkGray);
	DrawRoundedCard(Canvas, RX, CY, CW, CH, FLinearColor(0.08f, 0.10f, 0.15f), ColDarkGray);
	DrawTextPixel(TEXT("本场结果"), LX + 18.f, CY + 14.f, ColCyan, Font);
	DrawTextSlateLarge(bPractice ? TEXT("练习完成 · 未评级") : FString::Printf(TEXT("%d 分 · %s"), Session.FinalScore, Session.bPassed ? TEXT("合格") : TEXT("未合格")), LX + 20.f, CY + 44.f,
		bPractice ? ColCyan : (Session.bPassed ? ColGreen : ColRed), TitleFont, 26);
	DrawTextPixel(FString::Printf(TEXT("训练用时 %.1f 秒  ·  实际行驶 %.1f 米"), Session.DurationSeconds, Session.DistanceMeters), LX + 20.f, CY + 88.f, ColWhite, Font);
	DrawTextWrapped(bPractice ? TEXT("引导练习不参与模拟考试合格率与最高成绩。") : Analysis.PerformanceRating, LX + 20.f, CY + 122.f, CW - 40.f, 2, ColGray, Font);
	int32 Completed = 0;
	int32 Skipped = 0;
	for (const FZoneStatus& Zone : Session.ZoneStatuses)
	{
		if (Zone.State == 2) ++Completed;
		else if (Zone.State == 3) ++Skipped;
	}
	DrawTextPixel(FString::Printf(TEXT("完成项目 %d / %d  ·  跳过 %d 项"), Completed, Session.ZoneStatuses.Num(), Skipped), LX + 20.f, CY + 168.f, ColGray, Font);
	DrawTextWrapped(FString::Printf(TEXT("本场薄弱项：%s"), *Session.PrimaryWeaknessName), LX + 20.f, CY + 200.f, CW - 40.f, 2, ColYellow, Font);
	DrawRoundedCard(Canvas, LX + 14.f, CY + 244.f, CW - 28.f, CH - 260.f, FLinearColor(0.12f, 0.15f, 0.21f), ColGold);
	const FString Advice = Session.CoachAdvice.IsEmpty() ? UExamErrorAnalyzer::GetCoachAdviceForCategory(Session.PrimaryWeakness) : Session.CoachAdvice;
	DrawTextWrapped(Advice, LX + 24.f, CY + 255.f, CW - 48.f, FMath::Max(1, FMath::FloorToInt((CH - 282.f) / 22.f)), ColWhite, Font);
	const int32 Count = Session.Deductions.Num();
	const int32 PageCount = FMath::Max(1, (Count + 5) / 6);
	const int32 Page = FMath::Clamp(PC->GetHistoryDetailPage(), 0, PageCount - 1);
	DrawTextPixel(FString::Printf(TEXT("全部扣分 · %d 条 · 第 %d / %d 页"), Count, Page + 1, PageCount), RX + 18.f, CY + 14.f, ColCyan, Font);
	if (Count == 0)
	{
		DrawTextWrapped(bPractice ? TEXT("本场为引导练习，不执行考试扣分。可继续进行模拟考试，检验完整操作流程。") : TEXT("本场没有扣分记录。"), RX + 22.f, CY + 68.f, CW - 44.f, 4, bPractice ? ColGray : ColGreen, Font);
	}
	for (int32 Row = 0; Row < 6; ++Row)
	{
		const int32 Index = Page * 6 + Row;
		if (!Session.Deductions.IsValidIndex(Index)) break;
		const FDeduction& Ded = Session.Deductions[Index];
		const float DY = CY + 44.f + Row * 62.f;
		const FString Category = UExamErrorAnalyzer::GetCategoryDisplayName(UExamErrorAnalyzer::ClassifyDeductionReason(Ded.Reason));
		DrawRoundedCard(Canvas, RX + 14.f, DY, CW - 28.f, 56.f, FLinearColor(0.11f, 0.13f, 0.19f), ColDarkGray);
		DrawTextWrapped(FString::Printf(TEXT("%d. -%d分 · 开始后 %.1f秒 · %s"), Index + 1, Ded.Points, Ded.TimeSeconds, *Category), RX + 22.f, DY + 4.f, CW - 44.f, 1, ColYellow, Font, 18.f);
		DrawTextWrapped(Ded.Reason, RX + 22.f, DY + 23.f, CW - 44.f, 2, ColWhite, Font, 16.f);
	}
	DrawTextWrapped(TEXT("PageUp / PageDown 翻扣分页    Enter / Esc 返回档案列表    F3 关闭并返回"), X + 24.f, Y + H - 42.f, W - 48.f, 1, ColYellow, Font);
}

void AKeMuSanHUD::DrawPauseOverlay(AKeMuSanGameMode* GM)
{
	UFont* BigFont = GEngine->GetLargeFont();
	UFont* SmallFont = GEngine->GetMediumFont();
	const float CX = Canvas->SizeX * 0.5f;
	const float CY = Canvas->SizeY * 0.5f;

	DrawFilledRect(Canvas, 0.f, 0.f, Canvas->SizeX, Canvas->SizeY, FLinearColor(0.f, 0.f, 0.f, 0.70f));
	DrawRoundedCard(Canvas, CX - 220.f, CY - 90.f, 440.f, 180.f, FLinearColor(0.06f, 0.08f, 0.12f, 0.95f), ColGold, 2.0f);

	const FString P1 = TEXT("游戏已暂停");
	const FString P2 = TEXT("按 Esc 或 Enter 键继续驾驶");
	DrawTextBig(P1, CX - BigFont->GetStringSize(*P1) * 2.0f * 0.5f, CY - 55.f, ColYellow, BigFont, 2);
	DrawTextPixel(P2, CX - SmallFont->GetStringSize(*P2) * 0.5f, CY + 25.f, ColWhite, SmallFont);
}

void AKeMuSanHUD::DrawKeyHelp(AExamController* EC, AKeMuSanPawn* Car)
{
	UFont* SmallFont = GEngine->GetMediumFont();
	const FString KeyHints = TEXT("快捷键：W/S油门刹车  A/D转向  空格手刹  F安全带  B喇叭  M观察  V切视角  T校准后视镜  F3档案  Esc暂停");
	DrawTextShadowedPixel(KeyHints, 20.f, Canvas->SizeY - 24.f, ColGray, SmallFont);
}

FString AKeMuSanHUD::GetLightStateText(const AKeMuSanPawn* Car) const
{
	if (!Car) return TEXT("");
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
