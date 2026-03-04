// Snapshot Games

#pragma once

#include "CoreMinimal.h"
#include "ListenForLinkServer.generated.h"


UCLASS()
class GOTOBP_API UListenForLinkServer : public UObject
{
	GENERATED_BODY()
private:
	void OnMessageReceived(const FString& Message);
	
public:
	void Init();
	void Deinit();
	
};
