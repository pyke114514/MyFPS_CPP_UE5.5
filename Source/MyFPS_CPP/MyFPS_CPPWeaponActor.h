#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MyFPS_CPPWeaponActor.generated.h"

class AMyFPS_CPPCharacter;
class USkeletalMeshComponent;
class UAudioComponent;
class UInputMappingContext;
class UInputAction;
class USoundBase;
class UAnimMontage;
class UEnhancedInputComponent;
class UNiagaraSystem;
class UMyFPS_CPPPickUpComponent;
struct FEnhancedInputActionEventBinding;

UENUM(BlueprintType)
enum class EWeaponFireMode : uint8
{
	SemiAuto UMETA(DisplayName = "Semi Auto"),
	FullAuto UMETA(DisplayName = "Full Auto")
};

UENUM(BlueprintType)
enum class EWeaponTargetSlot : uint8
{
	Primary UMETA(DisplayName = "Primary"),
	Secondary UMETA(DisplayName = "Secondary")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWeaponReady);

UCLASS(MinimalAPI)
class AMyFPS_CPPWeaponActor : public AActor
{
	GENERATED_BODY()

public:
	AMyFPS_CPPWeaponActor();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	bool Equip(AMyFPS_CPPCharacter* TargetCharacter);

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void ActivateWeapon();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void DeactivateWeapon();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void DropWeapon();

	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsActiveWeapon() const { return bIsActiveWeapon; }

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void Fire();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void StopFire();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void Reload();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void View();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void StopView();

