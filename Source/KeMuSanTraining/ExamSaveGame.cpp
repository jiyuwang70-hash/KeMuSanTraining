// 科目三学员训练存档与历史记录系统实现
#include "ExamSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
	// Save/export APIs run on the game thread. Preserve a concrete recovery
	// instruction when the file system cannot restore an interrupted JSON swap.
	FString LastJsonExportDiagnostic;

	struct FArchivePaths
	{
		FString Slot;
		FString JsonPath;
		bool bValid = false;
		bool bDisabledForAutomation = false;
	};

	const FArchivePaths& GetArchivePaths()
	{
		static const FArchivePaths Paths = []()
		{
			FArchivePaths Value;
			const TCHAR* CommandLine = FCommandLine::Get();
			FString ExamStartMode;
			const bool bTest = FParse::Param(CommandLine, TEXT("test-archive-analysis")) ||
				FParse::Param(CommandLine, TEXT("test-archive-reload")) ||
				FParse::Param(CommandLine, TEXT("test-route-geometry")) ||
				FParse::Param(CommandLine, TEXT("test-input-chain")) ||
				FParse::Param(CommandLine, TEXT("autotest")) ||
				FParse::Value(CommandLine, TEXT("test-exam-start="), ExamStartMode);
			const bool bHasSlot = FParse::Value(CommandLine, TEXT("exam-save-slot="), Value.Slot);
			const bool bHasPath = FParse::Value(CommandLine, TEXT("exam-history-path="), Value.JsonPath);
			if (bTest && (!bHasSlot || !bHasPath || !Value.Slot.StartsWith(TEXT("KeMuSanTest_"))))
			{
				Value.bDisabledForAutomation = true;
				UE_LOG(LogTemp, Warning, TEXT("[KeMuSanArchive] Automated archive disabled: supply an isolated KeMuSanTest_ slot and absolute JSON path."));
				return Value;
			}
			if (!bHasSlot) Value.Slot = TEXT("KeMuSanExamSave");
			if (Value.Slot.IsEmpty() || Value.Slot.Len() > 80)
			{
				UE_LOG(LogTemp, Error, TEXT("[KeMuSanArchive] Invalid save slot length."));
				return Value;
			}
			for (TCHAR Char : Value.Slot)
			{
				if (!((Char >= 'a' && Char <= 'z') || (Char >= 'A' && Char <= 'Z') ||
					(Char >= '0' && Char <= '9') || Char == '_' || Char == '-'))
				{
					UE_LOG(LogTemp, Error, TEXT("[KeMuSanArchive] Invalid save slot: use letters, numbers, underscore or dash."));
					return Value;
				}
			}
			if (!bHasPath)
			{
				Value.JsonPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"), TEXT("ExamHistory.json")));
			}
			else if (Value.JsonPath.IsEmpty() || FPaths::IsRelative(Value.JsonPath))
			{
				UE_LOG(LogTemp, Error, TEXT("[KeMuSanArchive] JSON override must be an absolute file path."));
				return Value;
			}
			FPaths::NormalizeFilename(Value.JsonPath);
			// An isolated test must never export over the learner's normal JSON file.
			const FString NormalPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"), TEXT("ExamHistory.json")));
			if (bTest && FPaths::IsSamePath(Value.JsonPath, NormalPath))
			{
				UE_LOG(LogTemp, Error, TEXT("[KeMuSanArchive] Test refused: JSON path is the real learner archive."));
				return Value;
			}
			Value.bValid = true;
			return Value;
		}();
		return Paths;
	}

	UExamSaveGame* LoadArchiveFromSlot(const FString& Slot)
	{
		TArray<uint8> Data;
		if (!UGameplayStatics::LoadDataFromSlot(Data, Slot, 0) || Data.Num() < 16) return nullptr;
		uint32 HeaderTag = 0;
		int32 EngineSaveVersion = 0;
		FMemory::Memcpy(&HeaderTag, Data.GetData(), sizeof(HeaderTag));
		FMemory::Memcpy(&EngineSaveVersion, Data.GetData() + sizeof(HeaderTag), sizeof(EngineSaveVersion));
		// UE's legacy fallback treats arbitrary bytes as a serialized FString.
		// This project only creates standard GVAS archives; reject a broken or
		// truncated header before asking the engine to deserialize it.
		if (HeaderTag != 0x53415647 || EngineSaveVersion < 1 || EngineSaveVersion > 3) return nullptr;
		UExamSaveGame* Save = Cast<UExamSaveGame>(UGameplayStatics::LoadGameFromMemory(Data));
		if (Save && (Save->FormatVersion < 0 || Save->FormatVersion > 2 || Save->TotalExamsCount < 0 ||
			Save->TotalPracticeCount < 0 || Save->PassedExamsCount < 0 || Save->PassedExamsCount > Save->TotalExamsCount ||
			Save->BestScore < 0 || Save->BestScore > 100 || Save->HistorySessions.Num() > 20)) return nullptr;
		return Save;
	}
}

