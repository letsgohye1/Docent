#include "SubTitle.h"

void USubTitle::NativeConstruct()
{
	Super::NativeConstruct();

	if (SubtitleText)
	{
		SubtitleText->SetVisibility(ESlateVisibility::Hidden);
	}
}

void USubTitle::SetSubtitle(const FString& Text, bool bAutoHide)
{
	if (!SubtitleText) return;

	SubtitleText->SetText(FText::FromString(Text));
	SubtitleText->SetVisibility(ESlateVisibility::Visible);

	GetWorld()->GetTimerManager().ClearTimer(HideTimerHandle);
	
	if (bAutoHide)
	{
		GetWorld()->GetTimerManager().SetTimer(
			HideTimerHandle,
			this,
			&USubTitle::HideSubtitle,
			HideDelay,
			false
		);
	}
}

void USubTitle::HideSubtitle()
{
	if (SubtitleText)
	{
		SubtitleText->SetVisibility(ESlateVisibility::Hidden);
	}
}