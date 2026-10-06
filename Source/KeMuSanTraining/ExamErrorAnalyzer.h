// 考试错误分析与教练诊断引擎
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ExamTypes.h"
#include "ExamErrorAnalyzer.generated.h"

// 单场考试详细诊断分析结果
USTRUCT(BlueprintType)
struct FExamAnalysisResult
{
	GENERATED_BODY()

	// 各维度失分统计（分值）
	UPROPERTY(BlueprintReadOnly) int32 ObservationDeductions = 0;
	UPROPERTY(BlueprintReadOnly) int32 LightingDeductions = 0;
	UPROPERTY(BlueprintReadOnly) int32 VehicleControlDeductions = 0;
	UPROPERTY(BlueprintReadOnly) int32 RulesAndWayDeductions = 0;
	UPROPERTY(BlueprintReadOnly) int32 OtherDeductions = 0;

	// 各维度失分次数
	UPROPERTY(BlueprintReadOnly) int32 ObservationCount = 0;
	UPROPERTY(BlueprintReadOnly) int32 LightingCount = 0;
	UPROPERTY(BlueprintReadOnly) int32 VehicleControlCount = 0;
	UPROPERTY(BlueprintReadOnly) int32 RulesAndWayCount = 0;
	UPROPERTY(BlueprintReadOnly) int32 OtherCount = 0;

	// 是否发生一票否决致命违规（直接扣100分判挂科）
	UPROPERTY(BlueprintReadOnly) bool bHadFatalViolation = false;
	UPROPERTY(BlueprintReadOnly) FString FatalViolationReason;

	// 当场最主要的失误薄弱维度
	UPROPERTY(BlueprintReadOnly) EErrorCategory PrimaryWeakness = EErrorCategory::Other;
	UPROPERTY(BlueprintReadOnly) FString PrimaryWeaknessName;

	// 综合表现评定等级短语（如：“五星特级学员”、“规范优良”、“安全观察欠缺”、“操作失误较多”）
	UPROPERTY(BlueprintReadOnly) FString PerformanceRating;

	// 专业教练实操点拨建议
	UPROPERTY(BlueprintReadOnly) TArray<FString> CoachAdvices;

	// 错题归类清单字符串
	UPROPERTY(BlueprintReadOnly) TArray<FString> CategorizedErrorDetails;
};

UCLASS()
class KEMUSANTRAINING_API UExamErrorAnalyzer : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// 根据扣分原因字符串进行模式识别与维度智能分类
	UFUNCTION(BlueprintPure, Category = "Exam|Analysis")
	static EErrorCategory ClassifyDeductionReason(const FString& Reason);

	// 获取维度分类的中文显示名
	UFUNCTION(BlueprintPure, Category = "Exam|Analysis")
	static FString GetCategoryDisplayName(EErrorCategory Category);

	// 分析单场考试结果并生成诊断报告
	UFUNCTION(BlueprintCallable, Category = "Exam|Analysis")
	static FExamAnalysisResult AnalyzeExamSession(
		int32 FinalScore,
		bool bFailIssued,
		const TArray<FDeduction>& Deductions,
		float DurationSeconds,
		float DistanceMeters);

	// 获取针对该维度的深度教练教学建议
	UFUNCTION(BlueprintPure, Category = "Exam|Analysis")
	static FString GetCoachAdviceForCategory(EErrorCategory Category);

	// 与本项目实际违规及按键相对应的简短复练建议。
	UFUNCTION(BlueprintPure, Category = "Exam|Analysis")
	static FString GetCoachAdviceForReason(const FString& Reason);
};
