#include "MyFPS_CPPCharacter.h"
#include "MyFPS_CPPWeaponActor.h"
#include "MyFPS_CPPGameMode.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/DamageEvents.h"
#include "InputActionValue.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

AMyFPS_CPPCharacter::AMyFPS_CPPCharacter()
{
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.f);

	bReplicates = true;

	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCameraComponent->SetupAttachment(GetCapsuleComponent());
	FirstPersonCameraComponent->SetRelativeLocation(FVector(-10.f, 0.f, 60.f));
	FirstPersonCameraComponent->bUsePawnControlRotation = true;

	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh1P"));
	Mesh1P->SetOnlyOwnerSee(true);
	Mesh1P->SetupAttachment(FirstPersonCameraComponent);
	Mesh1P->bCastDynamicShadow = false;
	Mesh1P->CastShadow = false;
	Mesh1P->SetRelativeLocation(FVector(-30.f, 0.f, -150.f));
}

void AMyFPS_CPPCharacter::BeginPlay()
{
	Super::BeginPlay();

	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore);

	CurrentHP = MaxHP;

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->MaxWalkSpeed = WalkSpeed;
	}

	ApplyMovementSpeed();

	if (HasAuthority())
	{
		auto SpawnAndEquip = [this](TSubclassOf<AMyFPS_CPPWeaponActor> WeaponClass, EWeaponSlot Slot)
			{
				if (!WeaponClass)
				{
					return;
				}

				if (UWorld* World = GetWorld())
				{
					FActorSpawnParameters SpawnParams;
					SpawnParams.Owner = this;
					SpawnParams.Instigator = this;
					SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

					if (AMyFPS_CPPWeaponActor* SpawnedWeapon = World->SpawnActor<AMyFPS_CPPWeaponActor>(WeaponClass, SpawnParams))
					{
						if (!EquipWeaponActorInSlot(SpawnedWeapon, Slot))
						{
							SpawnedWeapon->Destroy();
						}
					}
				}
			};

		if (DefaultPrimaryWeaponClass)
		{
			SpawnAndEquip(DefaultPrimaryWeaponClass, EWeaponSlot::Primary);
		}

		if (DefaultWeaponClass)
		{
			SpawnAndEquip(DefaultWeaponClass, EWeaponSlot::Secondary);
		}

		if (PrimaryWeaponActor)
		{
			SetCurrentWeaponSlot(EWeaponSlot::Primary);
		}
		else if (SecondaryWeaponActor)
		{
			SetCurrentWeaponSlot(EWeaponSlot::Secondary);
		}
	}
}

void AMyFPS_CPPCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	if (HasAuthority())
	{
		GetWorldTimerManager().ClearTimer(RespawnTimerHandle);
	}

	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (PC->IsLocalController())
		{
			if (ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
			{
				if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
					ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
				{
					if (WeaponMappingContext)
					{
						Subsystem->RemoveMappingContext(WeaponMappingContext);
					}
				}
			}
		}
	}

	Super::EndPlay(EndPlayReason);
}

void AMyFPS_CPPCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMyFPS_CPPCharacter, Sprinting);
	DOREPLIFETIME(AMyFPS_CPPCharacter, Aiming);
	DOREPLIFETIME(AMyFPS_CPPCharacter, PrimaryWeaponActor);
	DOREPLIFETIME(AMyFPS_CPPCharacter, SecondaryWeaponActor);
	DOREPLIFETIME(AMyFPS_CPPCharacter, CurrentWeaponSlot);
	DOREPLIFETIME(AMyFPS_CPPCharacter, CurrentHP);
	DOREPLIFETIME(AMyFPS_CPPCharacter, bIsDead);
}

bool AMyFPS_CPPCharacter::EquipWeaponActor(AMyFPS_CPPWeaponActor* NewWeapon)
{
	if (!IsValid(NewWeapon))
	{
		return false;
	}

	const EWeaponSlot DesiredSlot = ConvertTargetSlotToWeaponSlot(NewWeapon->GetTargetSlot());
	return EquipWeaponActorInSlot(NewWeapon, DesiredSlot);
}

