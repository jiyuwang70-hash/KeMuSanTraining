#include "VehicleDynamicsComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

UVehicleDynamicsComponent::UVehicleDynamicsComponent()
{
 PrimaryComponentTick.bCanEverTick=true;
 PrimaryComponentTick.TickGroup=TG_PrePhysics;
}
void UVehicleDynamicsComponent::BeginPlay()
{
 Super::BeginPlay();
 Body=Cast<UBoxComponent>(GetOwner()->GetRootComponent());
 if(!Body)return;
 // The public spawn/reset positions are road-level. The dynamic root is at
 // chassis height; all existing visual/camera children retain their world height.
 for(USceneComponent* Child:Body->GetAttachChildren())
  Child->SetRelativeLocation(Child->GetRelativeLocation()-FVector(0,0,RideHeightCm));
 GetOwner()->AddActorWorldOffset(FVector(0,0,RideHeightCm),false,nullptr,ETeleportType::TeleportPhysics);
 Body->SetCenterOfMass(FVector(0,0,-12));
}
void UVehicleDynamicsComponent::SetHeavyTruck()
{
 if(RoofHull){RoofHull->UnWeldFromParent();RoofHull->DestroyComponent();RoofHull=nullptr;}
 bHeavy=true;RideHeightCm=150.f;WheelRadiusCm=44.f;TireFriction=.95f;
 if(Body)Body->SetCenterOfMass(FVector(0,0,30));
}
bool UVehicleDynamicsComponent::IsOverturned() const
{
 return Body && Body->GetUpVector().Z<.35f;
}
void UVehicleDynamicsComponent::TickComponent(float Dt,ELevelTick Type,FActorComponentTickFunction* Function)
{
 Super::TickComponent(Dt,Type,Function);
 EnsureRoofHull();
 NormalLoad=0;GroundedWheels=0;ContactCenter=FVector::ZeroVector;
 if(!Body||!Body->IsSimulatingPhysics()||Body->GetUpVector().Z<.10f)return;
 const int32 Axles=bHeavy?3:2;
 const float HalfTrack=bHeavy?118.f:84.f;
 const float AxleX[]={bHeavy?320.f:135.f,bHeavy?-180.f:-135.f,-325.f};
 const float WheelMass=Body->GetMass()/(Axles*2.f);
 const float Omega=6.6f;
 const float Spring=WheelMass*Omega*Omega;
 const float Damping=2.f*WheelMass*Omega*.82f;
 const float Rest=RideHeightCm-WheelRadiusCm+980.f/(Omega*Omega);
 FCollisionQueryParams Query(SCENE_QUERY_STAT(VehicleSuspension),false,GetOwner());
 for(int32 Axle=0;Axle<Axles;++Axle)for(float Side:{-1.f,1.f})
 {
  const FVector Anchor=Body->GetComponentTransform().TransformPosition(FVector(AxleX[Axle],Side*HalfTrack,0));
  FHitResult Hit;
  if(!GetWorld()->LineTraceSingleByChannel(Hit,Anchor,Anchor-Body->GetUpVector()*(Rest+WheelRadiusCm+35.f),ECC_WorldStatic,Query))continue;
  const float Compression=Rest-(Hit.Distance-WheelRadiusCm);
  if(Compression<=0.f)continue;
  const float VerticalSpeed=FVector::DotProduct(Body->GetPhysicsLinearVelocityAtPoint(Anchor),Hit.ImpactNormal);
  const float Force=FMath::Clamp(Spring*Compression-Damping*VerticalSpeed,0.f,WheelMass*980.f*3.f);
  // UE forces are kg*cm/s². Ground force at the wheel lever arm produces
  // pitch and load transfer; there is no pose lerp or automatic upright torque.
  Body->AddForceAtLocation(Hit.ImpactNormal*Force,Anchor);
  NormalLoad+=Force;ContactCenter+=Hit.ImpactPoint;++GroundedWheels;
  // Wheel and rim centers follow suspension travel instead of floating with the body.
  const float WheelZ=Body->GetComponentTransform().InverseTransformPosition(Hit.ImpactPoint+Body->GetUpVector()*WheelRadiusCm).Z;
  for(USceneComponent* Child:Body->GetAttachChildren())if(auto* Mesh=Cast<UStaticMeshComponent>(Child))
  {
   if(!Mesh->IsVisible()||!Mesh->GetStaticMesh()||Mesh->GetStaticMesh()->GetName()!=TEXT("Cylinder"))continue;
   FVector Local=Mesh->GetRelativeLocation();
   if(FMath::Abs(Local.X-AxleX[Axle])<5.f && Local.Y*Side>60.f)
   {Local.Z=FMath::Clamp(WheelZ,-RideHeightCm+WheelRadiusCm-25.f,-RideHeightCm+WheelRadiusCm+35.f);Mesh->SetRelativeLocation(Local);}
  }
 }
 if(GroundedWheels)ContactCenter/=GroundedWheels;
 // Small rolling resistance, also active while the driving controller is off.
 if(GroundedWheels && !IsOverturned())
 {
  const FVector V=Body->GetPhysicsLinearVelocity();
  ApplyTireForce(-Body->GetForwardVector()*FVector::DotProduct(V,Body->GetForwardVector())*Body->GetMass()*.015f);
 }
}
void UVehicleDynamicsComponent::ApplyTireForce(const FVector& Force)
{
 if(!Body||!GroundedWheels||IsOverturned())return;
 Body->AddForceAtLocation(Force.GetClampedToMaxSize(NormalLoad*TireFriction),ContactCenter);
}

void UVehicleDynamicsComponent::EnsureRoofHull()
{
 if(RoofHull||!Body||!Body->IsSimulatingPhysics())return;
 const float TotalMass=Body->GetMass();
 // Welded child shapes add their own mass. Allocate the chassis/roof shares
 // before welding, preserving the configured whole-vehicle mass.
 Body->SetMassOverrideInKg(NAME_None,TotalMass*.88f,true);
 RoofHull=NewObject<UBoxComponent>(GetOwner());
 RoofHull->SetupAttachment(Body);
 RoofHull->SetBoxExtent(bHeavy?FVector(315,122,105):FVector(115,85,55));
 RoofHull->SetRelativeLocation(bHeavy?FVector(-115,0,35):FVector(-20,0,50));
 RoofHull->SetCollisionObjectType(ECC_Vehicle);
 RoofHull->SetCollisionResponseToAllChannels(ECR_Ignore);
 RoofHull->SetCollisionResponseToChannel(ECC_Vehicle,ECR_Block);
 RoofHull->SetCollisionResponseToChannel(ECC_WorldStatic,ECR_Block);
 RoofHull->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
 RoofHull->SetNotifyRigidBodyCollision(true);
 RoofHull->OnComponentHit.AddDynamic(this,&UVehicleDynamicsComponent::OnRoofHit);
 RoofHull->BodyInstance.bAutoWeld=true;
 RoofHull->SetMassOverrideInKg(NAME_None,TotalMass*.12f,true);
 RoofHull->SetPhysMaterialOverride(Body->BodyInstance.GetSimplePhysicalMaterial());
 RoofHull->RegisterComponent();RoofHull->WeldTo(Body);
 // Welded roof/cargo geometry participates in the same rigid body's inertia
 // and ground contacts, preventing a rolled roof from clipping through the road.
 Body->SetCenterOfMass(FVector::ZeroVector);
 const FVector GeometricCenter=Body->GetComponentTransform().InverseTransformPosition(Body->GetCenterOfMass());
 Body->SetCenterOfMass(((bHeavy?FVector(0,0,30):FVector(0,0,-12))-GeometricCenter)/.88f);
}

void UVehicleDynamicsComponent::SetCollisionActive(bool Active)
{
 if(RoofHull)RoofHull->SetCollisionEnabled(Active?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
}

void UVehicleDynamicsComponent::OnRoofHit(UPrimitiveComponent*,AActor* Other,UPrimitiveComponent* OtherComponent,FVector Impulse,const FHitResult& Hit)
{
 ++RoofContacts;
 // Chaos dispatches welded-shape hits to the original child component.
 // Forward through the normal vehicle hit path for dents and exam contact rules.
 if(Body)Body->OnComponentHit.Broadcast(Body,Other,OtherComponent,Impulse,Hit);
}

float UVehicleDynamicsComponent::LowestColliderZ() const
{
 float Lowest=FLT_MAX;
 for(auto* Hull:{Body,RoofHull})if(Hull)
 {
  const FVector E=Hull->GetUnscaledBoxExtent();
  for(float X:{-1.f,1.f})for(float Y:{-1.f,1.f})for(float Z:{-1.f,1.f})
   Lowest=FMath::Min(Lowest,static_cast<float>(Hull->GetComponentTransform().TransformPosition(FVector(E.X*X,E.Y*Y,E.Z*Z)).Z));
 }
 return Lowest;
}
