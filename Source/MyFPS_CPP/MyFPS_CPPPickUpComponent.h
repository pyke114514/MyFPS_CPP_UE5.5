#pragma once

#include "CoreMinimal.h"
#include "Components/SphereComponent.h"
#include "MyFPS_CPPPickUpComponent.generated.h"

class AMyFPS_CPPCharacter;
class AMyFPS_CPPWeaponActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPickUp, AMyFPS_CPPCharacter*, PickUpCharacter);

UCLASS(MinimalAPI, Blueprintable, BlueprintType)
class UMyFPS_CPPPickUpComponent : public USphereComponent
{
	GENERATED_BODY()

public:
	UMyFPS_CPPPickUpComponent();

	/** 玩家成功拾取武器时回调 */
	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FOnPickUp OnPickUp;

	void TryPickup(AMyFPS_CPPCharacter* Character);

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
	virtual void OnRegister() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UFUNCTION()
	void OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void OnSphereEndOverlap(UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

private:
	/** 缓存父级武器，避免每次 Cast */
	UPROPERTY(Transient)
	AMyFPS_CPPWeaponActor* CachedWeapon = nullptr;

	void CacheWeaponFromOwner();

	void ProcessCurrentOverlaps();   // ★ 新增，供 Tick 调用
};