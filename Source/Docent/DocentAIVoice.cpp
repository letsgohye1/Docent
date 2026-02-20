#include "DocentAIVoice.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "TimerManager.h"
#include "Http.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

ADocentAIVoice::ADocentAIVoice()
{
	PrimaryActorTick.bCanEverTick = false;

	// RootComponent 생성
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// MediaSoundComponent 생성 — 이게 있어야 소리가 남
	MediaSoundComponent = CreateDefaultSubobject<UMediaSoundComponent>(TEXT("MediaSoundComponent"));
	MediaSoundComponent->SetupAttachment(RootComponent);
}

void ADocentAIVoice::BeginPlay()
{
	Super::BeginPlay();

	// MediaSoundComponent에 MediaPlayer 연결
	if (MediaSoundComponent && MediaPlayer)
	{
		MediaSoundComponent->SetMediaPlayer(MediaPlayer);
		// Disable spatialization (make it global) by setting it as UI sound
		MediaSoundComponent->bIsUISound = true;
		
		// Bind playback events
		MediaPlayer->OnMediaOpened.AddDynamic(this, &ADocentAIVoice::OnMediaOpened);
		MediaPlayer->OnMediaOpenFailed.AddDynamic(this, &ADocentAIVoice::OnMediaOpenFailed);
		MediaPlayer->OnPlaybackSuspended.AddDynamic(this, &ADocentAIVoice::OnPlaybackSuspended);
		MediaPlayer->OnEndReached.AddDynamic(this, &ADocentAIVoice::OnPlaybackEnd);
		
		UE_LOG(LogTemp, Log, TEXT("[DocentAI] MediaSoundComponent linked and events bound"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[DocentAI] MediaPlayer not assigned"));
	}

	CheckServerStatus();

	// 자막 위젯 생성
	if (SubtitleWidgetClass)
	{
		SubtitleWidget = CreateWidget<USubTitle>(GetWorld(), SubtitleWidgetClass);
		if (SubtitleWidget)
		{
			SubtitleWidget->AddToViewport(10);
			UE_LOG(LogTemp, Log, TEXT("[DocentAI] Subtitle widget created"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[DocentAI] SubtitleWidgetClass not assigned"));
	}
}

// ── 공통 POST 요청 헬퍼 ─────────────────────────────────────
TSharedRef<IHttpRequest> ADocentAIVoice::MakePostRequest(const FString& Endpoint)
{
	TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(ServerURL + Endpoint);
	Request->SetVerb("POST");
	Request->SetHeader("Content-Type", "application/json");
	Request->SetContentAsString("{}");
	return Request;
}

// ── 서버 상태 확인 ──────────────────────────────────────────
void ADocentAIVoice::CheckServerStatus()
{
	TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(ServerURL + "/status");
	Request->SetVerb("GET");
	Request->OnProcessRequestComplete().BindUObject(
		this, &ADocentAIVoice::OnStatusResponse);
	Request->ProcessRequest();
	UE_LOG(LogTemp, Log, TEXT("[DocentAI] Checking server status at: %s/status"), *ServerURL);
}

void ADocentAIVoice::OnStatusResponse(
	FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess)
{
	if (bSuccess && Response->GetResponseCode() == 200)
	{
		UE_LOG(LogTemp, Log, TEXT("[DocentAI] Server connection OK"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[DocentAI] Server connection failed - run python server.py first"));
		if (Response.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("[DocentAI] Response Code: %d, Content: %s"), Response->GetResponseCode(), *Response->GetContentAsString());
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[DocentAI] No response received. Check network or server availability."));
		}
	}
}

// ── 녹음 시작 ───────────────────────────────────────────────
void ADocentAIVoice::StartRecording()
{
	if (bIsRecording)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DocentAI] Already recording"));
		return;
	}

	TSharedRef<IHttpRequest> Request = MakePostRequest("/start");
	Request->OnProcessRequestComplete().BindUObject(
		this, &ADocentAIVoice::OnStartResponse);
	Request->ProcessRequest();

	bIsRecording = true;
	UE_LOG(LogTemp, Log, TEXT("[DocentAI] Start recording request sent to %s/start"), *ServerURL);
}

void ADocentAIVoice::OnStartResponse(
	FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess)
{
	if (bSuccess && Response->GetResponseCode() == 200)
	{
		UE_LOG(LogTemp, Log, TEXT("[DocentAI] Recording started"));
		if (SubtitleWidget)
			SubtitleWidget->SetSubtitle(TEXT("말씀해주세요..."));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[DocentAI] Failed to start recording"));
		if (Response.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("[DocentAI] Response Code: %d, Content: %s"), Response->GetResponseCode(), *Response->GetContentAsString());
		}
		bIsRecording = false;
	}
}

// ── 녹음 중지 ───────────────────────────────────────────────
void ADocentAIVoice::StopRecording()
{
	if (!bIsRecording)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DocentAI] Not recording"));
		return;
	}

	TSharedRef<IHttpRequest> Request = MakePostRequest("/stop");
	Request->SetTimeout(300.0f);
	Request->OnProcessRequestComplete().BindUObject(
		this, &ADocentAIVoice::OnStopResponse);
	Request->ProcessRequest();

	bIsRecording = false;

	if (SubtitleWidget)
		SubtitleWidget->SetSubtitle(TEXT("생각하는 중..."));

	UE_LOG(LogTemp, Log, TEXT("[DocentAI] Stop recording request sent to %s/stop"), *ServerURL);
}

void ADocentAIVoice::OnStopResponse(
	FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess)
{
	if (!bSuccess)
	{
		UE_LOG(LogTemp, Error, TEXT("[DocentAI] HTTP request failed"));
		if (SubtitleWidget) SubtitleWidget->HideSubtitle();
		return;
	}

	int32 Code = Response->GetResponseCode();
	if (Code != 200)
	{
		UE_LOG(LogTemp, Error, TEXT("[DocentAI] Server error %d: %s"),
			Code, *Response->GetContentAsString());
		if (SubtitleWidget) SubtitleWidget->HideSubtitle();
		return;
	}

	FString AIAnswer = FGenericPlatformHttp::UrlDecode(Response->GetHeader("X-AI-Answer"));
	FString UserText = FGenericPlatformHttp::UrlDecode(Response->GetHeader("X-User-Text"));
	FString AudioFile = Response->GetHeader("X-Audio-File");

	UE_LOG(LogTemp, Log, TEXT("[DocentAI] Received AudioFile Header: '%s'"), *AudioFile);

	UE_LOG(LogTemp, Log, TEXT("[User] %s"), *UserText);
	UE_LOG(LogTemp, Log, TEXT("[Docent] %s"), *AIAnswer);

	if (SubtitleWidget && !AIAnswer.IsEmpty())
		SubtitleWidget->SetSubtitle(AIAnswer, false);

	FTimerDelegate AudioDelegate;
	AudioDelegate.BindLambda([this, AudioFile]()
	{
		PlayAudioFromURL(ServerURL + "/audio/" + AudioFile);
	});
	// Wait 1.5s for server to finish writing file
	GetWorld()->GetTimerManager().SetTimer(
		AudioDelayHandle, AudioDelegate, 1.5f, false);
	UE_LOG(LogTemp, Log, TEXT("[DocentAI] Waiting 1.5s before playing audio: %s/audio/%s"), *ServerURL, *AudioFile);
}

// ── 입력창 토글 ─────────────────────────────────────────────
void ADocentAIVoice::ToggleInputWidget()
{
	if (!InputWidget && InputWidgetClass)
	{
		InputWidget = CreateWidget<UDocentInputWidget>(GetWorld(), InputWidgetClass);
		if (InputWidget)
		{
			InputWidget->AddToViewport(11);
			InputWidget->OnTextSubmitted.AddDynamic(this, &ADocentAIVoice::AskWithText);
			UE_LOG(LogTemp, Log, TEXT("[DocentAI] Input widget created and bound"));
		}
	}

	if (InputWidget)
		InputWidget->ToggleInput();
	else
		UE_LOG(LogTemp, Warning, TEXT("[DocentAI] Toggle failed - InputWidget or Class missing"));
}

// ── 텍스트 직접 입력 ────────────────────────────────────────
void ADocentAIVoice::AskWithText(const FString& UserText)
{
	if (UserText.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[DocentAI] Empty text"));
		return;
	}

	// 한글 포함 JSON 안전하게 직렬화
	TSharedRef<FJsonObject> JsonObject = MakeShareable(new FJsonObject());
	JsonObject->SetStringField(TEXT("text"), UserText);
	FString Body;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
	FJsonSerializer::Serialize(JsonObject, Writer);

	TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(ServerURL + "/ask");
	Request->SetVerb("POST");
	Request->SetHeader("Content-Type", "application/json");
	Request->SetContentAsString(Body);
	Request->SetTimeout(300.0f);
	Request->OnProcessRequestComplete().BindUObject(
		this, &ADocentAIVoice::OnAskResponse);
	Request->ProcessRequest();

	if (SubtitleWidget)
		SubtitleWidget->SetSubtitle(TEXT("생각하는 중..."), true);

	UE_LOG(LogTemp, Log, TEXT("[DocentAI] Text ask: %s (sent to %s/ask)"), *UserText, *ServerURL);
}

void ADocentAIVoice::OnAskResponse(
	FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess)
{
	if (!bSuccess || Response->GetResponseCode() != 200)
	{
		UE_LOG(LogTemp, Error, TEXT("[DocentAI] /ask error %d"),
			Response ? Response->GetResponseCode() : -1);
		if (Response.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("[DocentAI] Response Content: %s"), *Response->GetContentAsString());
		}
		if (SubtitleWidget) SubtitleWidget->HideSubtitle();
		return;
	}

	FString AIAnswer = FGenericPlatformHttp::UrlDecode(Response->GetHeader("X-AI-Answer"));
	FString UserText = FGenericPlatformHttp::UrlDecode(Response->GetHeader("X-User-Text"));
	FString AudioFile = Response->GetHeader("X-Audio-File");

	UE_LOG(LogTemp, Log, TEXT("[DocentAI] Received AudioFile Header (Ask): '%s'"), *AudioFile);

	UE_LOG(LogTemp, Log, TEXT("[User] %s"), *UserText);
	UE_LOG(LogTemp, Log, TEXT("[Docent] %s"), *AIAnswer);

	if (SubtitleWidget && !AIAnswer.IsEmpty())
		SubtitleWidget->SetSubtitle(AIAnswer, false);

	FTimerDelegate AudioDelegate;
	AudioDelegate.BindLambda([this, AudioFile]()
	{
		PlayAudioFromURL(ServerURL + "/audio/" + AudioFile);
	});
	// Wait 1.5s for server to finish writing file
	GetWorld()->GetTimerManager().SetTimer(
		AudioDelayHandle, AudioDelegate, 1.5f, false);
	UE_LOG(LogTemp, Log, TEXT("[DocentAI] Waiting 1.5s before playing audio: %s/audio/%s"), *ServerURL, *AudioFile);
}

