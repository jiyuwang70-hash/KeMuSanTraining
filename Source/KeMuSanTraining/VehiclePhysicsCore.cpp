#include "VehiclePhysicsCore.h"
#include "Components/BoxComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

void VehiclePhysics::Configure(UBoxComponent* Body, const FVector& Extent, float Mass)
{
 Body->SetBoxExtent(Extent);
 Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
 Body->SetCollisionObjectType(ECC_Vehicle);
 Body->SetCollisionResponseToAllChannels(ECR_Ignore);
 Body->SetCollisionResponseToChannel(ECC_Vehicle,ECR_Block);
 Body->SetEnableGravity(false);
 Body->BodyInstance.bLockZTranslation=true;
 Body->BodyInstance.bLockXRotation=true;
 Body->BodyInstance.bLockYRotation=true;
 Body->BodyInstance.bUseCCD=true;
 Body->SetConstraintMode(EDOFMode::SixDOF);
 Body->SetLinearDamping(0.08f);
 Body->SetAngularDamping(0.5f);
 Body->SetNotifyRigidBodyCollision(true);
 UPhysicalMaterial* Material=NewObject<UPhysicalMaterial>(Body);
 Material->Friction=0.55f;
 Material->Restitution=0.12f;
 Body->SetPhysMaterialOverride(Material);
 Body->SetMassOverrideInKg(NAME_None,Mass,true);
}

void VehiclePhysics::Drive(UBoxComponent* Body,float Accel,float YawRate,float Dt,bool Grip)
{
 if(!Body || !Body->IsSimulatingPhysics() || Dt<=0.f)return;
 const FVector Forward=Body->GetForwardVector(),Right=Body->GetRightVector();
 const FVector Velocity=Body->GetPhysicsLinearVelocity();
 FVector Acceleration=Forward*(FMath::Clamp(Accel,-12.f,4.f)*100.f);
 if(Grip)Acceleration-=Right*FVector::DotProduct(Velocity,Right)*4.f;
 Body->AddForce(Acceleration*Body->GetMass());
 const float Omega=Body->GetPhysicsAngularVelocityInRadians().Z;
 const float AngularAccel=FMath::Clamp((YawRate-Omega)*3.f,-2.5f,2.5f);
 Body->AddTorqueInRadians(FVector(0,0,AngularAccel),NAME_None,true);
}
