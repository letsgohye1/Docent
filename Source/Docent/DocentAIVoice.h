#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "MediaPlayer.h"
#include "MediaSoundComponent.h"
#include "SubTitle.h"
#include "DocentInputWidget.h"
#include "DocentAIVoice.generated.h"

UCLASS()
class DOCENT_API ADocentAIVoice : public AActor
{
	GENERATED_BODY()

public:
	ADocentAIVoice();

protected:
	virtual void BeginPlay() override;

public:
	// 디테일 패널에서 MP_AIVoice 할당
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Voice")
	UMediaPlayer* MediaPlayer;

	// 자동 생성됨 (할당 불필요)
	UPROPERTY(VisibleAnywhere, Category = "AI Voice")
	UMediaSoundComponent* MediaSoundComponent;

	// 디테일 패널에서 WBP_Subtitle 할당
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Voice")
	TSubclassOf<USubTitle> SubtitleWidgetClass;

	// 디테일 패널에서 WBP_DocentInput 할당
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Voice")
	TSubclassOf<UDocentInputWidget> InputWidgetClass;

	UFUNCTION(BlueprintCallable, Category = "AI Voice")
	void StartRecording();

	UFUNCTION(BlueprintCallable, Category = "AI Voice")
	void StopRecording();

	UFUNCTION(BlueprintCallable, Category = "AI Voice")
	void CheckServerStatus();

	UFUNCTION(BlueprintCallable, Category = "AI Voice")
	void ToggleInputWidget();

	UFUNCTION(BlueprintCallable, Category = "AI Voice")
	void AskWithText(const FString& UserText);

private:
	void OnStatusResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess);
	void OnStartResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess);
	void OnStopResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess);
	void OnAskResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess);
	void OnMediaOpened(FString OpenedUrl);
	void OnMediaOpenFailed(FString FailedUrl);
	void OnPlaybackSuspended();
	void OnPlaybackEnd();
	void PlayAudioFromURL(const FString& URL);
	TSharedRef<IHttpRequest> MakePostRequest(const FString& Endpoint);

	FString ServerURL = TEXT("http://127.0.0.1:6000");
	bool bIsRecording = false;

	UPROPERTY()
	USubTitle* SubtitleWidget = nullptr;

	UPROPERTY()
	UDocentInputWidget* InputWidget = nullptr;

	FTimerHandle AudioDelayHandle;
};