#include "DocentInputWidget.h"
#include "Components/EditableTextBox.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Framework/Application/SlateApplication.h"

void UDocentInputWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind button click
	if (SendButton)
	{
		SendButton->OnClicked.AddDynamic(this, &UDocentInputWidget::OnSendClicked);
	}

	// Hint text setup
	if (HintText)
	{
		HintText->SetText(FText::FromString(TEXT("[T] Input Question  |  [Enter] Send  |  [ESC] Close")));
	}

	// Start collapsed
	SetVisibility(ESlateVisibility::Collapsed);
}

// -- Open / Close / Toggle --------------------------------------
void UDocentInputWidget::OpenInput()
{
	if (bIsOpen) return;

	bIsOpen = true;
	SetVisibility(ESlateVisibility::Visible);

	// Focus input box
	if (InputBox)
	{
		InputBox->SetText(FText::GetEmpty());
		InputBox->SetKeyboardFocus();
	}

	// Switch to UI input mode
	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		FInputModeUIOnly InputMode;
		if (InputBox)
			InputMode.SetWidgetToFocus(InputBox->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);
		PC->SetShowMouseCursor(true);
	}
}

void UDocentInputWidget::CloseInput()
{
	if (!bIsOpen) return;

	bIsOpen = false;
	SetVisibility(ESlateVisibility::Collapsed);

	// Return to Game input mode
	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		FInputModeGameOnly InputMode;
		PC->SetInputMode(InputMode);
		PC->SetShowMouseCursor(false);
	}
}

void UDocentInputWidget::ToggleInput()
{
	if (bIsOpen)
		CloseInput();
	else
		OpenInput();
}

// -- Text Submission ----------------------------------------------
void UDocentInputWidget::OnSendClicked()
{
	SubmitText();
}

void UDocentInputWidget::SubmitText()
{
	if (!InputBox) return;

	FString Text = InputBox->GetText().ToString().TrimStartAndEnd();
	if (Text.IsEmpty()) return;

	// Delegate text to DocentAIVoice
	OnTextSubmitted.Broadcast(Text);

	// Clear and close
	InputBox->SetText(FText::GetEmpty());
	CloseInput();
}

// -- Key Event handling -----------------------------------------
FReply UDocentInputWidget::NativeOnKeyDown(
	const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Enter -> Submit
	if (InKeyEvent.GetKey() == EKeys::Enter)
	{
		SubmitText();
		return FReply::Handled();
	}

	// ESC -> Close
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		CloseInput();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}
