#include "BeepSynth.h"

#include "Components/AudioComponent.h"

// ---------------------------------------------------------------------------
// USimpleToneWave
// ---------------------------------------------------------------------------
USimpleToneWave::USimpleToneWave(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetSampleRate(16000);
	NumChannels = 1;
	bLooping = true;
}

int32 USimpleToneWave::OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples)
{
	OutAudio.SetNum(NumSamples * sizeof(int16));
	int16* Ptr = reinterpret_cast<int16*>(OutAudio.GetData());

	const double SR = static_cast<double>(SampleRate);
	const int32 P = Pattern;
	const float F = Frequency;

	for (int32 i = 0; i < NumSamples; ++i)
	{
		PatternPos += 1.0 / SR;
		if (PatternPos >= 1.0)
		{
			PatternPos -= 1.0;
		}

		float Sample = 0.f;

		switch (P)
		{
		case 1: // 叮
			if (PatternPos < 0.28f)
			{
				const float Env = FMath::Max(0.f, 1.f - static_cast<float>(PatternPos) / 0.28f);
				Sample = FMath::Sin(SamplePos * 2.0 * PI) * Env;
				SamplePos += 880.0 / SR;
			}
			break;
		case 2: // 双叮
		{
			const float T = static_cast<float>(PatternPos);
			const float T1 = T - 0.02f;
			const float T2 = T - 0.34f;
			float Env = 0.f;
			if (T1 >= 0.f && T1 < 0.22f) { Env = 1.f - T1 / 0.22f; }
			else if (T2 >= 0.f && T2 < 0.22f) { Env = 1.f - T2 / 0.22f; }
			if (Env > 0.f)
			{
				Sample = FMath::Sin(SamplePos * 2.0 * PI) * Env;
				SamplePos += 980.0 / SR;
			}
			break;
		}
		case 3: // 警示蜂鸣
			if (PatternPos < 0.5f)
			{
				const float T = static_cast<float>(PatternPos);
				const float Env = FMath::Max(0.f, 1.f - T / 0.5f);
				Sample = FMath::Sin(SamplePos * 2.0 * PI) * Env * 0.9f;
				SamplePos += 420.0 / SR;
			}
			break;
		case 4: // 喇叭
		{
			const double Phase1 = SamplePos * 2.0 * PI;
			const double Phase2 = Phase1 * 1.25;
			Sample = (FMath::Sin(Phase1) * 0.6f + FMath::Sin(Phase2) * 0.4f) * 0.8f;
			SamplePos += 430.0 / SR;
			break;
		}
		case 5: // 转向滴答
			if (PatternPos < 0.035f)
			{
				Sample = FMath::Sin(SamplePos * 2.0 * PI) * 0.5f;
				SamplePos += 2100.0 / SR;
			}
			break;
		case 6: // 引擎
		{
			const float Freq = 55.f + F / 6200.f * 130.f;
			Sample = FMath::Sin(SamplePos * 2.0 * PI) * 0.28f;
			Sample += FMath::Sin(SamplePos * 4.0 * PI) * 0.12f;
			SamplePos += Freq / SR;
			break;
		}
		default:
			break;
		}

		Ptr[i] = static_cast<int16>(FMath::Clamp(Sample, -1.f, 1.f) * 32000.f);
	}

	return NumSamples;
}

// ---------------------------------------------------------------------------
// ABeeper
// ---------------------------------------------------------------------------
ABeeper::ABeeper()
{
	PrimaryActorTick.bCanEverTick = true;
	AudioComp = CreateDefaultSubobject<UAudioComponent>(TEXT("Audio"));
	AudioComp->bIsUISound = true;
	AudioComp->SetVolumeMultiplier(0.6f);
}

void ABeeper::BeginPlay()
{
	Super::BeginPlay();

	Wave = NewObject<USimpleToneWave>(this);
	if (Wave && AudioComp)
	{
		Wave->SetPattern(0);
		AudioComp->SetSound(Wave);
		AudioComp->Play();
	}
}

void ABeeper::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!Wave)
	{
		return;
	}

	HornTimer -= DeltaSeconds;

	if (bHornOn)
	{
		Wave->SetPattern(4);
	}
	else if (bIndicatorOn)
	{
		Wave->SetPattern(5);
	}
	else if (HornTimer > 0.f)
	{
		// 一次性提示音（叮/双叮/警示）保持播放
	}
	else if (EngineRpm > 0.f)
	{
		Wave->SetPattern(6);
		Wave->SetFrequency(EngineRpm);
	}
	else
	{
		Wave->SetPattern(0);
	}
}

void ABeeper::PlayDing()
{
	if (Wave)
	{
		Wave->SetPattern(1);
	}
	HornTimer = 0.9f;
}

void ABeeper::PlayDoubleDing()
{
	if (Wave)
	{
		Wave->SetPattern(2);
	}
	HornTimer = 1.2f;
}

void ABeeper::PlayWarn()
{
	if (Wave)
	{
		Wave->SetPattern(3);
	}
	HornTimer = 1.2f;
}

void ABeeper::SetHorn(bool bOn)
{
	bHornOn = bOn;
	if (bOn)
	{
		HornTimer = 0.5f;
	}
}

void ABeeper::SetIndicator(bool bOn)
{
	bIndicatorOn = bOn;
}

void ABeeper::SetEngineRpm(float Rpm)
{
	EngineRpm = Rpm;
}