bool AMyFPS_CPPCharacter::EquipWeaponActorInSlot(AMyFPS_CPPWeaponActor* NewWeapon, EWeaponSlot Slot)
{
	if (!IsValid(NewWeapon))
	{
		return false;
	}

	if (Slot != ConvertTargetSlotToWeaponSlot(NewWeapon->GetTargetSlot()))
	{
		return false;
	}

	AMyFPS_CPPWeaponActor*& SlotRef = GetWeaponSlotRef(Slot);
	const bool bSlotWasEmpty = (SlotRef == nullptr);

	if (SlotRef == NewWeapon)
	{
		return false;
	}

	if (SlotRef)
	{
		const bool bWasCurrent = (SlotRef == CurrentWeaponActor);
		SlotRef->DropWeapon();
		SlotRef = nullptr;

		if (bWasCurrent)
		{
			CurrentWeaponActor = nullptr;
		}
	}

	if (!NewWeapon->Equip(this))
	{
		return false;
	}

	SlotRef = NewWeapon;

	auto ShouldActivateNewSlot = [&](EWeaponSlot TargetSlot, bool bWasSlotEmpty) -> bool
		{
			if (!CurrentWeaponActor)
			{
				return true;
			}

			if (TargetSlot == CurrentWeaponSlot)
			{
				return true;
			}

			if (TargetSlot == EWeaponSlot::Primary && CurrentWeaponSlot == EWeaponSlot::Secondary)
			{
				return true;
			}

			return false;
		};

	const bool bShouldActivate = ShouldActivateNewSlot(Slot, bSlotWasEmpty);

	if (bShouldActivate)
	{
		SetCurrentWeaponSlot(Slot);
	}
	else
	{
		NewWeapon->DeactivateWeapon();
	}

	return true;
}

void AMyFPS_CPPCharacter::SwitchWeapon()
{
	if (!HasAuthority())
	{
		ServerSwitchWeapon();
		return;
	}

	SwitchWeaponInternal();
}

void AMyFPS_CPPCharacter::ServerSwitchWeapon_Implementation()
{
	SwitchWeaponInternal();
}

void AMyFPS_CPPCharacter::SwitchWeaponInternal()
{
	if (bWeaponSwitchPending || GetWeaponCount() < 2 || !CurrentWeaponActor)
	{
		return;
	}

	const EWeaponSlot TargetSlot = GetAlternateSlot(CurrentWeaponSlot);
	AMyFPS_CPPWeaponActor* TargetWeapon = GetWeaponInSlot(TargetSlot);
	if (!TargetWeapon || TargetWeapon == CurrentWeaponActor)
	{
		return;
	}

	CurrentWeaponActor->StopView();
	StopAim();
	StopSprint();

	PendingWeaponSlot = TargetSlot;
	bWeaponSwitchPending = true;

	CurrentWeaponActor->SetWeaponActionsLocked(true);

	if (!bWeaponSwitchPending)
	{
		return;
	}

	bWeaponSwitchPending = false;

	SetCurrentWeaponSlot(PendingWeaponSlot);

	if (CurrentWeaponActor)
	{
		CurrentWeaponActor->SetWeaponActionsLocked(false);
	}
}

void AMyFPS_CPPCharacter::DropCurrentWeapon()
{
	if (!HasAuthority())
	{
		ServerDropCurrentWeapon();
		return;
	}

	DropCurrentWeaponInternal();
}

void AMyFPS_CPPCharacter::ServerDropCurrentWeapon_Implementation()
{
	DropCurrentWeaponInternal();
}

void AMyFPS_CPPCharacter::DropCurrentWeaponInternal()
{
	if (!CanDropCurrentWeapon())
	{
		return;
	}

	AMyFPS_CPPWeaponActor* WeaponToDrop = CurrentWeaponActor;
	const EWeaponSlot DroppedSlot = CurrentWeaponSlot;

	if (!WeaponToDrop)
	{
		return;
	}

	WeaponToDrop->StopView();
	StopAim();
	StopSprint();

	RemoveWeaponFromSlot(DroppedSlot);
	CurrentWeaponActor = nullptr;

	WeaponToDrop->DropWeapon();

	const EWeaponSlot AlternateSlot = GetAlternateSlot(DroppedSlot);
	if (AMyFPS_CPPWeaponActor* RemainingWeapon = GetWeaponInSlot(AlternateSlot))
	{
		SetCurrentWeaponSlot(AlternateSlot);
	}

	UpdateOverlaps();
}

