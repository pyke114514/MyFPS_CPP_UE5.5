#include "MyFPS_CPPGameMode.h"
#include "MyFPS_CPPCharacter.h"
#include "MyFPS_CPPGameState.h"
#include "MyFPS_CPPPlayerState.h"
#include "MyFPS_CPPAIController.h"
#include "MyFPS_CPPWeaponActor.h"
#include "UObject/ConstructorHelpers.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerStart.h"

AMyFPS_CPPGameMode::AMyFPS_CPPGameMode()
{
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/MyAssets/Blueprints/BP_FirstPersonCharacter"));
	DefaultPawnClass = PlayerPawnClassFinder.Class;

	PlayerStateClass = AMyFPS_CPPPlayerState::StaticClass();
	GameStateClass = AMyFPS_CPPGameState::StaticClass();

	WarmupDuration = 30.f;       // 热身 30s
	MatchDuration = 600.f;       // 正式比赛 10min
	PostMatchDuration = 10.f;    // 结算 10s

	CurrentPhase = EMatchPhase::Warmup;
	CurrentPhaseTimeRemaining = WarmupDuration;

	PrimaryActorTick.bCanEverTick = true;
}

void AMyFPS_CPPGameMode::StartPlay()
{
	Super::StartPlay();
	EnterMatchPhase(EMatchPhase::Warmup);
}

void AMyFPS_CPPGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (CurrentPhase == EMatchPhase::Ended && PostMatchDuration <= 0.f)
	{
		return; // 如果不需要倒计时可提前返回
	}

	if (CurrentPhaseTimeRemaining > 0.f)
	{
		CurrentPhaseTimeRemaining = FMath::Max(0.f, CurrentPhaseTimeRemaining - DeltaSeconds);

		if (AMyFPS_CPPGameState* GS = GetGameState<AMyFPS_CPPGameState>())
		{
			GS->SetRemainingTime(CurrentPhaseTimeRemaining);
		}

		if (CurrentPhaseTimeRemaining <= 0.f)
		{
			AdvanceToNextPhase();
		}
	}
}

void AMyFPS_CPPGameMode::EnterMatchPhase(EMatchPhase NewPhase)
{
	CurrentPhase = NewPhase;

	switch (NewPhase)
	{
	case EMatchPhase::Warmup:
		CurrentPhaseTimeRemaining = WarmupDuration;
		SpawnOneAIForWarmup();
		break;
	case EMatchPhase::InProgress:
		CurrentPhaseTimeRemaining = MatchDuration;
		break;
	case EMatchPhase::Ended:
	default:
		CurrentPhaseTimeRemaining = PostMatchDuration;
		break;
	}

	if (AMyFPS_CPPGameState* GS = GetGameState<AMyFPS_CPPGameState>())
	{
		GS->SetMatchPhase(NewPhase);
		GS->SetRemainingTime(CurrentPhaseTimeRemaining);
		GS->RefreshScoreboard();
	}
}

void AMyFPS_CPPGameMode::AdvanceToNextPhase()
{
	const EMatchPhase FinishedPhase = CurrentPhase;

	switch (FinishedPhase)
	{
	case EMatchPhase::Warmup:
		EnterMatchPhase(EMatchPhase::InProgress);
		break;

	case EMatchPhase::InProgress:
		EnterMatchPhase(EMatchPhase::Ended);
		break;

	case EMatchPhase::Ended:
		// 已经在结算阶段，防止重复触发
		CurrentPhaseTimeRemaining = -1.f;
		break;
	}

	HandlePhaseEnded(FinishedPhase);
}

void AMyFPS_CPPGameMode::HandlePhaseEnded(EMatchPhase FinishedPhase)
{
	switch (FinishedPhase)
	{
	case EMatchPhase::Warmup:
		HandleMatchStart();
		break;
	case EMatchPhase::InProgress:
		HandleMatchEnd();
		break;
	case EMatchPhase::Ended:
		if (!HasAuthority())
		{
			return;
		}

		UGameplayStatics::OpenLevel(this, MainMenuMapName);
		break;
	default:
		break;
	}
}

void AMyFPS_CPPGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (AMyFPS_CPPGameState* GS = GetGameState<AMyFPS_CPPGameState>())
	{
		GS->RefreshScoreboard();
	}
}

