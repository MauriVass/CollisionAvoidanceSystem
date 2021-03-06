// Fill out your copyright notice in the Description page of Project Settings.

//Collapse all CTRL+M CTRL+O

#include "DrawDebugHelpers.h"
#include <Collis_Avoid_Sys/Experience.h>
#include "Client.h"
#include "DeepAgent.h"
#include "WheeledVehicleMovementComponent.h"
#include "Misc/FileHelper.h"

void ADeepAgent::BeginPlay() {
	Super::BeginPlay();

	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	GetWorld()->GetFirstPlayerController()->bShowMouseCursor = true;

	//Get the component responsible for the sensors' starting position
	TArray<USceneComponent*> Childrens;
	this->GetMesh()->GetChildrenComponents(true, Childrens);
	if (Childrens.Num() > 0)
		ADeepAgent::SensorPosition = Cast<UStaticMeshComponent>(Childrens[0]);

	ADeepAgent::Client = GetWorld()->SpawnActor<AClient>();
	ADeepAgent::Client->SetActorLabel("Client");
	ADeepAgent::Client->SetDeepAgent(this);

	ADeepAgent::MovementComponent = this->GetVehicleMovementComponent();
	ADeepAgent::initialTransform = this->GetTransform();

	ADeepAgent::ManualControll = false;

	ADeepAgent::IsTraining = true;
	ADeepAgent::IsStateSpaceDescrete = true;
	ADeepAgent::Clockwise = true;
	ADeepAgent::Episode = 0;
	ADeepAgent::NumberActions = 5;
	ADeepAgent::Epsilon = 1.0;
	ADeepAgent::EpsilonDecay = 8.2 * FMath::Pow(10,-5);
	ADeepAgent::MinEpsilon = 0.03;
	ADeepAgent::MaxNumberSteps = 2000;
	ADeepAgent::NumberFitSteps = 1;
	ADeepAgent::AngleExtension = 170;
	ADeepAgent::NegativeReward = -5;

	//0-> Deep Q-Network
	//1-> Double Deep Q-Network
	//2-> Dueling Deep Q-Network
	ADeepAgent::ModelSpecification = 1;

	//Add some delay so that the server can elaborate and reply to the client
	ADeepAgent::timer = 0;
	ADeepAgent::TickTime = 0.028; 
	if(ADeepAgent::ModelSpecification == 2)
		ADeepAgent::TickTime = 0.03;

	//Prioritize the negative experiences during training
	bool prioritize = false;
	ADeepAgent::Client->SendMetadata(ADeepAgent::NumberSensor, ADeepAgent::NumberActions, ADeepAgent::NegativeReward, ADeepAgent::ModelSpecification, prioritize);
	ADeepAgent::RestartGame();
	UE_LOG(LogTemp, Error, TEXT("Begin"));

	//Add you path where to save the rewards .csv file
	ADeepAgent::SaveDirectory = FString("add here");
	ADeepAgent::FileName = FString("run.rewards_");
	ADeepAgent::FileName.AppendInt(ADeepAgent::ModelSpecification);
	ADeepAgent::FileName.Append("_.csv");
	ADeepAgent::WriteToFile(0,0,0,0,false,false,false);

	ADeepAgent::IsGameStarting = true;
	ADeepAgent::ToggleIsGameStarting();
}

void ADeepAgent::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);

	if (!ADeepAgent::ManualControll) {
		ADeepAgent::timer += DeltaTime;
		if (ADeepAgent::timer > ADeepAgent::TickTime) {
			//if (ADeepAgent::Episode >= 150)
				//ADeepAgent::IsGameStarting = false;

			ADeepAgent::Step();
			ADeepAgent::timer = 0;
		}
		ADeepAgent::PerformAction(ADeepAgent::Action);
	}
}

void ADeepAgent::SetAction(int action)
{
	ADeepAgent::Action = action;
}
void ADeepAgent::SetThrottleAction(float throttle)
{
	ADeepAgent::ThrottleAction = throttle;
}
void ADeepAgent::SetSteerAction(float steer)
{
	ADeepAgent::SteerAction = steer;
}
void ADeepAgent::SetTickTime(float tickTime)
{
	ADeepAgent::TickTime = tickTime;
}
void ADeepAgent::SetManualControll(bool manualControll)
{
	ADeepAgent::ManualControll = manualControll;
}
void ADeepAgent::SetIsGameEnded(bool value)
{
	ADeepAgent::IsGameEnded = value;
}

