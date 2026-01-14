#pragma once

#include "CoreMinimal.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "TimerManager.h"
#include "GameFramework/GameModeBase.h"
#include "MyFPS_CPPCharacter.generated.h"

class UInputComponent;
class USkeletalMeshComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class AMyFPS_CPPWeaponActor;
enum class EWeaponTargetSlot : uint8;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

UENUM(BlueprintType)
enum class EWeaponSlot : uint8
{
	Primary UMETA(DisplayName = "Primary"),
	Secondary UMETA(DisplayName = "Secondary")
};

UCLASS(MinimalAPI,config = Game)
class AMyFPS_CPPCharacter : public ACharacter
{
	GENERATED_BODY()
	
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Mesh, meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* Mesh1P;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FirstPersonCameraComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputMappingContext* DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	UInputMappingContext* WeaponMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* JumpAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* SprintAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* LookAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* SwitchWeaponAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* DropWeaponAction;

	/** 死亡后多少秒重生 */
	UPROPERTY(EditDefaultsOnly, Category = "Respawn", meta = (AllowPrivateAccess = "true"))
	float RespawnDelay = 5.f;

	/** 记录死亡前控制该角色的 Controller，用于重生时复用 */
	UPROPERTY()
	TWeakObjectPtr<AController> CachedController;

	/** 服务器端用于延迟重生的计时器 */
	FTimerHandle RespawnTimerHandle;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "角色|状态")
	float WalkSpeed = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "角色|状态")
	float SprintSpeed = 700.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "角色|状态")
	float AimSpeed = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "角色|状态", ReplicatedUsing = OnRep_Aiming)
	bool Aiming = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "角色|状态")
	bool CanAim = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "角色|状态", ReplicatedUsing = OnRep_Sprinting)
	bool Sprinting = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "角色|状态")
	bool CanSprint = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "角色|状态")
	bool Reloading = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "角色|状态")
	bool Viewing = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "角色|状态")
	float MaxHP = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "角色|状态", Replicated)
	float CurrentHP;

	UPROPERTY(BlueprintReadOnly, Category = "角色|状态", ReplicatedUsing = OnRep_IsDead)
	bool bIsDead = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "角色|武器")
	AMyFPS_CPPWeaponActor* CurrentWeaponActor = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "角色|武器", ReplicatedUsing = OnRep_PrimaryWeaponActor)
	AMyFPS_CPPWeaponActor* PrimaryWeaponActor = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "角色|武器", ReplicatedUsing = OnRep_SecondaryWeaponActor)
	AMyFPS_CPPWeaponActor* SecondaryWeaponActor = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "角色|武器", ReplicatedUsing = OnRep_CurrentWeaponSlot)
	EWeaponSlot CurrentWeaponSlot = EWeaponSlot::Secondary;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "角色|武器")
	TSubclassOf<AMyFPS_CPPWeaponActor> DefaultPrimaryWeaponClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "角色|武器")
	TSubclassOf<AMyFPS_CPPWeaponActor> DefaultWeaponClass;

	UFUNCTION(BlueprintCallable, Category = "角色|武器")
	bool EquipWeaponActor(AMyFPS_CPPWeaponActor* NewWeapon);

	UFUNCTION(BlueprintCallable, Category = "角色|武器")
	bool EquipWeaponActorInSlot(AMyFPS_CPPWeaponActor* NewWeapon, EWeaponSlot Slot);

	UFUNCTION(BlueprintCallable, Category = "角色|武器")
	void SwitchWeapon();

	UFUNCTION(BlueprintCallable, Category = "角色|武器")
	void DropCurrentWeapon();

	AMyFPS_CPPCharacter();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void NotifyControllerChanged() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void HandleActiveWeaponOutOfAmmo(AMyFPS_CPPWeaponActor* DepletedWeapon);

	virtual float TakeDamage(
		float DamageAmount,
		const FDamageEvent& DamageEvent,
		AController* EventInstigator,
		AActor* DamageCauser
	) override;

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);

	UFUNCTION(BlueprintCallable, Category = "角色|运动")
	void Aim();

	UFUNCTION(BlueprintCallable, Category = "角色|运动")
	void StopAim();

	UFUNCTION(BlueprintCallable, Category = "角色|运动")
	void Sprint();

	UFUNCTION(BlueprintCallable, Category = "角色|运动")
	void StopSprint();

	USkeletalMeshComponent* GetMesh1P() const { return Mesh1P; }
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }
	bool CanEquipWeaponInSlot(EWeaponTargetSlot SlotType, const AMyFPS_CPPWeaponActor* Weapon = nullptr) const;

	UFUNCTION(Client, Reliable)
	void ClientOnHitEvent();

	UFUNCTION(Client, Reliable)
	void ClientOnHittedEvent();

	UFUNCTION(Client, Reliable)
	void ClientOnKillEvent();

	UFUNCTION(BlueprintImplementableEvent, Category = "MyEvents")
	void OnHitEvent();

	UFUNCTION(BlueprintImplementableEvent, Category = "MyEvents")
	void OnHittedEvent();

	UFUNCTION(BlueprintImplementableEvent, Category = "MyEvents")
	void OnKillEvent();

