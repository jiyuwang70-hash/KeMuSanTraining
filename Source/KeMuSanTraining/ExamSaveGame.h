// 科目三学员训练存档与历史记录系统
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "ExamTypes.h"
#include "ExamErrorAnalyzer.h"
#include "ExamSaveGame.generated.h"

// 单次考试历史归档记录
USTRUCT(BlueprintType)
struct FExamSessionRecord
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FString SessionId;
	UPROPERTY(BlueprintReadOnly) FString FormattedTime;
	UPROPERTY(BlueprintReadOnly) EGamePlayMode PlayMode = EGamePlayMode::GuidedPractice;
	UPROPERTY(BlueprintReadOnly) ETransmissionType Transmission = ETransmissionType::Manual;
	UPROPERTY(BlueprintReadOnly) int32 FinalScore = 100;
	UPROPERTY(BlueprintReadOnly) bool bPassed = false;
	UPROPERTY(BlueprintReadOnly) FString ResultSummary;
	UPROPERTY(BlueprintReadOnly) float DurationSeconds = 0.f;
	UPROPERTY(BlueprintReadOnly) float DistanceMeters = 0.f;
	UPROPERTY(BlueprintReadOnly) TArray<FDeduction> Deductions;
	UPROPERTY(BlueprintReadOnly) TArray<FZoneStatus> ZoneStatuses;
	UPROPERTY(BlueprintReadOnly) EErrorCategory PrimaryWeakness = EErrorCategory::Other;
	UPROPERTY(BlueprintReadOnly) FString PrimaryWeaknessName;
	UPROPERTY(BlueprintReadOnly) FString CoachAdvice;
};

// 错题排行榜项
USTRUCT(BlueprintType)
struct FErrorFrequencyItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FString Reason;
	UPROPERTY(BlueprintReadOnly) int32 Count = 0;
	UPROPERTY(BlueprintReadOnly) FString CategoryName;
};

UCLASS()
class KEMUSANTRAINING_API UExamSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	// 旧存档缺少此字段时保持0；写入时设置为当前格式版本。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SaveGame")
	int32 FormatVersion = 0;

	// 累计考核场次与合格率统计
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SaveGame")
	int32 TotalExamsCount = 0;

	// 引导练习单独累计，不计入模拟考试的成绩和合格率。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SaveGame")
	int32 TotalPracticeCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SaveGame")
	int32 PassedExamsCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SaveGame")
	int32 BestScore = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SaveGame")
	float BestDurationSeconds = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SaveGame")
	float TotalDistanceDrivenMeters = 0.f;

	// 最近历史考试记录清单（保留最近 20 场）
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SaveGame")
	TArray<FExamSessionRecord> HistorySessions;

	// 历史所有扣分原因出现频次字典
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SaveGame")
	TMap<FString, int32> ErrorFrequencyMap;

	// 历史四大维度累计扣分
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SaveGame")
	TMap<FString, int32> CategoryDeductionsMap;

	// 历史详情仅保留20场；ID清单用于防止重复结算再次增加累计统计。
	UPROPERTY()
	TSet<FString> RecordedSessionIds;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "SaveGame")
	bool bLastBinarySaveSucceeded = false;
	UPROPERTY(Transient, BlueprintReadOnly, Category = "SaveGame")
	bool bLastJsonExportSucceeded = false;
	UPROPERTY(Transient, BlueprintReadOnly, Category = "SaveGame")
	bool bLastSessionAlreadyRecorded = false;
	UPROPERTY(Transient, BlueprintReadOnly, Category = "SaveGame")
	FString LastPersistenceMessage;

	// 获取默认存档槽位名
	static const FString& GetDefaultSlotName();
	static const FString& GetHistoryFilePath();

	UFUNCTION(BlueprintPure, Category = "Exam|SaveGame")
	int32 GetSessionCount() const { return TotalExamsCount + TotalPracticeCount; }

	// 加载或创建全新存档实例
	UFUNCTION(BlueprintCallable, Category = "Exam|SaveGame")
	static UExamSaveGame* LoadOrCreateSaveGame();

	// 归档单场考试记录并落盘（.sav与JSON）
	UFUNCTION(BlueprintCallable, Category = "Exam|SaveGame")
	static bool RecordAndSaveSession(const FExamSessionRecord& SessionRecord, UExamSaveGame*& OutSaveGame);

	// 获取历史高频错题排行榜（默认前Top N项）
	UFUNCTION(BlueprintCallable, Category = "Exam|SaveGame")
	TArray<FErrorFrequencyItem> GetTopFrequentErrors(int32 TopN = 3) const;

	// 导出易读的 JSON 历史分析档案至指定或默认文件
	UFUNCTION(BlueprintCallable, Category = "Exam|SaveGame")
	static bool ExportToJsonFile(const UExamSaveGame* SaveGame, const FString& TargetFilePath = TEXT(""));

	// 清空全部历史记录（重置）
	UFUNCTION(BlueprintCallable, Category = "Exam|SaveGame")
	static void ResetAllHistory();
};
