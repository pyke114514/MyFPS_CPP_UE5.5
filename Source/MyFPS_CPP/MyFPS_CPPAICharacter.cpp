#include "MyFPS_CPPAICharacter.h"
#include "MyFPS_CPPWeaponActor.h"

#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Damage.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AISense_Damage.h"

#include "AIController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

AMyFPS_CPPAICharacter::AMyFPS_CPPAICharacter()
{
	AutoPossessAI = EAutoPossessAI::PlacedInWorld;

	// 先确保父类构造的组件已经创建
	if (Mesh1P)
	{
		Mesh1P->DestroyComponent();
		Mesh1P = nullptr;
	}

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->bUseControllerDesiredRotation = true;
		MoveComp->bOrientRotationToMovement = false;
	}

	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = true;

	// 感知组件配置
	PerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("PerceptionComponent"));

	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	if (SightConfig)
	{
		SightConfig->SightRadius = 3000.f;
		SightConfig->LoseSightRadius = 3500.f;
		SightConfig->PeripheralVisionAngleDegrees = 70.f;
		SightConfig->SetMaxAge(2.f);
		SightConfig->DetectionByAffiliation.bDetectEnemies = true;
		SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
		SightConfig->DetectionByAffiliation.bDetectNeutrals = true;

		if (PerceptionComponent)
		{
			PerceptionComponent->ConfigureSense(*SightConfig);
			PerceptionComponent->SetDominantSense(SightConfig->GetSenseImplementation());
		}
	}

	DamageConfig = CreateDefaultSubobject<UAISenseConfig_Damage>(TEXT("DamageConfig"));
	if (DamageConfig && PerceptionComponent)
	{
		DamageConfig->SetMaxAge(5.f);
		PerceptionComponent->ConfigureSense(*DamageConfig);
	}

	if (PerceptionComponent)
	{
		PerceptionComponent->OnPerceptionUpdated.AddDynamic(this, &AMyFPS_CPPAICharacter::HandlePerceptionUpdated);
	}
}


void AMyFPS_CPPAICharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMyFPS_CPPAICharacter, CurrentTargetActor);
	DOREPLIFETIME(AMyFPS_CPPAICharacter, bHasDetectedPlayer);
}

void AMyFPS_CPPAICharacter::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		TryEquipDefaults();
	}
}

void AMyFPS_CPPAICharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	HandleControllerReady();
}

void AMyFPS_CPPAICharacter::OnRep_Controller()
{
	Super::OnRep_Controller();
	HandleControllerReady();
}

void AMyFPS_CPPAICharacter::HandleControllerReady()
{
	// AI 无需输入映射，但可以在这里做行为树初始化或黑板赋值
	if (HasAuthority())
	{
		TryEquipDefaults();
	}
}

void AMyFPS_CPPAICharacter::TryEquipDefaults()
{
	// 父类 BeginPlay 已经根据 DefaultPrimaryWeaponClass 等生成/装备
	// 这里确保当前武器激活
	if (HasAuthority() && CurrentWeaponActor)
	{
		CurrentWeaponActor->ActivateWeapon();
		BindWeaponReadyDelegate();
	}
}

void AMyFPS_CPPAICharacter::StartFire()
{
	if (!HasAuthority())
	{
		return;
	}

	bWantsToFire = true;

	if (CurrentWeaponActor && !CurrentWeaponActor->AreWeaponActionsLocked())
	{
		CurrentWeaponActor->Fire();
	}
}

void AMyFPS_CPPAICharacter::StopFire()
{
	if (!HasAuthority())
	{
		return;
	}

	bWantsToFire = false;

	if (CurrentWeaponActor)
	{
		CurrentWeaponActor->SetWeaponActionsLocked(false);
		CurrentWeaponActor->StopFire();
	}
}

void AMyFPS_CPPAICharacter::RequestReload()
{
	if (!HasAuthority() || !CurrentWeaponActor)
	{
		return;
	}

	CurrentWeaponActor->Reload();
}

bool AMyFPS_CPPAICharacter::IsWeaponBusy() const
{
	if (!CurrentWeaponActor)
	{
		return true;
	}

	return CurrentWeaponActor->AreWeaponActionsLocked() || Reloading;
}

void AMyFPS_CPPAICharacter::SetTargetActor(AActor* NewTarget)
{
	if (CurrentTargetActor == NewTarget)
	{
		return;
	}

	CurrentTargetActor = NewTarget;

	if (HasAuthority())
	{
		if (!CurrentTargetActor && bWantsToFire)
		{
			StopFire();
		}
	}
}

void AMyFPS_CPPAICharacter::HandlePerceptionUpdated(const TArray<AActor*>& /*UpdatedActors*/)
{
	if (!HasAuthority() || !PerceptionComponent)
	{
		return;
	}

	TArray<AActor*> SeenActors;
	PerceptionComponent->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), SeenActors);

	AActor* BestCandidate = ChooseBestTarget(SeenActors);

	if (BestCandidate)
	{
		if (!CurrentTargetActor)
		{
			SetHasDetectedPlayer(true);
			SetTargetActor(BestCandidate);
		}
		else if (IsBetterVisualTarget(BestCandidate, CurrentTargetActor))
		{
			SetTargetActor(BestCandidate);
		}
	}
	else
	{
		// 没有任何目标时清空
		SetTargetActor(nullptr);
		SetHasDetectedPlayer(false);
	}
}

