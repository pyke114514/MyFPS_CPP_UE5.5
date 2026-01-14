#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MyFPS_CPPPlayerController.generated.h"

UCLASS(MinimalAPI)
class AMyFPS_CPPPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;

protected:
	UFUNCTION(Server, Reliable)
	void ServerRequestSetPlayerName(const FString& InPlayerName);
};