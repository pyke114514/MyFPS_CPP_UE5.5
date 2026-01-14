#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "MyFPS_CPPPlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnScoreChanged);

UCLASS(MinimalAPI)
class AMyFPS_CPPPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	AMyFPS_CPPPlayerState();

	/** 新增 API：加分 / 加击杀 / 加死亡 */
	void AddKill(bool bIsHeadShot);
	void AddDeath();

	/** 新增：重置所有统计（热身结束时调用） */
	void ResetStats();

	UFUNCTION(BlueprintPure, Category = "Stats")
	int32 GetKills() const { return Kills; }

	UFUNCTION(BlueprintPure, Category = "Stats")
	int32 GetDeaths() const { return Deaths; }

	UFUNCTION(BlueprintPure, Category = "Stats")
	float GetHeadShotRate() const { return HeadShotRate; }

	/** 分数变化广播（客户端 UI 可绑定） */
	UPROPERTY(BlueprintAssignable, Category = "Stats")
	FOnScoreChanged OnScoreChanged;

protected:
	UPROPERTY(ReplicatedUsing = OnRep_Stats, VisibleAnywhere, Category = "Stats")
	int32 Kills;

	UPROPERTY(ReplicatedUsing = OnRep_Stats, VisibleAnywhere, Category = "Stats")
	int32 Deaths;

	UPROPERTY(ReplicatedUsing = OnRep_Stats, VisibleAnywhere, Category = "Stats")
	int32 Kills_HeadShot;

	UPROPERTY(ReplicatedUsing = OnRep_Stats, VisibleAnywhere, Category = "Stats")
	float HeadShotRate;
	/** PlayerState 已自带 Score(float)，直接复用 */

	UFUNCTION()
	void OnRep_Stats();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 封装更新，确保广播只触发一次 */
	void BroadcastStatsChanged();
};