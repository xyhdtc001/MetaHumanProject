// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MetaHumanPlayerCharacter.generated.h"

UCLASS()
class AMetaHumanPlayerCharacter : public ACharacter
{
	GENERATED_BODY()
public:
	AMetaHumanPlayerCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	
	virtual void BeginPlay() override;
};