void ADeepAgent::ToggleIsTraining()
{
	ADeepAgent::IsTraining = !ADeepAgent::IsTraining;
}
void ADeepAgent::ToggleIsGameStarting()
{
	ADeepAgent::IsGameStarting = !ADeepAgent::IsGameStarting;
	GetWorld()->GetFirstPlayerController()->SetPause(!ADeepAgent::IsGameStarting);
}

TArray<float> ADeepAgent::GetInput() {
	TArray<float> currentSate;

	FVector Start;
	FRotator Rot;

	Start = ADeepAgent::SensorPosition->GetComponentLocation();
	Rot = ADeepAgent::SensorPosition->GetComponentRotation();
	ADeepAgent::totalDistance = 0;

	//Array of rays
	TArray<FHitResult, TFixedAllocator<ADeepAgent::NumberSensor>> Hit;
	FHitResult hit;
	Hit.Init(hit, ADeepAgent::NumberSensor);
	FCollisionQueryParams FParams;

	float maxDistance = 1000;
	ADeepAgent::TargetVector = FVector::ZeroVector;
	ADeepAgent::TargetVector_all = FVector::ZeroVector;
	for (int i = 0; i < ADeepAgent::NumberSensor; i++)
	{
		FRotator angle = FRotator::ZeroRotator;
		//Rays are equally spaced over a ADeepAgent::AngleExtension range
		angle.Yaw = -ADeepAgent::AngleExtension / 2 + ADeepAgent::AngleExtension / (ADeepAgent::NumberSensor - 1) * i;
		FVector direction = (Rot + angle).Vector() * maxDistance;
		FVector end = Start + direction;
		GetWorld()->LineTraceSingleByChannel(Hit[i], Start, end, ECollisionChannel::ECC_WorldStatic, FParams);


		AActor* ActorHit = Hit[i].GetActor();

		float lifeTime = ADeepAgent::TickTime*3;
		float value = Hit[i].Distance > 0 ? Hit[i].Distance / maxDistance : 1;
		if (ActorHit) {
			DrawDebugLine(GetWorld(), Start, end, FColor::Red, false, lifeTime, 0, 5);
			if (ADeepAgent::IsStateSpaceDescrete)
				value = 0;

			ADeepAgent::TargetVector_all += direction * Hit[i].Distance / maxDistance;
		}
		else {
			DrawDebugLine(GetWorld(), Start, end, FColor::Green, false, lifeTime, 0, 5);
			if (ADeepAgent::IsStateSpaceDescrete)
				value = 1;
			ADeepAgent::TargetVector += direction;
			ADeepAgent::TargetVector_all += direction * 2;
		}
		currentSate.Add(value);
	}

	if (Hit[0].Distance == 0) {
		ADeepAgent::MinDistance = Hit[ADeepAgent::NumberSensor - 1].Distance;
	}
	else if (Hit[ADeepAgent::NumberSensor - 1].Distance == 0) {
		ADeepAgent::MinDistance = Hit[0].Distance;
	}
	else
		ADeepAgent::MinDistance = FMath::Min(Hit[0].Distance, Hit[ADeepAgent::NumberSensor-1].Distance);

	ADeepAgent::MinDistance = (ADeepAgent::MinDistance - maxDistance / 2) / maxDistance;

	return currentSate;
}
float ADeepAgent::GetEpsilon()
{
	return ADeepAgent::Epsilon;
}
float ADeepAgent::GetReward()
{
	return ADeepAgent::Reward;
}
float ADeepAgent::GetCumulativeReward()
{
	return ADeepAgent::CumulativeReward;
}
int ADeepAgent::GetNumberSteps() {
	return ADeepAgent::NumberSteps;
}
int ADeepAgent::GetEpisode() {
	return ADeepAgent::Episode;
}
bool ADeepAgent::GetIsTraining()
{
	return ADeepAgent::IsTraining;
}
float ADeepAgent::GetTickTime()
{
	return ADeepAgent::TickTime;
}
bool ADeepAgent::GetManualControll()
{
	return ADeepAgent::ManualControll;
}
float ADeepAgent::GetAverageSpeed()
{
	return ADeepAgent::AverageSpeed / ADeepAgent::NumberSteps;
}

float ADeepAgent::GetAngle()
{
	return ADeepAgent::Angle;
}

