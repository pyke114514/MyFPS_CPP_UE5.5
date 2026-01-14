#pragma once

#include "CoreMinimal.h"
#include "MyFPS_CPPCharacter.h"
#include "Perception/AIPerceptionTypes.h"
#include "MyFPS_CPPAICharacter.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UAISenseConfig_Damage;   // ★ 新增

/**
 * 纯服务器驱动的 AI 角色，复用玩家角色的武器/动画/属性。
 * 行为树或其他系统可通过公开函数控制开火、换弹、设置目标等。
 */

UCLASS(MinimalAPI)
class AMyFPS_CPPAICharacter : public AMyFPS_CPPCharacter
{
	GENERATED_BODY()

public:
	AMyFPS_CPPAICharacter();

	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_Controller() override;

	/** AI 不需要玩家输入映射，保持空实现即可 */
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override {}
	virtual void NotifyControllerChanged() override {}

	virtual void HandleActiveWeaponOutOfAmmo(AMyFPS_CPPWeaponActor* DepletedWeapon) override;

	/** 行为树或脚本可直接指定当前目标 */
	UFUNCTION(BlueprintCallable, Category = "AI|Targeting")
	void SetTargetActor(AActor* NewTarget);

	UFUNCTION(BlueprintCallable, Category = "AI|Targeting")
	AActor* GetTargetActor() const { return CurrentTargetActor; }

	/** 触发/停止射击（会复用武器类里的 RPC） */
	UFUNCTION(BlueprintCallable, Category = "AI|Combat")
	void StartFire();

	UFUNCTION(BlueprintCallable, Category = "AI|Combat")
	void StopFire();

	/** 请求换弹 */
	UFUNCTION(BlueprintCallable, Category = "AI|Combat")
	void RequestReload();

	/** 判断当前武器是否因换弹/锁定等无法射击 */
	UFUNCTION(BlueprintPure, Category = "AI|Combat")
	bool IsWeaponBusy() const;

	UFUNCTION(BlueprintCallable, Category = "AI|Awareness")
	void SetHasDetectedPlayer(bool bNewState);

	UFUNCTION(BlueprintCallable, Category = "AI|Awareness")
	bool GetHasDetectedPlayer() const { return bHasDetectedPlayer; }

	virtual float TakeDamage(float DamageAmount,
		FDamageEvent const& DamageEvent,
		AController* EventInstigator,
		AActor* DamageCauser) override;

protected:
	/** 感知组件：可在蓝图里进一步配置或禁用 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Perception")
	UAIPerceptionComponent* PerceptionComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Perception")
	UAISenseConfig_Sight* SightConfig;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Perception")
	UAISenseConfig_Damage* DamageConfig = nullptr;   // ★ 新增

	UPROPERTY(Replicated)
	bool bHasDetectedPlayer = false;

	/** 当前锁定的目标，供 BT Blackboard 或动画蓝图使用 */
	UPROPERTY(ReplicatedUsing = OnRep_CurrentTargetActor, BlueprintReadOnly, Category = "AI|Targeting")
	AActor* CurrentTargetActor;

	/** 是否正在持续射击（用于断线或离开感知自动停火） */
	UPROPERTY(BlueprintReadOnly, Category = "AI|Combat")
	bool bWantsToFire = false;

	/** 感知回调：默认按最近目标选取，可按需改写 */
	UFUNCTION()
	void HandlePerceptionUpdated(const TArray<AActor*>& UpdatedActors);

	UFUNCTION()
	void OnRep_CurrentTargetActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 辅助函数：AI 控制器就绪后尝试装备/激活默认武器 */
	void HandleControllerReady();
	void TryEquipDefaults();

	/** 根据感知到的 Actor 选出目标（仅示例逻辑） */
	AActor* ChooseBestTarget(const TArray<AActor*>& Candidates) const;

	bool IsBetterVisualTarget(AActor* Candidate, AActor* Current) const;

	/** 每次当前武器槽变化（含初始装备和切枪）都会调用 */
	virtual void OnRep_CurrentWeaponSlot(EWeaponSlot PreviousSlot) override;

	void BindWeaponReadyDelegate();

	UFUNCTION()
	void HandleWeaponReady();
};