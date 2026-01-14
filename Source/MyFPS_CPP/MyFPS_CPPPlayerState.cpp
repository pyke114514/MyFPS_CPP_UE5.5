#include "MyFPS_CPPPlayerState.h"
#include "Net/UnrealNetwork.h"

AMyFPS_CPPPlayerState::AMyFPS_CPPPlayerState()
{
	Kills = 0;
	Kills_HeadShot = 0;
	Deaths = 0;
	SetScore(0.f);
	HeadShotRate = 0.f;
}

void AMyFPS_CPPPlayerState::AddKill(bool bIsHeadShot)
{
	if (HasAuthority())
	{
		++Kills;
		if (bIsHeadShot) {
			++Kills_HeadShot;
		}
		if (Kills == 0) {
			HeadShotRate = 0;
		}
		else {
			HeadShotRate = static_cast<float>(Kills_HeadShot) / Kills;
		}
		float ScoreDelta = bIsHeadShot ? 2.f : 1.f;
		SetScore(GetScore() + ScoreDelta);
		BroadcastStatsChanged();
	}
}

void AMyFPS_CPPPlayerState::AddDeath()
{
	if (HasAuthority())
	{
		++Deaths;
		BroadcastStatsChanged();
	}
}

void AMyFPS_CPPPlayerState::ResetStats()
{
	if (!HasAuthority())
	{
		return;
	}

	Kills = 0;
	Kills_HeadShot = 0;
	Deaths = 0;
	HeadShotRate = 0.f;
	SetScore(0.f);

	BroadcastStatsChanged();
}

void AMyFPS_CPPPlayerState::OnRep_Stats()
{
	BroadcastStatsChanged();
}

void AMyFPS_CPPPlayerState::BroadcastStatsChanged()
{
	OnScoreChanged.Broadcast();
}

void AMyFPS_CPPPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMyFPS_CPPPlayerState, Kills);
	DOREPLIFETIME(AMyFPS_CPPPlayerState, Kills_HeadShot);
	DOREPLIFETIME(AMyFPS_CPPPlayerState, Deaths);
	DOREPLIFETIME(AMyFPS_CPPPlayerState, HeadShotRate);
}