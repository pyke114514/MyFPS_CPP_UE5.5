#include "MyFPS_CPPGameState.h"
#include "Net/UnrealNetwork.h"
#include "MyFPS_CPPPlayerState.h"

AMyFPS_CPPGameState::AMyFPS_CPPGameState()
{
	MatchPhase = EMatchPhase::Warmup;
	RemainingTime = 0.f;
}

void AMyFPS_CPPGameState::SetMatchPhase(EMatchPhase NewPhase)
{
	if (HasAuthority() && MatchPhase != NewPhase)
	{
		MatchPhase = NewPhase;
		OnRep_MatchPhase();
	}
}

void AMyFPS_CPPGameState::SetRemainingTime(float NewTime)
{
	if (HasAuthority())
	{
		RemainingTime = NewTime;
	}
}

void AMyFPS_CPPGameState::RefreshScoreboard()
{
	if (!HasAuthority())
	{
		return;
	}

	CachedScoreboard.Empty();

	for (APlayerState* PS : PlayerArray)
	{
		if (const AMyFPS_CPPPlayerState* MyPS = Cast<AMyFPS_CPPPlayerState>(PS))
		{
			FPlayerScoreRow Row;
			Row.PlayerName = MyPS->GetPlayerName();
			Row.Score = MyPS->GetScore();
			Row.Kills = MyPS->GetKills();
			Row.Deaths = MyPS->GetDeaths();
			Row.HeadShotRate = MyPS->GetHeadShotRate();
			CachedScoreboard.Add(Row);
		}
	}

	CachedScoreboard.Sort([](const FPlayerScoreRow& A, const FPlayerScoreRow& B)
		{
			if (FMath::IsNearlyEqual(A.Score, B.Score))
			{
				return A.Kills > B.Kills;
			}
			return A.Score > B.Score;
		});

	OnRep_ScoreboardRows(); // 服务器自身也立即更新
}

void AMyFPS_CPPGameState::PrepareSettlementResults()
{
	if (!HasAuthority())
	{
		return;
	}

	// 确保 scoreboard 是最新（如果 HandleMatchEnd 里已调用，可去掉这条）
	if (CachedScoreboard.Num() == 0)
	{
		RefreshScoreboard();
	}

	TopPlayerNames.SetNum(3);

	for (int32 Rank = 0; Rank < 3; ++Rank)
	{
		if (CachedScoreboard.IsValidIndex(Rank))
		{
			TopPlayerNames[Rank] = CachedScoreboard[Rank].PlayerName;
		}
		else
		{
			TopPlayerNames[Rank] = TEXT("");
		}
	}

	// 服务器本地也立即触发
	OnRep_MatchPhase();
}

void AMyFPS_CPPGameState::OnRep_MatchPhase() {
	if (MatchPhase == EMatchPhase::Ended && TopPlayerNames.Num() >= 3)
	{
		ShowSettlementUI(TopPlayerNames);
	}
}

void AMyFPS_CPPGameState::OnRep_ScoreboardRows() {
	OnScoreboardUpdated.Broadcast();
}

void AMyFPS_CPPGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMyFPS_CPPGameState, MatchPhase);
	DOREPLIFETIME(AMyFPS_CPPGameState, RemainingTime);
	DOREPLIFETIME(AMyFPS_CPPGameState, CachedScoreboard);
	DOREPLIFETIME(AMyFPS_CPPGameState, TopPlayerNames);
}