#include "MyFPS_CPPWeaponActor.h"
#include "MyFPS_CPPCharacter.h"
#include "MyFPS_CPPPickUpComponent.h"

#include "Kismet/KismetMathLibrary.h"
#include "Animation/AnimInstance.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

AMyFPS_CPPWeaponActor::AMyFPS_CPPWeaponActor()
{

	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	SpreadReturnStartValue = ShootSpreadMin;
	CurrentShootSpread = ShootSpreadMin;
	TimeSinceLastShot = ShootSpreadResetTime;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	WeaponMeshStatic = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMeshStatic"));
	PickupComponent = CreateDefaultSubobject<UMyFPS_CPPPickUpComponent>(TEXT("WeaponPickupComponent"));

	if (WeaponMesh)
	{
		WeaponMesh->SetupAttachment(Root);
	}

	if (WeaponMeshStatic)
	{
		WeaponMeshStatic->SetupAttachment(Root);
	}

	if (PickupComponent)
	{
		PickupComponent->SetupAttachment(WeaponMeshStatic);
	}

	if (WeaponMeshStatic)
	{
		WeaponMeshStatic->SetIsReplicated(true);
		WeaponMeshStatic->bReplicatePhysicsToAutonomousProxy = true;
	}
}

void AMyFPS_CPPWeaponActor::BeginPlay()
{
	Super::BeginPlay();

	CacheMagazineReverseMesh();
	CacheAimSocket();
	SetStaticMeshComponentsOwnerVisibility();

	if (MagazineReverseMesh)
	{
		MagazineReverseMesh->SetVisibility(false, true);
	}

	if (!bIsEquipped && WeaponMeshStatic)
	{
		ApplyDroppedPhysicsState();
	}

	if (HasAuthority())
	{
		CurrentAmmo = MaxAmmo;
		TotalAmmo = DefaultMagazineNum * MaxAmmo;
	}

	if (HasAuthority() && !bIsEquipped)
	{
		ScheduleAutoDestroy();
	}
}

void AMyFPS_CPPWeaponActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindInput();
	OwningCharacter = nullptr;

	if (UWorld* World = GetWorld())
	{
		FTimerManager& TM = World->GetTimerManager();
		TM.ClearTimer(ServerAutoFireTimerHandle);
		TM.ClearTimer(ServerReloadTimerHandle);
		TM.ClearTimer(PickupReactivateHandle);
		TM.ClearTimer(WeaponReadyDelayTimerHandle);
		TM.ClearTimer(AutoDestroyTimerHandle);
	}

	UnbindInput();
	StopActiveSound();
	OnWeaponReady.Clear();
	ReloadMontageEndedDelegate.Unbind();
	ViewMontageEndedDelegate.Unbind();
	FireMontageEndedDelegate.Unbind();

	Super::EndPlay(EndPlayReason);
}

void AMyFPS_CPPWeaponActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMyFPS_CPPWeaponActor, CurrentAmmo);
	DOREPLIFETIME(AMyFPS_CPPWeaponActor, TotalAmmo);
	DOREPLIFETIME(AMyFPS_CPPWeaponActor, bIsEquipped);
	DOREPLIFETIME(AMyFPS_CPPWeaponActor, bIsActiveWeapon);
	DOREPLIFETIME(AMyFPS_CPPWeaponActor, OwningCharacter);
}

void AMyFPS_CPPWeaponActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 仅在有效 DeltaTime 下更新射击扩散
	if (DeltaTime > KINDA_SMALL_NUMBER)
	{
		UpdateShootSpread(DeltaTime);
	}
}

bool AMyFPS_CPPWeaponActor::Equip(AMyFPS_CPPCharacter* TargetCharacter)
{
	if (HasAuthority())
	{
		return HandleServerEquip(TargetCharacter);
	}
	else
	{
		ServerEquip(TargetCharacter);
		return CanEquip(TargetCharacter);
	}
}

void AMyFPS_CPPWeaponActor::HandlePickupComponentPostEquip()
{
	if (WeaponMeshStatic)
	{
		WeaponMeshStatic->SetSimulatePhysics(false);
		WeaponMeshStatic->SetCollisionProfileName(TEXT("NoCollision"));
	}

	EnablePickupComponent(false);
}

void AMyFPS_CPPWeaponActor::EnablePickupComponent(bool bEnable)
{
	if (!PickupComponent)
	{
		return;
	}

	bPickupComponentDisabled = !bEnable;

	if (bEnable)
	{
		PickupComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		PickupComponent->SetGenerateOverlapEvents(true);
	}
	else
	{
		PickupComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PickupComponent->SetGenerateOverlapEvents(false);
	}
}

void AMyFPS_CPPWeaponActor::ActivateWeapon()
{
	if (HasAuthority())
	{
		HandleServerActivateWeapon();
	}
	else
	{
		ServerActivateWeapon();
	}
}

void AMyFPS_CPPWeaponActor::DeactivateWeapon()
{
	if (HasAuthority())
	{
		HandleServerDeactivateWeapon();
	}
	else
	{
		ServerDeactivateWeapon();
	}
}

void AMyFPS_CPPWeaponActor::DropWeapon()
{
	if (HasAuthority())
	{
		HandleServerDropWeapon();
	}
	else
	{
		ServerDropWeapon();
	}
}

