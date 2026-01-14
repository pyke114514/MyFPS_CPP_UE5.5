#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "MyFPS_CPPGameState.generated.h"

UENUM(BlueprintType)
enum class EMatchPhase : uint8
{
	Warmup,
	InProgress,
	Ended
};

USTRUCT(BlueprintType)
struct FPlayerScoreRow
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString PlayerName;

	UPROPERTY(BlueprintReadOnly)
	float Score;

	UPROPERTY(BlueprintReadOnly)
	int32 Kills;

	UPROPERTY(BlueprintReadOnly)
	int32 Deaths;

	UPROPERTY(BlueprintReadOnly)
	float HeadShotRate;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnScoreboardUpdated);

UCLASS(MinimalAPI)
class AMyFPS_CPPGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AMyFPS_CPPGameState();

	void SetMatchPhase(EMatchPhase NewPhase);
	void SetRemainingTime(float NewTime);

	UFUNCTION(BlueprintCallable, Category = "Match")
	void RefreshScoreboard();

	UFUNCTION(BlueprintPure, Category = "Match")
	EMatchPhase GetMatchPhase() const { return MatchPhase; }

	UFUNCTION(BlueprintPure, Category = "Match")
	float GetRemainingTime() const { return RemainingTime; }

	/** 新增：直接获取缓存（客户端 HUD 推荐使用） */
	UFUNCTION(BlueprintPure, Category = "Match")
	const TArray<FPlayerScoreRow>& GetCachedScoreboard() const { return CachedScoreboard; }

	UPROPERTY(BlueprintAssignable, Category = "Match|Scoreboard")
	FOnScoreboardUpdated OnScoreboardUpdated;

	UPROPERTY(BlueprintReadOnly, Replicated)
	TArray<FString> TopPlayerNames;

	/** 服务器调用：整理前三名 */
	void PrepareSettlementResults();

protected:
	UPROPERTY(ReplicatedUsing = OnRep_MatchPhase, VisibleAnywhere, Category = "Match")
	EMatchPhase MatchPhase;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Match")
	float RemainingTime;

	/** 缓存的排行榜 */
	UPROPERTY(ReplicatedUsing = OnRep_ScoreboardRows, VisibleAnywhere, Category = "Match")
	TArray<FPlayerScoreRow> CachedScoreboard;

	/** 蓝图实现 -> 真正创建/显示结算 UI */
	UFUNCTION(BlueprintImplementableEvent, Category = "Settlement")
	void ShowSettlementUI(const TArray<FString>& OrderedPlayerNames);

	UFUNCTION()
	void OnRep_MatchPhase();

	UFUNCTION()
	void OnRep_ScoreboardRows();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};