void AMyFPS_CPPCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			if (DefaultMappingContext)
			{
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}
}

void AMyFPS_CPPCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AMyFPS_CPPCharacter::Move);
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AMyFPS_CPPCharacter::Look);

		if (SprintAction)
		{
			EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started, this, &AMyFPS_CPPCharacter::Sprint);
			EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &AMyFPS_CPPCharacter::StopSprint);
		}

		if (SwitchWeaponAction)
		{
			EnhancedInputComponent->BindAction(SwitchWeaponAction, ETriggerEvent::Started, this, &AMyFPS_CPPCharacter::SwitchWeapon);
		}

		if (DropWeaponAction)
		{
			EnhancedInputComponent->BindAction(DropWeaponAction, ETriggerEvent::Started, this, &AMyFPS_CPPCharacter::DropCurrentWeapon);
		}
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' requires Enhanced Input component."), *GetNameSafe(this));
	}

	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (PC->IsLocalController())
		{
			if (ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
			{
				if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
					ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
				{
					if (WeaponMappingContext)
					{
						Subsystem->AddMappingContext(WeaponMappingContext, /*Priority=*/1);
					}
				}
			}
		}
	}

	if (IsLocallyControlled())
	{
		auto TryBind = [](AMyFPS_CPPWeaponActor* Weapon)
			{
				if (Weapon)
				{
					Weapon->TryBindInput();
				}
			};

		TryBind(PrimaryWeaponActor);
		TryBind(SecondaryWeaponActor);
	}
}

void AMyFPS_CPPCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		AddMovementInput(GetActorForwardVector(), MovementVector.Y);
		AddMovementInput(GetActorRightVector(), MovementVector.X);
	}
}

void AMyFPS_CPPCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void AMyFPS_CPPCharacter::Aim()
{
	SetAimingStateInternal(true, false);

	if (!HasAuthority())
	{
		ServerSetAiming(true);
	}
}

void AMyFPS_CPPCharacter::StopAim()
{
	SetAimingStateInternal(false, false);

	if (!HasAuthority())
	{
		ServerSetAiming(false);
	}
}

void AMyFPS_CPPCharacter::Sprint()
{
	SetSprintingStateInternal(true, false);

	if (!HasAuthority())
	{
		ServerSetSprinting(true);
	}
}

void AMyFPS_CPPCharacter::StopSprint()
{
	SetSprintingStateInternal(false, false);

	if (!HasAuthority())
	{
		ServerSetSprinting(false);
	}
}

void AMyFPS_CPPCharacter::ServerSetSprinting_Implementation(bool bNewSprinting)
{
	if (bNewSprinting && !CanSprint)
	{
		return;
	}

	SetSprintingStateInternal(bNewSprinting, false);
}

void AMyFPS_CPPCharacter::ServerSetAiming_Implementation(bool bNewAiming)
{
	if (bNewAiming && !CanAim)
	{
		return;
	}

	SetAimingStateInternal(bNewAiming, false);
}

void AMyFPS_CPPCharacter::OnRep_Sprinting()
{
	SetSprintingStateInternal(Sprinting, true);
}

void AMyFPS_CPPCharacter::OnRep_Aiming()
{
	SetAimingStateInternal(Aiming, true);
}

void AMyFPS_CPPCharacter::SetSprintingStateInternal(bool bNewSprinting, bool bFromReplication)
{
	isSprintHeld = bNewSprinting;

	if (!bFromReplication && Sprinting == bNewSprinting)
	{
		return;
	}

	if (!bFromReplication)
	{
		Sprinting = isSprintHeld;
	}

	if (bNewSprinting)
	{
		Aiming = false;
	}

	else
	{
		CanAim = true;
		Aiming = isAimHeld;
	}

	ApplyMovementSpeed();
}