const FString& UExamSaveGame::GetDefaultSlotName()
{
	return GetArchivePaths().Slot;
}

const FString& UExamSaveGame::GetHistoryFilePath()
{
	return GetArchivePaths().JsonPath;
}

UExamSaveGame* UExamSaveGame::LoadOrCreateSaveGame()
{
	if (!GetArchivePaths().bValid)
	{
		if (GetArchivePaths().bDisabledForAutomation)
		{
			UExamSaveGame* EmptyProfile = NewObject<UExamSaveGame>();
			EmptyProfile->LastPersistenceMessage = TEXT("自动验证未设置独立存档，已禁用档案读写。");
			return EmptyProfile;
		}
		return nullptr;
	}
	const FString& Slot = GetDefaultSlotName();
	if (UGameplayStatics::DoesSaveGameExist(Slot, 0))
	{
		if (UExamSaveGame* ExamSave = LoadArchiveFromSlot(Slot))
		{
			for (const FExamSessionRecord& Session : ExamSave->HistorySessions)
			{
				if (!Session.SessionId.IsEmpty()) ExamSave->RecordedSessionIds.Add(Session.SessionId);
			}
			return ExamSave;
		}
		// A present but unreadable archive is not an empty profile. Keep its bytes
		// untouched so the learner can recover or back it up.
		UE_LOG(LogTemp, Error, TEXT("[KeMuSanArchive] Existing save failed to load; preserved and refusing overwrite. Slot=%s"), *Slot);
		return nullptr;
	}

	// 否则创建全新存档对象
	UExamSaveGame* NewSave = Cast<UExamSaveGame>(UGameplayStatics::CreateSaveGameObject(UExamSaveGame::StaticClass()));
	if (!NewSave)
	{
		NewSave = NewObject<UExamSaveGame>();
	}
	return NewSave;
}

