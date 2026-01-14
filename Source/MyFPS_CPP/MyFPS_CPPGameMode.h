#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MyFPS_CPPGameState.h"
#include "MyFPS_CPPAIController.h"
#include "MyFPS_CPPGameMode.generated.h"

class AMyFPS_CPPPlayerState;
class AMyFPS_CPPGameState;
class AMyFPS_CPPCharacter;

UCLASS(MinimalAPI)
class AMyFPS_CPPGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMyFPS_CPPGameMode();

	virtual void StartPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	void NotifyPlayerKilled(AMyFPS_CPPCharacter* Victim, AController* KillerController, bool bIsHeadShot);

protected:
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	UPROPERTY(EditDefaultsOnly, Category = "Maps")
	FName MainMenuMapName = TEXT("Tittle");

	/** 热身/比赛/结算阶段时长（秒） */
	UPROPERTY(EditDefaultsOnly, Category = "Match")
	float WarmupDuration;

	UPROPERTY(EditDefaultsOnly, Category = "Match")
	float MatchDuration;

	UPROPERTY(EditDefaultsOnly, Category = "Match")
	float PostMatchDuration;

	UPROPERTY(EditDefaultsOnly, Category = "Match")
	int32 DesiredPlayerCount = 8;

	UPROPERTY(EditDefaultsOnly, Category = "AI")
	TArray<FString> AIPresetNames;

	UPROPERTY(EditDefaultsOnly, Category = "AI")
	TSubclassOf<AMyFPS_CPPAIController> BotControllerClass;

private:
	void EnterMatchPhase(EMatchPhase NewPhase);
	void AdvanceToNextPhase();
	void HandlePhaseEnded(EMatchPhase FinishedPhase);
	void SpawnOneAIForWarmup();
	void HandleMatchStart();
	void HandleMatchEnd();

	UPROPERTY()
	TSet<FString> UsedAINames;

	FString AcquireUniqueAIName();

	AActor* ChooseRandomPlayerStart(AController* Player) const;

	EMatchPhase CurrentPhase;
	float CurrentPhaseTimeRemaining;
};