void AMyFPS_CPPCharacter::SetAimingStateInternal(bool bNewAiming, bool bFromReplication)
{
	isAimHeld = bNewAiming;

	if (!bFromReplication && Aiming == bNewAiming)
	{
		return;
	}

	if (!bFromReplication)
	{
		Aiming = isAimHeld;
	}

	if (bNewAiming)
	{
		Sprinting = false;
	}
	else
	{
		CanSprint = true;
		Sprinting = isSprintHeld;
	}

	ApplyMovementSpeed();
}

void AMyFPS_CPPCharacter::ApplyMovementSpeed()
{
	float TargetSpeed = WalkSpeed;

	if (Aiming)
	{
		TargetSpeed = AimSpeed;
	}
	else if (Sprinting)
	{
		TargetSpeed = SprintSpeed;
	}

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		if (!FMath::IsNearlyEqual(MoveComp->MaxWalkSpeed, TargetSpeed))
		{
			MoveComp->MaxWalkSpeed = TargetSpeed;
		}
	}
}

void AMyFPS_CPPCharacter::SetCurrentWeaponSlot(EWeaponSlot NewSlot)
{
	if (!HasAuthority())
	{
		return;
	}

	ApplyCurrentWeaponSlot(NewSlot);
}

void AMyFPS_CPPCharacter::ApplyCurrentWeaponSlot(EWeaponSlot NewSlot)
{
	if (CurrentWeaponSlot == NewSlot && CurrentWeaponActor == GetWeaponInSlot(NewSlot))
	{
		if (CurrentWeaponActor && !CurrentWeaponActor->IsActiveWeapon())
		{
			CurrentWeaponActor->ActivateWeapon();
		}
		return;
	}

	if (CurrentWeaponActor)
	{
		CurrentWeaponActor->DeactivateWeapon();
	}

	CurrentWeaponSlot = NewSlot;
	CurrentWeaponActor = GetWeaponInSlot(NewSlot);

	if (CurrentWeaponActor)
	{
		CurrentWeaponActor->ActivateWeapon();
	}
}

void AMyFPS_CPPCharacter::HandleReplicatedWeaponSlot(EWeaponSlot Slot, AMyFPS_CPPWeaponActor* PreviousWeapon, AMyFPS_CPPWeaponActor* NewWeapon)
{
	if (PreviousWeapon && PreviousWeapon != CurrentWeaponActor)
	{
		PreviousWeapon->DeactivateWeapon();
	}

	if (Slot == CurrentWeaponSlot)
	{
		CurrentWeaponActor = NewWeapon;
		if (CurrentWeaponActor)
		{
			CurrentWeaponActor->ActivateWeapon();
		}
	}
	else if (NewWeapon)
	{
		NewWeapon->DeactivateWeapon();
	}
}

AMyFPS_CPPWeaponActor* AMyFPS_CPPCharacter::GetWeaponInSlot(EWeaponSlot Slot) const
{
	return (Slot == EWeaponSlot::Primary) ? PrimaryWeaponActor : SecondaryWeaponActor;
}

AMyFPS_CPPWeaponActor*& AMyFPS_CPPCharacter::GetWeaponSlotRef(EWeaponSlot Slot)
{
	return (Slot == EWeaponSlot::Primary) ? PrimaryWeaponActor : SecondaryWeaponActor;
}

void AMyFPS_CPPCharacter::RemoveWeaponFromSlot(EWeaponSlot Slot)
{
	AMyFPS_CPPWeaponActor*& SlotRef = GetWeaponSlotRef(Slot);
	if (SlotRef)
	{
		SlotRef = nullptr;
	}
}

int32 AMyFPS_CPPCharacter::GetWeaponCount() const
{
	int32 Count = 0;
	Count += PrimaryWeaponActor ? 1 : 0;
	Count += SecondaryWeaponActor ? 1 : 0;
	return Count;
}

