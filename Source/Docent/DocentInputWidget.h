#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/EditableTextBox.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "DocentInputWidget.generated.h"

// Delegate for passing text to DocentAIVoice
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTextSubmitted, const FString&, UserText);

UCLASS()
class DOCENT_API UDocentInputWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Delegate for text submission
	UPROPERTY(BlueprintAssignable, Category = "Docent")
	FOnTextSubmitted OnTextSubmitted;

	// Widget Open/Close functions
	UFUNCTION(BlueprintCallable, Category = "Docent")
	void OpenInput();

	UFUNCTION(BlueprintCallable, Category = "Docent")
	void CloseInput();

	UFUNCTION(BlueprintCallable, Category = "Docent")
	void ToggleInput();

protected:
	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	// BindWidget matching WBP naming
	UPROPERTY(meta = (BindWidget))
	UEditableTextBox* InputBox;

	UPROPERTY(meta = (BindWidget))
	UButton* SendButton;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* HintText;

	// Handle button click or enter key
	UFUNCTION()
	void OnSendClicked();

	void SubmitText();

	bool bIsOpen = false;
};