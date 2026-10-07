#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VehicleDamageComponent.generated.h"
class UProceduralMeshComponent;
class UStaticMeshComponent;
struct FDamagePanel
{
 UProceduralMeshComponent* Mesh=nullptr;
 TArray<FVector> Original,Vertices,Normals;
 TArray<int32> Triangles;
 TArray<FVector2D> UV;
};
// Permanent impact-driven surface plasticity; the Chaos chassis stays rigid.
UCLASS()
class KEMUSANTRAINING_API UVehicleDamageComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 virtual void BeginPlay() override;
 void RebuildPanels();
 void Impact(const FHitResult& Hit,const FVector& Impulse,float MassKg);
 void Repair();
 float MaxDentCm() const;
 int32 DamagedVertexCount() const;
private:
 TArray<FDamagePanel> Panels;
 TSet<UStaticMeshComponent*> Converted;
 float LastImpactTime=-1000.f;
 void Upload(FDamagePanel& Panel,bool Create);
};
