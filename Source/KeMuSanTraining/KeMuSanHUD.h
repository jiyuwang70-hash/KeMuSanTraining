// 科目三模拟游戏 HUD（Canvas 绘制：全中文沉浸首屏、现代仪表盘、光学后视镜、30cm停车标尺）
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ExamTypes.h"
#include "KeMuSanHUD.generated.h"

class AKeMuSanGameMode;
class AExamController;
class AKeMuSanPawn;
class UTextureRenderTarget2D;

UCLASS()
class KEMUSANTRAINING_API AKeMuSanHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

protected:
	AKeMuSanGameMode* GetGameMode() const;
	AExamController* GetExamController() const;
	AKeMuSanPawn* GetCar() const;

	// Pixel-Perfect 锐利文字绘制系统（消除字体贴图双线性重采样发花）
	void DrawTextPixel(const FString& Text, float X, float Y, const FLinearColor& Color, const UFont* Font);
	void DrawTextBig(const FString& Text, float X, float Y, const FLinearColor& Color, const UFont* Font, int32 IntScale = 2);
	void DrawTextShadowedPixel(const FString& Text, float X, float Y, const FLinearColor& Color, const UFont* Font);
	void DrawTextShadowed(const FString& Text, float X, float Y, float Scale, const FLinearColor& Color, const UFont* Font);
	void DrawTextSlateLarge(const FString& Text, float X, float Y, const FLinearColor& Color, const UFont* Font, int32 PointSize = 26);
	float MeasureTextSlate(const FString& Text, const UFont* Font, int32 PointSize = 26);
	void DrawTextWrapped(const FString& Text, float X, float Y, float MaxWidth, int32 MaxLines, const FLinearColor& Color, const UFont* Font, float LineHeight = 22.f);

	// 核心界面子系统
	void DrawMenu();
	void DrawTopHeader(AExamController* EC, AKeMuSanPawn* Car);
	void DrawStepGuide(AExamController* EC, AKeMuSanPawn* Car);
	void DrawScorePanel(AExamController* EC);
	void DrawProgressList(AExamController* EC);
	void DrawMirrors(AKeMuSanPawn* Car);
	void DrawMirrorFrame(float X, float Y, float W, float H, const FString& Label, UTextureRenderTarget2D* Target, bool bActive, const FMirrorOpticalState& State);
	void DrawMirrorAdjustOverlay(AKeMuSanPawn* Car);
	void DrawModernDashboard(AKeMuSanPawn* Car, AExamController* EC);
	void DrawPullOverRadar(AExamController* EC);
	void DrawLightTestPanel(AExamController* EC);
	void DrawResultPanel(AExamController* EC, AKeMuSanGameMode* GM);
	void DrawHistoryAnalysisPanel(AExamController* EC);
	void DrawHistorySessionDetails(AExamController* EC);
	void DrawPauseOverlay(AKeMuSanGameMode* GM);
	void DrawMiniMap(AExamController* EC, AKeMuSanPawn* Car);
	void DrawKeyHelp(AExamController* EC, AKeMuSanPawn* Car);

	// 辅助方法
	FString GetLightStateText(const AKeMuSanPawn* Car) const;

	// 起步辅助阶段状态单向推进标记
	bool bRecordedHorn = false;
};
