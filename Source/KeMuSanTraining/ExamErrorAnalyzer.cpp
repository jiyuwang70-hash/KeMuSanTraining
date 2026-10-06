// 考试错误分析与教练诊断引擎实现
#include "ExamErrorAnalyzer.h"

EErrorCategory UExamErrorAnalyzer::ClassifyDeductionReason(const FString& Reason)
{
	// Traffic lights govern right of way. Check them before the general “灯”
	// match so a red-light violation never receives lamp-operation advice.
	if (Reason.Contains(TEXT("红灯")) || Reason.Contains(TEXT("黄灯")))
	{
		return EErrorCategory::RulesAndWay;
	}
	// 1. 安全观察类
	if (Reason.Contains(TEXT("观察")) ||
		Reason.Contains(TEXT("后视镜")) ||
		Reason.Contains(TEXT("盲区")) ||
		Reason.Contains(TEXT("转头")) ||
		Reason.Contains(TEXT("观望")) ||
		Reason.Contains(TEXT("回头")) ||
		Reason.Contains(TEXT("未看")))
	{
		return EErrorCategory::ObservationSafety;
	}

	// 2. 灯光信号类
	if (Reason.Contains(TEXT("灯")) ||
		Reason.Contains(TEXT("远光")) ||
		Reason.Contains(TEXT("近光")) ||
		Reason.Contains(TEXT("示廓")) ||
		Reason.Contains(TEXT("双闪")) ||
		Reason.Contains(TEXT("雾灯")) ||
		Reason.Contains(TEXT("闪烁")) ||
		Reason.Contains(TEXT("3秒")))
	{
		return EErrorCategory::LightingSignal;
	}

	// 3. 车辆操纵类
	if (Reason.Contains(TEXT("手刹")) ||
		Reason.Contains(TEXT("驻车制动")) ||
		Reason.Contains(TEXT("挡位")) ||
		Reason.Contains(TEXT("熄火")) ||
		Reason.Contains(TEXT("加减挡")) ||
		Reason.Contains(TEXT("跳挡")) ||
		Reason.Contains(TEXT("越级")) ||
		Reason.Contains(TEXT("溜车")) ||
		Reason.Contains(TEXT("后溜")) ||
		Reason.Contains(TEXT("离合")) ||
		Reason.Contains(TEXT("空挡")))
	{
		return EErrorCategory::VehicleControl;
	}

	// 4. 路权规范与安全底线类
	if (Reason.Contains(TEXT("实线")) ||
		Reason.Contains(TEXT("中心线")) ||
		Reason.Contains(TEXT("分界线")) ||
		Reason.Contains(TEXT("压线")) ||
		Reason.Contains(TEXT("边缘石")) ||
		Reason.Contains(TEXT("路缘石")) ||
		Reason.Contains(TEXT("停车")) ||
		Reason.Contains(TEXT("车道")) ||
		Reason.Contains(TEXT("路面")) ||
		Reason.Contains(TEXT("路口")) ||
		Reason.Contains(TEXT("红灯")) ||
		Reason.Contains(TEXT("黄灯")) ||
		Reason.Contains(TEXT("横道")) ||
		Reason.Contains(TEXT("行人")) ||
		Reason.Contains(TEXT("减速")) ||
		Reason.Contains(TEXT("公交")) ||
		Reason.Contains(TEXT("学校")) ||
		Reason.Contains(TEXT("超速")) ||
		Reason.Contains(TEXT("时速")) ||
		Reason.Contains(TEXT("安全带")) ||
		Reason.Contains(TEXT("碰撞")) ||
		Reason.Contains(TEXT("出道路")) ||
		Reason.Contains(TEXT("右侧超车")) ||
		Reason.Contains(TEXT("超车")) ||
		Reason.Contains(TEXT("掉头")) ||
		Reason.Contains(TEXT("直线行驶")))
	{
		return EErrorCategory::RulesAndWay;
	}

	return EErrorCategory::Other;
}

FString UExamErrorAnalyzer::GetCategoryDisplayName(EErrorCategory Category)
{
	switch (Category)
	{
	case EErrorCategory::ObservationSafety: return TEXT("安全观察类");
	case EErrorCategory::LightingSignal:    return TEXT("灯光信号类");
	case EErrorCategory::VehicleControl:    return TEXT("车辆操纵类");
	case EErrorCategory::RulesAndWay:       return TEXT("路权规范类");
	default:                                return TEXT("其他综合类");
	}
}

