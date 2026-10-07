#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VehicleDynamicsComponent.generated.h"
class UBoxComponent;
class UPrimitiveComponent;
// Ray suspension runs independently of the driving AI/player Tick.
UCLASS()
class KEMUSANTRAINING_API UVehicleDynamicsComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UVehicleDynamicsComponent();
 virtual void BeginPlay() override;
 virtual void TickComponent(float Dt,ELevelTick Type,FActorComponentTickFunction* Function) override;
 void SetHeavyTruck();
 void SetCollisionActive(bool Active);
 float RideHeightCm=85.f,WheelRadiusCm=32.5f;
 float NormalLoad=0.f; int32 GroundedWheels=0;
 FVector ContactCenter=FVector::ZeroVector;
 float TireFriction=1.20f;
 int32 RoofContacts=0;
 float LowestColliderZ() const;
 bool IsOverturned() const;
 void ApplyTireForce(const FVector& Force);
private:
 UPROPERTY() UBoxComponent* Body=nullptr;
 UPROPERTY() UBoxComponent* RoofHull=nullptr;
 void EnsureRoofHull();
 UFUNCTION() void OnRoofHit(UPrimitiveComponent* Component,AActor* OtherActor,UPrimitiveComponent* OtherComponent,FVector Impulse,const FHitResult& Hit);
 bool bHeavy=false;
};