void AMyFPS_CPPWeaponActor::BindInput()
{
	if (bIsPreviewInstance || !OwningCharacter || !bIsActiveWeapon)
	{
		return;
	}

	if (!OwningCharacter->IsLocallyControlled())
	{
		return;
	}

	if (APlayerController* PC = Cast<APlayerController>(OwningCharacter->GetController()))
	{
		if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PC->InputComponent))
		{
			CachedInputComponent = EnhancedInputComponent;

			if (FireAction)
			{
				FEnhancedInputActionEventBinding& PressBinding = EnhancedInputComponent->BindAction(
					FireAction, ETriggerEvent::Started, this, &AMyFPS_CPPWeaponActor::OnFirePressed);
				FirePressedBinding = &PressBinding;

				FEnhancedInputActionEventBinding& ReleaseBinding = EnhancedInputComponent->BindAction(
					FireAction, ETriggerEvent::Completed, this, &AMyFPS_CPPWeaponActor::OnFireReleased);
				FireReleasedBinding = &ReleaseBinding;
			}

			if (ReloadAction)
			{
				FEnhancedInputActionEventBinding& Binding = EnhancedInputComponent->BindAction(
					ReloadAction, ETriggerEvent::Started, this, &AMyFPS_CPPWeaponActor::Reload);
				ReloadBinding = &Binding;
			}

			if (ViewAction)
			{
				FEnhancedInputActionEventBinding& Binding = EnhancedInputComponent->BindAction(
					ViewAction, ETriggerEvent::Started, this, &AMyFPS_CPPWeaponActor::View);
				ViewBinding = &Binding;
			}
		}
	}
}

void AMyFPS_CPPWeaponActor::UnbindInput()
{
	if (!CachedInputComponent)
	{
		return;
	}

	if (FirePressedBinding)
	{
		CachedInputComponent->RemoveBinding(*FirePressedBinding);
		FirePressedBinding = nullptr;
	}

	if (FireReleasedBinding)
	{
		CachedInputComponent->RemoveBinding(*FireReleasedBinding);
		FireReleasedBinding = nullptr;
	}

	if (ReloadBinding)
	{
		CachedInputComponent->RemoveBinding(*ReloadBinding);
		ReloadBinding = nullptr;
	}

	if (ViewBinding)
	{
		CachedInputComponent->RemoveBinding(*ViewBinding);
		ViewBinding = nullptr;
	}

	CachedInputComponent = nullptr;
}

void AMyFPS_CPPWeaponActor::OnFirePressed()
{
	if (!CanLocalRequestFire())
	{
		return;
	}

	bFireInputHeld = true;
	Fire();
}

void AMyFPS_CPPWeaponActor::OnFireReleased()
{
	bFireInputHeld = false;

	if (!OwningCharacter)
	{
		return;
	}

	StopFire();
}

void AMyFPS_CPPWeaponActor::Unholster()
{
	if (bIsPreviewInstance || !OwningCharacter)
	{
		return;
	}

	if (OwningCharacter->Reloading)
	{
		OwningCharacter->Reloading = false;
		OwningCharacter->CanAim = false;
		OwningCharacter->Aiming = false;
	}

	PlayFPCharacterAndWeaponMontage(C_UnholsterAnimation, W_UnholsterAnimation);
	PlaySound(UnholsterSound);
}

void AMyFPS_CPPWeaponActor::Fire()
{
	if (!CanLocalRequestFire())
	{
		return;
	}

	if (HasAuthority())
	{
		HandleServerStartFire();
	}
	else
	{
		ServerStartFire();
	}
}

void AMyFPS_CPPWeaponActor::StopFire() {
	if (HasAuthority())
	{
		HandleServerStopFire();
	}
	else
	{
		ServerStopFire();
	}
}

void AMyFPS_CPPWeaponActor::Reload()
{
	if (!CanLocalRequestReload())
	{
		return;
	}

	if (HasAuthority())
	{
		HandleServerReloadRequest();
	}
	else
	{
		ServerReload();
	}
}

void AMyFPS_CPPWeaponActor::View()
{
	if (bWeaponActionsLocked || bIsPreviewInstance || !OwningCharacter || !OwningCharacter->GetController() || !bIsActiveWeapon)
	{
		return;
	}

	bFireInputHeld = false;

	OwningCharacter->Viewing = true;
	OwningCharacter->CanAim = true;
	OwningCharacter->Aiming = false;

	PlayFPCharacterAndWeaponMontage(C_ViewAnimation, nullptr);
	PlaySound(ViewSound);

	bool bDelegateBound = false;

	if (C_ViewAnimation)
	{
		if (USkeletalMeshComponent* Mesh = OwningCharacter->GetMesh1P())
		{
			if (UAnimInstance* Anim = Mesh->GetAnimInstance())
			{
				ViewMontageEndedDelegate.Unbind();
				ViewMontageEndedDelegate.BindUObject(this, &AMyFPS_CPPWeaponActor::OnViewMontageEnded);
				Anim->Montage_SetEndDelegate(ViewMontageEndedDelegate, C_ViewAnimation);
				bDelegateBound = true;
			}
		}
	}

	if (!bDelegateBound)
	{
		HandleViewFinished();
	}
}

void AMyFPS_CPPWeaponActor::OnFireMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage != C_FireAnimation && Montage != C_FireAimedAnimation)
	{
		return;
	}

	HandleFireFinished();
}

void AMyFPS_CPPWeaponActor::OnReloadMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage != C_ReloadAnimation)
	{
		return;
	}

	if (bInterrupted)
	{
		HandleReloadFinished();
	}
}

void AMyFPS_CPPWeaponActor::OnViewMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage != C_ViewAnimation)
	{
		return;
	}

	HandleViewFinished();
}

void AMyFPS_CPPWeaponActor::HandleFireFinished()
{
	if (OwningCharacter)
	{
		OwningCharacter->CanSprint = true;
	}

	if (!HasAuthority())
	{
		NotifyWeaponReady();
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		NotifyWeaponReady();
		return;
	}

	const float TimeSinceLastFire = World->GetTimeSeconds() - LastServerFireTime;
	const float RemainingCooldown = FMath::Max(FireInterval - TimeSinceLastFire, 0.f);

	if (RemainingCooldown <= KINDA_SMALL_NUMBER)
	{
		NotifyWeaponReady();
	}
	else
	{
		GetWorldTimerManager().ClearTimer(WeaponReadyDelayTimerHandle);
		GetWorldTimerManager().SetTimer(
			WeaponReadyDelayTimerHandle,
			this,
			&AMyFPS_CPPWeaponActor::NotifyWeaponReady,
			RemainingCooldown,
			false);
	}
}

void AMyFPS_CPPWeaponActor::HandleReloadFinished()
{
	if (!OwningCharacter)
	{
		return;
	}

	OwningCharacter->Reloading = false;
	OwningCharacter->CanAim = true;

	if (MagazineReverseMesh)
	{
		MagazineReverseMesh->SetVisibility(false, true);
	}
}

