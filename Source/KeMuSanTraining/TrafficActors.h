// 场景辅助 Actor：AI 车辆、行人、红绿灯、环境车流管理器
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TrafficActors.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class FRouteTrack;

// 简单 AI 车：由 TrafficManager 驱动位置与速度（用于会车 / 超车 / 环境车流）
UCLASS()
class KEMUSANTRAINING_API AAICar : public AActor
{
	GENERATED_BODY()

public:
	AAICar();

	void InitRoute(const FVector& InStart, const FVector& InEnd, float InSpeedKmh);
	virtual void Tick(float DeltaSeconds) override;

	bool IsActive() const { return bActive; }
	FVector GetCarLocation() const { return GetActorLocation(); }
	float GetHeadingDeg() const { return GetActorRotation().Yaw; }

	// 车流模式接口（位置由管理器每帧写入）
	void Activate(float InSpeedKmh);
	void Deactivate();
	void SetPose(const FVector& Pos, float YawDeg);
	void SetSpeedMs(float InSpeed) { SpeedMs = InSpeed; }
	float GetSpeedMs() const { return SpeedMs; }

protected:
	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* Body = nullptr;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* Cabin = nullptr;

	FVector StartLoc = FVector::ZeroVector;
	FVector EndLoc = FVector::ZeroVector;
	float SpeedMs = 4.f;
	bool bActive = false;
	bool bRouteMode = false;
};

// 过马路的行人
UCLASS()
class KEMUSANTRAINING_API APedestrian : public AActor
{
	GENERATED_BODY()

public:
	APedestrian();

	// 先在路缘等待，再由交通管理器根据车流安全距离放行。
	void PrepareCrossing(const FVector& From, const FVector& To, float SpeedMs);
	void ReleaseCrossing();
	// 兼容直接触发的场景：准备后立即放行。
	void StartCrossing(const FVector& From, const FVector& To, float SpeedMs);
	void StopCrossing();
	virtual void Tick(float DeltaSeconds) override;

	bool IsCrossing() const { return bActive; }
	bool IsWaiting() const { return bQueued && bWaiting; }
	bool IsQueued() const { return bQueued; }
	FVector GetLocation() const { return GetActorLocation(); }
	float GetProgress01() const { return TotalDist > 0.f ? FMath::Clamp(Traveled / TotalDist, 0.f, 1.f) : 0.f; }
	// 是否正在路面上（斑马线附近）
	bool IsOnRoad(float CrosswalkXLocal, float RoadHalfWidth) const;

protected:
	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* BodyComp = nullptr;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* HeadComp = nullptr;

	FVector From = FVector::ZeroVector;
	FVector To = FVector::ZeroVector;
	float Speed = 1.2f;
	bool bActive = false;
	bool bQueued = false;
	bool bWaiting = false;
	float Traveled = 0.f;
	float TotalDist = 1.f;
};

// 红绿灯（服务主线行驶方向）
UCLASS()
class KEMUSANTRAINING_API ATrafficLight : public AActor
{
	GENERATED_BODY()

public:
	ATrafficLight();

	virtual void Tick(float DeltaSeconds) override;

	// 0 红灯  1 黄灯  2 绿灯
	int32 GetState() const { return State; }
	float GetRemaining() const { return Remaining; }
	void ResetLight();

protected:
	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* Pole = nullptr;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* HeadRed = nullptr;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* HeadYellow = nullptr;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* HeadGreen = nullptr;

	UMaterialInstanceDynamic* RedMat = nullptr;
	UMaterialInstanceDynamic* YellowMat = nullptr;
	UMaterialInstanceDynamic* GreenMat = nullptr;

	int32 State = 2;
	float Remaining = 18.f;
};

// 环境车流管理器：
//  - 在路网上持续生成对向车 / 同向慢车，随考生推进循环回收
//  - 提供会车（对向来车）、超车（前方慢车）两个考试事件车
//  - 信号路口红灯等待时放行横向车流，营造真实路口
//  - 检测考试车与社会车辆碰撞
// 自行车（非机动车道行驶）
UCLASS()
class KEMUSANTRAINING_API ABicycle : public AActor
{
	GENERATED_BODY()

public:
	ABicycle();

	void Activate(const FVector& Pos, float YawDeg, float InSpeedMs);
	void Deactivate();
	bool IsActive() const { return bActive; }
	virtual void Tick(float DeltaSeconds) override;

	FVector GetLocation() const { return GetActorLocation(); }
	float GetHeadingDeg() const { return GetActorRotation().Yaw; }

protected:
	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* Frame = nullptr;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* Seat = nullptr;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* WheelF = nullptr;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* WheelR = nullptr;

	float SpeedMs = 4.f;
	bool bActive = false;
};

// 环境车流管理器
UCLASS()
class KEMUSANTRAINING_API ATrafficManager : public AActor
{
	GENERATED_BODY()

public:
	ATrafficManager();