bool UExamSaveGame::RecordAndSaveSession(const FExamSessionRecord& SessionRecord, UExamSaveGame*& OutSaveGame)
{
	if (!GetArchivePaths().bValid || SessionRecord.SessionId.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("[KeMuSanArchive] Session refused: archive configuration or session ID is invalid."));
		return false;
	}
	if (SessionRecord.FinalScore < 0 || SessionRecord.FinalScore > 100 ||
		!FMath::IsFinite(SessionRecord.DurationSeconds) || SessionRecord.DurationSeconds < 0.f ||
		!FMath::IsFinite(SessionRecord.DistanceMeters) || SessionRecord.DistanceMeters < 0.f)
	{
		UE_LOG(LogTemp, Error, TEXT("[KeMuSanArchive] Session refused: score, duration or distance is invalid."));
		return false;
	}
	// Preserve a pending in-memory record after an unsuccessful disk write.
	UExamSaveGame* Save = OutSaveGame ? OutSaveGame : LoadOrCreateSaveGame();
	if (!Save)
	{
		return false;
	}

	Save->bLastBinarySaveSucceeded = false;
	Save->bLastJsonExportSucceeded = false;
	Save->bLastSessionAlreadyRecorded = Save->RecordedSessionIds.Contains(SessionRecord.SessionId);
	if (!Save->bLastSessionAlreadyRecorded)
	{
		if (SessionRecord.PlayMode == EGamePlayMode::SimulatedExam)
		{
			Save->TotalExamsCount++;
			if (SessionRecord.bPassed)
			{
				Save->PassedExamsCount++;
				if (Save->BestDurationSeconds <= 0.001f || SessionRecord.DurationSeconds < Save->BestDurationSeconds)
				{
					Save->BestDurationSeconds = SessionRecord.DurationSeconds;
				}
			}
			Save->BestScore = FMath::Max(Save->BestScore, SessionRecord.FinalScore);
		}
		else
		{
			Save->TotalPracticeCount++;
		}
		Save->TotalDistanceDrivenMeters += FMath::Max(0.f, SessionRecord.DistanceMeters);

		for (const FDeduction& Ded : SessionRecord.Deductions)
		{
			if (Ded.Points <= 0) continue;
			Save->ErrorFrequencyMap.FindOrAdd(Ded.Reason)++;
			const EErrorCategory Cat = UExamErrorAnalyzer::ClassifyDeductionReason(Ded.Reason);
			Save->CategoryDeductionsMap.FindOrAdd(UExamErrorAnalyzer::GetCategoryDisplayName(Cat)) += Ded.Points;
		}
		FExamSessionRecord ArchivedRecord = SessionRecord;
		if (ArchivedRecord.PlayMode == EGamePlayMode::GuidedPractice) ArchivedRecord.bPassed = false;
		Save->HistorySessions.Insert(ArchivedRecord, 0);
		if (Save->HistorySessions.Num() > 20) Save->HistorySessions.SetNum(20);
		Save->RecordedSessionIds.Add(SessionRecord.SessionId);
	}

	OutSaveGame = Save;
	Save->FormatVersion = 2;

	// 1. 保存为 UE 标准 .sav 二进制槽位
	// Recheck an existing file even when the caller has a cached profile: a damaged
	// file must not silently be replaced by the next completed training session.
	const FString& Slot = GetDefaultSlotName();
	const bool bExistingArchiveReadable = !UGameplayStatics::DoesSaveGameExist(Slot, 0) ||
		LoadArchiveFromSlot(Slot) != nullptr;
	Save->bLastBinarySaveSucceeded = bExistingArchiveReadable && UGameplayStatics::SaveGameToSlot(Save, Slot, 0);

	// 2. 同步导出易读的 JSON 档案
	Save->bLastJsonExportSucceeded = bExistingArchiveReadable && ExportToJsonFile(Save);
	if (!bExistingArchiveReadable)
	{
		Save->LastPersistenceMessage = TEXT("已有存档读取失败，旧文件已保留；本场暂存在内存，请备份并修复存档。");
	}
	else if (Save->bLastBinarySaveSucceeded && Save->bLastJsonExportSucceeded)
	{
		Save->LastPersistenceMessage = Save->bLastSessionAlreadyRecorded ? TEXT("记录已归档，重复结算未增加场次。") : TEXT("本场已保存，重启后可在学员档案查看。");
	}
	else
	{
		Save->LastPersistenceMessage = FString::Printf(TEXT("本场保存未完整完成：存档%s，JSON%s。请检查磁盘空间或写入权限。"),
			Save->bLastBinarySaveSucceeded ? TEXT("成功") : TEXT("失败"), Save->bLastJsonExportSucceeded ? TEXT("成功") : TEXT("失败"));
		if (!Save->bLastJsonExportSucceeded && !LastJsonExportDiagnostic.IsEmpty())
		{
			Save->LastPersistenceMessage += TEXT(" ") + LastJsonExportDiagnostic;
		}
	}
	UE_LOG(LogTemp, Display, TEXT("[KeMuSanArchive] Session=%s BinarySaved=%d JsonExported=%d Duplicate=%d"),
		*SessionRecord.SessionId, Save->bLastBinarySaveSucceeded, Save->bLastJsonExportSucceeded, Save->bLastSessionAlreadyRecorded);
	return Save->bLastBinarySaveSucceeded && Save->bLastJsonExportSucceeded;
}

TArray<FErrorFrequencyItem> UExamSaveGame::GetTopFrequentErrors(int32 TopN) const
{
	TArray<FErrorFrequencyItem> Items;
	if (TopN <= 0) return Items;
	for (const auto& Kvp : ErrorFrequencyMap)
	{
		if (Kvp.Value <= 0) continue;
		FErrorFrequencyItem Item;
		Item.Reason = Kvp.Key;
		Item.Count = Kvp.Value;
		Item.CategoryName = UExamErrorAnalyzer::GetCategoryDisplayName(UExamErrorAnalyzer::ClassifyDeductionReason(Kvp.Key));
		Items.Add(Item);
	}

	// 按出现频次由高到低排序
	Items.Sort([](const FErrorFrequencyItem& A, const FErrorFrequencyItem& B)
	{
		return A.Count == B.Count ? A.Reason < B.Reason : A.Count > B.Count;
	});

	if (Items.Num() > TopN)
	{
		Items.SetNum(TopN);
	}
	return Items;
}