void AMyFPS_CPPWeaponActor::RefillAmmo()
{
	if (!HasAuthority())
	{
		return;
	}

	int NeededAmmo = MaxAmmo - CurrentAmmo;

	if (TotalAmmo < NeededAmmo) {
		NeededAmmo = TotalAmmo;
	}

	TotalAmmo -= NeededAmmo;
	CurrentAmmo += NeededAmmo;
}

void AMyFPS_CPPWeaponActor::HandleViewFinished()
{
	if (OwningCharacter)
	{
		OwningCharacter->Viewing = false;
	}
}

void AMyFPS_CPPWeaponActor::StopView()
{
	if (!OwningCharacter)
	{
		return;
	}

	UAnimInstance* CharAnim = OwningCharacter->GetMesh1P() ? OwningCharacter->GetMesh1P()->GetAnimInstance() : nullptr;

	if (!CharAnim || !C_ViewAnimation)
	{
		return;
	}

	if (!CharAnim->Montage_IsPlaying(C_ViewAnimation))
	{
		return;
	}

	CharAnim->StopAllMontages(0.2f);
	StopActiveSound();

	HandleViewFinished();
}

float AMyFPS_CPPWeaponActor::PlayFPCharacterAndWeaponMontage(UAnimMontage* CharacterMontage, UAnimMontage* WeaponMontage)
{
	if (bIsPreviewInstance || !OwningCharacter || !bIsEquipped)
	{
		bSkipNextSound = true;
		return 0.f;
	}

	UAnimInstance* CharAnim = OwningCharacter->GetMesh1P() ? OwningCharacter->GetMesh1P()->GetAnimInstance() : nullptr;
	UAnimInstance* WeaponAnim = WeaponMesh ? WeaponMesh->GetAnimInstance() : nullptr;
	float MaxDuration = 0.f;

	const bool bIsFireCharacterMontage =
		CharacterMontage &&
		(CharacterMontage == C_FireAnimation || CharacterMontage == C_FireAimedAnimation);

	const bool bIsFireWeaponMontage =
		WeaponMontage &&
		(WeaponMontage == W_FireAnimation);

	const bool bShouldPlayCharacter =
		CharacterMontage &&
		CharAnim &&
		(!CharAnim->Montage_IsPlaying(CharacterMontage) || bIsFireCharacterMontage);

	const bool bShouldPlayWeapon =
		WeaponMontage &&
		WeaponAnim &&
		(!WeaponAnim->Montage_IsPlaying(WeaponMontage) || bIsFireWeaponMontage);

	if (!bShouldPlayCharacter && !bShouldPlayWeapon)
	{
		bSkipNextSound = true;
		return 0.f;
	}

	bSkipNextSound = false;
	StopActiveSound();

	if (bShouldPlayCharacter)
	{
		const float Played = CharAnim->Montage_Play(CharacterMontage, 1.f);
		MaxDuration = FMath::Max(MaxDuration, Played);

		if (CharacterMontage == C_FireAnimation || CharacterMontage == C_FireAimedAnimation)
		{
			FireMontageEndedDelegate.Unbind();
			FireMontageEndedDelegate.BindUObject(this, &AMyFPS_CPPWeaponActor::OnFireMontageEnded);
			CharAnim->Montage_SetEndDelegate(FireMontageEndedDelegate, CharacterMontage);
		}

		if (CharacterMontage == C_ReloadAnimation)
		{
			ReloadMontageEndedDelegate.Unbind();
			ReloadMontageEndedDelegate.BindUObject(this, &AMyFPS_CPPWeaponActor::OnReloadMontageEnded);
			CharAnim->Montage_SetEndDelegate(ReloadMontageEndedDelegate, CharacterMontage);
		}

		if (!bShouldPlayWeapon && WeaponAnim)
		{
			WeaponAnim->StopAllMontages(0.2f);
		}
	}

	if (bShouldPlayWeapon)
	{
		const float Played = WeaponAnim->Montage_Play(WeaponMontage, 1.f);
		MaxDuration = FMath::Max(MaxDuration, Played);
	}

	return MaxDuration;
}

void AMyFPS_CPPWeaponActor::PlayTPCharacterMontage(UAnimMontage* CharacterMontage)
{
	if (bIsPreviewInstance || !OwningCharacter || !bIsEquipped)
	{
		return;
	}

	UAnimInstance* CharAnim = OwningCharacter->GetMesh() ? OwningCharacter->GetMesh()->GetAnimInstance() : nullptr;

	const bool bIsFireCharacterMontage = CharacterMontage && (CharacterMontage == TP_FireAnimation);

	const bool bShouldPlayCharacter = CharacterMontage && CharAnim && (!CharAnim->Montage_IsPlaying(CharacterMontage) || bIsFireCharacterMontage);

	if (!bShouldPlayCharacter)
	{
		return;
	}

	CharAnim->Montage_Play(CharacterMontage, 1.f);
}

void AMyFPS_CPPWeaponActor::PlaySound(USoundBase* SoundToPlay)
{
	if (bIsPreviewInstance || !SoundToPlay || !OwningCharacter)
	{
		return;
	}

	if (bSkipNextSound)
	{
		bSkipNextSound = false;
		return;
	}

	StopActiveSound();

	if (UAudioComponent* NewAudioComponent = UGameplayStatics::SpawnSoundAtLocation(
		this,
		SoundToPlay,
		OwningCharacter->GetActorLocation()))
	{
		ActiveAudioComponent = NewAudioComponent;
	}
}

void AMyFPS_CPPWeaponActor::StopActiveSound()
{
	if (ActiveAudioComponent.IsValid())
	{
		if (ActiveAudioComponent->GetSound() == FireSound)
		{
			return;
		}

		ActiveAudioComponent->Stop();
	}

	ActiveAudioComponent = nullptr;
}

