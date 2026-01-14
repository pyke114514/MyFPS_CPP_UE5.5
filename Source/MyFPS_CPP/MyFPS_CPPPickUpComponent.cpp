#include "MyFPS_CPPPickUpComponent.h"
#include "MyFPS_CPPCharacter.h"
#include "MyFPS_CPPWeaponActor.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"

UMyFPS_CPPPickUpComponent::UMyFPS_CPPPickUpComponent()
{
	SphereRadius = 32.f;

	SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetCollisionResponseToAllChannels(ECR_Ignore);
	SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	SetGenerateOverlapEvents(true);
	SetHiddenInGame(true);
	SetCanEverAffectNavigation(false);

	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UMyFPS_CPPPickUpComponent::BeginPlay()
{
	Super::BeginPlay();

	CacheWeaponFromOwner();

	if (!CachedWeapon)
	{
		UE_LOG(LogTemp, Warning, TEXT("PickUpComponent: No owning weapon found on %s"), *GetNameSafe(GetOwner()));
		SetComponentTickEnabled(false);
		SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return;
	}

	OnComponentBeginOverlap.AddDynamic(this, &UMyFPS_CPPPickUpComponent::OnSphereBeginOverlap);

	PrimaryComponentTick.TickInterval = 0.1f;
}

void UMyFPS_CPPPickUpComponent::OnRegister()
{
	Super::OnRegister();
	CacheWeaponFromOwner();
}

#if WITH_EDITOR
void UMyFPS_CPPPickUpComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	CacheWeaponFromOwner();
}
#endif

void UMyFPS_CPPPickUpComponent::CacheWeaponFromOwner()
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		CachedWeapon = nullptr;
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		CachedWeapon = nullptr;
		return;
	}

	// 如果组件直接挂在武器 Actor 上，Owner 就是武器
	if (AMyFPS_CPPWeaponActor* Weapon = Cast<AMyFPS_CPPWeaponActor>(OwnerActor))
	{
		CachedWeapon = Weapon;
		return;
	}

	// 若组件在武器的子组件上，可以继续向上追溯
	if (USceneComponent* ParentComp = GetAttachParent())
	{
		if (AMyFPS_CPPWeaponActor* WeaponFromAttach = Cast<AMyFPS_CPPWeaponActor>(ParentComp->GetOwner()))
		{
			CachedWeapon = WeaponFromAttach;
			return;
		}
	}

	CachedWeapon = nullptr;
}

void UMyFPS_CPPPickUpComponent::OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	AMyFPS_CPPCharacter* Character = Cast<AMyFPS_CPPCharacter>(OtherActor);

	TryPickup(Character);
}

void UMyFPS_CPPPickUpComponent::OnSphereEndOverlap(UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	AMyFPS_CPPCharacter* Character = Cast<AMyFPS_CPPCharacter>(OtherActor);

	if (Character && IsComponentTickEnabled()) {
		SetComponentTickEnabled(false);
	}
}

void UMyFPS_CPPPickUpComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ProcessCurrentOverlaps();
}

void UMyFPS_CPPPickUpComponent::TryPickup(AMyFPS_CPPCharacter* Character)
{
	if (!Character || !CachedWeapon)
	{
		return;
	}

	if (!Character->CanEquipWeaponInSlot(CachedWeapon->GetTargetSlot(), CachedWeapon))
	{
		SetComponentTickEnabled(true);
		return;
	}

	Character->EquipWeaponActor(CachedWeapon);
	SetComponentTickEnabled(false);
}

void UMyFPS_CPPPickUpComponent::ProcessCurrentOverlaps()
{
	if (!CachedWeapon)
	{
		return;
	}

	TArray<AActor*> OverlappingActors;
	GetOverlappingActors(OverlappingActors, AMyFPS_CPPCharacter::StaticClass());

	for (AActor* Actor : OverlappingActors)
	{
		if (AMyFPS_CPPCharacter* Character = Cast<AMyFPS_CPPCharacter>(Actor))
		{
			TryPickup(Character);
		}
	}
}
