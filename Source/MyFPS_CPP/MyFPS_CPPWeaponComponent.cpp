// Copyright Epic Games, Inc. All Rights Reserved.


#include "MyFPS_CPPWeaponComponent.h"
#include "MyFPS_CPPCharacter.h"
#include "MyFPS_CPPProjectile.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Animation/AnimInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"

// Sets default values for this component's properties
UMyFPS_CPPWeaponComponent::UMyFPS_CPPWeaponComponent()
{
	// Default offset from the character location for projectiles to spawn
	MuzzleOffset = FVector(100.0f, 0.0f, 10.0f);
}


void UMyFPS_CPPWeaponComponent::Unholster() {
	if (C_UnholsterAnimation && W_UnholsterAnimation) {
		UE_LOG(LogTemp, Warning, TEXT("1"));
		UAnimInstance* C_AnimInstance = Character->GetMesh1P()->GetAnimInstance();
		UAnimInstance* W_AnimInstance = USkeletalMeshComponent::GetAnimInstance();
		if (C_AnimInstance && W_AnimInstance)
		{
			UE_LOG(LogTemp, Warning, TEXT("2"));
			C_AnimInstance->Montage_Play(C_UnholsterAnimation, 1.f);
			W_AnimInstance->Montage_Play(W_UnholsterAnimation, 1.f);
		}
	}
	if (UnholsterSound != nullptr)
	{
		UGameplayStatics::PlaySoundAtLocation(this, UnholsterSound, Character->GetActorLocation());
	}
}


void UMyFPS_CPPWeaponComponent::Fire()
{
	if (Character == nullptr || Character->GetController() == nullptr)
	{
		return;
	}

	// Try and fire a projectile
	if (ProjectileClass != nullptr)
	{
		UWorld* const World = GetWorld();
		if (World != nullptr)
		{
			APlayerController* PlayerController = Cast<APlayerController>(Character->GetController());
			const FRotator SpawnRotation = PlayerController->PlayerCameraManager->GetCameraRotation();
			// MuzzleOffset is in camera space, so transform it to world space before offsetting from the character location to find the final muzzle position
			const FVector SpawnLocation = GetOwner()->GetActorLocation() + SpawnRotation.RotateVector(MuzzleOffset);
	
			//Set Spawn Collision Handling Override
			FActorSpawnParameters ActorSpawnParams;
			ActorSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
	
			// Spawn the projectile at the muzzle
			World->SpawnActor<AMyFPS_CPPProjectile>(ProjectileClass, SpawnLocation, SpawnRotation, ActorSpawnParams);
		}
	}
	// Try and play a firing animation if specified
	if (C_FireAnimation && W_FireAnimation)
	{
		// Get the animation object for the arms mesh
		UAnimInstance* C_AnimInstance = Character->GetMesh1P()->GetAnimInstance();
		UAnimInstance* W_AnimInstance = USkeletalMeshComponent::GetAnimInstance();
		if (C_AnimInstance && W_AnimInstance)
		{
			C_AnimInstance->Montage_Play(C_FireAnimation, 1.f);
			W_AnimInstance->Montage_Play(W_FireAnimation, 1.f);
		}
	}
	
	// Try and play the sound if specified
	if (FireSound != nullptr)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, Character->GetActorLocation());
	}
	
}


void UMyFPS_CPPWeaponComponent::Reload() {
	if (C_ReloadAnimation && W_ReloadAnimation) {
		UAnimInstance* C_AnimInstance = Character->GetMesh1P()->GetAnimInstance();
		UAnimInstance* W_AnimInstance = USkeletalMeshComponent::GetAnimInstance();
		if (C_AnimInstance && W_AnimInstance)
		{
			C_AnimInstance->Montage_Play(C_ReloadAnimation, 1.f);
			W_AnimInstance->Montage_Play(W_ReloadAnimation, 1.f);
		}
	}
	// Try and play the sound if specified
	if (ReloadSound != nullptr)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ReloadSound, Character->GetActorLocation());
	}
}


bool UMyFPS_CPPWeaponComponent::AttachWeapon(AMyFPS_CPPCharacter* TargetCharacter)
{
	// Check that the character is valid, and has no weapon component yet
	if (!TargetCharacter)
	{
		return false;
	}

	Character = TargetCharacter;

	// Attach the weapon to the First Person Character
	FAttachmentTransformRules AttachmentRules(EAttachmentRule::SnapToTarget, true);
	AttachToComponent(Character->GetMesh1P(), AttachmentRules, FName(TEXT("SOCKET_Weapon")));

	// Set up action bindings
	if (APlayerController* PlayerController = Cast<APlayerController>(Character->GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			// Set the priority of the mapping to 1, so that it overrides the Jump action with the Fire action when using touch input
			Subsystem->AddMappingContext(FireMappingContext, 1);
		}

		if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerController->InputComponent))
		{
			// Fire
			EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Triggered, this, &UMyFPS_CPPWeaponComponent::Fire);
			EnhancedInputComponent->BindAction(ReloadAction, ETriggerEvent::Started, this, &UMyFPS_CPPWeaponComponent::Reload);
		}
	}

	Unholster();
	return true;
}

void UMyFPS_CPPWeaponComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// ensure we have a character owner
	if (Character != nullptr)
	{
		// remove the input mapping context from the Player Controller
		if (APlayerController* PlayerController = Cast<APlayerController>(Character->GetController()))
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
			{
				Subsystem->RemoveMappingContext(FireMappingContext);
			}
		}
	}

	// maintain the EndPlay call chain
	Super::EndPlay(EndPlayReason);
}