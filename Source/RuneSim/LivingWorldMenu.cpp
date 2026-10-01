// Living World settings panel (Settings > Environment > Living World), drawn with Slate.
// Layout follows the RuneSim settings mockup: header with the enable switch, a left rail of tabs
// (Population, Behavior, Simulation), sectioned rows of segmented controls and steppers, and a footer
// with the save state, Discard and Apply & Save. Edits go to a draft until applied.
#include "LivingWorldSubsystem.h"
#include "SimPTZ.h"
#include "SimReplay.h"
#include "SimScenario.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateImageBrush.h"
#include "Brushes/SlateNoResource.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"

namespace LivingMenu
{
    FLinearColor Hex(uint32 RGB, float Alpha = 1.f) { return FLinearColor::FromSRGBColor(FColor((RGB >> 16) & 255, (RGB >> 8) & 255, RGB & 255, uint8(Alpha * 255))); }
    // Palette sampled from the mockup.
    const FLinearColor Panel = Hex(0x111A24), Header = Hex(0x152230), Rail = Hex(0x0F1720), Divider = Hex(0x243243);
    const FLinearColor Well = Hex(0x0E151D), WellLine = Hex(0x2C3B4C), Ink = Hex(0xF1F3F5), Muted = Hex(0x9AA7B4), Dim = Hex(0x6E7B88);
    const FLinearColor Amber = Hex(0xF2C46D), AmberText = Hex(0x1A1408), AmberTint = Hex(0x3A3122), Apply = Hex(0x5E4E2D), ApplyText = Hex(0xF3E4C2);