void ADeepAgent::RewardFunction(TArray<float> currentState)
{
	//Different reward fuctions tried

	//float ADeepAgent::Reward = 0.1;

	//##### Reward by magnitude velocity #####
	//ADeepAgent::Reward = ADeepAgent::GetMesh()->GetPhysicsLinearVelocity().Size()/2000.0;

	//##### Reward by number of green rays (not bad) #####
	//ADeepAgent::Reward = -1;
	//for (int i = 0; i < currentState.Num(); i++)
	//{
	//	if (currentState[i] == 1)
	//	{
	//		ADeepAgent::Reward++;
	//	}
	//}
	//ADeepAgent::Reward = ADeepAgent::Reward / 10;
	////multiplied by velocity (punish if the velocity is under a given threshold)
	//ADeepAgent::Reward *= (ADeepAgent::GetMesh()->GetPhysicsLinearVelocity().Size() - 90) / 1000;

	//##### No ADeepAgent::Reward (only negative ADeepAgent::Reward when hitting a wall) #####
	//float ADeepAgent::Reward = 0;
	
	//##### Distance from walls Reward #####
	//float ADeepAgent::Reward = (ADeepAgent::totalDistance) /10000.0;

	//##### Sum of green rays #####
	FVector start = ADeepAgent::SensorPosition->GetComponentLocation();
	FVector carDirection = ADeepAgent::SensorPosition->GetForwardVector();

	DrawDebugLine(GetWorld(), start, start + ADeepAgent::TargetVector * 2000, FColor::Orange, false, ADeepAgent::TickTime * 3, 0, 5);
	//DrawDebugLine(GetWorld(), start, start + ADeepAgent::TargetVector_all * 2000, FColor::White, false, ADeepAgent::TickTime*3, 0, 5);
	DrawDebugLine(GetWorld(), start, start + carDirection * 2000, FColor::Blue, false, ADeepAgent::TickTime*3, 0, 5);

	/*float rewardDir = (FVector::DotProduct(ADeepAgent::TargetVector, carDirection) - 0.2) / 3;
	float rewardSpeed = (ADeepAgent::GetMesh()->GetPhysicsLinearVelocity().Size() - 200) / 1000;
	ADeepAgent::Reward = rewardDir + rewardSpeed;*/
	//UE_LOG(LogTemp, Error, TEXT("%f %f"), rewardDir, rewardSpeed);

	//###### Angle between direction and target ######
	float dot = (FVector::DotProduct(ADeepAgent::TargetVector, carDirection)) / (ADeepAgent::TargetVector.Size() * carDirection.Size());
	float cos_1 = FMath::Acos(dot);
	ADeepAgent::Angle = FMath::RadiansToDegrees(cos_1);
	float sign = FVector::CrossProduct(carDirection , ADeepAgent::TargetVector).Z; //// ADeepAgent::TargetVector.Size()
	ADeepAgent::Angle = sign > 0 ? ADeepAgent::Angle : -ADeepAgent::Angle;

	//The agent is rewarded more when it drives to the safest direction
	float rewardAngle = -1;
	if ((ADeepAgent::Angle > 0 && ADeepAgent::Action > 2) ||
		(ADeepAgent::Angle < 0 && ADeepAgent::Action < 2) ||
		(FMath::Abs(ADeepAgent::Angle) < 20 && ADeepAgent::Action == 2))
	{
		for (int i = 0; i < currentState.Num(); i++)
		{
			if (currentState[i] == 1)
				rewardAngle += 1;

		}
	}
	rewardAngle /= 10;
	//The reward is proportional to the speed too
	// -250 and /1000 are encouraging factors
	float rewardSpeed = (ADeepAgent::GetMesh()->GetPhysicsLinearVelocity().Size() - 250) / 1000;
	
	float rewardDistance = 0; 
	ADeepAgent::Reward = rewardAngle + rewardSpeed + rewardDistance;

	if (ADeepAgent::IsGameEnded) {
		ADeepAgent::Reward = ADeepAgent::NegativeReward;
	}
}

