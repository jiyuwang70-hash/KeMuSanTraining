// 道路生成器：运行时用基础几何体搭建大型城市考试路网
// 主线：东段直行 -> 转角路口 -> 北段 -> 转角路口 -> 西段 -> 掉头返回
// 同时生成横向街道、人行道、街景建筑群、路灯、树木与全套路面标线
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RoadLayout.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "RoadBuilder.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInstanceDynamic;
class UMaterial;
class UDirectionalLightComponent;
class USkyLightComponent;
class USkyAtmosphereComponent;
class UExponentialHeightFogComponent;

struct FHISMGroupKey
{
	UStaticMesh* Mesh = nullptr;
	UMaterialInterface* Material = nullptr;

	bool operator==(const FHISMGroupKey& Other) const
	{
		return Mesh == Other.Mesh && Material == Other.Material;
	}
};

FORCEINLINE uint32 GetTypeHash(const FHISMGroupKey& Key)
{
	return HashCombine(GetTypeHash(Key.Mesh), GetTypeHash(Key.Material));
}

UCLASS()
class KEMUSANTRAINING_API ARoadBuilder : public AActor
{
	GENERATED_BODY()

public:
	ARoadBuilder();

	virtual void BeginPlay() override;

	// 路线轨迹（供交通管理器 / 考试控制器共用）
	FRouteTrack* GetTrack() { return &Track; }

	// 场景搭建完成后调用：重新捕捉天光，避免偏色
	void FinishBuild();

protected:
	UPROPERTY(VisibleAnywhere)
	USceneComponent* Root = nullptr;

	UPROPERTY(VisibleAnywhere)
	UDirectionalLightComponent* Sun = nullptr;

	UPROPERTY(VisibleAnywhere)
	USkyLightComponent* Sky = nullptr;

	UPROPERTY(VisibleAnywhere)
	USkyAtmosphereComponent* Atmosphere = nullptr;

	UPROPERTY(VisibleAnywhere)
	UExponentialHeightFogComponent* HeightFog = nullptr;

	UPROPERTY(VisibleAnywhere)
	class UPostProcessComponent* PostProcess = nullptr;

	UStaticMesh* CubeMesh = nullptr;
	UStaticMesh* CylMesh = nullptr;
	UStaticMesh* ConeMesh = nullptr;
	UStaticMesh* PlaneMesh = nullptr;
	UMaterial* BaseMat = nullptr;

	FRouteTrack Track;
	float PieceLayerZ = 0.f;  // Current layer Z offset (set per build phase)
	int32 PiecesPlaced = 0;

	TMap<uint32, UMaterialInstanceDynamic*> MatCache;

	UPROPERTY()
	TArray<UHierarchicalInstancedStaticMeshComponent*> HISMComponents;

	TMap<FHISMGroupKey, UHierarchicalInstancedStaticMeshComponent*> HISMMap;

	// ---- 放置工具（自动缓存材质；Extents 为米制全尺寸）----
	UStaticMeshComponent* AddPiece(UStaticMesh* Mesh, const FVector& Loc, const FVector& Extents,
		const FLinearColor& Color, const FRotator& Rot = FRotator::ZeroRotator, bool bRaiseZ = true);
	UStaticMeshComponent* AddBox(const FVector& Loc, const FVector& Extents, const FLinearColor& Color,
		const FRotator& Rot = FRotator::ZeroRotator);
	UStaticMeshComponent* AddCylinder(const FVector& Loc, const FVector& Extents, const FLinearColor& Color,
		const FRotator& Rot = FRotator::ZeroRotator);
	UStaticMeshComponent* AddCone(const FVector& Loc, const FVector& Extents, const FLinearColor& Color,
		const FRotator& Rot = FRotator::ZeroRotator);

	UMaterialInstanceDynamic* GetMat(const FLinearColor& Color);

	// ---- 场景分区 ----
	void BuildGround();
	void BuildRoadSurface();
	void BuildMarkings();
	void BuildZoneFacilities();
	void BuildStreetFurniture();
	void BuildCityBlocks();

	// 矩形沥青（含两侧带断口的人行道）
	void AsphaltRect(const FVector2D& Min, const FVector2D& Max);
	void WalkwayStrip(const FVector2D& Min, const FVector2D& Max);

	// 标线工具
	void PaintZebra(const FVector& Center, float FlowYawDeg, float HalfSpan);
	void PaintArrow(const FVector& Pos, float YawDeg);
	void PoleWithPlate(const FVector& BasePos, float FaceYawDeg, const FLinearColor& PlateColor,
		float PlateW = 1.2f, float PlateH = 0.9f);
	void MakeTree(const FVector& BasePos, float ScaleMul = 1.f);
	void MakeStreetLamp(const FVector& BasePos, float ArmYawDeg);
};
