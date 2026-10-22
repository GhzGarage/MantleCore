// Copyright Joshua Eger. All Rights Reserved.
#include "SMantleThumbnailSection.h"

#include "DesktopPlatformModule.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "IDesktopPlatform.h"
#include "Misc/Paths.h"
#include "PropertyCustomizationHelpers.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateBrush.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "MantleThumbnailSection"

namespace
{
	/** How big the preview is drawn: a square one is 256, a wide one is 480 across. */
	FVector2f PreviewSize(const FMantleThumbnailTarget& Target)
	{
		return Target.Width == Target.Height ? FVector2f(256.0f, 256.0f) : FVector2f(480.0f, 480.0f * Target.Height / Target.Width);
	}
}

void SMantleThumbnailSection::Construct(const FArguments& InArgs)
{
	ChoiceAttribute = InArgs._Choice;
	Target = InArgs._Target;
	OnChoiceChanged = InArgs._OnChoiceChanged;
	OnUseView = InArgs._OnUseView;
	Render = InArgs._Render;
	UseViewLabel = InArgs._UseViewLabel.IsEmpty() ? FMantleThumbnailText::UseThisView() : InArgs._UseViewLabel;

	const FVector2f Size = PreviewSize(Target);
	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
		[
			SNew(STextBlock).Font(FAppStyle::GetFontStyle("BoldFont")).Text(FMantleThumbnailText::Heading())
		]
		+ SVerticalBox::Slot().AutoHeight() [ MakeModeRow(EMantleThumbnailMode::Automatic) ]
		+ SVerticalBox::Slot().AutoHeight() [ MakeModeRow(EMantleThumbnailMode::Angle) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(24.0f, 0.0f, 0.0f, 4.0f)
		[
			SNew(SVerticalBox)
			.Visibility_Lambda([this] { return Current().Mode == EMantleThumbnailMode::Angle ? EVisibility::Visible : EVisibility::Collapsed; })
			+ SVerticalBox::Slot().AutoHeight() [ InArgs._AngleContent.Widget ]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.Text(UseViewLabel)
					.IsEnabled_Lambda([this] { return OnUseView.IsBound(); })
					.OnClicked_Lambda([this]
					{
						ViewNote = OnUseView.IsBound() ? OnUseView.Execute() : FText::GetEmpty();
						Refresh();
						return FReply::Handled();
					})
				]
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).AutoWrapText(true).ColorAndOpacity(FLinearColor(1.0f, 0.4f, 0.3f))
				.Visibility_Lambda([this] { return ViewNote.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
				.Text_Lambda([this] { return ViewNote; })
			]
		]
		+ SVerticalBox::Slot().AutoHeight() [ MakeModeRow(EMantleThumbnailMode::Custom) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(24.0f, 0.0f, 0.0f, 4.0f)
		[
			MakeCustomControls()
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 8.0f, 0.0f, 4.0f)
		[
			SNew(SBox).WidthOverride(Size.X).HeightOverride(Size.Y)
			[
				SNew(SImage)
				.Image_Lambda([this]() -> const FSlateBrush* { return Brush.IsValid() ? Brush.Get() : FAppStyle::GetBrush("WhiteBrush"); })
				.ColorAndOpacity_Lambda([this] { return Brush.IsValid() ? FSlateColor(FLinearColor::White) : FSlateColor(FLinearColor(0.02f, 0.02f, 0.02f)); })
			]
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
		[
			SNew(STextBlock).AutoWrapText(true).ColorAndOpacity(FSlateColor::UseSubduedForeground()).Text(FMantleThumbnailText::PreviewCaption(Target))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock).AutoWrapText(true)
			.Visibility_Lambda([this] { return Result.Warning.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
			.ColorAndOpacity(FLinearColor(1.0f, 0.75f, 0.2f))
			.Text_Lambda([this] { return FText::FromString(Result.Warning); })
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock).AutoWrapText(true)
			.Visibility_Lambda([this] { return Result.Error.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
			.ColorAndOpacity(FLinearColor(1.0f, 0.4f, 0.3f))
			.Text_Lambda([this] { return FText::FromString(Result.Error); })
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left).Padding(0.0f, 4.0f, 0.0f, 0.0f)
		[
			SNew(SButton)
			.Text(LOCTEXT("RefreshPreview", "Refresh picture"))
			.ToolTipText(LOCTEXT("RefreshPreviewTip", "Draw the picture again from the current choice."))
			.OnClicked_Lambda([this] { Refresh(); return FReply::Handled(); })
		]
	];
	Refresh();
}

FMantleThumbnailChoice SMantleThumbnailSection::Current() const
{
	return ChoiceAttribute.Get(FMantleThumbnailChoice());
}

void SMantleThumbnailSection::SetChoice(const FMantleThumbnailChoice& NewChoice)
{
	ViewNote = FText::GetEmpty();
	OnChoiceChanged.ExecuteIfBound(NewChoice);
	Refresh();
}

void SMantleThumbnailSection::SetMode(EMantleThumbnailMode Mode)
{
	FMantleThumbnailChoice Next = Current();
	if (Next.Mode != Mode)
	{
		Next.Mode = Mode;
		SetChoice(Next);
	}
}

TSharedRef<SWidget> SMantleThumbnailSection::MakeModeRow(EMantleThumbnailMode Mode)
{
	return SNew(SCheckBox)
		.Style(FAppStyle::Get(), "RadioButton")
		.IsChecked_Lambda([this, Mode] { return Current().Mode == Mode ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
		.OnCheckStateChanged_Lambda([this, Mode](ECheckBoxState State)
		{
			if (State == ECheckBoxState::Checked)
			{
				SetMode(Mode);
			}
		})
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(4.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock).Text(FMantleThumbnailText::ModeName(Mode))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(4.0f, 0.0f, 0.0f, 4.0f)
			[
				SNew(STextBlock).AutoWrapText(true).ColorAndOpacity(FSlateColor::UseSubduedForeground()).Text(FMantleThumbnailText::ModeHelp(Mode))
			]
		];
}

TSharedRef<SWidget> SMantleThumbnailSection::MakeCustomControls()
{
	return SNew(SVerticalBox)
		.Visibility_Lambda([this] { return Current().Mode == EMantleThumbnailMode::Custom ? EVisibility::Visible : EVisibility::Collapsed; })
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(STextBlock).Text(LOCTEXT("CustomTexture", "Texture"))
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f)
			[
				SNew(SObjectPropertyEntryBox)
				.AllowedClass(UTexture2D::StaticClass())
				.AllowClear(true)
				.DisplayThumbnail(false)
				.ObjectPath_Lambda([this] { return Current().Texture.ToString(); })
				.OnObjectChanged_Lambda([this](const FAssetData& Asset)
				{
					FMantleThumbnailChoice Next = Current();
					Next.Texture = TSoftObjectPtr<UTexture2D>(Asset.GetSoftObjectPath());
					if (!Next.Texture.IsNull())
					{
						// One source at a time, so the picture shown is the picture named.
						Next.ImageFile.FilePath.Reset();
					}
					SetChoice(Next);
				})
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(STextBlock).Text(LOCTEXT("CustomFile", "PNG file"))
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(SEditableTextBox)
				.Text_Lambda([this] { return FText::FromString(Current().ImageFile.FilePath); })
				.HintText(LOCTEXT("CustomFileHint", "Path to a .png file"))
				.OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type)
				{
					FMantleThumbnailChoice Next = Current();
					if (Next.ImageFile.FilePath != Text.ToString())
					{
						Next.ImageFile.FilePath = Text.ToString();
						Next.Texture.Reset();
						SetChoice(Next);
					}
				})
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("CustomBrowse", "Browse..."))
				.OnClicked_Lambda([this]
				{
					IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
					if (!DesktopPlatform)
					{
						return FReply::Handled();
					}
					const void* ParentWindow = FSlateApplication::IsInitialized() ? FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr) : nullptr;
					const FString Existing = Current().ImageFile.FilePath;
					TArray<FString> Files;
					if (DesktopPlatform->OpenFileDialog(ParentWindow, TEXT("Choose the thumbnail picture"), Existing.IsEmpty() ? FString() : FPaths::GetPath(Existing),
						FString(), TEXT("PNG image (*.png)|*.png"), EFileDialogFlags::None, Files) && !Files.IsEmpty())
					{
						FMantleThumbnailChoice Next = Current();
						Next.ImageFile.FilePath = FPaths::ConvertRelativePathToFull(Files[0]);
						Next.Texture.Reset();
						SetChoice(Next);
					}
					return FReply::Handled();
				})
			]
		];
}

void SMantleThumbnailSection::Refresh()
{
	Result = FMantleThumbnailResolver::Resolve(Current(), Target, Render);
	Brush.Reset();
	if (Result.Succeeded())
	{
		// FColor is BGRA in memory, the order the dynamic brush takes. A new name each time, so Slate never shows the old picture.
		TArray<uint8> Bytes;
		Bytes.Append(reinterpret_cast<const uint8*>(Result.Pixels.GetData()), Result.Pixels.Num() * sizeof(FColor));
		static int32 Serial = 0;
		Brush = FSlateDynamicImageBrush::CreateWithImageData(*FString::Printf(TEXT("MantleThumbnailSection_%d"), ++Serial),
			FVector2f(Target.Width, Target.Height), Bytes);
	}
	Invalidate(EInvalidateWidgetReason::Layout | EInvalidateWidgetReason::Paint);
}

#undef LOCTEXT_NAMESPACE