void ADocentAIVoice::OnMediaOpened(FString OpenedUrl)
{
	if (MediaPlayer)
	{
		MediaPlayer->Play();
		UE_LOG(LogTemp, Log, TEXT("[DocentAI] Media opened and playing: %s"), *OpenedUrl);
	}
}

void ADocentAIVoice::OnMediaOpenFailed(FString FailedUrl)
{
	UE_LOG(LogTemp, Error, TEXT("[DocentAI] Media OPEN FAILED: %s"), *FailedUrl);
}

void ADocentAIVoice::OnPlaybackSuspended()
{
	UE_LOG(LogTemp, Warning, TEXT("[DocentAI] Playback SUSPENDED (buffering)"));
}

void ADocentAIVoice::OnPlaybackEnd()
{
	if (SubtitleWidget)
	{
		SubtitleWidget->HideSubtitle();
		UE_LOG(LogTemp, Log, TEXT("[DocentAI] Playback ended, hiding subtitle"));
	}
}

// -- MediaPlayer playback --------------------------------
void ADocentAIVoice::PlayAudioFromURL(const FString& URL)
{
	if (!MediaPlayer)
	{
		UE_LOG(LogTemp, Error, TEXT("[DocentAI] No MediaPlayer - assign MP_AIVoice in details panel"));
		return;
	}

	// Close previous media before opening new one
	MediaPlayer->Close();
	UE_LOG(LogTemp, Log, TEXT("[DocentAI] MediaPlayer->Close() called before opening new URL"));
	
	MediaPlayer->OpenUrl(URL);
	UE_LOG(LogTemp, Log, TEXT("[DocentAI] MediaPlayer->OpenUrl: %s"), *URL);
}