void AMyFPS_CPPWeaponActor::CacheMagazineReverseMesh()
{
	if (MagazineReverseMesh || !WeaponMesh)
	{
		return;
	}

	TArray<USceneComponent*> ChildComponents;
	WeaponMesh->GetChildrenComponents(true, ChildComponents);

	const FName TargetName(TEXT("Magazine_Reverse"));

	for (USceneComponent* Child : ChildComponents)
	{
		if (!Child)
		{
			continue;
		}

		if (Child->GetFName() == TargetName)
		{
			if (UStaticMeshComponent* StaticChild = Cast<UStaticMeshComponent>(Child))
			{
				MagazineReverseMesh = StaticChild;
				break;
			}
		}
	}
}

void AMyFPS_CPPWeaponActor::CacheAimSocket()
{
	if (AimSocketComponent || !WeaponMesh)
	{
		return;
	}

	TArray<USceneComponent*> ChildComponents;
	WeaponMesh->GetChildrenComponents(true, ChildComponents);

	const FName TargetName(TEXT("AimSocket"));

	for (USceneComponent* Child : ChildComponents)
	{
		if (!Child)
		{
			continue;
		}

		if (Child->GetFName() == TargetName)
		{
			AimSocketComponent = Child;
			break;
		}
	}
}

void AMyFPS_CPPWeaponActor::SetStaticMeshComponentsOwnerVisibility()
{
	if (!WeaponMesh || !WeaponMeshStatic)
	{
		return;
	}

	TArray<USceneComponent*> FPChildren;
	WeaponMesh->GetChildrenComponents(true, FPChildren);

	for (USceneComponent* Child : FPChildren)
	{
		if (!Child) continue;

		if (UStaticMeshComponent* StaticMesh = Cast<UStaticMeshComponent>(Child))
		{
			StaticMesh->SetOnlyOwnerSee(true);
		}
	}

	TArray<USceneComponent*> TPChildren;
	WeaponMeshStatic->GetChildrenComponents(true, TPChildren);

	for (USceneComponent* Child : TPChildren)
	{
		if (!Child) continue;

		if (UStaticMeshComponent* StaticMesh = Cast<UStaticMeshComponent>(Child))
		{
			StaticMesh->SetOwnerNoSee(true);
		}
	}
}

void AMyFPS_CPPWeaponActor::SetStaticMeshComponentsVisibility(bool visible)
{
	if (!WeaponMesh || !WeaponMeshStatic)
	{
		return;
	}

	TArray<USceneComponent*> FPChildren;
	WeaponMesh->GetChildrenComponents(true, FPChildren);

	for (USceneComponent* Child : FPChildren)
	{
		if (!Child) continue;

		if (UStaticMeshComponent* StaticMesh = Cast<UStaticMeshComponent>(Child))
		{
			if (StaticMesh == MagazineReverseMesh && visible) {
				continue;
			}

			StaticMesh->SetVisibility(visible);
		}
	}

	TArray<USceneComponent*> TPChildren;
	WeaponMeshStatic->GetChildrenComponents(true, TPChildren);

	for (USceneComponent* Child : TPChildren)
	{
		if (!Child) continue;

		if (UStaticMeshComponent* StaticMesh = Cast<UStaticMeshComponent>(Child))
		{
			StaticMesh->SetVisibility(visible);
		}
	}
}

void AMyFPS_CPPWeaponActor::SetWeaponActionsLocked(bool bLocked)
{
	if (bWeaponActionsLocked == bLocked)
	{
		return;
	}

	bWeaponActionsLocked = bLocked;

	if (bWeaponActionsLocked)
	{
		bFireInputHeld = false;

		if (OwningCharacter)
		{
			StopFire();
		}
	}
}

bool AMyFPS_CPPWeaponActor::AreWeaponActionsLocked() const
{
	return bWeaponActionsLocked;
}

bool AMyFPS_CPPWeaponActor::CanLocalRequestFire() const
{
	if (bWeaponActionsLocked || bIsPreviewInstance || !OwningCharacter || !bIsActiveWeapon)
	{
		return false;
	}

	if (!OwningCharacter->IsLocallyControlled() && !HasAuthority())
	{
		return false;
	}

	return true;
}

bool AMyFPS_CPPWeaponActor::CanLocalRequestReload() const
{
	if (bWeaponActionsLocked || bIsPreviewInstance || !OwningCharacter || !bIsActiveWeapon)
	{
		return false;
	}

	if (!OwningCharacter->IsLocallyControlled() && !HasAuthority())
	{
		return false;
	}

	if (CurrentAmmo >= MaxAmmo)
	{
		return false;
	}

	if (TotalAmmo <= 0)
	{
		return false;
	}

	return true;
}

bool AMyFPS_CPPWeaponActor::CanServerFire() const
{
	if (!OwningCharacter || !bIsActiveWeapon || bWeaponActionsLocked)
	{
		return false;
	}

	if (OwningCharacter->Reloading)
	{
		return false;
	}

	if (CurrentAmmo <= 0)
	{
		return false;
	}

	const float TimeSinceLastFire = GetWorld()->GetTimeSeconds() - LastServerFireTime;
	if (TimeSinceLastFire + 0.001f < FireInterval) // 小容差
	{
		return false;
	}

	return true;
}

bool AMyFPS_CPPWeaponActor::CanServerReload() const
{
	if (!OwningCharacter || bWeaponActionsLocked || !bIsActiveWeapon)
	{
		return false;
	}

	if (OwningCharacter->Reloading)
	{
		return false;
	}

	if (CurrentAmmo >= MaxAmmo)
	{
		return false;
	}

	if (TotalAmmo <= 0) {
		return  false;
	}

	return true;
}

bool AMyFPS_CPPWeaponActor::HandleServerEquip(AMyFPS_CPPCharacter* TargetCharacter)
{
	if (bIsPreviewInstance || !IsValid(TargetCharacter))
	{
		return false;
	}

	if (!TargetCharacter->CanEquipWeaponInSlot(TargetSlot, this))
	{
		return false;
	}

	OwningCharacter = TargetCharacter;
	OnRep_OwningCharacter(nullptr);

	HandlePickupComponentPostEquip();
	bIsEquipped = true;
	OnRep_IsEquipped();

	bIsActiveWeapon = false;
	OnRep_IsActiveWeapon();

	BindInput();

	return true;
}