FString UExamErrorAnalyzer::GetCoachAdviceForCategory(EErrorCategory Category)
{
	switch (Category)
	{
		case EErrorCategory::ObservationSafety:
			return TEXT("起步、变道前按 M 观察；确认后方安全，再打灯等满3秒转向。");

	case EErrorCategory::LightingSignal:
			return TEXT("Q/E 开左/右转向灯后等满3秒；灯光题看清题意再按1–5作答。");

	case EErrorCategory::VehicleControl:
			return TEXT("先松手刹再给 W 油门；手动挡用1挡起步，逐级换挡，避免低速高挡。");

	case EErrorCategory::RulesAndWay:
			return TEXT("用 S 提前减速；红灯停车，斑马线先让行，靠边停车控制路缘间距。");

	default:
			return TEXT("根据本场错误和当前项目提示复练；先完成一项操作，再继续下一项。");
	}
}

FString UExamErrorAnalyzer::GetCoachAdviceForReason(const FString& Reason)
{
	if (Reason.Contains(TEXT("红灯")) || Reason.Contains(TEXT("黄灯")))
		return TEXT("接近停止线先松 W，用 S 停车；等待绿灯，确认路口安全再通行。");
	if (Reason.Contains(TEXT("行人")) || Reason.Contains(TEXT("横道")))
		return TEXT("接近斑马线提前用 S 减速；行人正在过街时停车等待，不能抢行。");
	if (Reason.Contains(TEXT("未系安全带")))
		return TEXT("在上车准备阶段按 F 系好安全带，再按 M 观察后继续起步。");
	if (Reason.Contains(TEXT("驻车制动")) || Reason.Contains(TEXT("手刹")))
		return TEXT("起步前按空格松手刹，确认手刹标志消失，再用 W 加速。");
	if (Reason.Contains(TEXT("后溜")) || Reason.Contains(TEXT("溜车")))
		return TEXT("停车时保持 S 制动；起步先选合适挡位再缓慢加速，防止向后溜车。");
	if (Reason.Contains(TEXT("熄火")))
		return TEXT("先用 S 停稳，手动挡按1选择1挡，再缓慢给 W 油门起步。");
	if (Reason.Contains(TEXT("换挡")) || Reason.Contains(TEXT("加减挡")))
		return TEXT("手动挡按1–5逐级换挡，先匹配车速；按提示完成加挡和减挡。");
	if (Reason.Contains(TEXT("停车")) || Reason.Contains(TEXT("路缘石")))
		return TEXT("E 开右转向灯并等满3秒，按 M 观察；用 S 停稳，挂 N 后拉手刹。");
	if (Reason.Contains(TEXT("灯光模拟")))
		return TEXT("灯光题按1近光、2远光、3交替、4示廓双闪、5雾灯双闪作答。");
	if (Reason.Contains(TEXT("学校")) || Reason.Contains(TEXT("公交")) || Reason.Contains(TEXT("时速")) || Reason.Contains(TEXT("超速")))
		return TEXT("提前松 W、用 S 控制车速，按画面当前路段限速通行。");
	if (Reason.Contains(TEXT("碰撞")))
		return TEXT("与前车和行人保持距离；提前用 S 制动，确认道路畅通再加速。");
	if (Reason.Contains(TEXT("直线行驶")))
		return TEXT("直线路段放缓 A/D 转向输入，小幅修正方向，保持车道中心。");
	if (Reason.Contains(TEXT("驶出路面")) || Reason.Contains(TEXT("分界线")) || Reason.Contains(TEXT("压线")))
		return TEXT("先松 W 降速，轻点 A/D 回正方向；保持本车道，避免压线和驶出路面。");
	return GetCoachAdviceForCategory(ClassifyDeductionReason(Reason));
}

