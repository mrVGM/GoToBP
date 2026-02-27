// Snapshot Games

#pragma once

#include "CoreMinimal.h"
#include "ListenForLinkServer.generated.h"


UCLASS()
class GOTOBP_API UListenForLinkServer : public UObject
{
	GENERATED_BODY()
private:

	void* Pipe = nullptr;

	double ActivatedAt = -1.0;
	FDelegateHandle KeyDownHandle;
	FDelegateHandle MouseDownHandle;
	
	void StartListeningForInput();
	void StopListeningForInput() const;

	void OnMessageReceived(const FString& Message);
	
public:
	void Init();
	void Deinit();
	
};
