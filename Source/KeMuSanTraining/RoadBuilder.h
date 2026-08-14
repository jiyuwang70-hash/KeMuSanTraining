// 道路生成器：在运行时用基础几何体搭建整条科目三考试路线
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RoadBuilder.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UDirectionalLightComponent;
class USkyLightComponent;
class USkyAtmosphereComponent;

UCLASS()
class KEMUSANTRAINING_API ARoadBuilder : public AActor
{
	GENERATED_BODY()

public:
	ARoadBuilder();

	virtual void BeginPlay() override;

protected:
	UPROPERTY(VisibleAnywhere)
	USceneComponent* Root = nullptr;

	UPROPERTY(VisibleAnywhere)
	UDirectionalLightComponent* Sun = nullptr;

	UPROPERTY(VisibleAnywhere)
	USkyLightComponent* Sky = nullptr;

	UPROPERTY(VisibleAnywhere)
	USkyAtmosphereComponent* Atmosphere = nullptr;

	UStaticMesh* CubeMesh = nullptr;
	UStaticMesh* CylMesh = nullptr;
	UStaticMesh* ConeMesh = nullptr;
	UStaticMesh* PlaneMesh = nullptr;
	UMaterial* BaseMat = nullptr;

	// 在局部坐标放置一个盒子
	UStaticMeshComponent* AddBox(const FVector& Loc, const FVector& Scale, const FLinearColor& Color);
	UStaticMeshComponent* AddCylinder(const FVector& Loc, const FVector& Scale, const FLinearColor& Color);
	UStaticMeshComponent* AddCone(const FVector& Loc, const FVector& Scale, const FLinearColor& Color);

	void BuildRoad();
	void BuildIntersection();
	void BuildScenery();
};