float AMyFPS_CPPAICharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	const float Applied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	if (Applied > 0.f && HasAuthority())
	{
		if (UWorld* World = GetWorld())
		{
			APawn* InstigatorPawn = EventInstigator ? EventInstigator->GetPawn() : nullptr;
			AActor* DamageInstigatorActor = DamageCauser ? DamageCauser : InstigatorPawn;

			const FVector InstigatorLocation = DamageInstigatorActor
				? DamageInstigatorActor->GetActorLocation()
				: GetActorLocation();

			UAISense_Damage::ReportDamageEvent(
				World,
				this,
				DamageInstigatorActor,
				Applied,
				GetActorLocation(),
				InstigatorLocation
			);
		}

		SetHasDetectedPlayer(true);

		AActor* TrueAttacker = nullptr;

		if (EventInstigator)
		{
			TrueAttacker = EventInstigator->GetPawn();
		}

		if (!TrueAttacker && DamageCauser)
		{
			if (APawn* OwnerPawn = Cast<APawn>(DamageCauser->GetOwner()))
			{
				TrueAttacker = OwnerPawn;
			}
			else
			{
				TrueAttacker = DamageCauser;
			}
		}

		if (TrueAttacker)
		{
			SetTargetActor(TrueAttacker); // 攻击优先：总是切换到最新攻击者
		}
	}

	return Applied;
}

AActor* AMyFPS_CPPAICharacter::ChooseBestTarget(const TArray<AActor*>& Candidates) const
{
	AActor* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();

	const FVector MyLocation = GetActorLocation();

	for (AActor* Candidate : Candidates)
	{
		if (!IsValid(Candidate) || Candidate == this)
		{
			continue;
		}

		if (const AMyFPS_CPPCharacter* CandidateCharacter = Cast<AMyFPS_CPPCharacter>(Candidate))
		{
			if (CandidateCharacter->bIsDead)
			{
				continue;
			}
		}

		const float DistSq = FVector::DistSquared(MyLocation, Candidate->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			Best = Candidate;
			BestDistSq = DistSq;
		}
	}

	return Best;
}

void AMyFPS_CPPAICharacter::OnRep_CurrentTargetActor()
{
	// 客户端可在这里刷新动画状态 / IK / UI 等
	if (!HasAuthority() && !CurrentTargetActor && bWantsToFire)
	{
		// 目标丢失时确保停火
		bWantsToFire = false;
	}
}

void AMyFPS_CPPAICharacter::SetHasDetectedPlayer(bool bNewState)
{
	if (bHasDetectedPlayer == bNewState)
	{
		return;
	}

	bHasDetectedPlayer = bNewState;

	// 如果你希望在变回 false 时清理目标/停火，可在这里处理
	if (!bHasDetectedPlayer)
	{
		SetTargetActor(nullptr);
		StopFire();
	}
}

bool AMyFPS_CPPAICharacter::IsBetterVisualTarget(AActor* Candidate, AActor* Current) const
{
	if (!IsValid(Candidate))
	{
		return false;
	}

	if (!IsValid(Current))
	{
		return true;
	}

	const FVector MyLocation = GetActorLocation();
	const float DistToCandidateSq = FVector::DistSquared(MyLocation, Candidate->GetActorLocation());
	const float DistToCurrentSq = FVector::DistSquared(MyLocation, Current->GetActorLocation());

	// 候选更近才替换
	return DistToCandidateSq + KINDA_SMALL_NUMBER < DistToCurrentSq;
}

void AMyFPS_CPPAICharacter::BindWeaponReadyDelegate()
{
	if (!HasAuthority() || !CurrentWeaponActor)
	{
		return;
	}

	CurrentWeaponActor->OnWeaponReady.RemoveDynamic(this, &AMyFPS_CPPAICharacter::HandleWeaponReady);
	CurrentWeaponActor->OnWeaponReady.AddDynamic(this, &AMyFPS_CPPAICharacter::HandleWeaponReady);
}

void AMyFPS_CPPAICharacter::HandleWeaponReady()
{
	if (!HasAuthority())
	{
		return;
	}

	if (bWantsToFire && CurrentTargetActor && !IsWeaponBusy())
	{
		CurrentWeaponActor->StopFire();

		CurrentWeaponActor->Fire();
	}
}

void AMyFPS_CPPAICharacter::HandleActiveWeaponOutOfAmmo(AMyFPS_CPPWeaponActor* DepletedWeapon)
{
	if (!HasAuthority() || !DepletedWeapon || DepletedWeapon != CurrentWeaponActor)
	{
		return;
	}

	if (CurrentWeaponSlot != EWeaponSlot::Primary)
	{
		// 副武器耗尽：不切换，符合需求
		return;
	}

	AMyFPS_CPPWeaponActor* SecondaryWeapon = GetWeaponInSlot(EWeaponSlot::Secondary);
	if (!SecondaryWeapon || !SecondaryWeapon->HasAnyAmmo())
	{
		return;
	}

	SetCurrentWeaponSlot(EWeaponSlot::Secondary);
}

void AMyFPS_CPPAICharacter::OnRep_CurrentWeaponSlot(EWeaponSlot PreviousSlot)
{
	Super::OnRep_CurrentWeaponSlot(PreviousSlot);
	BindWeaponReadyDelegate();
}