// MyFPS_CPPAIController.h

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "MyFPS_CPPAIController.generated.h"

/**
 * 基于插值的 AI 控制器，让 SetFocus 旋转更平滑
 */
UCLASS(MinimalAPI)
class AMyFPS_CPPAIController : public AAIController
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;

	AMyFPS_CPPAIController();
	UPROPERTY(EditAnywhere, Category = "Spawn")
	TSubclassOf<APawn> DesiredPawnClass;

	UFUNCTION(Server, Reliable)
	void ServerRequestSetPlayerName(const FString& InPlayerName);

protected:
	/** 控制器旋转插值速度（越小越慢） */
	UPROPERTY(EditAnywhere, Category = "Aiming")
	float ControlRotationInterpSpeed;

	/** 是否在没有 Focus 时也平滑对准 */
	UPROPERTY(EditAnywhere, Category = "Aiming")
	bool bSmoothWhenNoFocus;

	virtual void UpdateControlRotation(float DeltaTime, bool bUpdatePawn) override;
};