void ADeepAgent::PerformAction(int action)
{
	//Different actions space where tested too

	//1) 3 actions
	/*switch (ADeepAgent::Action)
	{
	case 0:
		ADeepAgent::MovementComponent->SetSteeringInput(-1);
		ADeepAgent::MovementComponent->SetThrottleInput(0.4);
		break;
	case 1:
		ADeepAgent::MovementComponent->SetSteeringInput(1);
		break;
	case 2:
		ADeepAgent::MovementComponent->SetSteeringInput(1);
		ADeepAgent::MovementComponent->SetThrottleInput(0.4);
		break;
	default:
		break;
	}*/

	//2) 2 actions
	/*ADeepAgent::MovementComponent->SetThrottleInput(0.7);
	switch (ADeepAgent::Action)
	{
	case 0:
		ADeepAgent::MovementComponent->SetSteeringInput(1);
		break;
	case 1:
		ADeepAgent::MovementComponent->SetSteeringInput(-1);
		break;
	default:
		break;
	}*/

	//3) 5 Actions
	/*UE_LOG(LogTemp, Error, TEXT("%f %f %f"),
		(ADeepAgent::MovementComponent->Velocity.Size()),
		ADeepAgent::MovementComponent->GetMaxSpeed(),
		ADeepAgent::MovementComponent->GetForwardSpeed());*/
	switch (ADeepAgent::Action)
	{
	case 0:
		ADeepAgent::MovementComponent->SetThrottleInput(0.2);	//Slightly accelerate
		ADeepAgent::MovementComponent->SetSteeringInput(-1);	//Steer to the left
		break;
	case 1:
		ADeepAgent::MovementComponent->SetThrottleInput(0.5);	//Accelerate
		ADeepAgent::MovementComponent->SetSteeringInput(-0.5);	//Slightly steer to the left
		break;
	case 2:
		ADeepAgent::MovementComponent->SetThrottleInput(1);		//Accelerate
		break;
	case 3:
		ADeepAgent::MovementComponent->SetThrottleInput(0.5);	//accelerate
		ADeepAgent::MovementComponent->SetSteeringInput(0.5);	//Slightly steer to the right
		break;
	case 4:
		ADeepAgent::MovementComponent->SetThrottleInput(0.2);	//Slightly accelerate
		ADeepAgent::MovementComponent->SetSteeringInput(1);		//Steer to the right
		break;
	default:
		break;
	}
}

/*
Repeated every tick
*/
void ADeepAgent::Step()
{
	//CURRENT STATE FROM INPUT SENSORS
	TArray<float> currentState = ADeepAgent::GetInput();

	//EXPLORATION - EXPLOITATION TRADEOFF
	float exploitation = FMath::SRand();
	if (exploitation > ADeepAgent::Epsilon || !ADeepAgent::IsTraining) {
		//Chose best action
		ADeepAgent::Client->Predict(currentState);
		UE_LOG(LogTemp, Error, TEXT("Best Action chosen: %d"), ADeepAgent::Action);
		//UE_LOG(LogTemp, Error, TEXT("Best Action chosen: %f %f"), ADeepAgent::ThrottleAction, ADeepAgent::SteerAction);
	}
	else {
		//Choose random action
		ADeepAgent::Action = FMath::RandRange(0, ADeepAgent::NumberActions - 1);

		if (ADeepAgent::Epsilon > ADeepAgent::MinEpsilon) {
			ADeepAgent::Epsilon -= ADeepAgent::EpsilonDecay;
		}
		else {
			ADeepAgent::Epsilon = ADeepAgent::MinEpsilon;
			ADeepAgent::NumberFitSteps = 2;
		}
		UE_LOG(LogTemp, Error, TEXT("Random Action chosen: %d"), ADeepAgent::Action);
	}

	//PERFORM ACTION
	ADeepAgent::PerformAction(ADeepAgent::Action);

	//NEXT STATE
	TArray<float> nextState = ADeepAgent::GetInput();

	//GAME STATUS
	//bool gameStatus = ADeepAgent::GameStatus;

	//ADeepAgent::Reward
	ADeepAgent::RewardFunction(currentState);
	ADeepAgent::CumulativeReward += ADeepAgent::Reward;
	ADeepAgent::AverageSpeed += ADeepAgent::GetMesh()->GetPhysicsLinearVelocity().Size(); //Cumulative nvm

	//FIT NEURAL NETWORK WITH THIS EXPERIENCE
	if (ADeepAgent::IsTraining) {
		Experience currentExp = Experience(currentState, ADeepAgent::Action, nextState, ADeepAgent::Reward, IsGameEnded);
		if ((!ADeepAgent::PreviousExperience.GetInitilized() ||
			!ADeepAgent::PreviousExperience.CheckEqualExperiences(currentExp))) {
			//SEND EXPERIENCE
			counter++;
			try
			{
				if (counter % ADeepAgent::NumberFitSteps == 0)//&& (ADeepAgent::Epsilon > ADeepAgent::MinEpsilon)
					ADeepAgent::Client->SendExperience(currentState, ADeepAgent::Action, nextState, ADeepAgent::Reward, ADeepAgent::IsGameEnded);

			}
			catch (const std::exception&)
			{
				UE_LOG(LogTemp, Error, TEXT("%d %f"), ADeepAgent::NumberFitSteps, ADeepAgent::NumberFitSteps);
			}
			
		}

		if (!ADeepAgent::PreviousExperience.GetInitilized()) {
			ADeepAgent::PreviousExperience.InitializeExperience(currentState, ADeepAgent::Action, nextState, ADeepAgent::Reward, IsGameEnded);
		}
		else {
			ADeepAgent::PreviousExperience = currentExp;
		}
	}

	ADeepAgent::NumberSteps++;
	if (ADeepAgent::IsGameEnded || (ADeepAgent::IsTraining && ADeepAgent::NumberSteps>=ADeepAgent::MaxNumberSteps) || (!ADeepAgent::IsTraining && ADeepAgent::NumberSteps >= ADeepAgent::MaxNumberSteps * 3)) {
		ADeepAgent::RestartGame();
	}
}