protected:
	bool bWeaponSwitchPending = false;
	EWeaponSlot PendingWeaponSlot = EWeaponSlot::Primary;

	bool isSprintHeld = false;
	bool isAimHeld = false;

	void SetCurrentWeaponSlot(EWeaponSlot NewSlot);
	void ApplyCurrentWeaponSlot(EWeaponSlot NewSlot);
	void HandleReplicatedWeaponSlot(EWeaponSlot Slot, AMyFPS_CPPWeaponActor* PreviousWeapon, AMyFPS_CPPWeaponActor* NewWeapon);

	AMyFPS_CPPWeaponActor* GetWeaponInSlot(EWeaponSlot Slot) const;
	AMyFPS_CPPWeaponActor*& GetWeaponSlotRef(EWeaponSlot Slot);
	void RemoveWeaponFromSlot(EWeaponSlot Slot);
	int32 GetWeaponCount() const;
	bool CanDropCurrentWeapon() const;
	EWeaponSlot GetAlternateSlot(EWeaponSlot Slot) const;
	EWeaponSlot ConvertTargetSlotToWeaponSlot(EWeaponTargetSlot SlotType) const;
	void SwitchWeaponInternal();
	void DropCurrentWeaponInternal();

	void ApplyMovementSpeed();
	void SetSprintingStateInternal(bool bNewSprinting, bool bFromReplication);
	void SetAimingStateInternal(bool bNewAiming, bool bFromReplication);

	void DropWeaponFromSlot(EWeaponSlot Slot);
	void DropAllWeaponsOnDeath();

	UFUNCTION()
	void OnRep_Sprinting();

	UFUNCTION()
	void OnRep_Aiming();

	UFUNCTION()
	void OnRep_PrimaryWeaponActor(AMyFPS_CPPWeaponActor* PreviousWeapon);

	UFUNCTION()
	void OnRep_SecondaryWeaponActor(AMyFPS_CPPWeaponActor* PreviousWeapon);

	UFUNCTION()
	virtual void OnRep_CurrentWeaponSlot(EWeaponSlot PreviousSlot);

	UFUNCTION()
	void OnRep_IsDead();

	UFUNCTION(Server, Reliable)
	void ServerSetSprinting(bool bNewSprinting);

	UFUNCTION(Server, Reliable)
	void ServerSetAiming(bool bNewAiming);

	UFUNCTION(Server, Reliable)
	void ServerSwitchWeapon();

	UFUNCTION(Server, Reliable)
	void ServerDropCurrentWeapon();

	UFUNCTION()
	void HandleDeath(AController* Killer, bool bIsHeadShot);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastEnterRagdoll();

	UFUNCTION()
	void RespawnAfterDeath();
};