#pragma once
#include "CoreMinimal.h"
class UBoxComponent;

// Free 3D Chaos chassis, ray suspension and load-limited tire contact forces.
namespace VehiclePhysics
{
 void Configure(UBoxComponent* Body, const FVector& HalfExtentCm, float MassKg);
 void Drive(UBoxComponent* Body, float AccelerationMs2, float DesiredYawRateRad, float Dt, bool bGrip=true, float SideAccelerationMs2=0.f);
}