	void SetWeaponActionsLocked(bool bLocked);
	bool AreWeaponActionsLocked() const;
	EWeaponTargetSlot GetTargetSlot() const { return TargetSlot; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void TryBindInput();

	UPROPERTY(BlueprintAssignable)
	FOnWeaponReady OnWeaponReady;

	bool HasAnyAmmo() const { return CurrentAmmo > 0 || TotalAmmo > 0; }
	void NotifyOwnerOutOfAmmo();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;

	void HandlePickupComponentPostEquip();
	void EnablePickupComponent(bool bEnable);
	void ApplyDroppedPhysicsState();
	void DropWeaponPhysically(const FVector& ViewLoc, const FRotator& ViewRot);

	void AttachToOwningCharacterComponents();
	void DetachFromCharacterComponents(AMyFPS_CPPCharacter* Character);
	void ApplyAnimBlueprints() const;
	void ShowWeaponMeshes(bool bShow);
	void HandleActivateWeaponVisuals(bool bFromReplication);
	void HandleDeactivateWeaponVisuals(bool bFromReplication);

	void Unholster();
	void BindInput();
	void UnbindInput();
	float PlayFPCharacterAndWeaponMontage(UAnimMontage* CharacterMontage, UAnimMontage* WeaponMontage);
	void PlayTPCharacterMontage(UAnimMontage* CharacterMontage);
	void PlaySound(USoundBase* SoundToPlay);

	void HandleFireFinished();
	void HandleReloadFinished();
	void RefillAmmo();
	void HandleViewFinished();

	void CacheMagazineReverseMesh();
	void CacheAimSocket();

	void SetStaticMeshComponentsOwnerVisibility();
	void SetStaticMeshComponentsVisibility(bool visible);

	void StopActiveSound();

	void HandleFireVFX(const FVector& HitPointLocal);

	// 输入事件
	void OnFirePressed();
	void OnFireReleased();

	void OnFireMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	void OnReloadMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	void OnViewMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	// RPC
	UFUNCTION(Server, Reliable)
	void ServerEquip(AMyFPS_CPPCharacter* TargetCharacter);

	UFUNCTION(Server, Reliable)
	void ServerActivateWeapon();

	UFUNCTION(Server, Reliable)
	void ServerDeactivateWeapon();

	UFUNCTION(Server, Reliable)
	void ServerDropWeapon();

	UFUNCTION(Server, Reliable)
	void ServerStartFire();

	UFUNCTION(Server, Reliable)
	void ServerStopFire();

	UFUNCTION(Server, Reliable)
	void ServerReload();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayFireEffects(bool bWasAiming, FVector_NetQuantize ImpactPoint, bool bHadImpact);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayReloadEffects();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastFinishReloadEffects();

	// Replication callbacks
	UFUNCTION()
	void OnRep_OwningCharacter(AMyFPS_CPPCharacter* PreviousOwner);

	UFUNCTION()
	void OnRep_IsEquipped();

	UFUNCTION()
	void OnRep_IsActiveWeapon();

private:
	bool CanEquip(AMyFPS_CPPCharacter* TargetCharacter) const;

	bool CanLocalRequestFire() const;
	bool CanLocalRequestReload() const;

	bool CanServerFire() const;
	bool CanServerReload() const;

	bool HandleServerEquip(AMyFPS_CPPCharacter* TargetCharacter);
	void HandleServerActivateWeapon();
	void HandleServerDeactivateWeapon();
	void HandleServerDropWeapon();

	void HandleServerStartFire();
	void HandleServerStopFire();
	void HandleServerAutoFire();

	void PerformServerFire();
	bool PerformServerLineTrace(FHitResult& OutHit, FVector& OutShotDirection) const;
	void ApplyServerDamage(const FHitResult& Hit, const FVector& ShotDirection);

	void HandleServerReloadRequest();
	void HandleServerReloadComplete();

	float GetReloadAnimationDuration() const;
	void NotifyWeaponReady();

	void ScheduleAutoDestroy();
	void CancelAutoDestroy();
	void HandleAutoDestroyTimerExpired();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	USceneComponent* Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	USkeletalMeshComponent* WeaponMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	USkeletalMeshComponent* WeaponMeshStatic;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Inventory")
	EWeaponTargetSlot TargetSlot = EWeaponTargetSlot::Primary;

	UPROPERTY(EditDefaultsOnly, Category = "VFX")
	UNiagaraSystem* FireVFX;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character ABP")
	TSubclassOf<UAnimInstance> FPCharacterAnimBlueprintClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character ABP")
	TSubclassOf<UAnimInstance> TPCharacterAnimBlueprintClass;

	UPROPERTY()
	UStaticMeshComponent* MagazineReverseMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Pickup")
	UMyFPS_CPPPickUpComponent* PickupComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Aim", meta = (AllowPrivateAccess = "true"))
	USceneComponent* AimSocketComponent = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Gameplay")
	EWeaponFireMode FireMode = EWeaponFireMode::SemiAuto;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Gameplay", meta = (ClampMin = "0.07", ClampMax = "1.5"))
	float FireInterval = 0.17f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Gameplay", meta = (ClampMin = "1", ClampMax = "200"))
	int MaxAmmo = 12;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Gameplay", meta = (ClampMin = "0", ClampMax = "5"))
	int DefaultMagazineNum = 2;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Gameplay", Replicated)
	int TotalAmmo = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Gameplay", Replicated)
	int CurrentAmmo = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Gameplay", meta = (ClampMin = "10.0", ClampMax = "200.0"))
	float BaseDamage = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Gameplay", meta = (ClampMin = "0.0", ClampMax = "10.0"))
	float DamageVariance = 5.f;


	/** 移动带来的扩散（度）：速度 / MoveSpreadSpeedScale → 0~1 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Spread", meta = (ClampMin = "0.0", ClampMax = "10.0"))
	float MoveSpreadMin = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Spread", meta = (ClampMin = "0.0", ClampMax = "45.0"))
	float MoveSpreadMax = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Spread", meta = (ClampMin = "1.0", ClampMax = "1200.0"))
	float MoveSpreadSpeedScale = 700.f;

	/** 射击累积扩散（度） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Spread", meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float ShootSpreadMin = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Spread", meta = (ClampMin = "0.0", ClampMax = "45.0"))
	float ShootSpreadMax = 15.0f;

	/** 每次射击增加多少扩散（度） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Spread", meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float ShootSpreadPerShot = 2.5f;

	/** 扩散回落时间（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Spread", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float ShootSpreadResetTime = 0.4f;

	float SpreadReturnStartValue = 0.f;
	float TimeSinceLastShot = 0.f;

	/** 当前射击扩散，会在 Tick 中逐步回落 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Spread")
	float CurrentShootSpread = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Input")
	UInputAction* FireAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Input")
	UInputAction* ReloadAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Input")
	UInputAction* ViewAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Audio")
	USoundBase* UnholsterSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Audio")
	USoundBase* FireSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Audio")
	USoundBase* ReloadSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Audio")
	USoundBase* ViewSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|C_Animation")
	UAnimMontage* C_UnholsterAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|C_Animation")
	UAnimMontage* C_FireAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|C_Animation")
	UAnimMontage* C_FireAimedAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|C_Animation")
	UAnimMontage* C_ReloadAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|C_Animation")
	UAnimMontage* C_ViewAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|W_Animation")
	UAnimMontage* W_UnholsterAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|W_Animation")
	UAnimMontage* W_FireAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|W_Animation")
	UAnimMontage* W_ReloadAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|TP_Animation")
	UAnimMontage* TP_FireAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|TP_Animation")
	UAnimMontage* TP_ReloadAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Attachment")
	FName AttachSocketName = TEXT("SOCKET_Weapon");

private:
	UPROPERTY(ReplicatedUsing = OnRep_IsEquipped)
	bool bIsEquipped = false;

	UPROPERTY(ReplicatedUsing = OnRep_IsActiveWeapon)
	bool bIsActiveWeapon = false;

	UPROPERTY(ReplicatedUsing = OnRep_OwningCharacter)
	AMyFPS_CPPCharacter* OwningCharacter = nullptr;

	UPROPERTY()
	UEnhancedInputComponent* CachedInputComponent = nullptr;

	FEnhancedInputActionEventBinding* FirePressedBinding = nullptr;
	FEnhancedInputActionEventBinding* FireReleasedBinding = nullptr;
	FEnhancedInputActionEventBinding* ReloadBinding = nullptr;
	FEnhancedInputActionEventBinding* ViewBinding = nullptr;

	bool bIsPreviewInstance = false;
	TWeakObjectPtr<UAudioComponent> ActiveAudioComponent = nullptr;
	bool bSkipNextSound = false;

	bool bFireInputHeld = false;

	FTimerHandle ServerAutoFireTimerHandle;
	FTimerHandle ServerReloadTimerHandle;
	FTimerHandle PickupReactivateHandle;
	FTimerHandle WeaponReadyDelayTimerHandle;

	bool bPickupComponentDisabled = false;

	bool bWeaponActionsLocked = false;

	float LastServerFireTime = -100.f;

	FOnMontageEnded ReloadMontageEndedDelegate;
	FOnMontageEnded ViewMontageEndedDelegate;
	FOnMontageEnded FireMontageEndedDelegate;

	// --- Auto-destroy on ground ---
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Gameplay")
	float UnpickedLifeSpan = 5.f;

	FTimerHandle AutoDestroyTimerHandle;

	float GetMoveSpreadAngle() const;
	float GetShootSpreadAngle() const;
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	float GetTotalSpreadAngle() const;
	void  UpdateShootSpread(float DeltaTime);
	void  AddShootSpreadOnFire();
};