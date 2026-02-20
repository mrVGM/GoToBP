// Snapshot Games

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "ListenForLinkServer.generated.h"

struct FLinkData
{
	FString Graph;
	FVector2D Location;
	
	friend FArchive& operator<<(FArchive& Ar, FLinkData& Msg)
	{
		Ar << Msg.Graph;
		Ar << Msg.Location;
		return Ar;
	}
};

UCLASS()
class GOTOBP_API UListenForLinkServer : public UObject
{
	GENERATED_BODY()
private:
	
	bool Activated = false;
	FDelegateHandle KeyDownHandle;
	FDelegateHandle MouseDownHandle;
	
	void StartListeningForInput();
	void StopListeningForInput();
	
	bool EnableLinks(const FString& ProtocolName, const FString& ApplicationPath);
	FString GetScriptPath();
	
	void OnMessageReceived(const FString& Message);

public:
	void Init();
	void Deinit();
	
};
