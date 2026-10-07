#pragma once
#include "CoreMinimal.h"
class UBoxComponent;

// Actual Chaos dynamic bodies. Flat training roads constrain vertical/roll/pitch
// motion; longitudinal/lateral motion and yaw remain physically simulated.
namespace VehiclePhysics
{
 void Configure(UBoxComponent* Body, const FVector& HalfExtentCm, float MassKg);
 void Drive(UBoxComponent* Body, float AccelerationMs2, float DesiredYawRateRad, float Dt, bool bGrip=true);
}
