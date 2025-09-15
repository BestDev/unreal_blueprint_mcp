#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SSplitter.h"
#include "MCPToolbarWidget.h"
#include "MCPStatusWidget.h"
#include "MCPLogViewerWidget.h"
#include "MCPClientTesterWidget.h"
#include "Framework/Docking/TabManager.h"

DECLARE_DELEGATE_OneParam(FOnMCPDashboardAction, const FString&);

/**
 * Main dashboard widget that integrates all MCP UI components
 */
class UNREALBLUEPRINTMCP_API SMCPDashboardWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMCPDashboardWidget) {}
		SLATE_EVENT(FOnMCPDashboardAction, OnDashboardAction)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Update all widgets with server status */
	void UpdateServerStatus(bool bIsRunning, int32 Port, const FString& URL, 
		int32 ClientCount, const FDateTime& StartTime);

	/** Add log entry to log viewer */
	void AddLogEntry(const FString& Level, const FString& Category, const FString& Message);

	/** Add network activity entry */
	void AddNetworkActivity(const FString& Method, const FString& Endpoint, float ResponseTime);

	/** Update network statistics */
	void UpdateNetworkStats(int32 RequestsPerSecond, float LatestResponseTime);

	/** Set server URL for client tester */
	void SetServerURL(const FString& URL);

	/** Get toolbar widget */
	TSharedPtr<SMCPToolbarWidget> GetToolbarWidget() const { return ToolbarWidget; }

	/** Get status widget */
	TSharedPtr<SMCPStatusWidget> GetStatusWidget() const { return StatusWidget; }

	/** Get log viewer widget */
	TSharedPtr<SMCPLogViewerWidget> GetLogViewerWidget() const { return LogViewerWidget; }

	/** Get client tester widget */
	TSharedPtr<SMCPClientTesterWidget> GetClientTesterWidget() const { return ClientTesterWidget; }

private:
	/** Dashboard action event */
	FOnMCPDashboardAction OnDashboardAction;

	/** UI Components */
	TSharedPtr<SMCPToolbarWidget> ToolbarWidget;
	TSharedPtr<SMCPStatusWidget> StatusWidget;
	TSharedPtr<SMCPLogViewerWidget> LogViewerWidget;
	TSharedPtr<SMCPClientTesterWidget> ClientTesterWidget;

	/** Tab manager for organizing panels */
	TSharedPtr<FTabManager> TabManager;
	TSharedPtr<FTabManager::FLayout> TabLayout;

	/** Current server state */
	bool bServerRunning = false;
	int32 CurrentPort = 0;
	FString ServerURL;

	/** Handle toolbar actions */
	void OnToolbarAction(const FString& Action);

	/** Handle status widget actions */
	void OnStatusAction(const FString& Action);

	/** Handle log viewer actions */
	void OnLogAction(const FString& Action);

	/** Handle client tester actions */
	void OnClientAction(const FString& Action);

	/** Create tab spawners */
	void RegisterTabSpawners();

	/** Create default layout */
	TSharedRef<FTabManager::FLayout> CreateDefaultLayout();

	/** Tab spawner functions */
	TSharedRef<SDockTab> SpawnStatusTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnLogViewerTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnClientTesterTab(const FSpawnTabArgs& Args);

	/** Tab IDs */
	static const FName StatusTabId;
	static const FName LogViewerTabId;
	static const FName ClientTesterTabId;
};