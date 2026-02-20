// Copyright Epic Games, Inc. All Rights Reserved.

#include "GoToBP.h"

#include <string>

#include "ListenForLinkServer.h"
#include "Windows/MinWindows.h"

#define LOCTEXT_NAMESPACE "FGoToBPModule"


void FGoToBPModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

	Server = NewObject<UListenForLinkServer>();
	Server->AddToRoot();
	Server->Init();

}

void FGoToBPModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	Server->Deinit();
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FGoToBPModule, GoToBP)