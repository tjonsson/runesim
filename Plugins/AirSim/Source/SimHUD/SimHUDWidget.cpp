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
            TEXT("  CAMERAS                        VR CONTROLS\n")
            TEXT("  F   FPV                        V   Toggle VR\n")
            TEXT("  O   Chase                      N   Next camera\n")
            TEXT("  B   Fly-with-me                J   Prev camera\n")
            TEXT("  G   Ground observer            H   Toggle help\n")
            TEXT("  M   Manual                     Home Return to origin\n")
            TEXT("  I   Front                      \n")
            TEXT("  K   Backup                     VR CONTROLLERS\n")
            TEXT("  C   Cycle                      R Trigger  Fly forward\n")
            TEXT("  P   Spectator                  L Trigger  Fly backward\n")
            TEXT("  -   No display                 L Grip     Return to origin\n")
            TEXT("                                 R Grip     Fast move (hold)\n")
            TEXT("  GENERAL                        \n")
            TEXT("  F1  Toggle help                VR KEYBOARD\n")
            TEXT("  Enter Switch drone             WASD/QE Move (Manual)\n")
            TEXT("  Tab Report   R Record          \n")
            TEXT("  T   Trace    Backspace Reset   \n")
            TEXT("  1/2/3/0 Toggle subwindows      \n"));

        const FName help_widget_names[] = { FName(TEXT("F1HelpText")), FName(TEXT("F1HelpTextBlock")) };
        for (const FName& widget_name : help_widget_names) {
            if (UTextBlock* text_block = Cast<UTextBlock>(WidgetTree->FindWidget(widget_name))) {
                text_block->SetText(help_text);
                text_block->SetMinDesiredWidth(700.0f);
                text_block->SetAutoWrapText(false);
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

void USimHUDWidget::showCameraName(const FString& name)
{
    if (WidgetTree) {
        if (UTextBlock* text_block = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("CenterMessage")))) {
            text_block->SetText(FText::FromString(name));
            text_block->SetColorAndOpacity(FSlateColor(FLinearColor(0.0f, 1.0f, 1.0f, 0.6f)));
            text_block->SetVisibility(ESlateVisibility::Visible);
        }
    }

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(CameraNameTimer);
        World->GetTimerManager().SetTimer(CameraNameTimer, this,
            &USimHUDWidget::clearCameraName, 5.0f, false);
    }
}

void USimHUDWidget::clearCameraName()
{
    hideCenterMessage();
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
