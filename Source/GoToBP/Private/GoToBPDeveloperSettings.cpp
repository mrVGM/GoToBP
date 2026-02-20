// Fill out your copyright notice in the Description page of Project Settings.


#include "GoToBPDeveloperSettings.h"

FKey UGoToBP::GetActivationKey()
{
	const UGoToBP* settings = GetDefault<UGoToBP>();
	return settings->ActivationKey;
}
