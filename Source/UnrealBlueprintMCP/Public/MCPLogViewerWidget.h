#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Engine/EngineTypes.h"

/** Log entry data structure */
USTRUCT()
struct FMCPLogEntry
{
	GENERATED_BODY()

	FDateTime Timestamp;
	FString Level;
	FString Category;
	FString Message;
	FString SourceFile;
	int32 LineNumber = 0;

	FMCPLogEntry() = default;
	
	FMCPLogEntry(const FString& InLevel, const FString& InCategory, const FString& InMessage)
		: Timestamp(FDateTime::Now())
		, Level(InLevel)
		, Category(InCategory)
		, Message(InMessage)
		, LineNumber(0)
	{
	}
};

DECLARE_DELEGATE_OneParam(FOnMCPLogAction, const FString&);

/**
 * Real-time log viewer widget for MCP server events
 */
class UNREALBLUEPRINTMCP_API SMCPLogViewerWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMCPLogViewerWidget) {}
		SLATE_EVENT(FOnMCPLogAction, OnLogAction)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Add new log entry */
	void AddLogEntry(const FString& Level, const FString& Category, const FString& Message);

	/** Clear all logs */
	void ClearLogs();

	/** Export logs to file */
	void ExportLogs(const FString& FilePath);

	/** Set log level filter */
	void SetLogLevelFilter(const FString& Level);

	/** Set auto-scroll enabled */
	void SetAutoScroll(bool bEnabled);

private:
	/** Log action event */
	FOnMCPLogAction OnLogAction;

	/** UI Elements */
	TSharedPtr<SListView<TSharedPtr<FMCPLogEntry>>> LogListView;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> LogLevelFilter;
	TSharedPtr<SCheckBox> AutoScrollCheckBox;
	TSharedPtr<STextBlock> LogCountText;

	/** Log data */
	TArray<TSharedPtr<FMCPLogEntry>> LogEntries;
	TArray<TSharedPtr<FMCPLogEntry>> FilteredLogEntries;
	TArray<TSharedPtr<FString>> LogLevelOptions;

	/** Settings */
	bool bAutoScroll = true;
	FString CurrentLogFilter = TEXT("All");
	int32 MaxLogEntries = 1000;

	/** Generate row widget for log entry */
	TSharedRef<ITableRow> OnGenerateLogRow(TSharedPtr<FMCPLogEntry> Entry, const TSharedRef<STableViewBase>& OwnerTable);

	/** Filter logs based on current settings */
	void RefreshFilteredLogs();

	/** Log level filter changed */
	void OnLogLevelFilterChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);

	/** Auto-scroll checkbox changed */
	void OnAutoScrollChanged(ECheckBoxState NewState);

	/** Clear logs button clicked */
	FReply OnClearLogsClicked();

	/** Export logs button clicked */
	FReply OnExportLogsClicked();

	/** Get log level color */
	FSlateColor GetLogLevelColor(const FString& Level) const;

	/** Get current log filter text */
	FText GetCurrentFilterText() const;

	/** Get log count text */
	FText GetLogCountText() const;

	/** Scroll to bottom if auto-scroll is enabled */
	void ScrollToBottomIfNeeded();

	/** Limit log entries to max count */
	void TrimLogEntries();
};