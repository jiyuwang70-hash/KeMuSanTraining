// 科目三统一单位转换工具：米制内部仿真逻辑 <-> UE世界厘米空间
#pragma once

#include "CoreMinimal.h"

namespace KeMuSanUnits
{
	// 比例系数：1 米 = 100 厘米 (UE 世界空间)
	constexpr float MToCm = 100.0f;
	constexpr float CmToM = 0.01f;

	// 米制位置 -> UE 世界厘米位置
	FORCEINLINE FVector MetersToWorld(const FVector& InMeters)
	{
		return InMeters * MToCm;
	}

	// UE 世界厘米位置 -> 米制位置
	FORCEINLINE FVector WorldToMeters(const FVector& InWorldCm)
	{
		return InWorldCm * CmToM;
	}

	// 标量米 -> 厘米
	FORCEINLINE float MetersToWorld(float InMeters)
	{
		return InMeters * MToCm;
	}

	// 标量厘米 -> 米
	FORCEINLINE float WorldToMeters(float InWorldCm)
	{
		return InWorldCm * CmToM;
	}
}