    struct FBrushes
    {
        FSlateColorBrush White{FLinearColor::White};
        FSlateRoundedBoxBrush FieldBox{Well, 2.f, WellLine, 1.f};
        FSlateRoundedBoxBrush Card{Hex(0x0F1720), 2.f, WellLine, 1.f};
        FSlateRoundedBoxBrush CardOn{Hex(0x1B1A15), 2.f, Amber, 1.5f};
        FSlateRoundedBoxBrush AmberFill{Amber, 2.f};
        FSlateRoundedBoxBrush SwitchOn{Amber, 4.f};
        FSlateRoundedBoxBrush SwitchOff{Hex(0x3A4756), 4.f};
        FSlateRoundedBoxBrush Knob{Hex(0x1A1408), 2.f};
        FSlateRoundedBoxBrush ApplyBox{Apply, 2.f, Hex(0x7A6740), 1.f};
        FSlateRoundedBoxBrush KeyBox{FLinearColor::Transparent, 2.f, Hex(0xB9A57A), 1.f};
        FButtonStyle Flat, Soft;
        TMap<FName, TSharedPtr<FSlateBrush>> Icons;
        FBrushes()
        {
            Flat.SetNormal(FSlateNoResource()).SetHovered(FSlateNoResource()).SetPressed(FSlateNoResource()).SetDisabled(FSlateNoResource())
                .SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));
            Soft = Flat;
            Soft.SetHovered(FSlateColorBrush(Hex(0xFFFFFF, .06f))).SetPressed(FSlateColorBrush(Hex(0xFFFFFF, .1f)));
        }
        const FSlateBrush* Icon(FName Name)
        {
            if (const TSharedPtr<FSlateBrush>* Found = Icons.Find(Name)) return Found->Get();
            const FString Path = FPaths::ProjectContentDir() / TEXT("Slate/LivingWorld") / (Name.ToString() + TEXT(".svg"));
            TSharedPtr<FSlateBrush> Brush = MakeShared<FSlateVectorImageBrush>(Path, FVector2f(24.f, 24.f));
            Icons.Add(Name, Brush);
            return Brush.Get();
        }
    };
    FBrushes& B() { static FBrushes Instance; return Instance; }

    FSlateFontInfo Font(const char* Weight, int32 Size, int32 Spacing = 0)
    {
        FSlateFontInfo Info = FCoreStyle::GetDefaultFontStyle(Weight, Size);
        Info.LetterSpacing = Spacing;
        return Info;
    }
    TSharedRef<SWidget> Label(const FString& Value, const FSlateFontInfo& InFont, const FLinearColor& Color)
    {
        return SNew(STextBlock).Text(FText::FromString(Value)).Font(InFont).ColorAndOpacity(Color);
    }
    TSharedRef<SWidget> Icon(FName Name, const FLinearColor& Color, float Size = 22.f)
    {
        return SNew(SBox).WidthOverride(Size).HeightOverride(Size)[SNew(SImage).Image(B().Icon(Name)).ColorAndOpacity(Color)];
    }
    TSharedRef<SWidget> Rule() { return SNew(SBox).HeightOverride(1.f)[SNew(SBorder).BorderImage(&B().White).BorderBackgroundColor(Divider)]; }

    /** A settings row that highlights (amber tint and left bar) while hovered, like the focused row in the mockup. */
    class SRow : public SCompoundWidget
    {
    public:
        SLATE_BEGIN_ARGS(SRow) {}
            SLATE_DEFAULT_SLOT(FArguments, Content)
        SLATE_END_ARGS()
        void Construct(const FArguments& Args)
        {
            ChildSlot
            [
                SNew(SBorder).BorderImage(&B().White).Padding(0)
                .BorderBackgroundColor_Lambda([this]() { return bHover ? AmberTint : FLinearColor::Transparent; })
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(3.f)
                        [SNew(SBorder).BorderImage(&B().White).BorderBackgroundColor_Lambda([this]() { return bHover ? Amber : FLinearColor::Transparent; })]]
                    + SHorizontalBox::Slot().FillWidth(1.f).Padding(12.f, 6.f, 0.f, 6.f)[Args._Content.Widget]
                ]
            ];
        }
        virtual void OnMouseEnter(const FGeometry& Geometry, const FPointerEvent& Event) override { bHover = true; SCompoundWidget::OnMouseEnter(Geometry, Event); }
        virtual void OnMouseLeave(const FPointerEvent& Event) override { bHover = false; SCompoundWidget::OnMouseLeave(Event); }
    private:
        bool bHover = false;
    };

    TSharedRef<SWidget> Row(const FString& Name, TSharedRef<SWidget> Control, const FString& Hint = FString())
    {
        TSharedRef<SVerticalBox> Words = SNew(SVerticalBox) + SVerticalBox::Slot().AutoHeight()[Label(Name, Font("Bold", 15), Ink)];
        if (!Hint.IsEmpty()) Words->AddSlot().AutoHeight().Padding(0, 2, 0, 0)[SNew(STextBlock).Text(FText::FromString(Hint)).Font(Font("Regular", 11)).ColorAndOpacity(Muted).AutoWrapText(true)];
        return SNew(SRow)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(0, 0, 16, 0)[Words]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Control]
        ];
    }

    TSharedRef<SWidget> Section(FName IconName, const FString& Title, TFunction<FText()> Right = nullptr)
    {
        TSharedRef<SHorizontalBox> Box = SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)[Icon(IconName, Muted, 20.f)]
            + SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Label(Title, Font("Bold", 12, 260), Muted)];
        if (Right) Box->AddSlot().AutoWidth().VAlign(VAlign_Center)[SNew(STextBlock).Font(Font("Regular", 12)).ColorAndOpacity(Muted).Text_Lambda(TFunction<FText()>(Right))];
        return SNew(SBox).Padding(FMargin(0, 18, 0, 8))[Box];
    }

    /** Off / Sparse / Normal / Dense style choice; the selected cell is filled amber. */
    TSharedRef<SWidget> Segmented(TArray<FString> Choices, TFunction<int32()> Get, TFunction<void(int32)> Set)
    {
        TSharedRef<SHorizontalBox> Cells = SNew(SHorizontalBox);
        for (int32 I = 0; I < Choices.Num(); ++I)
            Cells->AddSlot().AutoWidth().Padding(2)
            [
                SNew(SButton).ButtonStyle(&B().Soft).ContentPadding(0).OnClicked_Lambda([Set, I]() { Set(I); return FReply::Handled(); })
                [
                    SNew(SBorder).Padding(FMargin(14, 8)).BorderImage_Lambda([Get, I]() -> const FSlateBrush* { return Get() == I ? &B().AmberFill : &B().Flat.Normal; })
                    [
                        SNew(SBox).MinDesiredWidth(56.f).HAlign(HAlign_Center)
                        [SNew(STextBlock).Text(FText::FromString(Choices[I])).Font(Font("Regular", 13))
                            .ColorAndOpacity_Lambda([Get, I]() { return Get() == I ? AmberText : Hex(0xC9D1D9); })]
                    ]
                ]
            ];
        return SNew(SBorder).BorderImage(&B().FieldBox).Padding(2)[Cells];
    }

    /** − value + counter in a bordered field. */
    TSharedRef<SWidget> Stepper(TFunction<FText()> Value, TFunction<void(int32)> Step)
    {
        auto Arrow = [Step](FName Name, int32 Delta)
        {
            return SNew(SButton).ButtonStyle(&B().Soft).ContentPadding(FMargin(16, 10)).OnClicked_Lambda([Step, Delta]() { Step(Delta); return FReply::Handled(); })
                [Icon(Name, Amber, 18.f)];
        };
        return SNew(SBox).WidthOverride(300.f)
        [
            SNew(SBorder).BorderImage(&B().FieldBox).Padding(0)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth()[Arrow(TEXT("minus"), -1)]
                + SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Center).VAlign(VAlign_Center)
                    [SNew(STextBlock).Font(Font("Bold", 16)).ColorAndOpacity(Ink).Text_Lambda(TFunction<FText()>(Value))]
                + SHorizontalBox::Slot().AutoWidth()[Arrow(TEXT("plus"), +1)]
            ]
        ];
    }

    /** Pill switch: amber with the knob right when on. */
    TSharedRef<SWidget> Switch(TFunction<bool()> Get, TFunction<void()> Toggle)
    {
        return SNew(SButton).ButtonStyle(&B().Flat).ContentPadding(0).OnClicked_Lambda([Toggle]() { Toggle(); return FReply::Handled(); })
        [
            SNew(SBox).WidthOverride(66.f).HeightOverride(36.f)
            [
                SNew(SBorder).HAlign(HAlign_Left).BorderImage_Lambda([Get]() -> const FSlateBrush* { return Get() ? &B().SwitchOn : &B().SwitchOff; })
                .Padding_Lambda([Get]() { return Get() ? FMargin(39.f, 7.f, 7.f, 7.f) : FMargin(7.f); })
                [SNew(SBox).WidthOverride(20.f).HeightOverride(22.f)[SNew(SBorder).BorderImage(&B().Knob)]]
            ]
        ];
    }

    /** Outlined secondary action button. */
    TSharedRef<SWidget> Action(TFunction<FText()> Caption, TFunction<void()> Do)
    {
        return SNew(SButton).ButtonStyle(&B().Soft).ContentPadding(0).OnClicked_Lambda([Do]() { Do(); return FReply::Handled(); })
            [SNew(SBorder).BorderImage(&B().FieldBox).Padding(FMargin(14, 9)).HAlign(HAlign_Center)
                [SNew(STextBlock).Font(Font("Bold", 12, 120)).ColorAndOpacity(Ink).Text_Lambda(TFunction<FText()>(Caption))]];
    }

    bool Same(const FLivingWorldOptions& A, const FLivingWorldOptions& Bv)
    {
        return A.bEnabled == Bv.bEnabled && A.Preset == Bv.Preset && A.Population == Bv.Population && A.CrowdDensity == Bv.CrowdDensity &&
            A.TrafficDensity == Bv.TrafficDensity && A.Planes == Bv.Planes && A.Helicopters == Bv.Helicopters && A.Drones == Bv.Drones &&
            A.BirdFlocks == Bv.BirdFlocks && A.FlockSize == Bv.FlockSize && A.bReactive == Bv.bReactive &&
            FMath::IsNearlyEqual(A.ActivityRadiusMeters, Bv.ActivityRadiusMeters) && A.MaxActors == Bv.MaxActors && A.Seed == Bv.Seed &&
            FMath::IsNearlyEqual(A.AmbientVolumeDb, Bv.AmbientVolumeDb) && A.bCombatTargets == Bv.bCombatTargets &&
            A.SensorStreams == Bv.SensorStreams && A.bRuntimePerches == Bv.bRuntimePerches;
    }
}

