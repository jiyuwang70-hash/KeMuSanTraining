// 程序化提示音：叮 / 双叮 / 警示 / 喇叭 / 转向滴答 / 引擎声
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Sound/SoundWaveProcedural.h"
#include "BeepSynth.generated.h"

class UAudioComponent;

// 简单合成波形（16bit PCM 单声道）
UCLASS()
class KEMUSANTRAINING_API USimpleToneWave : public USoundWaveProcedural
{
	GENERATED_BODY()

public:
	USimpleToneWave(const FObjectInitializer& ObjectInitializer);

	// 0 静音  1 叮  2 双叮  3 警示  4 喇叭  5 转向滴答  6 引擎
	void SetPattern(int32 InPattern) { Pattern = InPattern; }
	void SetFrequency(float F) { Frequency = F; }
	int32 GetPattern() const { return Pattern; }

	virtual int32 OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples) override;

private:
	volatile int32 Pattern = 0;
	volatile float Frequency = 880.f;
	double SamplePos = 0.0;
	double PatternPos = 0.0;
};

// 提示音管理器
UCLASS()
class KEMUSANTRAINING_API ABeeper : public AActor
{
	GENERATED_BODY()

public:
	ABeeper();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	void PlayDing();
	void PlayDoubleDing();
	void PlayWarn();

	// 持续音（每帧由考试控制器驱动）
	void SetHorn(bool bOn);
	void SetIndicator(bool bOn);
	void SetEngineRpm(float Rpm);

protected:
	UPROPERTY(VisibleAnywhere)
	UAudioComponent* AudioComp = nullptr;

	UPROPERTY()
	USimpleToneWave* Wave = nullptr;

	bool bHornOn = false;
	bool bIndicatorOn = false;
	float EngineRpm = 900.f;
	float HornTimer = 0.f;
};
