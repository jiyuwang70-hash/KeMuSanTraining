// 场景辅助 Actor：AI 车辆、行人、红绿灯
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TrafficActors.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;

// 简单 AI 车（直线行驶，用于会车 / 超车）
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

protected:
	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* Body = nullptr;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* Cabin = nullptr;

	FVector StartLoc = FVector::ZeroVector;
	FVector EndLoc = FVector::ZeroVector;
	float SpeedMs = 4.f;
	bool bActive = false;
};

// 过马路的行人
UCLASS()
class KEMUSANTRAINING_API APedestrian : public AActor
{
	GENERATED_BODY()

public:
	APedestrian();

	void StartCrossing(const FVector& From, const FVector& To, float SpeedMs);
	void StopCrossing();
	virtual void Tick(float DeltaSeconds) override;

	bool IsCrossing() const { return bActive; }
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
	float Traveled = 0.f;
	float TotalDist = 1.f;
};

// 红绿灯（只服务 +X 行驶方向）
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