bool AMyFPS_CPPCharacter::CanDropCurrentWeapon() const
{
	return CurrentWeaponActor != nullptr && GetWeaponCount() > 1;
}

EWeaponSlot AMyFPS_CPPCharacter::GetAlternateSlot(EWeaponSlot Slot) const
{
	return (Slot == EWeaponSlot::Primary) ? EWeaponSlot::Secondary : EWeaponSlot::Primary;
}

bool AMyFPS_CPPCharacter::CanEquipWeaponInSlot(EWeaponTargetSlot SlotType, const AMyFPS_CPPWeaponActor* Weapon) const
{
	const AMyFPS_CPPWeaponActor* SlotWeapon =
		(SlotType == EWeaponTargetSlot::Primary) ? PrimaryWeaponActor : SecondaryWeaponActor;

	return SlotWeapon == nullptr || SlotWeapon == Weapon;
}

EWeaponSlot AMyFPS_CPPCharacter::ConvertTargetSlotToWeaponSlot(EWeaponTargetSlot SlotType) const
{
	return (SlotType == EWeaponTargetSlot::Primary)
		? EWeaponSlot::Primary
		: EWeaponSlot::Secondary;
}

void AMyFPS_CPPCharacter::OnRep_PrimaryWeaponActor(AMyFPS_CPPWeaponActor* PreviousWeapon)
{
	HandleReplicatedWeaponSlot(EWeaponSlot::Primary, PreviousWeapon, PrimaryWeaponActor);
}

void AMyFPS_CPPCharacter::OnRep_SecondaryWeaponActor(AMyFPS_CPPWeaponActor* PreviousWeapon)
{
	HandleReplicatedWeaponSlot(EWeaponSlot::Secondary, PreviousWeapon, SecondaryWeaponActor);
}

void AMyFPS_CPPCharacter::OnRep_CurrentWeaponSlot(EWeaponSlot PreviousSlot)
{
	ApplyCurrentWeaponSlot(CurrentWeaponSlot);
	if (PreviousSlot != CurrentWeaponSlot)
	{
		if (AMyFPS_CPPWeaponActor* PrevWeapon = GetWeaponInSlot(PreviousSlot))
		{
			PrevWeapon->DeactivateWeapon();
		}
	}
}

float AMyFPS_CPPCharacter::TakeDamage(
	float DamageAmount,
	const FDamageEvent& DamageEvent,
	AController* EventInstigator,
	AActor* DamageCauser
)
{
	if (!HasAuthority() || DamageAmount <= 0.f || bIsDead)
	{
		return 0.f;
	}


	ClientOnHittedEvent();

	float FinalDamage = DamageAmount;
	bool bIsHeadShot = false;

	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		const FPointDamageEvent* PointDamage = static_cast<const FPointDamageEvent*>(&DamageEvent);
		const FName HitBone = PointDamage->HitInfo.BoneName;
		bIsHeadShot = HitBone == TEXT("head");
		
		if (bIsHeadShot)
		{
			FinalDamage *= 2.0f;
		}
	}

	const float OldHP = CurrentHP;
	CurrentHP = FMath::Clamp(CurrentHP - FinalDamage, 0.f, MaxHP);

	if (!FMath::IsNearlyEqual(OldHP, CurrentHP) && CurrentHP <= 0.f)
	{
		HandleDeath(EventInstigator, bIsHeadShot);
	}

	return FinalDamage;
}

void AMyFPS_CPPCharacter::DropWeaponFromSlot(EWeaponSlot Slot)
{
	if (AMyFPS_CPPWeaponActor* Weapon = GetWeaponInSlot(Slot))
	{
		if (Weapon == CurrentWeaponActor)
		{
			Weapon->StopView();
			CurrentWeaponActor = nullptr;
		}

		Weapon->DropWeapon();
		RemoveWeaponFromSlot(Slot);
	}
}

void AMyFPS_CPPCharacter::DropAllWeaponsOnDeath()
{
	DropWeaponFromSlot(EWeaponSlot::Primary);
	DropWeaponFromSlot(EWeaponSlot::Secondary);

	bWeaponSwitchPending = false;
	PendingWeaponSlot = CurrentWeaponSlot = EWeaponSlot::Primary;

	UpdateOverlaps();
}