bool AMyFPS_CPPWeaponActor::CanEquip(AMyFPS_CPPCharacter* TargetCharacter) const
{
	if (bIsPreviewInstance || !IsValid(TargetCharacter))
	{
		return false;
	}

	if (!TargetCharacter->CanEquipWeaponInSlot(TargetSlot, this))
	{
		return false;
	}

	USkeletalMeshComponent* Mesh = TargetCharacter->GetMesh();
	USkeletalMeshComponent* Mesh1P = TargetCharacter->GetMesh1P();

	if (!Mesh1P || !Mesh)
	{
		return false;
	}

	return true;
}

void AMyFPS_CPPWeaponActor::HandleServerActivateWeapon()
{
	if (!OwningCharacter || bIsPreviewInstance)
	{
		return;
	}

	if (!bIsEquipped)
	{
		bIsEquipped = true;
		OnRep_IsEquipped();
	}

	bIsActiveWeapon = true;
	OnRep_IsActiveWeapon();
}

void AMyFPS_CPPWeaponActor::HandleServerDeactivateWeapon()
{
	if (!OwningCharacter || bIsPreviewInstance)
	{
		return;
	}

	bIsActiveWeapon = false;
	OnRep_IsActiveWeapon();
}

void AMyFPS_CPPWeaponActor::HandleServerDropWeapon()
{
	if (!bIsEquipped || bIsPreviewInstance || !OwningCharacter)
	{
		return;
	}

	FVector ViewLoc;
	FRotator ViewRot;

	ViewLoc = OwningCharacter->GetFirstPersonCameraComponent()->GetComponentLocation();
	ViewRot = OwningCharacter->GetFirstPersonCameraComponent()->GetComponentRotation();

	HandleServerStopFire();

	StopActiveSound();
	UnbindInput();

	DetachFromCharacterComponents(OwningCharacter);

	DropWeaponPhysically(ViewLoc,ViewRot);

	constexpr float PickupReactivateDelay = 0.5f;
	GetWorldTimerManager().SetTimer(
		PickupReactivateHandle,
		[this]()
		{
			EnablePickupComponent(true);
		},
		PickupReactivateDelay,
		false);

	bIsEquipped = false;
	OnRep_IsEquipped();

	bIsActiveWeapon = false;
	OnRep_IsActiveWeapon();

	OwningCharacter = nullptr;
	OnRep_OwningCharacter(nullptr);

	CachedInputComponent = nullptr;

	ScheduleAutoDestroy();
}

void AMyFPS_CPPWeaponActor::HandleServerStartFire()
{
	if (!CanServerFire())
	{
		if (CurrentAmmo <= 0)
		{
			if (CanServerReload())
			{
				HandleServerReloadRequest();
			}
			else if (!HasAnyAmmo())
			{
				NotifyOwnerOutOfAmmo();
			}
		}
		return;
	}

	PerformServerFire();

	if (FireMode == EWeaponFireMode::FullAuto && !GetWorldTimerManager().IsTimerActive(ServerAutoFireTimerHandle))
	{
		GetWorldTimerManager().SetTimer(ServerAutoFireTimerHandle, this, &AMyFPS_CPPWeaponActor::HandleServerAutoFire, FireInterval, true, FireInterval);
	}
}

void AMyFPS_CPPWeaponActor::HandleServerStopFire()
{
	if (GetWorldTimerManager().IsTimerActive(ServerAutoFireTimerHandle))
	{
		GetWorldTimerManager().ClearTimer(ServerAutoFireTimerHandle);
	}
}

void AMyFPS_CPPWeaponActor::HandleServerAutoFire()
{
	if (!CanServerFire())
	{
		if (CurrentAmmo <= 0)
		{
			if (CanServerReload())
			{
				HandleServerReloadRequest();
			}
			else if(!HasAnyAmmo())
			{
				NotifyOwnerOutOfAmmo();
				HandleServerStopFire();
			}
		}
		return;
	}

	PerformServerFire();
}

void AMyFPS_CPPWeaponActor::PerformServerFire()
{
	if (!OwningCharacter)
	{
		return;
	}

	LastServerFireTime = GetWorld()->GetTimeSeconds();
	CurrentAmmo = FMath::Clamp(CurrentAmmo - 1, 0, MaxAmmo);

	FHitResult Hit;
	FVector ShotDirection;
	const bool bHit = PerformServerLineTrace(Hit, ShotDirection);

	if (bHit)
	{
		ApplyServerDamage(Hit, ShotDirection);
	}

	const bool bWasAiming = OwningCharacter->Aiming && OwningCharacter->CanAim;
	const FVector ImpactPoint = bHit ? Hit.ImpactPoint : (AimSocketComponent ? AimSocketComponent->GetComponentLocation() + ShotDirection * 5000.f : FVector::ZeroVector);

	OwningCharacter->CanSprint = false;

	MulticastPlayFireEffects(bWasAiming, ImpactPoint, bHit);

	AddShootSpreadOnFire();
}

bool AMyFPS_CPPWeaponActor::PerformServerLineTrace(FHitResult& OutHit, FVector& OutShotDirection) const
{
	if (!OwningCharacter)
	{
		return false;
	}

	if (UWorld* World = GetWorld())
	{
		const FVector Start = OwningCharacter->GetFirstPersonCameraComponent()->GetComponentLocation();
		const FVector Forward = OwningCharacter->GetControlRotation().Vector();

		FVector ShotDir = Forward;
		const float SpreadAngleDeg = GetTotalSpreadAngle();
		if (SpreadAngleDeg > KINDA_SMALL_NUMBER)
		{
			const float SpreadRad = FMath::DegreesToRadians(SpreadAngleDeg);
			ShotDir = UKismetMathLibrary::RandomUnitVectorInConeInRadians(Forward, SpreadRad);
		}

		OutShotDirection = ShotDir;
		const FVector End = Start + ShotDir * 10000.f;

		FCollisionQueryParams Params(SCENE_QUERY_STAT(WeaponTrace), false);
		Params.AddIgnoredActor(OwningCharacter);
		Params.AddIgnoredActor(this);

		return World->LineTraceSingleByChannel(
			OutHit,
			Start,
			End,
			ECC_GameTraceChannel1,
			Params);
	}

	return false;
}

