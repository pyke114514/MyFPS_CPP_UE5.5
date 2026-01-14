// MyFPS_CPPAIController.cpp

#include "MyFPS_CPPAIController.h"
#include "MyFPS_CPPGameState.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"

AMyFPS_CPPAIController::AMyFPS_CPPAIController()
{
	bWantsPlayerState = true;
	ControlRotationInterpSpeed = 6.f;
	bSmoothWhenNoFocus = false;
}

void AMyFPS_CPPAIController::BeginPlay()
{
	Super::BeginPlay();
}

void AMyFPS_CPPAIController::UpdateControlRotation(float DeltaTime, bool bUpdatePawn)
{
	APawn* MyPawn = GetPawn();
	if (!MyPawn)
	{
		return;
	}
	const AActor* FocusActor = GetFocusActor();
	const FVector FocusLocation = GetFocalPoint();

	if (!FocusActor && FocusLocation.IsNearlyZero() && !bSmoothWhenNoFocus)
	{
		return;
	}

	const FVector PawnLocation = MyPawn->GetActorLocation();
	FVector LookAtLocation = FocusLocation;

	if (FocusActor)
	{
		LookAtLocation = FocusActor->GetActorLocation();
	}

	if (LookAtLocation.IsNearlyZero())
	{
		return;
	}

	const FRotator DesiredRotation = (LookAtLocation - PawnLocation).Rotation();
	const FRotator CurrentRotation = GetControlRotation();

	const FRotator SmoothedRotation = FMath::RInterpTo(
		CurrentRotation,
		DesiredRotation,
		DeltaTime,
		ControlRotationInterpSpeed
	);

	SetControlRotation(SmoothedRotation);

	if (bUpdatePawn)
	{
		MyPawn->FaceRotation(SmoothedRotation, DeltaTime);
	}
}


void AMyFPS_CPPAIController::ServerRequestSetPlayerName_Implementation(const FString& InPlayerName)
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