void ULivingWorldSubsystem::ApplyMenuDraft()
{
    if (!MenuDraft.IsValid()) return;
    ApplyOptions(*MenuDraft);
    *MenuDraft = Options; // Sanitized values become the new baseline: "All changes saved".
}

void ULivingWorldSubsystem::DiscardMenuDraft()
{
    if (MenuDraft.IsValid()) *MenuDraft = Options;
}

TSharedRef<SWidget> ULivingWorldSubsystem::BuildMenuContent()
{
    using namespace LivingMenu;
    MenuDraft = MakeShared<FLivingWorldOptions>(Options);
    TSharedRef<FLivingWorldOptions> Draft = MenuDraft.ToSharedRef();
    auto Custom = [Draft]() { Draft->Preset = ELivingPreset::Custom; };
    auto IntStep = [Draft, Custom](int32 FLivingWorldOptions::*Member, int32 Min, int32 Max, bool bPopulation)
    {
        return [Draft, Custom, Member, Min, Max, bPopulation](int32 Delta)
        { Draft.Get().*Member = FMath::Clamp(Draft.Get().*Member + Delta, Min, Max); if (bPopulation) Custom(); };
    };
    auto IntText = [Draft](int32 FLivingWorldOptions::*Member) { return [Draft, Member]() { return FText::AsNumber(Draft.Get().*Member); }; };

    // ---------------- Population ----------------
    TSharedRef<SVerticalBox> Population = SNew(SVerticalBox);
    Population->AddSlot().AutoHeight().Padding(0, 0, 0, 12)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1.f)[Label(TEXT("ACTIVITY PRESET"), Font("Bold", 13, 200), Ink)]
        + SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Font(Font("Bold", 12, 160))
            .ColorAndOpacity_Lambda([Draft]() { return Draft->Preset == ELivingPreset::Custom ? Amber : Muted; })
            .Text_Lambda([Draft]() { const TCHAR* Names[] = {TEXT("QUIET PRESET"), TEXT("BALANCED PRESET"), TEXT("BUSY PRESET"), TEXT("CUSTOM SETTINGS")};
                return FText::FromString(Names[FMath::Clamp(int32(Draft->Preset), 0, 3)]); })]
    ];
    {
        TSharedRef<SHorizontalBox> Cards = SNew(SHorizontalBox);
        const TCHAR* Titles[] = {TEXT("QUIET"), TEXT("BALANCED"), TEXT("BUSY")};
        const TCHAR* Notes[] = {TEXT("Room to explore"), TEXT("Everyday activity"), TEXT("A lively environment")};
        for (int32 I = 0; I < 3; ++I)
            Cards->AddSlot().FillWidth(1.f).Padding(I == 0 ? 0.f : 6.f, 0.f, I == 2 ? 0.f : 6.f, 0.f)
            [
                SNew(SButton).ButtonStyle(&B().Soft).ContentPadding(0)
                .OnClicked_Lambda([Draft, I]() { Draft->ApplyPreset(static_cast<ELivingPreset>(I)); return FReply::Handled(); })
                [
                    SNew(SBorder).Padding(FMargin(16, 14))
                    .BorderImage_Lambda([Draft, I]() -> const FSlateBrush* { return int32(Draft->Preset) == I ? &B().CardOn : &B().Card; })
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(Titles[I])).Font(Font("Bold", 15, 80))
                            .ColorAndOpacity_Lambda([Draft, I]() { return int32(Draft->Preset) == I ? Amber : Ink; })]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)[Label(Notes[I], Font("Regular", 13), Muted)]
                    ]
                ]
            ];
        Population->AddSlot().AutoHeight()[Cards];
    }
    {
        // Population mix dropdown. The list opens inline under the field: game viewports do not keep popup menus open.
        const TCHAR* Mixes[] = {TEXT("Civilians"), TEXT("Military"), TEXT("Mixed")};
        TSharedRef<bool> MixOpen = MakeShared<bool>(false);
        TSharedRef<SVerticalBox> Choices = SNew(SVerticalBox);
        for (int32 I = 0; I < 3; ++I)
            Choices->AddSlot().AutoHeight()
            [
                SNew(SButton).ButtonStyle(&B().Soft).ContentPadding(FMargin(20, 9))
                .OnClicked_Lambda([Draft, MixOpen, I]() { Draft->Population = static_cast<ELivingPopulation>(I); *MixOpen = false; return FReply::Handled(); })
                [SNew(STextBlock).Text(FText::FromString(Mixes[I])).Font(Font("Regular", 14))
                    .ColorAndOpacity_Lambda([Draft, I]() { return int32(Draft->Population) == I ? Amber : Ink; })]
            ];
        Population->AddSlot().AutoHeight().Padding(0, 22, 0, 0)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Top).Padding(0, 10, 0, 0)[Label(TEXT("Population mix"), Font("Bold", 16), Ink)]
            + SHorizontalBox::Slot().AutoWidth()
            [
                SNew(SBox).WidthOverride(156.f)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SButton).ButtonStyle(&B().Soft).ContentPadding(0).OnClicked_Lambda([MixOpen]() { *MixOpen = !*MixOpen; return FReply::Handled(); })
                        [
                            SNew(SBorder).BorderImage(&B().FieldBox).Padding(FMargin(20, 10, 14, 10))
                            [
                                SNew(SHorizontalBox)
                                + SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[SNew(STextBlock).Font(Font("Bold", 15)).ColorAndOpacity(Ink)
                                    .Text_Lambda([Draft]() { const TCHAR* N[] = {TEXT("Civilians"), TEXT("Military"), TEXT("Mixed")}; return FText::FromString(N[FMath::Clamp(int32(Draft->Population), 0, 2)]); })]
                                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                                    [SNew(SBox).WidthOverride(18.f).HeightOverride(18.f)[SNew(SImage).ColorAndOpacity(Ink)
                                        .Image_Lambda([MixOpen]() { return B().Icon(*MixOpen ? TEXT("chevron-up") : TEXT("chevron-down")); })]]
                            ]
                        ]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
                    [
                        SNew(SBorder).BorderImage(&B().FieldBox).Padding(2)
                        .Visibility_Lambda([MixOpen]() { return *MixOpen ? EVisibility::Visible : EVisibility::Collapsed; })[Choices]
                    ]
                ]
            ]
        ];
    }
    Population->AddSlot().AutoHeight().Padding(0, 20, 0, 0)[Rule()];
    Population->AddSlot().AutoHeight()[Section(TEXT("car"), TEXT("ON THE GROUND"))];
    Population->AddSlot().AutoHeight()[Row(TEXT("Crowds"), Segmented({TEXT("Off"), TEXT("Sparse"), TEXT("Normal"), TEXT("Dense")},
        [Draft]() { return Draft->CrowdDensity; }, [Draft, Custom](int32 V) { Draft->CrowdDensity = V; Custom(); }))];
    Population->AddSlot().AutoHeight().Padding(0, 6, 0, 0)[Row(TEXT("Ground traffic"), Segmented({TEXT("Off"), TEXT("Light"), TEXT("Normal"), TEXT("Heavy")},
        [Draft]() { return Draft->TrafficDensity; }, [Draft, Custom](int32 V) { Draft->TrafficDensity = V; Custom(); }))];
    Population->AddSlot().AutoHeight().Padding(0, 14, 0, 0)[Rule()];
    Population->AddSlot().AutoHeight()[Section(TEXT("plane"), TEXT("IN THE AIR"), []() { return FText::FromString(TEXT("Number of aircraft")); })];
    Population->AddSlot().AutoHeight()[Row(TEXT("Planes"), Stepper(IntText(&FLivingWorldOptions::Planes), IntStep(&FLivingWorldOptions::Planes, 0, 12, true)))];
    Population->AddSlot().AutoHeight().Padding(0, 6, 0, 0)[Row(TEXT("Helicopters"), Stepper(IntText(&FLivingWorldOptions::Helicopters), IntStep(&FLivingWorldOptions::Helicopters, 0, 12, true)))];
    Population->AddSlot().AutoHeight().Padding(0, 6, 0, 0)[Row(TEXT("Drones"), Stepper(IntText(&FLivingWorldOptions::Drones), IntStep(&FLivingWorldOptions::Drones, 0, 24, true)),
        TEXT("Quadcopters, FPV strike drones and Shahed-136s"))];
    Population->AddSlot().AutoHeight().Padding(0, 14, 0, 0)[Rule()];
    Population->AddSlot().AutoHeight()[Section(TEXT("bird"), TEXT("WILDLIFE"),
        [Draft]() { return FText::FromString(FString::Printf(TEXT("%d birds total"), Draft->BirdFlocks * Draft->FlockSize)); })];
    Population->AddSlot().AutoHeight()[Row(TEXT("Bird flocks"), Stepper(IntText(&FLivingWorldOptions::BirdFlocks), IntStep(&FLivingWorldOptions::BirdFlocks, 0, 8, true)))];
    Population->AddSlot().AutoHeight().Padding(0, 6, 0, 0)[Row(TEXT("Birds per flock"), Stepper(IntText(&FLivingWorldOptions::FlockSize), IntStep(&FLivingWorldOptions::FlockSize, 1, 24, true)))];
    {
        // Advanced settings, collapsed by default.
        TSharedRef<bool> Open = MakeShared<bool>(false);
        TSharedRef<SVerticalBox> Advanced = SNew(SVerticalBox);
        Advanced->AddSlot().AutoHeight()[Row(TEXT("Activity radius"), Stepper([Draft]() { return FText::FromString(FString::Printf(TEXT("%.0f m"), Draft->ActivityRadiusMeters)); },
            [Draft](int32 D) { Draft->ActivityRadiusMeters = FMath::Clamp(Draft->ActivityRadiusMeters + 100.f * D, 100.f, 3000.f); }), TEXT("Ambient life is kept within this distance of the view"))];
        Advanced->AddSlot().AutoHeight().Padding(0, 6, 0, 0)[Row(TEXT("Population limit"), Stepper(IntText(&FLivingWorldOptions::MaxActors),
            [Draft](int32 D) { Draft->MaxActors = FMath::Clamp(Draft->MaxActors + 10 * D, 10, 300); }), TEXT("Above about 200 agents the frame rate falls off"))];
        Advanced->AddSlot().AutoHeight().Padding(0, 6, 0, 0)[Row(TEXT("Scenario seed"), Stepper(IntText(&FLivingWorldOptions::Seed),
            [Draft](int32 D) { Draft->Seed = FMath::Max(0, Draft->Seed + D); }), TEXT("The same seed repeats the same spawns"))];
        Population->AddSlot().AutoHeight().Padding(0, 22, 0, 0)
        [
            SNew(SBorder).BorderImage(&B().Card).Padding(0)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(SButton).ButtonStyle(&B().Soft).ContentPadding(FMargin(20, 16)).OnClicked_Lambda([Open]() { *Open = !*Open; return FReply::Handled(); })
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 12, 0)[Icon(TEXT("settings"), Ink, 20.f)]
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Label(TEXT("ADVANCED SETTINGS"), Font("Bold", 13, 160), Ink)]
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12, 0, 0, 0)
                            [SNew(SBox).WidthOverride(18.f).HeightOverride(18.f)[SNew(SImage)
                                .Image_Lambda([Open]() { return B().Icon(*Open ? TEXT("chevron-up") : TEXT("chevron-down")); }).ColorAndOpacity(Ink)]]
                    ]
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(8, 0, 16, 14)
                [SNew(SBox).Visibility_Lambda([Open]() { return *Open ? EVisibility::Visible : EVisibility::Collapsed; })[Advanced]]
            ]
        ];
    }

    // ---------------- Behavior ----------------
    TSharedRef<SVerticalBox> Behavior = SNew(SVerticalBox);
    auto BoolRow = [Draft](const FString& Name, bool FLivingWorldOptions::*Member, const FString& Hint)
    {
        return Row(Name, Switch([Draft, Member]() { return Draft.Get().*Member; }, [Draft, Member]() { Draft.Get().*Member = !(Draft.Get().*Member); }), Hint);
    };
    Behavior->AddSlot().AutoHeight()[Section(TEXT("people"), TEXT("PEOPLE AND TRAFFIC"))];
    Behavior->AddSlot().AutoHeight()[BoolRow(TEXT("Reactive behavior"), &FLivingWorldOptions::bReactive,
        TEXT("People notice vehicles and threats, step aside or flee, and recover; drivers yield at crossings"))];
    Behavior->AddSlot().AutoHeight().Padding(0, 14, 0, 0)[Rule()];
    Behavior->AddSlot().AutoHeight()[Section(TEXT("bird"), TEXT("WILDLIFE"))];
    Behavior->AddSlot().AutoHeight()[BoolRow(TEXT("Bird landing sites"), &FLivingWorldOptions::bRuntimePerches,
        TEXT("Birds land and perch along walkways, and flush when people or vehicles come close"))];
    Behavior->AddSlot().AutoHeight().Padding(0, 14, 0, 0)[Rule()];
    Behavior->AddSlot().AutoHeight()[Section(TEXT("crosshair"), TEXT("VIRTUAL COMBAT"))];
    Behavior->AddSlot().AutoHeight()[BoolRow(TEXT("Combat targets"), &FLivingWorldOptions::bCombatTargets,
        TEXT("Aircraft, drones and vehicles can be designated and engaged by the simulated tripod (never people or birds)"))];
    Behavior->AddSlot().AutoHeight().Padding(0, 14, 0, 0)[Rule()];
    Behavior->AddSlot().AutoHeight()[Section(TEXT("sliders"), TEXT("SOUND"))];
    Behavior->AddSlot().AutoHeight()[Row(TEXT("Ambient volume"), Stepper([Draft]() { return FText::FromString(FString::Printf(TEXT("%.0f dB"), Draft->AmbientVolumeDb)); },
        [Draft](int32 D) { Draft->AmbientVolumeDb = FMath::Clamp(Draft->AmbientVolumeDb + 3.f * D, -60.f, 0.f); }))];

    // ---------------- Simulation ----------------
    TSharedRef<SVerticalBox> Simulation = SNew(SVerticalBox);
    Simulation->AddSlot().AutoHeight()[Section(TEXT("plane"), TEXT("CAMERA FEEDS"))];
    Simulation->AddSlot().AutoHeight()[Row(TEXT("Airborne sensor feeds"), Stepper(IntText(&FLivingWorldOptions::SensorStreams), IntStep(&FLivingWorldOptions::SensorStreams, 0, 4, false)),
        TEXT("Gimbal streams air-1 to air-4 carried by drones and aircraft; each costs about 1 fps"))];
    if (GetEngagementCamera())
    {
        auto Engage = [this](const TCHAR* Command) { return [this, Command]() { if (ASimPTZ* Camera = GetEngagementCamera()) Camera->ExecuteEngagementCommand(Command); }; };
        auto Fixed = [](const TCHAR* Value) { return [Value]() { return FText::FromString(Value); }; };
        auto Grid = [](TArray<TSharedRef<SWidget>> Buttons)
        {
            TSharedRef<SHorizontalBox> Cells = SNew(SHorizontalBox);
            for (int32 I = 0; I < Buttons.Num(); ++I) Cells->AddSlot().FillWidth(1.f).Padding(I ? 6.f : 0.f, 0, 0, 0)[Buttons[I]];
            return Cells;
        };
        Simulation->AddSlot().AutoHeight().Padding(0, 14, 0, 0)[Rule()];
        Simulation->AddSlot().AutoHeight()[Section(TEXT("crosshair"), TEXT("SIMULATED ENGAGEMENT"), []() { return FText::FromString(TEXT("Virtual only")); })];
        Simulation->AddSlot().AutoHeight()[Grid({
            Action(Fixed(TEXT("DESIGNATE  F7")), Engage(TEXT("designate"))), Action(Fixed(TEXT("NEXT  SHIFT+F7")), Engage(TEXT("next"))),
            Action([this]() { const ASimPTZ* C = EngagementCamera.Get(); return FText::FromString(C && C->bTrackTarget ? TEXT("STOP TRACK  CTRL+F7") : TEXT("TRACK  CTRL+F7")); },
                [this]() { if (ASimPTZ* C = GetEngagementCamera()) C->ExecuteEngagementCommand(C->bTrackTarget ? TEXT("track_off") : TEXT("track_on")); })})];
        Simulation->AddSlot().AutoHeight().Padding(0, 6, 0, 0)[Grid({
            Action(Fixed(TEXT("LAUNCH  F8")), Engage(TEXT("fire"))), Action(Fixed(TEXT("ABORT  SHIFT+F8")), Engage(TEXT("abort"))),
            Action([this]() { const ASimPTZ* C = EngagementCamera.Get(); return FText::FromString(FString::Printf(TEXT("VIEW: %s  F6"), C ? *C->GetViewName().ToUpper() : TEXT("-"))); }, Engage(TEXT("view")))})];
        Simulation->AddSlot().AutoHeight().Padding(0, 6, 0, 0)[Grid({
            Action(Fixed(TEXT("STRIKE")), Engage(TEXT("strike"))), Action(Fixed(TEXT("FIRE")), Engage(TEXT("burn"))), Action(Fixed(TEXT("SMOKE SCREEN")), Engage(TEXT("smoke_screen")))})];
        Simulation->AddSlot().AutoHeight().Padding(4, 10, 0, 0)[SNew(STextBlock).AutoWrapText(true).Font(Font("Regular", 12)).ColorAndOpacity(Amber)
            .Text_Lambda([this]() { return EngagementText(); })];
    }
    Simulation->AddSlot().AutoHeight().Padding(0, 14, 0, 0)[Rule()];
    Simulation->AddSlot().AutoHeight()[Section(TEXT("sliders"), TEXT("RECORDING AND REPLAY"))];
    Simulation->AddSlot().AutoHeight()
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1.f)[Action([this]() { return FText::FromString(IsValid(Recorder) ? TEXT("STOP AND SAVE") : TEXT("RECORD POPULATION")); }, [this]() { ToggleRecording(); })]
        + SHorizontalBox::Slot().FillWidth(1.f).Padding(6, 0, 0, 0)[Action([]() { return FText::FromString(TEXT("OVERLAY LAST RECORDING")); }, [this]() { PlayLastRecording(); })]
        + SHorizontalBox::Slot().FillWidth(1.f).Padding(6, 0, 0, 0)[Action([]() { return FText::FromString(TEXT("CLEAR REPLAY")); }, [this]() { ClearReplay(); })]
    ];
    Simulation->AddSlot().AutoHeight().Padding(4, 10, 0, 0)[SNew(STextBlock).AutoWrapText(true).Font(Font("Regular", 12)).ColorAndOpacity(Muted)
        .Text_Lambda([this]() { return FText::FromString(RecordingStatus); })];
    Simulation->AddSlot().AutoHeight().Padding(0, 14, 0, 0)[Rule()];
    Simulation->AddSlot().AutoHeight()[Section(TEXT("people"), TEXT("STATUS"))];
    Simulation->AddSlot().AutoHeight().Padding(4, 0, 0, 0)[SNew(STextBlock).AutoWrapText(true).Font(Font("Regular", 13)).ColorAndOpacity(Ink)
        .Text_Lambda([this]() { return FText::FromString(Status); })];

    // ---------------- Frame ----------------
    auto Nav = [this](int32 Index, FName IconName, const FString& Title)
    {
        return SNew(SButton).ButtonStyle(&B().Flat).ContentPadding(0).OnClicked_Lambda([this, Index]() { MenuTab = Index; return FReply::Handled(); })
        [
            SNew(SBorder).BorderImage(&B().White).Padding(0)
            .BorderBackgroundColor_Lambda([this, Index]() { return MenuTab == Index ? AmberTint : FLinearColor::Transparent; })
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(4.f)
                    [SNew(SBorder).BorderImage(&B().White).BorderBackgroundColor_Lambda([this, Index]() { return MenuTab == Index ? Amber : FLinearColor::Transparent; })]]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(20, 18, 14, 18)
                    [SNew(SBox).WidthOverride(22.f).HeightOverride(22.f)[SNew(SImage).Image(B().Icon(IconName))
                        .ColorAndOpacity_Lambda([this, Index]() { return MenuTab == Index ? Amber : Muted; })]]
                + SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
                    [SNew(STextBlock).Text(FText::FromString(Title)).Font(Font("Bold", 15, 150))
                        .ColorAndOpacity_Lambda([this, Index]() { return MenuTab == Index ? Amber : Hex(0xC9D1D9); })]
            ]
        ];
    };
    TSharedRef<SButton> ApplyButton = SNew(SButton).ButtonStyle(&B().Soft).ContentPadding(0)
        .OnClicked_Lambda([this]() { ApplyMenuDraft(); return FReply::Handled(); })
        [
            SNew(SBorder).BorderImage(&B().ApplyBox).Padding(FMargin(26, 14))
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 14, 0)
                    [SNew(SBorder).BorderImage(&B().KeyBox).Padding(FMargin(8, 3))[Label(TEXT("F"), Font("Bold", 12), ApplyText)]]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Label(TEXT("APPLY & SAVE"), Font("Bold", 14, 180), ApplyText)]
            ]
        ];
    MenuFocus = ApplyButton;

    return SNew(SBox).WidthOverride(1120.f).HeightOverride(980.f)
    [
        SNew(SBorder).BorderImage(&B().White).BorderBackgroundColor(Panel).Padding(0)
        [
            SNew(SVerticalBox)
            // Header.
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(SBorder).BorderImage(&B().White).BorderBackgroundColor(Header).Padding(FMargin(34, 30, 34, 30))
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 40, 0)[Label(TEXT("RUNE SIM"), Font("Bold", 16, 380), Ink)]
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Label(TEXT("SETTINGS"), Font("Regular", 13, 280), Muted)]
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(14, 0)[Icon(TEXT("chevron-right"), Muted, 16.f)]
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Label(TEXT("ENVIRONMENT"), Font("Regular", 13, 280), Hex(0xC9D1D9))]
                        + SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Right).VAlign(VAlign_Center)[Label(TEXT("WORLD CONFIGURATION"), Font("Regular", 12, 300), Muted)]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 22, 0, 0)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().FillWidth(1.f)
                        [
                            SNew(SVerticalBox)
                            + SVerticalBox::Slot().AutoHeight()[Label(TEXT("LIVING WORLD"), Font("Bold", 40, 60), Ink)]
                            + SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 0)[Label(TEXT("Configure the life and movement around you."), Font("Regular", 15), Hex(0xC9D1D9))]
                        ]
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 18, 0)
                            [Switch([Draft]() { return Draft->bEnabled; }, [Draft]() { Draft->bEnabled = !Draft->bEnabled; })]
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                            [SNew(SBox).MinDesiredWidth(96.f)[SNew(STextBlock).Font(Font("Bold", 15, 220)).ColorAndOpacity(Ink)
                                .Text_Lambda([Draft]() { return FText::FromString(Draft->bEnabled ? TEXT("ENABLED") : TEXT("DISABLED")); })]]
                    ]
                ]
            ]
            + SVerticalBox::Slot().AutoHeight()[Rule()]
            // Body: rail and content.
            + SVerticalBox::Slot().FillHeight(1.f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth()
                [
                    SNew(SBox).WidthOverride(230.f)
                    [
                        SNew(SBorder).BorderImage(&B().White).BorderBackgroundColor(Rail).Padding(FMargin(8, 32, 18, 16))
                        [
                            SNew(SVerticalBox)
                            + SVerticalBox::Slot().AutoHeight().Padding(18, 0, 0, 24)[Label(TEXT("WORLD SETTINGS"), Font("Regular", 12, 300), Muted)]
                            + SVerticalBox::Slot().AutoHeight()[Nav(0, TEXT("people"), TEXT("POPULATION"))]
                            + SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)[Nav(1, TEXT("sliders"), TEXT("BEHAVIOR"))]
                            + SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)[Nav(2, TEXT("crosshair"), TEXT("SIMULATION"))]
                        ]
                    ]
                ]
                + SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(1.f)[SNew(SBorder).BorderImage(&B().White).BorderBackgroundColor(Divider)]]
                + SHorizontalBox::Slot().FillWidth(1.f)
                [
                    SNew(SScrollBox)
                    + SScrollBox::Slot().Padding(FMargin(36, 30, 34, 30))
                    [
                        SNew(SWidgetSwitcher).WidgetIndex_Lambda([this]() { return MenuTab; })
                        + SWidgetSwitcher::Slot()[Population]
                        + SWidgetSwitcher::Slot()[Behavior]
                        + SWidgetSwitcher::Slot()[Simulation]
                    ]
                ]
            ]
            + SVerticalBox::Slot().AutoHeight()[Rule()]
            // Footer.
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(SBorder).BorderImage(&B().White).BorderBackgroundColor(Rail).Padding(FMargin(28, 18))
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 12, 0)
                        [SNew(SBox).WidthOverride(8.f).HeightOverride(8.f)[SNew(SBorder).BorderImage(&B().White)
                            .BorderBackgroundColor_Lambda([this, Draft]() { return LivingMenu::Same(*Draft, Options) ? Amber : Hex(0xE07B5A); })]]
                    + SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
                        [SNew(STextBlock).Font(Font("Regular", 14)).ColorAndOpacity(Ink)
                            .Text_Lambda([this, Draft]() { return FText::FromString(LivingMenu::Same(*Draft, Options) ? TEXT("All changes saved") : TEXT("Unsaved changes")); })]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 30, 0)
                    [
                        SNew(SButton).ButtonStyle(&B().Flat).ContentPadding(FMargin(8, 6)).OnClicked_Lambda([this]() { DiscardMenuDraft(); return FReply::Handled(); })
                        [SNew(STextBlock).Text(FText::FromString(TEXT("DISCARD CHANGES"))).Font(Font("Bold", 13, 200))
                            .ColorAndOpacity_Lambda([this, Draft]() { return LivingMenu::Same(*Draft, Options) ? Dim : Hex(0xC9D1D9); })]
                    ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ApplyButton]
                ]
            ]
        ]
    ];
}

