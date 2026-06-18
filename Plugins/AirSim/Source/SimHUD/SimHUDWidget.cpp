#include "SimHUDWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"

void USimHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();

    updateHelpText();
    hideCenterMessage();
}

void USimHUDWidget::updateHelpText()
{
    if (WidgetTree) {
        const FText help_text = FText::FromString(
            TEXT("Keyboard Controls\n")
            TEXT("F1: Toggle help\n")
            TEXT("Enter: Switch drone\n")
            TEXT("P: Spectator camera\n")
            TEXT("O: Original chase view\n")
            TEXT("C: Cycle vehicle cameras\n")
            TEXT("F: FPV view\n")
            TEXT("B: Fly-with-me view\n")
            TEXT("G: Ground observer view\n")
            TEXT("M: Manual camera view\n")
            TEXT("I: Front camera view\n")
            TEXT("K: Backup camera view\n")
            TEXT("-: No display view\n")
            TEXT("V: Toggle VR headset\n")
            TEXT("Tab: Toggle report\n")
            TEXT("R: Toggle recording\n")
            TEXT("T: Toggle trace\n")
            TEXT("1/2/3: Toggle subwindows\n")
            TEXT("0: Toggle all subwindows\n")
            TEXT("Backspace: Reset"));

        const FName help_widget_names[] = { FName(TEXT("F1HelpText")), FName(TEXT("F1HelpTextBlock")) };
        for (const FName& widget_name : help_widget_names) {
            if (UTextBlock* text_block = Cast<UTextBlock>(WidgetTree->FindWidget(widget_name))) {
                text_block->SetText(help_text);
            }
        }
    }
}

void USimHUDWidget::hideCenterMessage()
{
    if (WidgetTree) {
        if (UTextBlock* text_block = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("CenterMessage")))) {
            text_block->SetVisibility(ESlateVisibility::Collapsed);
            text_block->SetText(FText::GetEmpty());
        }
    }
}

void USimHUDWidget::updateDebugReport(const std::string& text)
{
    setReportText(FString(text.c_str()));
}

void USimHUDWidget::setReportVisible(bool is_visible)
{
    setReportContainerVisibility(is_visible);
}

void USimHUDWidget::toggleHelpVisibility()
{
    updateHelpText();
    setHelpContainerVisibility(!getHelpContainerVisibility());
}

void USimHUDWidget::setOnToggleRecordingHandler(OnToggleRecording handler)
{
    on_toggle_recording_ = handler;
}

void USimHUDWidget::onToggleRecordingButtonClick()
{
    on_toggle_recording_();
}