void AMyFPS_CPPWeaponActor::ApplyServerDamage(const FHitResult& Hit, const FVector& ShotDirection)
{
	if (!Hit.GetActor())
	{
		return;
	}

	const float DamageOffset = FMath::FRandRange(-DamageVariance, DamageVariance);
	const float FinalDamage = FMath::Max(BaseDamage + DamageOffset, 0.f);

	UGameplayStatics::ApplyPointDamage(
		Hit.GetActor(),
		FinalDamage,
		ShotDirection.GetSafeNormal(),
		Hit,
		OwningCharacter ? OwningCharacter->GetController() : nullptr,
		this,
		UDamageType::StaticClass());

	if (Hit.GetActor()->IsA(ACharacter::StaticClass()))
	{
		OwningCharacter->ClientOnHitEvent();
	}

	constexpr float PhysicsImpulseStrength = 20000.f;

	if (UPrimitiveComponent* HitComp = Hit.GetComponent())
	{
		if (HitComp->IsSimulatingPhysics())
		{
			const FVector ImpulseDir = ShotDirection.GetSafeNormal();
			const FVector Impulse = ImpulseDir * PhysicsImpulseStrength;

			HitComp->AddImpulseAtLocation(Impulse, Hit.ImpactPoint);
		}
	}
}

void AMyFPS_CPPWeaponActor::HandleServerReloadRequest()
{
	if (!CanServerReload())
	{
		return;
	}

	if (!OwningCharacter)
	{
		return;
	}

	OwningCharacter->Reloading = true;
	OwningCharacter->CanAim = false;

	MulticastPlayReloadEffects();

	const float ReloadDuration = GetReloadAnimationDuration();
	GetWorldTimerManager().ClearTimer(ServerReloadTimerHandle);
	GetWorldTimerManager().SetTimer(ServerReloadTimerHandle, this, &AMyFPS_CPPWeaponActor::HandleServerReloadComplete, ReloadDuration, false);
}

void AMyFPS_CPPWeaponActor::HandleServerReloadComplete()
{
	GetWorldTimerManager().ClearTimer(ServerReloadTimerHandle);

	if (!OwningCharacter)
	{
		return;
	}

	OwningCharacter->Reloading = false;
	OwningCharacter->CanAim = true;

	RefillAmmo();

	MulticastFinishReloadEffects();

	OnWeaponReady.Broadcast();
}

float AMyFPS_CPPWeaponActor::GetReloadAnimationDuration() const
{
	if (C_ReloadAnimation)
	{
		return C_ReloadAnimation->GetPlayLength();
	}

	return 2.0f;
}

void AMyFPS_CPPWeaponActor::HandleFireVFX(const FVector& HitPointLocation)
{
	if (!FireVFX || (!WeaponMesh && !WeaponMeshStatic))
	{
		return;
	}

	if (!OwningCharacter)
	{
		return;
	}

	USceneComponent* AttachComp = nullptr;

	const bool bUseFirstPersonVFX = OwningCharacter->IsPlayerControlled() && OwningCharacter->IsLocallyControlled();

	AttachComp = bUseFirstPersonVFX ? WeaponMesh : WeaponMeshStatic;

	if (!AttachComp)
	{
		return;
	}


	const FTransform SocketTransform = AttachComp->GetSocketTransform(TEXT("SOCKET_EmitPoint"), RTS_World);
	const FVector HitPointLocationLocal = HitPointLocation.IsNearlyZero() ? FVector(0, 1, 0) : SocketTransform.InverseTransformPosition(HitPointLocation);
	FVector DirLocal = HitPointLocationLocal.GetSafeNormal();

	FVector StandardDir = FVector(0.f, 1.f, 0.f);

	const float Dot = FVector::DotProduct(DirLocal, StandardDir);
	const float ClampedDot = FMath::Clamp(Dot, -1.f, 1.f);

	const float OffsetAngleDegrees = FMath::RadiansToDegrees(FMath::Acos(ClampedDot));

	if (OffsetAngleDegrees >= 10.f)
	{
		DirLocal = StandardDir;
	}

	UNiagaraComponent* NiagaraComp =
		UNiagaraFunctionLibrary::SpawnSystemAttached(
			FireVFX,
			AttachComp,
			TEXT("SOCKET_EmitPoint"),
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			EAttachLocation::SnapToTargetIncludingScale,
			true
		);

	if (NiagaraComp)
	{
		NiagaraComp->SetVectorParameter(TEXT("User.HitPointLocation"), HitPointLocationLocal);
		NiagaraComp->SetVectorParameter(TEXT("User.Dir"), DirLocal);
	}
}

void AMyFPS_CPPWeaponActor::ServerEquip_Implementation(AMyFPS_CPPCharacter* TargetCharacter)
{
	HandleServerEquip(TargetCharacter);
}

void AMyFPS_CPPWeaponActor::ServerActivateWeapon_Implementation()
{
	HandleServerActivateWeapon();
}

void AMyFPS_CPPWeaponActor::ServerDeactivateWeapon_Implementation()
{
	HandleServerDeactivateWeapon();
}

void AMyFPS_CPPWeaponActor::ServerDropWeapon_Implementation()
{
	HandleServerDropWeapon();
}

void AMyFPS_CPPWeaponActor::ServerStartFire_Implementation()
{
	HandleServerStartFire();
}

void AMyFPS_CPPWeaponActor::ServerStopFire_Implementation()
{
	HandleServerStopFire();
}

void AMyFPS_CPPWeaponActor::ServerReload_Implementation()
{
	HandleServerReloadRequest();
}