	virtual void Tick(float DeltaSeconds) override;

	void Setup(const FRouteTrack* InTrack);

	// 考试开始 / 结束
	void SetActive(bool bInActive);

	// 每帧由考试控制器更新考生状态
	void UpdatePlayer(const FVector& PlayerPos, const FVector& Heading, float PlayerS, bool bOnReturn,
		float PlayerSpeedKmh, bool bPlayerStopped, int32 TrafficLightState, float TrafficLightRemaining,
		bool bPedestrianWaiting, bool bPedestrianCrossing);

	// 重置全部车流（新的一局）
	void ResetTraffic();

	// 将行人交给交通管理器统一执行等待/放行策略。
	void SetPedestrian(APedestrian* InPedestrian);

	// 给自动测试驾驶员/辅助系统的安全建议速度，单位 km/h。
	// 该值只反映交通参与者和信号优先级，不替代考试评分规则。
	float GetRecommendedSpeedKmh() const;

	// 可复现交通脚本：默认 deterministic，允许命令行切换场景。
	FString GetScenarioName() const { return ScenarioName; }
	int32 GetScenarioSeed() const { return ScenarioSeed; }

	// 会车事件：在考生前方生成对向来车
	void SpawnMeetingCar(float PlayerS);
	bool IsMeetingCarActive() const;

	// 超车事件：在考生同车道前方生成低速车
	void SpawnSlowCar(float PlayerS);
	bool IsSlowCarActive() const;
	float GetSlowCarS() const;

	// 与任意活动车辆的碰撞检测（OBB 近似），返回是否接触
	bool HitsPlayer(const FVector& PlayerPos, float PlayerYawDeg) const;
	bool HitsPedestrian(const FVector& PlayerPos) const;
	bool HitsBicycle(const FVector& PlayerPos, float PlayerYawDeg) const;

	// 信号路口：考生红灯临近停车时放行横向车流
	void NotifyLightRed(bool bPlayerNearStopLine);

private:
	struct FAmbientCar
	{
		AAICar* Car = nullptr;

		float S = 0.f;
		int32 Dir = 1;        // +1 顺路线（同向），-1 逆路线（对向）
		float CruiseMs = 8.f;
		bool bFollowing = false;
		bool bYielding = false;
		bool bLastFollowing = false;
		bool bLastYielding = false;
	};

	AAICar* AcquireCar();
	void RecycleCar(AAICar* Car);
	void TrySpawnAmbient();
	void TickAmbient(float DT);
	void TickScripted(float DT);
	void TrySpawnCrosser();
	void TickCrosser(float DT);
	void UpdatePedestrianPolicy(float DT);
	void ApplyVehiclePolicy(float DT);
	void TrySpawnScenarioTraffic();
	void ActivateCrosswalkScenarioTraffic();
	bool IsPlayerApproachingCrosswalk() const;
	bool IsPlayerStoppedForCrosswalk() const;

	const FRouteTrack* Track = nullptr;
	bool bActive = false;

	UPROPERTY()
	TArray<AAICar*> CarPool;

	UPROPERTY()
	TArray<ABicycle*> BikePool;

	UPROPERTY()
	TArray<AAICar*> ParkedCars;

	UPROPERTY()
	AAICar* MeetingCar = nullptr;

	UPROPERTY()
	AAICar* SlowCar = nullptr;

	UPROPERTY()
	AAICar* Crosser = nullptr;

	UPROPERTY()
	APedestrian* PedestrianActor = nullptr;

	TArray<FAmbientCar> Ambients;

	FVector PlayerPos = FVector::ZeroVector;
	FVector PlayerHeading = FVector::ForwardVector;
	float PlayerS = 0.f;
	bool bPlayerOnReturn = false;
	float PlayerSpeedKmh = 0.f;
	bool bPlayerStopped = false;
	int32 TrafficLightState = 2;
	float TrafficLightRemaining = 0.f;
	bool bPedestrianWaiting = false;
	bool bPedestrianCrossing = false;
	bool bScenarioStarted = false;
	bool bCrosswalkVehiclesActivated = false;
	FString ScenarioName = TEXT("road_test_default");
	int32 ScenarioSeed = 20260823;
	FRandomStream ScenarioRandom;
	float PlayerYieldTimer = 0.f;
	float CrosswalkYieldTimer = 0.f;

	float SpawnTimer = 2.f;
	float CrossCooldown = 0.f;

	// 横向车流路径（信号路口）
	FVector CrosserFrom = FVector::ZeroVector;
	FVector CrosserTo = FVector::ZeroVector;
	FVector CrosserFrom2 = FVector::ZeroVector;
	FVector CrosserTo2 = FVector::ZeroVector;
	float CrosserSpeed = 8.f;

	// 自行车流
	void TrySpawnBike(float DT);
	void TickBikes(float DT);
	float BikeSpawnTimer = 3.f;
};
