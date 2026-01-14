#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "MyFPS_CPPGameInstance.generated.h"

UCLASS(MinimalAPI)
class UMyFPS_CPPGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;

	UFUNCTION(BlueprintCallable, Category = "Player")
	void SetDesiredPlayerName(const FString& NewName);

	UFUNCTION(BlueprintPure, Category = "Player")
	FString GetDesiredPlayerName() const { return DesiredPlayerName; }

	UFUNCTION(BlueprintCallable, Category = "LAN")
	void HostLANGame(const FString& MapName = TEXT("YourMapName"));

	UFUNCTION(BlueprintCallable, Category = "LAN")
	void JoinLANGame(const FString& Address);

	/** 返回最近一次 Host 成功时记录的本机 IP（无端口，默认 7777）。 */
	UFUNCTION(BlueprintPure, Category = "LAN")
	FString GetHostIPAddress() const { return LastHostIPAddress; }

protected:
	FString QueryLocalIPAddress() const;
	void ClientSetHostIPAddress(const FString& Address);

	UPROPERTY()
	FString DesiredPlayerName;

	FString LastHostIPAddress;
};