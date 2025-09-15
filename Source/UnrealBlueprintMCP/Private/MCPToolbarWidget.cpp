#include "MCPToolbarWidget.h"
#include "SlateOptMacros.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SHorizontalBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "EditorStyleSet.h"
#include "Engine/Engine.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SMCPToolbarWidget::Construct(const FArguments& InArgs)
{
	OnServerAction = InArgs._OnServerAction;

	ChildSlot
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.0f)
		[
			// Server status icon
			SAssignNew(StatusIcon, SImage)
			.Image(this, &SMCPToolbarWidget::GetStatusIconBrush)
			.ColorAndOpacity(FSlateColor::UseForeground())
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(4.0f, 2.0f)
		[
			// Status text
			SAssignNew(StatusText, STextBlock)
			.Text(this, &SMCPToolbarWidget::GetStatusText)
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.0f)
		[
			// Network activity indicator
			SAssignNew(NetworkActivityIndicator, SImage)
			.Image(this, &SMCPToolbarWidget::GetNetworkActivityBrush)
			.ColorAndOpacity(FLinearColor::Green)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.0f)
		[
			SNew(SSeparator)
			.Orientation(Orient_Vertical)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.0f)
		[
			// Start/Stop button
			SAssignNew(StartStopButton, SButton)
			.ButtonStyle(FEditorStyle::Get(), "FlatButton.Success")
			.OnClicked(this, &SMCPToolbarWidget::OnStartStopClicked)
			.ToolTipText(NSLOCTEXT("MCPToolbar", "StartStopTooltip", "Start or stop the MCP server"))
			[
				SNew(STextBlock)
				.Text_Lambda([this]() -> FText
				{
					return bServerRunning ? 
						NSLOCTEXT("MCPToolbar", "StopButton", "Stop") : 
						NSLOCTEXT("MCPToolbar", "StartButton", "Start");
				})
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.0f)
		[
			// Server info button
			SNew(SButton)
			.ButtonStyle(FEditorStyle::Get(), "FlatButton.Default")
			.OnClicked(this, &SMCPToolbarWidget::OnServerInfoClicked)
			.ToolTipText(NSLOCTEXT("MCPToolbar", "InfoTooltip", "Show server information"))
			[
				SNew(SImage)
				.Image(FEditorStyle::GetBrush("Icons.Info"))
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.0f)
		[
			// Quick restart button
			SNew(SButton)
			.ButtonStyle(FEditorStyle::Get(), "FlatButton.Default")
			.OnClicked(this, &SMCPToolbarWidget::OnQuickRestartClicked)
			.ToolTipText(NSLOCTEXT("MCPToolbar", "RestartTooltip", "Quick restart server"))
			.IsEnabled_Lambda([this]() { return bServerRunning; })
			[
				SNew(SImage)
				.Image(FEditorStyle::GetBrush("Icons.Refresh"))
			]
		]
	];
}

void SMCPToolbarWidget::UpdateServerStatus(bool bIsRunning, int32 Port, int32 ClientCount)
{
	bServerRunning = bIsRunning;
	CurrentPort = Port;
	this->ClientCount = ClientCount;

	// Update button style based on server status
	if (StartStopButton.IsValid())
	{
		FName ButtonStyle = bServerRunning ? 
			FName("FlatButton.Danger") : 
			FName("FlatButton.Success");
		StartStopButton->SetButtonStyle(FEditorStyle::Get(), ButtonStyle);
	}
}

void SMCPToolbarWidget::UpdateNetworkActivity(bool bIsActive)
{
	if (NetworkActivityIndicator.IsValid())
	{
		// Animate network activity indicator
		NetworkActivityIndicator->SetColorAndOpacity(bIsActive ? FLinearColor::Green : FLinearColor::Gray);
	}
}

FReply SMCPToolbarWidget::OnStartStopClicked()
{
	FString Action = bServerRunning ? TEXT("Stop") : TEXT("Start");
	OnServerAction.ExecuteIfBound(Action);
	return FReply::Handled();
}

FReply SMCPToolbarWidget::OnServerInfoClicked()
{
	OnServerAction.ExecuteIfBound(TEXT("ShowInfo"));
	return FReply::Handled();
}

FReply SMCPToolbarWidget::OnQuickRestartClicked()
{
	OnServerAction.ExecuteIfBound(TEXT("Restart"));
	return FReply::Handled();
}

const FSlateBrush* SMCPToolbarWidget::GetStatusIconBrush() const
{
	if (bServerRunning)
	{
		return FEditorStyle::GetBrush("Icons.CircleFilled");
	}
	else
	{
		return FEditorStyle::GetBrush("Icons.Circle");
	}
}

FText SMCPToolbarWidget::GetStatusText() const
{
	if (bServerRunning)
	{
		return FText::FromString(FString::Printf(TEXT("MCP:%d (%d)"), CurrentPort, ClientCount));
	}
	else
	{
		return NSLOCTEXT("MCPToolbar", "Stopped", "MCP:Stopped");
	}
}

const FSlateBrush* SMCPToolbarWidget::GetNetworkActivityBrush() const
{
	return FEditorStyle::GetBrush("Icons.Circle");
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION