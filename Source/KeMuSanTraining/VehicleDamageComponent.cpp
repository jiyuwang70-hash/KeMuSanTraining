#include "VehicleDamageComponent.h"
#include "ProceduralMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

void UVehicleDamageComponent::BeginPlay()
{
 Super::BeginPlay();RebuildPanels();
}
void UVehicleDamageComponent::Upload(FDamagePanel& P,bool Create)
{
 // Area-weighted face normals in linear time; no quadratic vertex-overlap search.
 P.Normals.Init(FVector::ZeroVector,P.Vertices.Num());
 for(int32 T=0;T<P.Triangles.Num();T+=3)
 {
  int32 A=P.Triangles[T],B=P.Triangles[T+1],C=P.Triangles[T+2];
  const FVector N=FVector::CrossProduct(P.Vertices[B]-P.Vertices[C],P.Vertices[A]-P.Vertices[C]);
  P.Normals[A]+=N;P.Normals[B]+=N;P.Normals[C]+=N;
 }
 TArray<FProcMeshTangent> Tangents;Tangents.Reserve(P.Vertices.Num());
 for(FVector& N:P.Normals)
 {
  N=N.GetSafeNormal();
  FVector Tangent=FVector::CrossProduct(FMath::Abs(N.Z)<.9f?FVector::UpVector:FVector::RightVector,N).GetSafeNormal();
  Tangents.Add(FProcMeshTangent(Tangent,false));
 }
 if(Create)P.Mesh->CreateMeshSection(0,P.Vertices,P.Triangles,P.Normals,P.UV,TArray<FColor>(),Tangents,false);
 else P.Mesh->UpdateMeshSection(0,P.Vertices,P.Normals,P.UV,TArray<FColor>(),Tangents);
}
void UVehicleDamageComponent::RebuildPanels()
{
 for(auto& P:Panels)if(P.Mesh)P.Mesh->DestroyComponent();
 Panels.Empty();
 // Hidden originals remain available for reset/rebuild and shared lamp materials.
 for(auto* C:Converted)if(IsValid(C))C->SetVisibility(!GetOwner()->Tags.Contains(TEXT("HeavyTruck")));
 Converted.Empty();
 TArray<UStaticMeshComponent*> Parts;GetOwner()->GetComponents(Parts);
 for(auto* Part:Parts)
 {
  const FString Name=Part->GetName();
  if(Part->GetAttachParent()!=GetOwner()->GetRootComponent()||!Part->IsVisible()||!Part->GetStaticMesh()||Part->GetStaticMesh()->GetName()!=TEXT("Cube"))continue;
  if(Name.Contains(TEXT("Seat"))||Name.Contains(TEXT("Dashboard"))||Name.Contains(TEXT("Steering"))||Name.Contains(TEXT("Mirror")))continue;
  FDamagePanel P;
  P.Mesh=NewObject<UProceduralMeshComponent>(GetOwner());
  P.Mesh->SetupAttachment(GetOwner()->GetRootComponent());
  P.Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  P.Mesh->RegisterComponent();P.Mesh->SetMaterial(0,Part->GetMaterial(0));
  const FTransform Transform=Part->GetRelativeTransform();
  // Six separately normaled, subdivided faces. Fine enough for a local dent,
  // unlike moving the eight corners of the engine cube.
  const FVector Normals[]={FVector(1,0,0),FVector(-1,0,0),FVector(0,1,0),FVector(0,-1,0),FVector(0,0,1),FVector(0,0,-1)};
  for(FVector N:Normals)
  {
   FVector U=N.X!=0?FVector(0,1,0):FVector(1,0,0);
   const FVector V=FVector::CrossProduct(N,U);
   const int32 NU=FMath::Clamp(FMath::CeilToInt(Transform.TransformVector(U*100).Size()/18.f),2,32);
   const int32 NV=FMath::Clamp(FMath::CeilToInt(Transform.TransformVector(V*100).Size()/18.f),2,32);
   const int32 Base=P.Vertices.Num();
   for(int32 Y=0;Y<=NV;++Y)for(int32 X=0;X<=NU;++X)
   {
    const FVector Pos=N*50.f+U*(100.f*X/NU-50.f)+V*(100.f*Y/NV-50.f);
    P.Vertices.Add(Transform.TransformPosition(Pos));P.UV.Add(FVector2D(float(X)/NU,float(Y)/NV));
   }
   for(int32 Y=0;Y<NV;++Y)for(int32 X=0;X<NU;++X)
   {
    int32 A=Base+Y*(NU+1)+X,B=A+1,C=A+NU+1,D=C+1;
    // UE clockwise front faces.
    P.Triangles.Append({A,C,B,B,C,D});
   }
  }
  P.Original=P.Vertices;Upload(P,true);Part->SetVisibility(false);Converted.Add(Part);Panels.Add(MoveTemp(P));
 }
}
void UVehicleDamageComponent::Impact(const FHitResult& Hit,const FVector& Impulse,float MassKg)
{
 const float DeltaV=Impulse.Size()/FMath::Max(1.f,MassKg)/100.f;
 if(DeltaV<.45f||GetWorld()->GetTimeSeconds()-LastImpactTime<.12f)return;
 LastImpactTime=GetWorld()->GetTimeSeconds();
 const FTransform Root=GetOwner()->GetActorTransform();
 const FVector Center=Root.InverseTransformPosition(Hit.ImpactPoint);
 const FVector Inward=Root.InverseTransformVectorNoScale(Impulse.GetSafeNormal());
 const float Depth=FMath::Clamp((DeltaV-.45f)*7.f,0.f,40.f);
 const float Radius=FMath::Clamp(85.f+DeltaV*8.f,85.f,180.f);
 for(auto& P:Panels)
 {
  bool Changed=false;
  for(int32 I=0;I<P.Vertices.Num();++I)
  {
   const float R=FVector::Dist(P.Vertices[I],Center)/Radius;
   if(R>=1.f)continue;
   const float Weight=FMath::Square(1.f-R*R);
   // A small deterministic fold perturbs the panel normal, making crumples
   // visible in lighting. Displacement always stays inside a finite crush budget.
   const float Fold=1.f+.16f*FMath::Sin(P.Original[I].Y*.18f+P.Original[I].Z*.13f);
   FVector Offset=P.Vertices[I]-P.Original[I]+Inward*(Depth*Weight*Fold);
   Offset=Offset.GetClampedToMaxSize(60.f);
   P.Vertices[I]=P.Original[I]+Offset;Changed=true;
  }
  if(Changed)Upload(P,false);
 }
 UE_LOG(LogTemp,Log,TEXT("[KeMuSanDamage] impact delta_v_ms=%.3f dent_cm=%.2f vertices=%d"),DeltaV,MaxDentCm(),DamagedVertexCount());
}
void UVehicleDamageComponent::Repair()
{
 LastImpactTime=-1000.f;
 for(auto& P:Panels)
 {
  bool Changed=false;
  for(int32 I=0;I<P.Vertices.Num();++I)if(!P.Vertices[I].Equals(P.Original[I],.001f)){Changed=true;break;}
  if(Changed){P.Vertices=P.Original;Upload(P,false);}
 }
}
float UVehicleDamageComponent::MaxDentCm() const
{
 float Max=0;for(const auto& P:Panels)for(int32 I=0;I<P.Vertices.Num();++I)Max=FMath::Max(Max,static_cast<float>(FVector::Dist(P.Vertices[I],P.Original[I])));return Max;
}
int32 UVehicleDamageComponent::DamagedVertexCount() const
{
 int32 Count=0;for(const auto& P:Panels)for(int32 I=0;I<P.Vertices.Num();++I)if(FVector::DistSquared(P.Vertices[I],P.Original[I])>.01f)++Count;return Count;
}