void AMyFPS_CPPGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);

	if (AMyFPS_CPPGameState* GS = GetGameState<AMyFPS_CPPGameState>())
	{
		GS->RefreshScoreboard();
	}
}

void AMyFPS_CPPGameMode::HandleMatchStart()
{
	if (!HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// ------------------------------------------------------------
	// 0. 清除旧 AI：先记录，后统一销毁（避免迭代中直接 Destroy）
	// ------------------------------------------------------------
	TArray<AMyFPS_CPPAIController*> AIControllersToDestroy;
	for (TActorIterator<AMyFPS_CPPAIController> It(World); It; ++It)
	{
		AMyFPS_CPPAIController* AIController = *It;
		if (AIController && !AIController->IsPendingKillPending())
		{
			AIControllersToDestroy.Add(AIController);
		}
	}

	for (AMyFPS_CPPAIController* AIController : AIControllersToDestroy)
	{
		if (APawn* Pawn = AIController->GetPawn())
		{
			Pawn->Destroy();
		}
		AIController->Destroy();
	}

	// ------------------------------------------------------------
	// 1. 统计真人数量，计算需要补齐的 AI 数量
	// ------------------------------------------------------------
	int32 HumanPlayerCount = 0;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		if (PC && !PC->IsPendingKillPending())
		{
			++HumanPlayerCount;
		}
	}

	const int32 MissingCount = FMath::Max(0, DesiredPlayerCount - HumanPlayerCount);
	TArray<AMyFPS_CPPAIController*> SpawnedAIControllers;

	for (int32 i = 0; i < MissingCount; ++i)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		if (AMyFPS_CPPAIController* NewAI = World->SpawnActor<AMyFPS_CPPAIController>(BotControllerClass, SpawnParams))
		{
			const FString UniqueName = AcquireUniqueAIName();
			NewAI->ServerRequestSetPlayerName(UniqueName);
			SpawnedAIControllers.Add(NewAI);
		}
	}

	// ------------------------------------------------------------
	// 2. 重置所有玩家统计 + 清空全地图武器实例
	// ------------------------------------------------------------
	if (AMyFPS_CPPGameState* GS = GetGameState<AMyFPS_CPPGameState>())
	{
		for (APlayerState* PS : GS->PlayerArray)
		{
			if (AMyFPS_CPPPlayerState* MyPS = Cast<AMyFPS_CPPPlayerState>(PS))
			{
				MyPS->ResetStats();
			}
		}
	}

	TArray<AActor*> WeaponsToDestroy;
	for (TActorIterator<AMyFPS_CPPWeaponActor> It(World); It; ++It)
	{
		if (AActor* Weapon = *It)
		{
			WeaponsToDestroy.Add(Weapon);
		}
	}

	for (AActor* Weapon : WeaponsToDestroy)
	{
		Weapon->Destroy();
	}

	// ------------------------------------------------------------
	// 3. 重新生成所有控制器（真人 + 新 AI）
	// ------------------------------------------------------------
	TArray<AController*> ControllersToRestart;

	for (FConstControllerIterator It = World->GetControllerIterator(); It; ++It)
	{
		AController* Controller = It->Get();
		if (Controller && !Controller->IsPendingKillPending())
		{
			ControllersToRestart.Add(Controller);
		}
	}

	for (AController* Controller : ControllersToRestart)
	{
		if (APawn* ExistingPawn = Controller->GetPawn())
		{
			ExistingPawn->Destroy();
		}

		RestartPlayer(Controller);
	}

	// ------------------------------------------------------------
	// 4. 刷新排行榜
	// ------------------------------------------------------------
	if (AMyFPS_CPPGameState* GS = GetGameState<AMyFPS_CPPGameState>())
	{
		GS->RefreshScoreboard();
	}
}

void AMyFPS_CPPGameMode::HandleMatchEnd()
{
	// 这里是正式比赛结束后的结算入口
	if (AMyFPS_CPPGameState* GS = GetGameState<AMyFPS_CPPGameState>())
	{
		GS->RefreshScoreboard();        // 仍可多处调用，单纯更新数据
		GS->PrepareSettlementResults(); // 只在结算时调用，写入 TopPlayerNames
		GS->SetMatchPhase(EMatchPhase::Ended); // 标记正式进入结算阶段
	}
}

