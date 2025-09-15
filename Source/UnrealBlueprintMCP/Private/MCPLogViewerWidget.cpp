#include "MCPLogViewerWidget.h"
#include "SlateOptMacros.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Views/STableRow.h"
#include "EditorStyleSet.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/FileHelper.h"
#include "Misc/DateTime.h"
#include "Framework/Application/SlateApplication.h"
#include "DesktopPlatformModule.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SMCPLogViewerWidget::Construct(const FArguments& InArgs)
{
	OnLogAction = InArgs._OnLogAction;

	// Initialize log level options
	LogLevelOptions.Add(MakeShareable(new FString(TEXT("All"))));
	LogLevelOptions.Add(MakeShareable(new FString(TEXT("Error"))));
	LogLevelOptions.Add(MakeShareable(new FString(TEXT("Warning"))));
	LogLevelOptions.Add(MakeShareable(new FString(TEXT("Info"))));
	LogLevelOptions.Add(MakeShareable(new FString(TEXT("Debug"))));

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(4.0f)
		[
			SNew(SVerticalBox)
			// Header with controls
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				SNew(SHorizontalBox)
				// Log level filter
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 8.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("MCPLogViewer", "LogLevel", "Level:"))
					.VAlign(VAlign_Center)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 16.0f, 0.0f)
				[
					SAssignNew(LogLevelFilter, SComboBox<TSharedPtr<FString>>)
					.OptionsSource(&LogLevelOptions)
					.OnGenerateWidget_Lambda([](TSharedPtr<FString> Option)
					{
						return SNew(STextBlock).Text(FText::FromString(*Option));
					})
					.OnSelectionChanged(this, &SMCPLogViewerWidget::OnLogLevelFilterChanged)
					[
						SNew(STextBlock)
						.Text(this, &SMCPLogViewerWidget::GetCurrentFilterText)
					]
				]
				
				// Auto-scroll checkbox
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 16.0f, 0.0f)
				[
					SAssignNew(AutoScrollCheckBox, SCheckBox)
					.IsChecked_Lambda([this]() { return bAutoScroll ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged(this, &SMCPLogViewerWidget::OnAutoScrollChanged)
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("MCPLogViewer", "AutoScroll", "Auto Scroll"))
					]
				]
				
				// Log count
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					SAssignNew(LogCountText, STextBlock)
					.Text(this, &SMCPLogViewerWidget::GetLogCountText)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
				]
				
				// Action buttons
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4.0f, 0.0f)
				[
					SNew(SButton)
					.ButtonStyle(FEditorStyle::Get(), "FlatButton.Default")
					.OnClicked(this, &SMCPLogViewerWidget::OnClearLogsClicked)
					.ToolTipText(NSLOCTEXT("MCPLogViewer", "ClearTooltip", "Clear all logs"))
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("MCPLogViewer", "Clear", "Clear"))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4.0f, 0.0f)
				[
					SNew(SButton)
					.ButtonStyle(FEditorStyle::Get(), "FlatButton.Default")
					.OnClicked(this, &SMCPLogViewerWidget::OnExportLogsClicked)
					.ToolTipText(NSLOCTEXT("MCPLogViewer", "ExportTooltip", "Export logs to file"))
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("MCPLogViewer", "Export", "Export"))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
					]
				]
			]
			
			// Log list view
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SAssignNew(LogListView, SListView<TSharedPtr<FMCPLogEntry>>)
				.ListItemsSource(&FilteredLogEntries)
				.OnGenerateRow(this, &SMCPLogViewerWidget::OnGenerateLogRow)
				.SelectionMode(ESelectionMode::Single)
				.ScrollbarVisibility(EVisibility::Visible)
			]
		]
	];

	// Set default filter selection
	if (LogLevelFilter.IsValid() && LogLevelOptions.Num() > 0)
	{
		LogLevelFilter->SetSelectedItem(LogLevelOptions[0]); // "All"
	}
}

void SMCPLogViewerWidget::AddLogEntry(const FString& Level, const FString& Category, const FString& Message)
{
	TSharedPtr<FMCPLogEntry> NewEntry = MakeShareable(new FMCPLogEntry(Level, Category, Message));
	
	LogEntries.Insert(NewEntry, 0); // Add to beginning for newest first
	TrimLogEntries();
	RefreshFilteredLogs();
	ScrollToBottomIfNeeded();
}

void SMCPLogViewerWidget::ClearLogs()
{
	LogEntries.Empty();
	FilteredLogEntries.Empty();
	
	if (LogListView.IsValid())
	{
		LogListView->RequestListRefresh();
	}
}

void SMCPLogViewerWidget::ExportLogs(const FString& FilePath)
{
	TArray<FString> LogLines;
	LogLines.Add(TEXT("MCP Server Log Export"));
	LogLines.Add(FString::Printf(TEXT("Exported: %s"), *FDateTime::Now().ToString()));
	LogLines.Add(TEXT(""));
	LogLines.Add(TEXT("Timestamp\tLevel\tCategory\tMessage"));
	
	for (const auto& Entry : LogEntries)
	{
		if (Entry.IsValid())
		{
			FString LogLine = FString::Printf(TEXT("%s\t%s\t%s\t%s"),
				*Entry->Timestamp.ToString(),
				*Entry->Level,
				*Entry->Category,
				*Entry->Message
			);
			LogLines.Add(LogLine);
		}
	}
	
	FFileHelper::SaveStringArrayToFile(LogLines, *FilePath);
}

