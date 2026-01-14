// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SkeletalMeshComponent.h"
#include "MyFPS_CPPWeaponComponent.generated.h"

class AMyFPS_CPPCharacter;

UCLASS(MinimalAPI)
class UMyFPS_CPPWeaponComponent : public USkeletalMeshComponent
{
	GENERATED_BODY()

public:
	/** Projectile class to spawn */
	UPROPERTY(EditDefaultsOnly, Category=Projectile)
	TSubclassOf<class AMyFPS_CPPProjectile> ProjectileClass;

	/** Sound to play each time we fire */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SoundList)
	USoundBase* UnholsterSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SoundList)
	USoundBase* FireSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SoundList)
	USoundBase* ReloadSound;
	
	/** AnimMontage to play each time we fire */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AnimationList)
	UAnimMontage* C_UnholsterAnimation;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AnimationList)
	UAnimMontage* W_UnholsterAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AnimationList)
	UAnimMontage* C_FireAnimation;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AnimationList)
	UAnimMontage* W_FireAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AnimationList)
	UAnimMontage* C_ReloadAnimation;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AnimationList)
	UAnimMontage* W_ReloadAnimation;

	/** Gun muzzle's offset from the characters location */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Gameplay)
	FVector MuzzleOffset;

	/** MappingContext */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	class UInputMappingContext* FireMappingContext;

	/** Fire Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	class UInputAction* FireAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	class UInputAction* ReloadAction;

	/** Sets default values for this component's properties */
	UMyFPS_CPPWeaponComponent();

	/** Attaches the actor to a FirstPersonCharacter */
	UFUNCTION(BlueprintCallable, Category="Weapon")
	bool AttachWeapon(AMyFPS_CPPCharacter* TargetCharacter);

	/** Make the weapon Fire a Projectile */
	UFUNCTION(BlueprintCallable, Category="Weapon")

	void Unholster();

	void Fire();

	void Reload();

protected:
	/** Ends gameplay for this component. */
	UFUNCTION()
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** The Character holding this weapon*/
	AMyFPS_CPPCharacter* Character;
};