void AMyFPS_CPPWeaponActor::MulticastPlayFireEffects_Implementation(
	bool bWasAiming,
	FVector_NetQuantize ImpactPoint,
	bool bHadImpact)
{
	if (!OwningCharacter)
	{
		return;
	}

	const bool bIsLocalPlayerOwner =
		OwningCharacter->IsLocallyControlled() &&
		OwningCharacter->IsPlayerControlled();

	if (bIsLocalPlayerOwner)
	{
		PlayFPCharacterAndWeaponMontage(
			bWasAiming ? C_FireAimedAnimation : C_FireAnimation,
			W_FireAnimation);
	}

	PlayTPCharacterMontage(TP_FireAnimation);
	PlaySound(FireSound);
	HandleFireVFX(bHadImpact ? FVector(ImpactPoint) : FVector::ZeroVector);

	if (!bIsLocalPlayerOwner)
	{
		HandleFireFinished();
	}
}

void AMyFPS_CPPWeaponActor::MulticastPlayReloadEffects_Implementation()
{
	if (!OwningCharacter)
	{
		return;
	}

	const bool bIsOwner = OwningCharacter->IsLocallyControlled();

	if (bIsOwner)
	{
		PlayFPCharacterAndWeaponMontage(C_ReloadAnimation, W_ReloadAnimation);
	}

	PlayTPCharacterMontage(TP_ReloadAnimation);

	PlaySound(ReloadSound);
}

void AMyFPS_CPPWeaponActor::MulticastFinishReloadEffects_Implementation()
{
	HandleReloadFinished();
}

void AMyFPS_CPPWeaponActor::AttachToOwningCharacterComponents()
{
	if (!OwningCharacter)
	{
		return;
	}

	if (WeaponMeshStatic)
	{
		WeaponMeshStatic->SetSimulatePhysics(false);
		WeaponMeshStatic->SetCollisionProfileName(TEXT("NoCollision"));
	}

	USkeletalMeshComponent* Mesh = OwningCharacter->GetMesh();
	USkeletalMeshComponent* Mesh1P = OwningCharacter->GetMesh1P();

	const FAttachmentTransformRules Rules(EAttachmentRule::SnapToTarget, true);

	if (Mesh1P && WeaponMesh)
	{
		WeaponMesh->AttachToComponent(Mesh1P, Rules, AttachSocketName);
	}

	if (Mesh && WeaponMeshStatic)
	{
		WeaponMeshStatic->AttachToComponent(Mesh, Rules, AttachSocketName);
	}
}

void AMyFPS_CPPWeaponActor::DetachFromCharacterComponents(AMyFPS_CPPCharacter* Character)
{
	if (!Character)
	{
		return;
	}

	if (WeaponMesh && WeaponMesh->GetAttachParent() == Character->GetMesh1P())
	{
		WeaponMesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	}

	if (WeaponMeshStatic && WeaponMeshStatic->GetAttachParent() == Character->GetMesh())
	{
		WeaponMeshStatic->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	}
}

void AMyFPS_CPPWeaponActor::ApplyAnimBlueprints() const
{
	if (!OwningCharacter)
	{
		return;
	}

	if (USkeletalMeshComponent* Mesh1P = OwningCharacter->GetMesh1P())
	{
		if (FPCharacterAnimBlueprintClass)
		{
			Mesh1P->SetAnimInstanceClass(FPCharacterAnimBlueprintClass);
		}
	}

	if (USkeletalMeshComponent* Mesh = OwningCharacter->GetMesh())
	{
		if (TPCharacterAnimBlueprintClass)
		{
			Mesh->SetAnimInstanceClass(TPCharacterAnimBlueprintClass);
		}
	}
}

void AMyFPS_CPPWeaponActor::ShowWeaponMeshes(bool bShow)
{
	if (WeaponMesh || WeaponMeshStatic)
	{
		WeaponMesh->SetVisibility(bShow);
		WeaponMeshStatic->SetVisibility(bShow);
	}

	if (WeaponMesh || WeaponMeshStatic)
	{
		SetStaticMeshComponentsVisibility(bShow);
	}
}

void AMyFPS_CPPWeaponActor::HandleActivateWeaponVisuals(bool bFromReplication)
{
	SetWeaponActionsLocked(false);
	ShowWeaponMeshes(true);
	TryBindInput();

	Unholster();
}

void AMyFPS_CPPWeaponActor::HandleDeactivateWeaponVisuals(bool bFromReplication)
{
	SetWeaponActionsLocked(true);
	
	// 如果已经不再被角色持有（例如刚被丢到地上），
	// 就不要把网格隐藏掉，否则客户端会看不到掉落物。

	if (!bIsEquipped || !OwningCharacter)
	{
		ShowWeaponMeshes(true);
	}
	else {
		ShowWeaponMeshes(false);
	}

	UnbindInput();

	if (bIsPreviewInstance || !OwningCharacter)
	{
		return;
	}

	if (OwningCharacter->Reloading)
	{
		OwningCharacter->Reloading = false;
		OwningCharacter->CanAim = false;
		OwningCharacter->Aiming = false;
	}
}

void AMyFPS_CPPWeaponActor::ApplyDroppedPhysicsState()
{
	if (!WeaponMeshStatic)
	{
		return;
	}

	WeaponMeshStatic->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	WeaponMeshStatic->SetCollisionObjectType(ECC_WorldDynamic);
	WeaponMeshStatic->SetCollisionResponseToAllChannels(ECR_Block);
	WeaponMeshStatic->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	if (HasAuthority())
	{
		WeaponMeshStatic->SetSimulatePhysics(true);
	}
	else
	{
		WeaponMeshStatic->SetSimulatePhysics(false);   // 客户端永远不自己模拟
	}
}