void ULivingWorldSubsystem::InstallMenu()
{
    using namespace LivingMenu;
    if (!GetWorld()->GetGameViewport()) return;
    MenuRoot = SNew(SOverlay)
        // Entry point, top right: Settings > Environment > Living World.
        + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(24.f, 60.f)
        [
            SNew(SButton).ButtonStyle(&B().Flat).ContentPadding(0)
            .Visibility_Lambda([this]() { return bMenuOpen ? EVisibility::Collapsed : EVisibility::Visible; })
            .OnClicked_Lambda([this]() { ToggleMenu(); return FReply::Handled(); })
            [
                SNew(SBorder).BorderImage(&B().Card).BorderBackgroundColor(FLinearColor(1.f, 1.f, 1.f, .92f)).Padding(FMargin(14, 9))
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)[Icon(TEXT("people"), Amber, 18.f)]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Label(TEXT("LIVING WORLD"), Font("Bold", 12, 200), Ink)]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12, 0, 0, 0)
                        [SNew(SBorder).BorderImage(&B().KeyBox).Padding(FMargin(6, 2))[Label(TEXT("F9"), Font("Bold", 10), Muted)]]
                ]
            ]
        ]
        // Engagement readout, bottom left.
        + SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(24.f, 24.f)
        [
            SNew(SBorder).BorderImage(&B().White).BorderBackgroundColor(Hex(0x0F1720, .8f)).Padding(FMargin(12, 8))
            .Visibility_Lambda([this]() { return EngagementCamera.IsValid() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)[Icon(TEXT("crosshair"), Amber, 16.f)]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [SNew(STextBlock).Font(Font("Regular", 11)).ColorAndOpacity(Amber).Text_Lambda([this]() { return EngagementText(); })]
            ]
        ]
        // The panel itself, centred over a dimmed view.
        + SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
        [
            SAssignNew(MenuPanel, SBorder).BorderImage(&B().White).BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, .45f))
            .HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(24.f)
            // Clicks around the panel must not reach the game, which would capture and hide the cursor.
            .OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
        ];
    MenuPanel->SetVisibility(EVisibility::Collapsed);
    GetWorld()->GetGameViewport()->AddViewportWidgetContent(MenuRoot.ToSharedRef(), 30);
}

