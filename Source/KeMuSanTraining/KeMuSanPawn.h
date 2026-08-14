// 科目三考试车 Pawn：手动挡运动学车辆 + 灯光/信号/安全带/手刹
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "ExamTypes.h"
#include "KeMuSanPawn.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;

UCLASS()
class KEMUSANTRAINING_API AKeMuSanPawn : public APawn
{
	GENERATED_BODY()

public:
	AKeMuSanPawn();

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	// ---- 输入（由按键调用） ----
	void AxisThrottle(float V);
	void AxisBrake(float V);
	void AxisSteer(float V);
	void ToggleHandbrake();
	void ToggleSeatbelt();
	void ToggleHazard();
	void ToggleFogLamp();
	void CycleLights();
	void FlashHighBeam();
	void ToggleLeftSignal();
	void ToggleRightSignal();
	void PressHorn();
	void ReleaseHorn();
	void NotifyHeadCheck();
	void SelectGear(int32 GearIndex); // 0=N 1=R 2..6=1..5挡

	// ---- 状态查询 ----
	float GetSpeedMs() const { return SpeedMs; }
	float GetSpeedKmh() const { return SpeedMs * 3.6f; }
	EGear GetGear() const { return Gear; }
	bool IsHandbrakeOn() const { return bHandbrake; }
	bool IsSeatbeltOn() const { return bSeatbelt; }
	bool IsLowBeamOn() const { return bLowBeam; }
	bool IsHighBeamOn() const { return bHighBeam; }
	bool IsFogLampOn() const { return bFogLamp; }
	bool IsOutlineOn() const { return bOutline; }
	bool IsHazardOn() const { return bHazard; }
	bool IsLeftSignalOn() const { return bLeftSignal; }
	bool IsRightSignalOn() const { return bRightSignal; }
	bool IsHornHeld() const { return bHornHeld; }
	bool IsStalled() const { return bStalled; }
	bool IsFlashHigh() const { return bFlashHigh; }
	float GetEngineRpm() const { return EngineRpm; }
	float GetYawDeg() const { return YawDeg; }
	float GetSteeringAngleDeg() const { return SteeringAngleDeg; }
	float GetThrottle() const { return ThrottleInput; }
	float GetBrake() const { return BrakeInput; }
	float GetLastHeadCheckTime() const { return LastHeadCheckTime; }

	// 复位到起点（开始新一局考试）
	void ResetVehicle(const FVector& Loc, const FRotator& Rot);

	// 强制停车（驶出路线终点时）
	void ForceStop();

protected:
	// ---- 视觉组件 ----
	UPROPERTY(VisibleAnywhere, Category = "Vehicle")
	USceneComponent* Root = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Vehicle")
	UStaticMeshComponent* Body = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Vehicle")
	UStaticMeshComponent* Cabin = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Vehicle")
	UStaticMeshComponent* WheelFL = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Vehicle")
	UStaticMeshComponent* WheelFR = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Vehicle")
	UStaticMeshComponent* WheelRL = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Vehicle")
	UStaticMeshComponent* WheelRR = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Vehicle")
	UStaticMeshComponent* LightFL = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Vehicle")
	UStaticMeshComponent* LightFR = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Vehicle")
	UStaticMeshComponent* TailL = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Vehicle")
	UStaticMeshComponent* TailR = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Vehicle")
	UStaticMeshComponent* SigL = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Vehicle")
	UStaticMeshComponent* SigR = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	USpringArmComponent* SpringArm = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	UCameraComponent* Camera = nullptr;

	// ---- 动态材质 ----
	UMaterialInstanceDynamic* TailMatL = nullptr;
	UMaterialInstanceDynamic* TailMatR = nullptr;
	UMaterialInstanceDynamic* SigMatL = nullptr;
	UMaterialInstanceDynamic* SigMatR = nullptr;
	UMaterialInstanceDynamic* LightMatL = nullptr;
	UMaterialInstanceDynamic* LightMatR = nullptr;

	// ---- 输入状态 ----
	float ThrottleInput = 0.f;
	float PrevThrottle = 0.f;
	float BrakeInput = 0.f;
	float SteeringInput = 0.f;
	bool bHandbrake = true;
	bool bSeatbelt = false;
	bool bLowBeam = false;
	bool bHighBeam = false;
	bool bFogLamp = false;
	bool bOutline = false;
	bool bHazard = false;
	bool bLeftSignal = false;
	bool bRightSignal = false;
	bool bHornHeld = false;

	// ---- 动力学状态 ----
	float SpeedMs = 0.f;
	float YawDeg = 0.f;
	float SteeringAngleDeg = 0.f;
	float EngineRpm = 900.f;
	EGear Gear = EGear::N;
	bool bStalled = false;
	float StallCooldown = 0.f;
	float SignalYawAccum = 0.f;
	float LastHeadCheckTime = -9999.f;
	bool bFlashHigh = false;
	float FlashHighTimer = 0.f;

	void UpdatePhysics(float DT);
	void UpdateVisuals(float DT);
};
