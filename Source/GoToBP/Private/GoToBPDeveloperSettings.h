// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GoToBPDeveloperSettings.generated.h"

/**
 * 
 */
UCLASS(Config=EditorPerProjectUserSettings)
class UGoToBP : public UDeveloperSettings
{
	GENERATED_BODY()
	
public:
	
	UPROPERTY(Config, EditDefaultsOnly, Category=GoToBP)
	FKey ActivationKey = FKey("L");
	
	static FKey GetActivationKey();
};
