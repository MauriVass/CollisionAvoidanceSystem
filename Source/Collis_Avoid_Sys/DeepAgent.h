// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WheeledVehicle.h"
#include "DeepAgent.generated.h"

/**
 * 
 */
UCLASS()
class COLLIS_AVOID_SYS_API ADeepAgent : public AWheeledVehicle
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
	void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent);

public:
	virtual void Tick(float DeltaTime) override;

private:
	void GetInput();

	USceneComponent* SensorPosition;

	static const int NumberSensor = 32;
};