void AMyFPS_CPPCharacter::HandleDeath(AController* Killer, bool bIsHeadShot)
{
	if (bIsDead)
	{
		return;
	}

	if (HasAuthority())
	{
		CachedController = Controller;
		GetWorldTimerManager().ClearTimer(RespawnTimerHandle);

		if (AMyFPS_CPPGameMode* GM = GetWorld()->GetAuthGameMode<AMyFPS_CPPGameMode>())
		{
			GM->NotifyPlayerKilled(this, Killer, bIsHeadShot);
		}
	}

	bIsDead = true;
	OnRep_IsDead();

	MulticastEnterRagdoll();

	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimer(
			RespawnTimerHandle,
			this,
			&AMyFPS_CPPCharacter::RespawnAfterDeath,
			RespawnDelay,
			false);
	}
}

void AMyFPS_CPPCharacter::MulticastEnterRagdoll_Implementation()
{
	USkeletalMeshComponent* FullBodyMesh = GetMesh();

	if (!FullBodyMesh)
	{
		UE_LOG(LogTemplateCharacter, Warning, TEXT("No full-body mesh to ragdoll."));
		return;
	}

	FullBodyMesh->SetCollisionProfileName(TEXT("Ragdoll"));
	FullBodyMesh->SetAllBodiesSimulatePhysics(true);
	FullBodyMesh->SetAllBodiesPhysicsBlendWeight(1.0f);
	FullBodyMesh->SetSimulatePhysics(true);
	FullBodyMesh->WakeAllRigidBodies();
	FullBodyMesh->bBlendPhysics = true;
	FullBodyMesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);

	FullBodyMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	FullBodyMesh->SetCollisionObjectType(ECC_PhysicsBody);
	FullBodyMesh->SetCollisionResponseToAllChannels(ECR_Block);     // 先对环境/世界保持阻挡
	FullBodyMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore); // 对玩家忽略

	// 如果使用第一人称手臂 Mesh1P，需要根据需求隐藏或保持
	if (Mesh1P)
	{
		Mesh1P->SetVisibility(false, true);
	}
}

void AMyFPS_CPPCharacter::OnRep_IsDead()
{
	DropAllWeaponsOnDeath();  // ★ 新增：死亡时丢掉所有武器

	// 停止移动 & 输入
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->DisableMovement();
	}

	StopAim();
	StopSprint();

	// 解除与控制器的绑定（避免再处理输入）
	DetachFromControllerPendingDestroy();

	// 禁用碰撞胶囊，避免 ragdoll 被“撑起来”
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void AMyFPS_CPPCharacter::RespawnAfterDeath()
{
	if (!HasAuthority())
	{
		return;
	}

	if (!CachedController.IsValid())
	{
		Destroy();                // 没有 Controller 也要清理尸体
		return;
	}

	if (AGameModeBase* GM = GetWorld()->GetAuthGameMode())
	{
		// 确保旧 Pawn 不再被占用
		CachedController->UnPossess();

		// 让 GameMode 使用默认 PlayerStart 重生
		GM->RestartPlayer(CachedController.Get());
	}

	Destroy();    // 销毁 ragdoll
}

void AMyFPS_CPPCharacter::HandleActiveWeaponOutOfAmmo(AMyFPS_CPPWeaponActor* DepletedWeapon)
{
	// 默认不做任何事，由子类AI实现具体策略
}

void AMyFPS_CPPCharacter::ClientOnHitEvent_Implementation()
{
	if (IsLocallyControlled())        // 保险起见，防止 Dedicated Server 也走进来
	{
		OnHitEvent();                 // Blueprint 实现
	}
}

void AMyFPS_CPPCharacter::ClientOnHittedEvent_Implementation()
{
	if (IsLocallyControlled())
	{
		OnHittedEvent();
	}
}

void AMyFPS_CPPCharacter::ClientOnKillEvent_Implementation()
{
	if (IsLocallyControlled())
	{
		OnKillEvent();
	}
}