bool UExamSaveGame::ExportToJsonFile(const UExamSaveGame* SaveGame, const FString& TargetFilePath)
{
	LastJsonExportDiagnostic.Empty();
	if (!SaveGame || !GetArchivePaths().bValid)
	{
		return false;
	}

	FString OutPath = TargetFilePath;
	if (OutPath.IsEmpty())
	{
		OutPath = GetHistoryFilePath();
	}
	// A directory is never a JSON export file, and must not be renamed or deleted.
	if (IFileManager::Get().DirectoryExists(*OutPath))
	{
		LastJsonExportDiagnostic = TEXT("JSON目标是文件夹，请选择JSON文件路径。");
		UE_LOG(LogTemp, Warning, TEXT("[KeMuSanArchive] JSON export refused: target is a directory."));
		return false;
	}

	// 确保目录存在
	const FString Dir = FPaths::GetPath(OutPath);
	if (!IFileManager::Get().MakeDirectory(*Dir, true)) return false;

	TSharedPtr<FJsonObject> RootObj = MakeShareable(new FJsonObject);

	// 档案基本概览
	RootObj->SetNumberField(TEXT("schema_version"), 2);
	RootObj->SetNumberField(TEXT("total_sessions_count"), SaveGame->GetSessionCount());
	RootObj->SetNumberField(TEXT("total_practice_count"), SaveGame->TotalPracticeCount);
	RootObj->SetNumberField(TEXT("total_exams_count"), SaveGame->TotalExamsCount);
	RootObj->SetNumberField(TEXT("passed_exams_count"), SaveGame->PassedExamsCount);
	const double PassRate = (SaveGame->TotalExamsCount > 0)
		? (static_cast<double>(SaveGame->PassedExamsCount) / static_cast<double>(SaveGame->TotalExamsCount) * 100.0)
		: 0.0;
	RootObj->SetNumberField(TEXT("pass_rate_percent"), PassRate);
	RootObj->SetNumberField(TEXT("best_score"), SaveGame->BestScore);
	RootObj->SetNumberField(TEXT("best_duration_seconds"), SaveGame->BestDurationSeconds);
	RootObj->SetNumberField(TEXT("total_distance_driven_meters"), SaveGame->TotalDistanceDrivenMeters);

	// 各维度累计扣分字典
	TSharedPtr<FJsonObject> CatDedsObj = MakeShareable(new FJsonObject);
	for (const auto& Kvp : SaveGame->CategoryDeductionsMap)
	{
		CatDedsObj->SetNumberField(Kvp.Key, Kvp.Value);
	}
	RootObj->SetObjectField(TEXT("category_deductions_summary"), CatDedsObj);
	TSharedPtr<FJsonObject> FrequenciesObj = MakeShareable(new FJsonObject);
	for (const auto& Kvp : SaveGame->ErrorFrequencyMap)
	{
		FrequenciesObj->SetNumberField(Kvp.Key, Kvp.Value);
	}
	RootObj->SetObjectField(TEXT("error_frequency_summary"), FrequenciesObj);

	// 易错题高频排行
	TArray<TSharedPtr<FJsonValue>> TopErrorsArr;
	const TArray<FErrorFrequencyItem> TopList = SaveGame->GetTopFrequentErrors(5);
	for (const FErrorFrequencyItem& Item : TopList)
	{
		TSharedPtr<FJsonObject> ErrorObj = MakeShareable(new FJsonObject);
		ErrorObj->SetStringField(TEXT("reason"), Item.Reason);
		ErrorObj->SetNumberField(TEXT("count"), Item.Count);
		ErrorObj->SetStringField(TEXT("category"), Item.CategoryName);
		TopErrorsArr.Add(MakeShareable(new FJsonValueObject(ErrorObj)));
	}
	RootObj->SetArrayField(TEXT("top_frequent_errors"), TopErrorsArr);

	// 历史场次列表
	TArray<TSharedPtr<FJsonValue>> SessionsArr;
	for (const FExamSessionRecord& Sess : SaveGame->HistorySessions)
	{
		TSharedPtr<FJsonObject> SessObj = MakeShareable(new FJsonObject);
		SessObj->SetStringField(TEXT("session_id"), Sess.SessionId);
		SessObj->SetStringField(TEXT("time"), Sess.FormattedTime);
		SessObj->SetStringField(TEXT("play_mode"), (Sess.PlayMode == EGamePlayMode::GuidedPractice) ? TEXT("引导练习") : TEXT("模拟考试"));
		SessObj->SetStringField(TEXT("transmission"), (Sess.Transmission == ETransmissionType::Auto) ? TEXT("自动挡(C2)") : TEXT("手动挡(C1)"));
		SessObj->SetNumberField(TEXT("final_score"), Sess.FinalScore);
		const bool bScoredExam = Sess.PlayMode == EGamePlayMode::SimulatedExam;
		SessObj->SetBoolField(TEXT("scored_exam"), bScoredExam);
		SessObj->SetBoolField(TEXT("passed"), bScoredExam && Sess.bPassed);
		SessObj->SetStringField(TEXT("result_summary"), Sess.ResultSummary);
		SessObj->SetNumberField(TEXT("duration_seconds"), Sess.DurationSeconds);
		SessObj->SetNumberField(TEXT("distance_meters"), Sess.DistanceMeters);
		SessObj->SetStringField(TEXT("primary_weakness"), Sess.PrimaryWeaknessName);
		SessObj->SetStringField(TEXT("coach_advice"), Sess.CoachAdvice);

		// 扣分清单
		TArray<TSharedPtr<FJsonValue>> DedsArr;
		for (const FDeduction& Ded : Sess.Deductions)
		{
			TSharedPtr<FJsonObject> DedObj = MakeShareable(new FJsonObject);
			DedObj->SetNumberField(TEXT("points"), Ded.Points);
			DedObj->SetStringField(TEXT("reason"), Ded.Reason);
			DedObj->SetNumberField(TEXT("time_seconds"), Ded.TimeSeconds);
			DedsArr.Add(MakeShareable(new FJsonValueObject(DedObj)));
		}
		SessObj->SetArrayField(TEXT("deductions"), DedsArr);

		TArray<TSharedPtr<FJsonValue>> ZonesArr;
		for (const FZoneStatus& Zone : Sess.ZoneStatuses)
		{
			TSharedPtr<FJsonObject> ZoneObj = MakeShareable(new FJsonObject);
			ZoneObj->SetStringField(TEXT("name"), Zone.Name);
			ZoneObj->SetNumberField(TEXT("state"), Zone.State);
			ZonesArr.Add(MakeShareable(new FJsonValueObject(ZoneObj)));
		}
		SessObj->SetArrayField(TEXT("zone_statuses"), ZonesArr);

		SessionsArr.Add(MakeShareable(new FJsonValueObject(SessObj)));
	}
	RootObj->SetArrayField(TEXT("recent_sessions"), SessionsArr);

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	if (FJsonSerializer::Serialize(RootObj.ToSharedRef(), Writer))
	{
		// UE's Replace=true move deletes the old destination before moving. Keep
		// the old JSON at a unique backup path until the new file is in place.
		const FString TempPath = OutPath + TEXT(".") + FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".tmp");
		if (!FFileHelper::SaveStringToFile(JsonString, *TempPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			LastJsonExportDiagnostic = TEXT("新JSON未写入，已有JSON保持原样。");
			return false;
		}
		const FString BackupPath = OutPath + TEXT(".") + FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".previous");
		const bool bHadOldJson = IFileManager::Get().FileExists(*OutPath);
		if (bHadOldJson && !IFileManager::Get().Move(*BackupPath, *OutPath, false, false, false, true))
		{
			IFileManager::Get().Delete(*TempPath, false, false, true);
			LastJsonExportDiagnostic = TEXT("旧JSON无法备份，已取消替换并保留原文件。");
			return false;
		}
		if (IFileManager::Get().Move(*OutPath, *TempPath, false, false, false, true))
		{
			if (bHadOldJson) IFileManager::Get().Delete(*BackupPath, false, false, true);
			return true;
		}
		IFileManager::Get().Delete(*TempPath, false, false, true);
		if (!bHadOldJson)
		{
			LastJsonExportDiagnostic = TEXT("JSON文件无法放入目标位置，请检查路径及写入权限。");
		}
		else if (IFileManager::Get().Move(*OutPath, *BackupPath, false, false, false, true))
		{
			LastJsonExportDiagnostic = TEXT("JSON替换失败，已恢复原JSON文件。");
		}
		else
		{
			LastJsonExportDiagnostic = FString::Printf(TEXT("JSON替换及还原失败；旧文件仍保留在 %s，请从此备份恢复。"), *BackupPath);
			UE_LOG(LogTemp, Error, TEXT("[KeMuSanArchive] JSON restore failed; previous archive preserved at %s"), *BackupPath);
		}
		return false;
	}

	return false;
}

void UExamSaveGame::ResetAllHistory()
{
	if (!GetArchivePaths().bValid || !LoadOrCreateSaveGame())
	{
		UE_LOG(LogTemp, Error, TEXT("[KeMuSanArchive] Reset refused; archive is not readable."));
		return;
	}
	UExamSaveGame* EmptySave = Cast<UExamSaveGame>(UGameplayStatics::CreateSaveGameObject(UExamSaveGame::StaticClass()));
	if (EmptySave)
	{
		EmptySave->FormatVersion = 2;
		const bool bBinarySaved = UGameplayStatics::SaveGameToSlot(EmptySave, GetDefaultSlotName(), 0);
		const bool bJsonSaved = bBinarySaved && ExportToJsonFile(EmptySave);
		UE_LOG(LogTemp, Display, TEXT("[KeMuSanArchive] Reset BinarySaved=%d JsonExported=%d"), bBinarySaved, bJsonSaved);
	}
}