void SMCPLogViewerWidget::SetLogLevelFilter(const FString& Level)
{
	CurrentLogFilter = Level;
	
	// Update combo box selection
	for (const auto& Option : LogLevelOptions)
	{
		if (Option.IsValid() && *Option == Level)
		{
			if (LogLevelFilter.IsValid())
			{
				LogLevelFilter->SetSelectedItem(Option);
			}
			break;
		}
	}
	
	RefreshFilteredLogs();
}

void SMCPLogViewerWidget::SetAutoScroll(bool bEnabled)
{
	bAutoScroll = bEnabled;
}

TSharedRef<ITableRow> SMCPLogViewerWidget::OnGenerateLogRow(TSharedPtr<FMCPLogEntry> Entry, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FMCPLogEntry>>, OwnerTable)
	[
		SNew(SHorizontalBox)
		// Timestamp
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Entry->Timestamp.ToString(TEXT("%H:%M:%S"))))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		]
		// Level
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Entry->Level))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
			.ColorAndOpacity(GetLogLevelColor(Entry->Level))
		]
		// Category
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Entry->Category))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		]
		// Message
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.Padding(2.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Entry->Message))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
			.AutoWrapText(true)
		]
	];
}

void SMCPLogViewerWidget::RefreshFilteredLogs()
{
	FilteredLogEntries.Empty();
	
	for (const auto& Entry : LogEntries)
	{
		if (!Entry.IsValid())
		{
			continue;
		}
		
		// Apply level filter
		if (CurrentLogFilter != TEXT("All") && Entry->Level != CurrentLogFilter)
		{
			continue;
		}
		
		FilteredLogEntries.Add(Entry);
	}
	
	if (LogListView.IsValid())
	{
		LogListView->RequestListRefresh();
	}
}

void SMCPLogViewerWidget::OnLogLevelFilterChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (NewSelection.IsValid())
	{
		CurrentLogFilter = *NewSelection;
		RefreshFilteredLogs();
	}
}

void SMCPLogViewerWidget::OnAutoScrollChanged(ECheckBoxState NewState)
{
	bAutoScroll = (NewState == ECheckBoxState::Checked);
	
	if (bAutoScroll)
	{
		ScrollToBottomIfNeeded();
	}
}

FReply SMCPLogViewerWidget::OnClearLogsClicked()
{
	ClearLogs();
	OnLogAction.ExecuteIfBound(TEXT("ClearLogs"));
	return FReply::Handled();
}

FReply SMCPLogViewerWidget::OnExportLogsClicked()
{
	// Open file dialog
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		TArray<FString> OutFileNames;
		FString DefaultFileName = FString::Printf(TEXT("MCP_Logs_%s.txt"), 
			*FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
		
		bool bResult = DesktopPlatform->SaveFileDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			TEXT("Export MCP Logs"),
			FPaths::ProjectLogDir(),
			DefaultFileName,
			TEXT("Text files (*.txt)|*.txt"),
			EFileDialogFlags::None,
			OutFileNames
		);
		
		if (bResult && OutFileNames.Num() > 0)
		{
			ExportLogs(OutFileNames[0]);
			OnLogAction.ExecuteIfBound(FString::Printf(TEXT("ExportLogs:%s"), *OutFileNames[0]));
		}
	}
	
	return FReply::Handled();
}

FSlateColor SMCPLogViewerWidget::GetLogLevelColor(const FString& Level) const
{
	if (Level == TEXT("Error"))
	{
		return FSlateColor(FLinearColor::Red);
	}
	else if (Level == TEXT("Warning"))
	{
		return FSlateColor(FLinearColor::Yellow);
	}
	else if (Level == TEXT("Info"))
	{
		return FSlateColor(FLinearColor::White);
	}
	else if (Level == TEXT("Debug"))
	{
		return FSlateColor(FLinearColor::Gray);
	}
	
	return FSlateColor::UseForeground();
}

FText SMCPLogViewerWidget::GetCurrentFilterText() const
{
	return FText::FromString(CurrentLogFilter);
}

FText SMCPLogViewerWidget::GetLogCountText() const
{
	return FText::FromString(FString::Printf(TEXT("Showing %d of %d logs"), 
		FilteredLogEntries.Num(), LogEntries.Num()));
}

void SMCPLogViewerWidget::ScrollToBottomIfNeeded()
{
	if (bAutoScroll && LogListView.IsValid() && FilteredLogEntries.Num() > 0)
	{
		LogListView->RequestScrollIntoView(FilteredLogEntries[0]); // First item is newest
	}
}

void SMCPLogViewerWidget::TrimLogEntries()
{
	if (LogEntries.Num() > MaxLogEntries)
	{
		LogEntries.RemoveAt(MaxLogEntries, LogEntries.Num() - MaxLogEntries);
	}
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION