#include "MyFPS_CPPGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Sockets.h"
#include "SocketSubsystem.h"

void UMyFPS_CPPGameInstance::Init()
{
	Super::Init();

	if (DesiredPlayerName.IsEmpty())
	{
		DesiredPlayerName = FPlatformProcess::ComputerName(); // 默认值
	}
}

void UMyFPS_CPPGameInstance::SetDesiredPlayerName(const FString& NewName)
{
	if (!NewName.IsEmpty())
	{
		DesiredPlayerName = NewName;
	}
}

void UMyFPS_CPPGameInstance::HostLANGame(const FString& MapName)
{
	if (UWorld* World = GetWorld())
	{
		const FString Options = TEXT("listen");
		UGameplayStatics::OpenLevel(World, FName(*MapName), true, Options);
	}

	LastHostIPAddress = QueryLocalIPAddress();
}

void UMyFPS_CPPGameInstance::JoinLANGame(const FString& Address)
{
	ClientSetHostIPAddress(Address);

	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			PC->ClientTravel(Address, ETravelType::TRAVEL_Absolute);
		}
	}
}

FString UMyFPS_CPPGameInstance::QueryLocalIPAddress() const
{
	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SocketSubsystem)
	{
		return TEXT("0.0.0.0");
	}

	bool bCanBind = false;
	TSharedRef<FInternetAddr> LocalAddr = SocketSubsystem->GetLocalHostAddr(*GLog, bCanBind);

	if (LocalAddr->IsValid())
	{
		return LocalAddr->ToString(false);
	}

	return TEXT("0.0.0.0");
}

void UMyFPS_CPPGameInstance::ClientSetHostIPAddress(const FString& Address)
{
	LastHostIPAddress = Address;
}