FExamAnalysisResult UExamErrorAnalyzer::AnalyzeExamSession(
	int32 FinalScore,
	bool bFailIssued,
	const TArray<FDeduction>& Deductions,
	float DurationSeconds,
	float DistanceMeters)
{
	FExamAnalysisResult Out;

	for (const FDeduction& Ded : Deductions)
	{
		if (Ded.Points <= 0) continue;
		const EErrorCategory Cat = ClassifyDeductionReason(Ded.Reason);
		const FString CatName = GetCategoryDisplayName(Cat);

		switch (Cat)
		{
		case EErrorCategory::ObservationSafety:
			Out.ObservationDeductions += Ded.Points;
			Out.ObservationCount++;
			break;
		case EErrorCategory::LightingSignal:
			Out.LightingDeductions += Ded.Points;
			Out.LightingCount++;
			break;
		case EErrorCategory::VehicleControl:
			Out.VehicleControlDeductions += Ded.Points;
			Out.VehicleControlCount++;
			break;
		case EErrorCategory::RulesAndWay:
			Out.RulesAndWayDeductions += Ded.Points;
			Out.RulesAndWayCount++;
			break;
		default:
			Out.OtherDeductions += Ded.Points;
			Out.OtherCount++;
			break;
		}

		if (Ded.Points >= 100)
		{
			Out.bHadFatalViolation = true;
			if (Out.FatalViolationReason.IsEmpty())
			{
				Out.FatalViolationReason = Ded.Reason;
			}
		}

		Out.CategorizedErrorDetails.Add(FString::Printf(TEXT("[%s] -%d分: %s"), *CatName, Ded.Points, *Ded.Reason));
	}

	// 计算最主要失误薄弱维度
	int32 MaxDed = 0;
	if (Out.ObservationDeductions > MaxDed)
	{
		MaxDed = Out.ObservationDeductions;
		Out.PrimaryWeakness = EErrorCategory::ObservationSafety;
	}
	if (Out.LightingDeductions > MaxDed)
	{
		MaxDed = Out.LightingDeductions;
		Out.PrimaryWeakness = EErrorCategory::LightingSignal;
	}
	if (Out.VehicleControlDeductions > MaxDed)
	{
		MaxDed = Out.VehicleControlDeductions;
		Out.PrimaryWeakness = EErrorCategory::VehicleControl;
	}
	if (Out.RulesAndWayDeductions > MaxDed)
	{
		MaxDed = Out.RulesAndWayDeductions;
		Out.PrimaryWeakness = EErrorCategory::RulesAndWay;
	}
	if (Out.OtherDeductions > MaxDed)
	{
		MaxDed = Out.OtherDeductions;
		Out.PrimaryWeakness = EErrorCategory::Other;
	}

	Out.PrimaryWeaknessName = MaxDed > 0 ? GetCategoryDisplayName(Out.PrimaryWeakness) : TEXT("暂无明显薄弱项");

	// 评定综合等级
	if (FinalScore >= 100 && Deductions.Num() == 0 && !bFailIssued)
	{
		Out.PerformanceRating = TEXT("本场100分 · 无扣分记录");
		Out.CoachAdvices.Add(TEXT("本场没有记录到扣分；继续保持观察、打灯和提前减速的操作顺序。"));
	}
	else if (FinalScore >= 90 && !bFailIssued)
	{
		Out.PerformanceRating = FString::Printf(TEXT("本场合格 · %d分"), FinalScore);
	}
	else
	{
		if (Out.bHadFatalViolation)
		{
			Out.PerformanceRating = TEXT("本场不合格 · 触发直接判定不合格的违规");
			Out.CoachAdvices.Add(FString::Printf(TEXT("先纠正：%s。%s"), *Out.FatalViolationReason, *GetCoachAdviceForReason(Out.FatalViolationReason)));
		}
		else
		{
			Out.PerformanceRating = FinalScore < 90
				? FString::Printf(TEXT("本场不合格 · %d分，低于90分及格线"), FinalScore)
				: FString::Printf(TEXT("本场不合格 · %d分，已触发不合格判定"), FinalScore);
		}

	}

	// Pick the actual highest-cost mistake in the weakest category. Category
	// summaries alone are too broad to tell a learner what to practise next.
	const FDeduction* PrimaryError = nullptr;
	for (const FDeduction& Ded : Deductions)
	{
		if (Ded.Points > 0 && ClassifyDeductionReason(Ded.Reason) == Out.PrimaryWeakness &&
			(!PrimaryError || Ded.Points > PrimaryError->Points)) PrimaryError = &Ded;
	}
	if (PrimaryError && (!Out.bHadFatalViolation || PrimaryError->Reason != Out.FatalViolationReason))
	{
		Out.CoachAdvices.Add(FString::Printf(TEXT("优先复练：%s。%s"), *PrimaryError->Reason, *GetCoachAdviceForReason(PrimaryError->Reason)));
	}
	if (Out.CoachAdvices.IsEmpty())
	{
		Out.CoachAdvices.Add(bFailIssued ? TEXT("本场已判定不合格，请结合违规记录复练；没有记录到明确原因时保留日志进行排查。") : TEXT("本场操作已记录，继续按项目提示练习观察、打灯与车速控制。"));
	}

	return Out;
}