void ADeepAgent::RestartGame()
{
	ADeepAgent::MovementComponent->StopMovementImmediately();

	//Calculate new location, add some randomness
	FVector pos = ADeepAgent::initialTransform.GetLocation();
	float offset = 150;
	float scaling_factor = 1.6;
	pos.X += FMath::FRandRange(-offset* scaling_factor, offset * scaling_factor);
	pos.Y += FMath::FRandRange(-offset, offset);

	//Calculate new rotation
	bool changeDirection = ADeepAgent::Clockwise; // FMath::RandBool();
	ADeepAgent::Clockwise = !ADeepAgent::Clockwise;
	FQuat rot = ADeepAgent::initialTransform.GetRotation();
	if (changeDirection && ADeepAgent::IsTraining) {
		//Not sure why:
		//The idea is to rotate the actor of 180 degrees so that it can travel both directions: clock and anti-clock wise
		//but with 180 degrees in the InYaw parameter it rotates the actor of 90 degrees
		//using 360 rotates the actor of 360 degrees (no change)
		//using 359 it works as expected -> worst thing ever, you should be ashamed of yourself UE4
		rot = rot + FQuat(FRotator(0,359,0));
	}
	
	//Set location and rotation
	this->GetMesh()->SetWorldLocationAndRotation(pos,rot,false,nullptr,ETeleportType::TeleportPhysics);// > AddLocalRotation(rot); //->SetAllPhysicsRotation( rot );

	this->GetMesh()->SetAllPhysicsLinearVelocity(FVector::ZeroVector);
	this->GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	ADeepAgent::MovementComponent->SetThrottleInput(1);

	ADeepAgent::AverageSpeed /= ADeepAgent::NumberSteps;
	if(ADeepAgent::Episode>0)
		ADeepAgent::WriteToFile(ADeepAgent::Episode, ADeepAgent::NumberSteps, ADeepAgent::CumulativeReward, ADeepAgent::AverageSpeed, ADeepAgent::IsGameEnded, ADeepAgent::Clockwise, true);
	
	ADeepAgent::NumberSteps = 0;
	ADeepAgent::CumulativeReward = 0;
	ADeepAgent::AverageSpeed = 0;
	ADeepAgent::IsGameEnded = false;
	UE_LOG(LogTemp, Error, TEXT("Restart"));
}

void ADeepAgent::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) {
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	InputComponent->BindAction("Input1", IE_Pressed, this, &ADeepAgent::Restart);
}

void ADeepAgent::WriteToFile(int episode, int numberSteps, float totalReward, float averageSpeed, bool gameEndedByCrush, bool clockwise, bool allowOverwriting)
{
	FString TextToSave = FString("");
	TextToSave += FString::FromInt(episode) + "," + FString::FromInt(numberSteps) + "," + FString::SanitizeFloat(totalReward) + "," + FString::SanitizeFloat(averageSpeed) + "," + FString::FromInt(gameEndedByCrush) + "," + FString::FromInt(clockwise);
	TextToSave += "\n";

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// CreateDirectoryTree returns true if the destination
	// directory existed prior to call or has been created
	// during the call.
	if (PlatformFile.CreateDirectoryTree(*ADeepAgent::SaveDirectory))
	{
		// Get absolute file path
		FString AbsoluteFilePath = ADeepAgent::SaveDirectory + "/" + ADeepAgent::FileName;

		//if (!PlatformFile.FileExists(*AbsoluteFilePath))
		{
			if(allowOverwriting)
				FFileHelper::SaveStringToFile(TextToSave, *AbsoluteFilePath, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), FILEWRITE_Append);
			else {
				TextToSave = "Episode,numberSteps,totalReward,averageSpeed,gameEndedByCrush,IsClockwise\n";
				FFileHelper::SaveStringToFile(TextToSave, *AbsoluteFilePath, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), EFileWrite::FILEWRITE_None);
			}
		}
	}
}