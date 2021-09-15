// Fill out your copyright notice in the Description page of Project Settings.


#include "Client.h"
#include <Http.h>
#include <Collis_Avoid_Sys/DeepAgent.h>

// Sets default values
AClient::AClient()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AClient::BeginPlay()
{
	Super::BeginPlay();
	
	AClient::UrlAddress = "192.168.1.7:5000/";
}


// Called every frame
void AClient::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AClient::SendExperience(TArray<int> currentState, int action, TArray<int> nextState, float reward, bool endGame)
{
	FString data = AClient::ConstructData(currentState,action,nextState,reward,endGame);

	FHttpModule* Http = &FHttpModule::Get();
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = Http->CreateRequest();
	HttpRequest->SetVerb("Post");
	HttpRequest->SetHeader("Content-Type", "text/plain");
	HttpRequest->SetURL(AClient::UrlAddress+"experience");
	HttpRequest->SetContentAsString(data);
	HttpRequest->ProcessRequest();
}


FString AClient::ConstructData(TArray<int> currentState, int action, TArray<int> nextState, float reward, bool endGame)
{
	FString result;
	//CURRENT STATE
	for (int i = 0; i < currentState.Num(); i++)
	{
		if (i > 0)
			result.Append(".");
		result.Append(FString::FromInt(currentState[i]));
	}
	result.Append(";");

	//ACTION CHOSEN
	result.Append(FString::SanitizeFloat(action));
	result.Append(";");

	//NEXT STATE
	for (int i = 0; i < nextState.Num(); i++)
	{
		if (i > 0)
			result.Append(".");
		result.Append(FString::FromInt(nextState[i]));
	}
	result.Append(";");


	//REWARD GOT
	result.Append(FString::FromInt(reward));
	result.Append(";");

	//THE GAME ENDED OR NOT (0 false, 1 true)
	result.Append(FString::FromInt(endGame));
	return result;
}

void AClient::Predict(TArray<int> currentState)
{
	FHttpModule* Http = &FHttpModule::Get();
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = Http->CreateRequest();
	HttpRequest->SetVerb("POST");
	HttpRequest->SetHeader("Content-Type", "text/plain");
	HttpRequest->SetURL(AClient::UrlAddress + "predict");

	FString data;
	//CURRENT STATE
	for (int i = 0; i < currentState.Num(); i++)
	{
		if (i > 0)
			data.Append(".");
		data.Append(FString::FromInt(currentState[i]));
	}
	HttpRequest->SetContentAsString(data);
	UE_LOG(LogTemp, Error, TEXT("Line trace has hit: %s"), *data);

	HttpRequest->OnProcessRequestComplete().BindUObject(this, &AClient::GetPrediction);
	HttpRequest->ProcessRequest();
}

void AClient::GetPrediction(FHttpRequestPtr request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	UE_LOG(LogTemp, Error, TEXT("Response"));
	if (bWasSuccessful)
	{
		FString response = Response->GetContentAsString();
		TArray<FString> result;
		response.ParseIntoArray(result, TEXT(";"), true);

		AClient::Agent->SetAction(FCString::Atoi(*result[0]));
		AClient::Agent->SetConfidence(FCString::Atof(*result[1]));

	}
}

void AClient::SetDeepAgent(ADeepAgent* agent)
{
	AClient::Agent = agent;
}
