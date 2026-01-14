#include "MyFPS_CPPPlayerController.h"
#include "MyFPS_CPPGameInstance.h"
#include "MyFPS_CPPGameState.h"
#include "GameFramework/PlayerState.h"

void AMyFPS_CPPPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalController())
	{
		if (const UMyFPS_CPPGameInstance* GI = GetGameInstance<UMyFPS_CPPGameInstance>())
		{
			const FString DesiredName = GI->GetDesiredPlayerName();
			if (!DesiredName.IsEmpty())
			{
				ServerRequestSetPlayerName(DesiredName);
			}
		}
	}
}

void AMyFPS_CPPPlayerController::ServerRequestSetPlayerName_Implementation(const FString& InPlayerName)
{
	if (PlayerState && !InPlayerName.IsEmpty())
	{
		PlayerState->SetPlayerName(InPlayerName);
	}

	if (AMyFPS_CPPGameState* GS = GetWorld() ? GetWorld()->GetGameState<AMyFPS_CPPGameState>() : nullptr)
	{
		GS->RefreshScoreboard();
	}
}