// 科目三模拟游戏 HUD（Canvas 绘制）
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "KeMuSanHUD.generated.h"

class AKeMuSanGameMode;
class AExamController;
class AKeMuSanPawn;

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

	void DrawTextShadowed(const FString& Text, float X, float Y, float Scale, const FLinearColor& Color, const UFont* Font);

	void DrawMenu();
	void DrawTopPrompt(AExamController* EC);
	void DrawScorePanel(AExamController* EC);
	void DrawProgressList(AExamController* EC);
	void DrawVehiclePanel(AKeMuSanPawn* Car, AExamController* EC);
	void DrawLightTestPanel(AExamController* EC);
	void DrawResultPanel(AExamController* EC, AKeMuSanGameMode* GM);
	void DrawPauseOverlay(AKeMuSanGameMode* GM);
	void DrawKeyHelp(AExamController* EC);

	// 灯光状态字符串
	FString GetLightStateText(const AKeMuSanPawn* Car) const;
};
