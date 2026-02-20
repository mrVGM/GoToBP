// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FGoToBPModule : public IModuleInterface
{
private:

	bool RegisterURLHandler();
public:
	class UListenForLinkServer* Server = nullptr;

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

};
