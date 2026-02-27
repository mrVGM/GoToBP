// Snapshot Games

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "ListenForLinkServer.generated.h"

struct FLinkData
{
	FString Graph;
	FVector2f Location;
	
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

	double ActivatedAt = -1.0;
	FDelegateHandle KeyDownHandle;
	FDelegateHandle MouseDownHandle;
	
	void StartListeningForInput();
	void StopListeningForInput() const;

	static bool EnableLinks(const FString& ProtocolName, const FString& Command);
	static FString GetScriptPath();
	
	void OnMessageReceived(const FString& Message);

public:
	void Init();
	void Deinit();
	
};
