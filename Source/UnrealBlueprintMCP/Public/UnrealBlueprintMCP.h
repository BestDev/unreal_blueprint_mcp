#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Framework/Commands/UICommandList.h"
#include "Widgets/Docking/SDockTab.h"

class FMCPJsonRpcServer;
class UMCPServerSettings;
class SMCPDashboardWidget;
class SMCPToolbarWidget;
class FMCPNotificationManager;
class FMCPEditorCommands;

/**
 * UnrealBlueprintMCP Module
 * 
 * Editor plugin that provides JSON-RPC server functionality for Blueprint integration.
 * Simple HTTP server that starts on plugin load and stops on plugin unload.
 */
class FUnrealBlueprintMCPModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	/** JSON-RPC Server instance */
	TSharedPtr<FMCPJsonRpcServer> JsonRpcServer;

	/** UI Components */
	TSharedPtr<SMCPDashboardWidget> DashboardWidget;
	TSharedPtr<SMCPToolbarWidget> ToolbarWidget;
	TSharedPtr<FUICommandList> CommandList;

	/** Tab manager for dashboard */
	static const FName MCPDashboardTabName;

	/** Initialize the plugin UI */
	void InitializePlugin();

	/** Initialize settings integration */
	void InitializeSettings();

	/** Initialize UI components */
	void InitializeUI();

	/** Initialize command bindings */
	void InitializeCommands();

	/** Cleanup plugin resources */
	void CleanupPlugin();

	/** Register menu extensions */
	void RegisterMenuExtensions();

	/** Unregister menu extensions */
	void UnregisterMenuExtensions();

	/** Register tab spawners */
	void RegisterTabSpawners();

	/** Unregister tab spawners */
	void UnregisterTabSpawners();

	/** MCP Server Control Functions */
	void StartMCPServer();
	void StopMCPServer();
	bool IsServerRunning() const;

	/** Menu Callback Functions */
	void OnStartServerClicked();
	void OnStopServerClicked();
	void OnServerStatusClicked();
	void OnRestartServerClicked();
	void OnCopyServerURLClicked();
	void OnConfigurePortClicked();
	void OnShowServerInfoClicked();

	/** Configuration dialog functions */
	void ShowPortConfigurationDialog();
	int32 GetSavedPort() const;
	void SavePort(int32 Port) const;

	/** Copy text to clipboard */
	void CopyToClipboard(const FString& Text) const;

	/** Show detailed server information */
	void ShowServerInformation() const;

	/** Settings event handlers */
	void OnSettingsChanged(const UMCPServerSettings* Settings);
	void OnApplyServerSettings(const UMCPServerSettings* Settings);

	/** Advanced settings dialog functions */
	void ShowAdvancedSettingsDialog();
	void ShowSettingsExportImportDialog();

	/** UI Event Handlers */
	void OnDashboardAction(const FString& Action);
	void OnToolbarAction(const FString& Action);

	/** Tab spawner functions */
	TSharedRef<SDockTab> OnSpawnMCPDashboardTab(const class FSpawnTabArgs& SpawnTabArgs);

	/** Command handlers */
	void ExecuteStartServer();
	void ExecuteStopServer();
	void ExecuteRestartServer();
	void ExecuteToggleServer();
	void ExecuteShowServerStatus();
	void ExecuteShowServerInfo();
	void ExecuteCopyServerURL();
	void ExecuteOpenStatusWidget();
	void ExecuteOpenLogViewer();
	void ExecuteOpenClientTester();
	void ExecuteOpenSettings();
	void ExecuteQuickRestart();
	void ExecuteQuickTest();
	void ExecuteExportLogs();
	void ExecuteClearLogs();
	void ExecuteSendTestRequest();
	void ExecuteOpenDebugger();
	void ExecuteShowAPIDocumentation();

	/** Command state queries */
	bool CanExecuteServerCommands() const;
	bool IsServerRunningForCommands() const;

	/** Update dashboard with server status */
	void UpdateDashboardStatus();

	/** Integration with notification system */
	void ShowNotification(const FText& Message, bool bIsError = false);
};