void AMyFPS_CPPWeaponActor::DropWeaponPhysically(const FVector& ViewLoc, const FRotator& ViewRot) {
	ApplyDroppedPhysicsState();

	const FVector Forward = ViewRot.Vector();
	const FVector Up = FVector::UpVector;

	const FVector DropOrigin = ViewLoc + Forward * 60.f + FVector::UpVector * 20.f;
	SetActorLocation(DropOrigin, false, nullptr, ETeleportType::TeleportPhysics);
	SetActorRotation(ViewRot);

	const FVector DropImpulse =
		Forward * 400.f +   // 前向力度
		Up * 200.f;       // 向上抬一点

	if (WeaponMeshStatic->IsSimulatingPhysics())
	{
		WeaponMeshStatic->SetPhysicsLinearVelocity(FVector::ZeroVector);
		WeaponMeshStatic->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);

		WeaponMeshStatic->AddImpulse(DropImpulse, NAME_None, true);

		const FVector RandomSpin(
			FMath::FRandRange(-200.f, 200.f),
			FMath::FRandRange(-200.f, 200.f),
			FMath::FRandRange(-200.f, 200.f));

		WeaponMeshStatic->AddAngularImpulseInDegrees(RandomSpin, NAME_None, true);
	}
}

void AMyFPS_CPPWeaponActor::OnRep_OwningCharacter(AMyFPS_CPPCharacter* PreviousOwner)
{
	if (PreviousOwner && PreviousOwner != OwningCharacter)
	{
		DetachFromCharacterComponents(PreviousOwner);
	}

	if (OwningCharacter)
	{
		AttachToOwningCharacterComponents();
		SetOwner(OwningCharacter);
		TryBindInput();   // 新增：拥有者就绪后再尝试绑定
	}
	else
	{
		SetOwner(nullptr);
	}
}

void AMyFPS_CPPWeaponActor::OnRep_IsEquipped()
{
	if (bIsEquipped)
	{
		HandlePickupComponentPostEquip();
		AttachToOwningCharacterComponents();
		if (HasAuthority()) {
			CancelAutoDestroy();
		}
	}
	else
	{
		DetachFromCharacterComponents(OwningCharacter);
		ApplyDroppedPhysicsState();
		ShowWeaponMeshes(true);
		if (HasAuthority())
		{
			ScheduleAutoDestroy();
		}
	}
}

void AMyFPS_CPPWeaponActor::OnRep_IsActiveWeapon()
{
	if (bIsActiveWeapon)
	{
		ApplyAnimBlueprints();
		HandleActivateWeaponVisuals(true);
	}
	else
	{
		HandleDeactivateWeaponVisuals(true);
	}
}

void AMyFPS_CPPWeaponActor::TryBindInput()
{
	if (CachedInputComponent) { return; }
	if (bIsPreviewInstance) { return; }
	if (!bIsActiveWeapon) { return; }
	if (!OwningCharacter) { return; }
	if (!OwningCharacter->IsLocallyControlled()) { return; }

	BindInput();
}

void AMyFPS_CPPWeaponActor::NotifyWeaponReady()
{
	if (HasAuthority())
	{
		GetWorldTimerManager().ClearTimer(WeaponReadyDelayTimerHandle);
	}

	OnWeaponReady.Broadcast();
}

void AMyFPS_CPPWeaponActor::NotifyOwnerOutOfAmmo()
{
	if (!HasAuthority() || !OwningCharacter)
	{
		return;
	}

	OwningCharacter->HandleActiveWeaponOutOfAmmo(this);
}

void AMyFPS_CPPWeaponActor::ScheduleAutoDestroy()
{
	if (!HasAuthority() || bIsEquipped)
	{
		return;
	}

	if (UnpickedLifeSpan <= 0.f)
	{
		Destroy();
		return;
	}

	GetWorldTimerManager().ClearTimer(AutoDestroyTimerHandle);
	GetWorldTimerManager().SetTimer(
		AutoDestroyTimerHandle,
		this,
		&AMyFPS_CPPWeaponActor::HandleAutoDestroyTimerExpired,
		UnpickedLifeSpan,
		false);
}

void AMyFPS_CPPWeaponActor::CancelAutoDestroy()
{
	if (!HasAuthority())
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(AutoDestroyTimerHandle);
}

void AMyFPS_CPPWeaponActor::HandleAutoDestroyTimerExpired()
{
	if (!HasAuthority())
	{
		return;
	}

	if (!bIsEquipped)
	{
		Destroy();
	}
}

float AMyFPS_CPPWeaponActor::GetMoveSpreadAngle() const
{
	if (!OwningCharacter)
	{
		return MoveSpreadMin;
	}

	const float Speed = OwningCharacter->GetVelocity().Size();
	const float Factor = FMath::Clamp(Speed / MoveSpreadSpeedScale, 0.f, 1.f);
	return FMath::Lerp(MoveSpreadMin, MoveSpreadMax, Factor);
}

void AMyFPS_CPPWeaponActor::AddShootSpreadOnFire()
{
	CurrentShootSpread = FMath::Clamp(
		CurrentShootSpread + ShootSpreadPerShot,
		ShootSpreadMin,
		ShootSpreadMax);

	SpreadReturnStartValue = CurrentShootSpread;
	TimeSinceLastShot = 0.f;
}

void AMyFPS_CPPWeaponActor::UpdateShootSpread(float DeltaTime)
{
	if (ShootSpreadResetTime <= KINDA_SMALL_NUMBER)
	{
		CurrentShootSpread = ShootSpreadMin;
		TimeSinceLastShot = ShootSpreadResetTime;
		return;
	}

	TimeSinceLastShot += DeltaTime;

	const float Alpha = FMath::Clamp(TimeSinceLastShot / ShootSpreadResetTime, 0.f, 1.f);
	CurrentShootSpread = FMath::Lerp(SpreadReturnStartValue, ShootSpreadMin, Alpha);

	if (Alpha >= 1.f - KINDA_SMALL_NUMBER)
	{
		CurrentShootSpread = ShootSpreadMin;
		SpreadReturnStartValue = ShootSpreadMin;
		TimeSinceLastShot = ShootSpreadResetTime;
	}
}

float AMyFPS_CPPWeaponActor::GetShootSpreadAngle() const
{
	return CurrentShootSpread;
}

float AMyFPS_CPPWeaponActor::GetTotalSpreadAngle() const
{
	float Total = GetMoveSpreadAngle() + GetShootSpreadAngle();
	if (OwningCharacter->Aiming)
	{
		Total *= 0.5f;
	}
	return Total;
}