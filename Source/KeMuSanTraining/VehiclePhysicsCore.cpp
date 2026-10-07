#include "VehiclePhysicsCore.h"
#include "Components/BoxComponent.h"
#include "VehicleDynamicsComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "GameFramework/Actor.h"

void VehiclePhysics::Configure(UBoxComponent* Body,const FVector& Extent,float Mass)
{
 Body->SetBoxExtent(Extent);
 Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
 Body->SetCollisionObjectType(ECC_Vehicle);
 Body->SetCollisionResponseToAllChannels(ECR_Ignore);
 Body->SetCollisionResponseToChannel(ECC_Vehicle,ECR_Block);
 Body->SetCollisionResponseToChannel(ECC_WorldStatic,ECR_Block);
 Body->SetEnableGravity(true);
 Body->BodyInstance.bLockZTranslation=false;
 Body->BodyInstance.bLockXRotation=false;
 Body->BodyInstance.bLockYRotation=false;
 Body->BodyInstance.bUseCCD=true;
 Body->SetConstraintMode(EDOFMode::SixDOF);
 Body->SetLinearDamping(.08f);Body->SetAngularDamping(.5f);
 Body->SetNotifyRigidBodyCollision(true);
 UPhysicalMaterial* Material=NewObject<UPhysicalMaterial>(Body);
 Material->Friction=.5f;Material->Restitution=.08f;
 Body->SetPhysMaterialOverride(Material);Body->SetMassOverrideInKg(NAME_None,Mass,true);
}
void VehiclePhysics::Drive(UBoxComponent* Body,float Accel,float YawRate,float Dt,bool Grip,float SideAccel)
{
 if(!Body||!Body->IsSimulatingPhysics()||Dt<=0.f)return;
 auto* Wheels=Body->GetOwner()->FindComponentByClass<UVehicleDynamicsComponent>();
 if(!Wheels||!Wheels->GroundedWheels||Wheels->IsOverturned())return;
 const FVector Forward=Body->GetForwardVector(),Right=Body->GetRightVector();
 const FVector Velocity=Body->GetPhysicsLinearVelocity();
 FVector Acceleration=Forward*(FMath::Clamp(Accel,-12.f,4.f)*100.f);
 if(Grip)Acceleration+=Right*(FMath::Clamp(SideAccel,-1.5f,1.5f)*100.f-FVector::DotProduct(Velocity,Right)*7.f);
 // A single friction circle caps combined brake/drive/lateral forces.
 Wheels->ApplyTireForce(Acceleration*Body->GetMass());
 if(Grip)
 {
  const float Omega=FVector::DotProduct(Body->GetPhysicsAngularVelocityInRadians(),Body->GetUpVector());
  const float AngularAccel=FMath::Clamp((YawRate-Omega)*3.f,-2.5f,2.5f);
  Body->AddTorqueInRadians(Body->GetUpVector()*AngularAccel,NAME_None,true);
 }
}