void ULivingWorldSubsystem::ToggleMenu()
{
    if (bMenuOpen) { CloseMenu(); return; }
    if (!MenuPanel.IsValid()) InstallMenu();
    if (!MenuPanel.IsValid()) return;
    MenuPanel->SetContent(BuildMenuContent());
    MenuPanel->SetVisibility(EVisibility::Visible);
    bMenuOpen = true;
    MenuController = GetWorld()->GetFirstPlayerController();
    if (MenuController.IsValid())
    {
        bPreviousCursor = MenuController->bShowMouseCursor;
        MenuController->bShowMouseCursor = true;
        MenuController->SetIgnoreMoveInput(true); MenuController->SetIgnoreLookInput(true);
        FInputModeGameAndUI Mode; Mode.SetWidgetToFocus(MenuFocus); Mode.SetHideCursorDuringCapture(false);
        MenuController->SetInputMode(Mode);
    }
    if (MenuFocus.IsValid()) FSlateApplication::Get().SetKeyboardFocus(MenuFocus);
}

void ULivingWorldSubsystem::CloseMenu()
{
    if (!bMenuOpen) return;
    bMenuOpen = false;
    if (MenuPanel.IsValid()) { MenuPanel->SetVisibility(EVisibility::Collapsed); MenuPanel->SetContent(SNullWidget::NullWidget); }
    MenuFocus.Reset(); MenuDraft.Reset();
    if (MenuController.IsValid())
    {
        MenuController->bShowMouseCursor = bPreviousCursor;
        MenuController->SetIgnoreMoveInput(false); MenuController->SetIgnoreLookInput(false);
        MenuController->SetInputMode(FInputModeGameOnly());
    }
}
