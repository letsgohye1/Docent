#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "SubTitle.generated.h"

UCLASS()
class DOCENT_API USubTitle : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(meta = (BindWidget))
	UTextBlock* SubtitleText;

	UFUNCTION(BlueprintCallable, Category = "Subtitle")
	void SetSubtitle(const FString& Text, bool bAutoHide = true);

	UFUNCTION(BlueprintCallable, Category = "Subtitle")
	void HideSubtitle();

protected:
	virtual void NativeConstruct() override;

private:
	FTimerHandle HideTimerHandle;
	float HideDelay = 300.0f;
};