void AMyFPS_CPPGameMode::NotifyPlayerKilled(AMyFPS_CPPCharacter* Victim, AController* KillerController, bool bIsHeadShot)
{
	if (!HasAuthority() || !Victim)
	{
		return;
	}

	if (AMyFPS_CPPPlayerState* VictimPS = Victim->GetPlayerState<AMyFPS_CPPPlayerState>())
	{
		VictimPS->AddDeath();
	}

	if (KillerController && KillerController != Victim->GetController())
	{
		if (AMyFPS_CPPCharacter* HumanPlayer = KillerController->GetPawn<AMyFPS_CPPCharacter>()) {
			HumanPlayer->ClientOnKillEvent();
		}
		if (AMyFPS_CPPPlayerState* KillerPS = KillerController->GetPlayerState<AMyFPS_CPPPlayerState>())
		{
			KillerPS->AddKill(bIsHeadShot);
		}
	}

	if (AMyFPS_CPPGameState* GS = GetGameState<AMyFPS_CPPGameState>())
	{
		GS->RefreshScoreboard();
	}
}

void AMyFPS_CPPGameMode::RestartPlayer(AController* NewPlayer)
{
	if (!NewPlayer || !HasAuthority())
	{
		return;
	}

	if (APlayerController* PC = Cast<APlayerController>(NewPlayer))
	{
		PC->StartSpot = nullptr;
		Super::RestartPlayer(PC);
		return;
	}

	if (AMyFPS_CPPAIController* AICon = Cast<AMyFPS_CPPAIController>(NewPlayer))
	{
		AActor* SpawnPoint = ChoosePlayerStart(AICon);
		FTransform SpawnTransform = SpawnPoint
			? SpawnPoint->GetActorTransform()
			: FTransform(FRotator::ZeroRotator, FVector::ZeroVector);

		TSubclassOf<APawn> PawnClass = AICon->DesiredPawnClass;

		FActorSpawnParameters SpawnInfo;
		SpawnInfo.Owner = AICon;
		SpawnInfo.Instigator = nullptr;
		SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		APawn* NewAIPawn = GetWorld()->SpawnActor<APawn>(PawnClass, SpawnTransform, SpawnInfo);
		AICon->Possess(NewAIPawn);
		return;
	}
}

AActor* AMyFPS_CPPGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	if (AActor* RandomStart = ChooseRandomPlayerStart(Player))
	{
		return RandomStart;
	}

	return Super::ChoosePlayerStart_Implementation(Player);
}

AActor* AMyFPS_CPPGameMode::ChooseRandomPlayerStart(AController* Player) const
{
	if (!GetWorld())
	{
		return nullptr;
	}

	TArray<AActor*> PlayerStarts;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), APlayerStart::StaticClass(), PlayerStarts);

	if (PlayerStarts.Num() == 0)
	{
		return nullptr;
	}

	const int32 RandomIndex = FMath::RandRange(0, PlayerStarts.Num() - 1);
	return PlayerStarts[RandomIndex];
}

FString AMyFPS_CPPGameMode::AcquireUniqueAIName()
{
	TArray<FString> Candidates;
	Candidates.Reserve(AIPresetNames.Num());

	for (const FString& Name : AIPresetNames)
	{
		if (!UsedAINames.Contains(Name))
		{
			Candidates.Add(Name);
		}
	}

	if (Candidates.Num() == 0)
	{
		// 名称池耗尽，可选择回退策略：加序号、复用、或返回默认
		return FString::Printf(TEXT("AI_%d"), UsedAINames.Num() + 1);
	}

	const int32 Index = FMath::RandRange(0, Candidates.Num() - 1);
	const FString ChosenName = Candidates[Index];
	UsedAINames.Add(ChosenName);
	return ChosenName;
}

void AMyFPS_CPPGameMode::SpawnOneAIForWarmup(){
	UWorld* World = GetWorld();

	if (!World)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	if (AMyFPS_CPPAIController* NewAI = World->SpawnActor<AMyFPS_CPPAIController>(BotControllerClass, SpawnParams))
	{
		const FString UniqueName = AcquireUniqueAIName();
		NewAI->ServerRequestSetPlayerName(UniqueName);

		RestartPlayer(NewAI);
	}
}