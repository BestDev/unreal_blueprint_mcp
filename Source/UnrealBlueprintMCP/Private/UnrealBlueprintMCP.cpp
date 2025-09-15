#include "UnrealBlueprintMCP.h"
#include "MCPJsonRpcServer.h"
#include "MCPServerSettings.h"
#include "MCPDashboardWidget.h"
#include "MCPToolbarWidget.h"
#include "MCPNotificationManager.h"
#include "MCPEditorCommands.h"
#include "Core.h"
#include "Modules/ModuleManager.h"
#include "LevelEditor.h"
#include "ToolMenus.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Misc/ConfigCacheIni.h"
#include "Engine/Engine.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"
#include "ISettingsModule.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "Framework/Docking/TabManager.h"
#include "EditorStyleSet.h"

static const FName UnrealBlueprintMCPTabName("UnrealBlueprintMCP");
const FName FUnrealBlueprintMCPModule::MCPDashboardTabName("MCPDashboard");

#define LOCTEXT_NAMESPACE "FUnrealBlueprintMCPModule"

void FUnrealBlueprintMCPModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file

	UE_LOG(LogTemp, Warning, TEXT("UnrealBlueprintMCP: Module startup"));

	// Initialize commands first
	FMCPEditorCommands::Register();

	InitializePlugin();
	InitializeUI();
	InitializeCommands();
}

void FUnrealBlueprintMCPModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	UE_LOG(LogTemp, Warning, TEXT("UnrealBlueprintMCP: Module shutdown"));

	CleanupPlugin();

	// Unregister commands
	FMCPEditorCommands::Unregister();
}

void FUnrealBlueprintMCPModule::InitializePlugin()
{
	// Create JSON-RPC server instance but don't start it automatically
	JsonRpcServer = MakeShared<FMCPJsonRpcServer>();
	UE_LOG(LogTemp, Warning, TEXT("UnrealBlueprintMCP: Plugin initialized. Server ready to start manually."));

	InitializeSettings();
	RegisterMenuExtensions();

	// Auto-start server if enabled in settings
	const UMCPServerSettings* Settings = UMCPServerSettings::Get();
	if (Settings && Settings->bAutoStartServer)
	{
		StartMCPServer();
		UE_LOG(LogTemp, Warning, TEXT("UnrealBlueprintMCP: Auto-started server based on settings"));
	}
}

void FUnrealBlueprintMCPModule::CleanupPlugin()
{
	UnregisterMenuExtensions();
	UnregisterTabSpawners();

	// Clear UI components
	DashboardWidget.Reset();
	ToolbarWidget.Reset();
	CommandList.Reset();

	// Unregister settings delegates
	UMCPServerSettings::OnSettingsChanged.RemoveAll(this);
	UMCPServerSettings::OnApplyServerSettings.RemoveAll(this);

	// Stop the JSON-RPC server
	if (JsonRpcServer.IsValid())
	{
		JsonRpcServer->StopServer();
		UE_LOG(LogTemp, Warning, TEXT("UnrealBlueprintMCP: JSON-RPC Server stopped"));
	}
}

void FUnrealBlueprintMCPModule::RegisterMenuExtensions()
{
	// Register menu extension for MCP Server controls
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, [this]()
	{
		FToolMenuOwnerScoped OwnerScoped(this);
		
		// Add toolbar widget to the main toolbar
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
		if (ToolbarMenu)
		{
			FToolMenuSection& MCPSection = ToolbarMenu->AddSection("MCPToolbar", LOCTEXT("MCPToolbarSection", "MCP Server"));
			MCPSection.AddEntry(FToolMenuEntry::InitWidget(
				"MCPToolbarWidget",
				SAssignNew(ToolbarWidget, SMCPToolbarWidget)
					.OnServerAction(this, &FUnrealBlueprintMCPModule::OnToolbarAction),
				LOCTEXT("MCPToolbarWidget", "MCP Server Controls"),
				true
			));
		}
		
		// Get the main menu
		UToolMenu* MainMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu");
		if (MainMenu)
		{
			// Add MCP Server section
			FToolMenuSection& Section = MainMenu->AddSection("MCPServer", LOCTEXT("MCPServerMenu", "MCP Server"));
			
			// Add submenu
			FToolMenuEntry& SubMenuEntry = Section.AddSubMenu(
				"MCPServerSubMenu",
				LOCTEXT("MCPServerSubMenu", "MCP Server"),
				LOCTEXT("MCPServerSubMenuTooltip", "MCP Server Control Options"),
				FNewToolMenuDelegate::CreateLambda([this](UToolMenu* SubMenu)
				{
					// Server Control Section
					FToolMenuSection& ControlSection = SubMenu->AddSection("MCPServerActions", LOCTEXT("MCPServerActions", "Server Control"));
					
					// Start Server
					ControlSection.AddMenuEntry(
						"StartMCPServer",
						LOCTEXT("StartMCPServer", "🚀 Start Server"),
						LOCTEXT("StartMCPServerTooltip", "Start the MCP JSON-RPC Server"),
						FSlateIcon(),
						FUIAction(FExecuteAction::CreateRaw(this, &FUnrealBlueprintMCPModule::OnStartServerClicked))
					);
					
					// Stop Server
					ControlSection.AddMenuEntry(
						"StopMCPServer",
						LOCTEXT("StopMCPServer", "⏹️ Stop Server"),
						LOCTEXT("StopMCPServerTooltip", "Stop the MCP JSON-RPC Server"),
						FSlateIcon(),
						FUIAction(FExecuteAction::CreateRaw(this, &FUnrealBlueprintMCPModule::OnStopServerClicked))
					);
					
					// Restart Server
					ControlSection.AddMenuEntry(
						"RestartMCPServer",
						LOCTEXT("RestartMCPServer", "🔄 Restart Server"),
						LOCTEXT("RestartMCPServerTooltip", "Restart the MCP JSON-RPC Server"),
						FSlateIcon(),
						FUIAction(FExecuteAction::CreateRaw(this, &FUnrealBlueprintMCPModule::OnRestartServerClicked))
					);
					
					// Server Information Section
					FToolMenuSection& InfoSection = SubMenu->AddSection("MCPServerInfo", LOCTEXT("MCPServerInfo", "Server Information"));
					
					// Server Status
					InfoSection.AddMenuEntry(
						"MCPServerStatus",
						LOCTEXT("MCPServerStatus", "📊 Server Status"),
						LOCTEXT("MCPServerStatusTooltip", "Check MCP Server Status"),
						FSlateIcon(),
						FUIAction(FExecuteAction::CreateRaw(this, &FUnrealBlueprintMCPModule::OnServerStatusClicked))
					);
					
					// Detailed Server Info
					InfoSection.AddMenuEntry(
						"MCPServerInfo",
						LOCTEXT("MCPServerInfo", "ℹ️ Server Information"),
						LOCTEXT("MCPServerInfoTooltip", "Show detailed server information"),
						FSlateIcon(),
						FUIAction(FExecuteAction::CreateRaw(this, &FUnrealBlueprintMCPModule::OnShowServerInfoClicked))
					);
					
					// Copy Server URL
					InfoSection.AddMenuEntry(
						"CopyServerURL",
						LOCTEXT("CopyServerURL", "📋 Copy Server URL"),
						LOCTEXT("CopyServerURLTooltip", "Copy server URL to clipboard"),
						FSlateIcon(),
						FUIAction(FExecuteAction::CreateRaw(this, &FUnrealBlueprintMCPModule::OnCopyServerURLClicked))
					);
					
					// Configuration Section
					FToolMenuSection& ConfigSection = SubMenu->AddSection("MCPServerConfig", LOCTEXT("MCPServerConfig", "Configuration"));
					
					// Configure Port (Legacy)
					ConfigSection.AddMenuEntry(
						"ConfigurePort",
						LOCTEXT("ConfigurePort", "⚙️ Configure Port (Legacy)"),
						LOCTEXT("ConfigurePortTooltip", "Configure server port settings (legacy method)"),
						FSlateIcon(),
						FUIAction(FExecuteAction::CreateRaw(this, &FUnrealBlueprintMCPModule::OnConfigurePortClicked))
					);

					// Project Settings
					ConfigSection.AddMenuEntry(
						"OpenProjectSettings",
						LOCTEXT("OpenProjectSettings", "🔧 Open Project Settings"),
						LOCTEXT("OpenProjectSettingsTooltip", "Open Project Settings > Plugins > MCP Server"),
						FSlateIcon(),
						FUIAction(FExecuteAction::CreateLambda([]()
						{
							FModuleManager::LoadModuleChecked<ISettingsModule>("Settings")
								.ShowViewer("Project", "Plugins", "MCP Server");
						}))
					);

					// Advanced Settings
					ConfigSection.AddMenuEntry(
						"AdvancedSettings",
						LOCTEXT("AdvancedSettings", "🛠️ Advanced Settings"),
						LOCTEXT("AdvancedSettingsTooltip", "Show advanced settings dialog"),
						FSlateIcon(),
						FUIAction(FExecuteAction::CreateRaw(this, &FUnrealBlueprintMCPModule::ShowAdvancedSettingsDialog))
					);

					// Export/Import Settings
					ConfigSection.AddMenuEntry(
						"ExportImportSettings",
						LOCTEXT("ExportImportSettings", "📁 Export/Import Settings"),
						LOCTEXT("ExportImportSettingsTooltip", "Export or import server settings"),
						FSlateIcon(),
						FUIAction(FExecuteAction::CreateRaw(this, &FUnrealBlueprintMCPModule::ShowSettingsExportImportDialog))
					);

					// Dashboard Section
					FToolMenuSection& DashboardSection = SubMenu->AddSection("MCPDashboard", LOCTEXT("MCPDashboard", "Dashboard"));
					
					// Open Dashboard
					DashboardSection.AddMenuEntry(
						"OpenDashboard",
						LOCTEXT("OpenDashboard", "📊 Open Dashboard"),
						LOCTEXT("OpenDashboardTooltip", "Open the comprehensive MCP Server Dashboard"),
						FSlateIcon(),
						FUIAction(FExecuteAction::CreateRaw(this, &FUnrealBlueprintMCPModule::ExecuteOpenStatusWidget))
					);
				})
			);
		}
	}));
}

void FUnrealBlueprintMCPModule::UnregisterMenuExtensions()
{
	// Unregister menu extensions here if needed
	UToolMenus::UnRegisterStartupCallback(this);
}

void FUnrealBlueprintMCPModule::StartMCPServer()
{
	if (!JsonRpcServer.IsValid())
	{
		JsonRpcServer = MakeShared<FMCPJsonRpcServer>();
	}

	if (JsonRpcServer.IsValid() && !IsServerRunning())
	{
		const UMCPServerSettings* Settings = UMCPServerSettings::Get();
		int32 PreferredPort = Settings ? Settings->ServerPort : 8080;
		
		bool bServerStarted = JsonRpcServer->StartServerWithFallback(PreferredPort);
		if (bServerStarted)
		{
			int32 ActualPort = JsonRpcServer->GetPort();
			// Update settings with actual port if different
			if (Settings && ActualPort != Settings->ServerPort)
			{
				UMCPServerSettings* MutableSettings = UMCPServerSettings::GetMutable();
				MutableSettings->ServerPort = ActualPort;
				MutableSettings->SaveConfig();
			}
			UE_LOG(LogTemp, Warning, TEXT("UnrealBlueprintMCP: JSON-RPC Server started on port %d"), ActualPort);
			
			// Update UI components
			UpdateDashboardStatus();
			if (ToolbarWidget.IsValid())
			{
				ToolbarWidget->UpdateServerStatus(true, ActualPort, 0);
			}
			
			// Show notification
			FMCPNotificationManager::Get().ShowServerStartNotification(ActualPort);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("UnrealBlueprintMCP: Failed to start JSON-RPC Server on any available port"));
			FMCPNotificationManager::Get().ShowServerErrorNotification(TEXT("Failed to start server on any available port"));
		}
	}
}

void FUnrealBlueprintMCPModule::StopMCPServer()
{
	if (JsonRpcServer.IsValid() && IsServerRunning())
	{
		JsonRpcServer->StopServer();
		UE_LOG(LogTemp, Warning, TEXT("UnrealBlueprintMCP: JSON-RPC Server stopped"));
		
		// Update UI components
		UpdateDashboardStatus();
		if (ToolbarWidget.IsValid())
		{
			ToolbarWidget->UpdateServerStatus(false, 0, 0);
		}
		
		// Show notification
		FMCPNotificationManager::Get().ShowServerStopNotification();
	}
}

bool FUnrealBlueprintMCPModule::IsServerRunning() const
{
	return JsonRpcServer.IsValid() && JsonRpcServer->IsRunning();
}

void FUnrealBlueprintMCPModule::OnStartServerClicked()
{
	if (IsServerRunning())
	{
		UE_LOG(LogTemp, Warning, TEXT("MCP Server is already running"));
		
		// Show notification
		FNotificationInfo Info(LOCTEXT("ServerAlreadyRunning", "MCP Server is already running"));
		Info.ExpireDuration = 3.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}
	else
	{
		StartMCPServer();
		
		// Show notification with more detailed information
		if (IsServerRunning())
		{
			int32 ActualPort = JsonRpcServer->GetPort();
			FText Message = FText::FromString(FString::Printf(TEXT("MCP Server started successfully on port %d"), ActualPort));
			
			FNotificationInfo Info(Message);
			Info.ExpireDuration = 3.0f;
			FSlateNotificationManager::Get().AddNotification(Info);
		}
		else
		{
			FText Message = LOCTEXT("ServerStartFailed", "Failed to start MCP Server on any available port. Check log for details.");
			
			FNotificationInfo Info(Message);
			Info.ExpireDuration = 5.0f;
			FSlateNotificationManager::Get().AddNotification(Info);
		}
	}
}

void FUnrealBlueprintMCPModule::OnStopServerClicked()
{
	if (!IsServerRunning())
	{
		UE_LOG(LogTemp, Warning, TEXT("MCP Server is not running"));
		
		// Show notification
		FNotificationInfo Info(LOCTEXT("ServerNotRunning", "MCP Server is not running"));
		Info.ExpireDuration = 3.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}
	else
	{
		StopMCPServer();
		
		// Show notification
		FNotificationInfo Info(LOCTEXT("ServerStopped", "MCP Server stopped"));
		Info.ExpireDuration = 3.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}
}

void FUnrealBlueprintMCPModule::OnServerStatusClicked()
{
	if (IsServerRunning())
	{
		int32 Port = JsonRpcServer->GetPort();
		int32 ClientCount = JsonRpcServer->GetConnectedClientCount();
		FDateTime StartTime = JsonRpcServer->GetServerStartTime();
		FTimespan UpTime = FDateTime::Now() - StartTime;
		
		FString StatusText = FString::Printf(TEXT("MCP Server is running on port %d\\nConnected clients: %d\\nUptime: %s"), 
			Port, ClientCount, *UpTime.ToString());
		
		UE_LOG(LogTemp, Warning, TEXT("MCP Server Status: %s"), *StatusText);
		
		FText StatusMessage = FText::FromString(FString::Printf(TEXT("Server running on port %d\\nClients: %d, Uptime: %s"), 
			Port, ClientCount, *UpTime.ToString()));
		
		FNotificationInfo Info(StatusMessage);
		Info.ExpireDuration = 5.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}
	else
	{
		FText StatusMessage = LOCTEXT("ServerStoppedStatus", "MCP Server is stopped");
		
		FNotificationInfo Info(StatusMessage);
		Info.ExpireDuration = 3.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}
}

void FUnrealBlueprintMCPModule::OnRestartServerClicked()
{
	if (IsServerRunning())
	{
		UE_LOG(LogTemp, Warning, TEXT("Restarting MCP Server..."));
		
		// Show notification
		FNotificationInfo Info(LOCTEXT("ServerRestarting", "Restarting MCP Server..."));
		Info.ExpireDuration = 2.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
		
		bool bRestarted = JsonRpcServer->RestartServer();
		
		FText Message = bRestarted ? 
			LOCTEXT("ServerRestarted", "MCP Server restarted successfully") :
			LOCTEXT("ServerRestartFailed", "Failed to restart MCP Server");
		
		FNotificationInfo RestartInfo(Message);
		RestartInfo.ExpireDuration = 3.0f;
		FSlateNotificationManager::Get().AddNotification(RestartInfo);
	}
	else
	{
		// If not running, just start it
		OnStartServerClicked();
	}
}

void FUnrealBlueprintMCPModule::OnCopyServerURLClicked()
{
	if (IsServerRunning())
	{
		FString ServerURL = JsonRpcServer->GetServerURL();
		CopyToClipboard(ServerURL);
		
		FText Message = FText::FromString(FString::Printf(TEXT("Server URL copied to clipboard: %s"), *ServerURL));
		
		FNotificationInfo Info(Message);
		Info.ExpireDuration = 3.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}
	else
	{
		FNotificationInfo Info(LOCTEXT("ServerNotRunningForURL", "Server is not running. Cannot copy URL."));
		Info.ExpireDuration = 3.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}
}

void FUnrealBlueprintMCPModule::OnConfigurePortClicked()
{
	ShowPortConfigurationDialog();
}

void FUnrealBlueprintMCPModule::OnShowServerInfoClicked()
{
	ShowServerInformation();
}

void FUnrealBlueprintMCPModule::ShowPortConfigurationDialog()
{
	// Create a simple input dialog for port configuration
	// This is a simplified implementation - in a full version you'd want a proper dialog widget
	
	int32 CurrentPort = GetSavedPort();
	
	// For now, use a simple log message and notification
	FString InfoText = FString::Printf(TEXT("Current saved port: %d\\nAvailable ports: 8080, 8081, 8082, 8083, 8084, 8090, 9000, 9001\\nTo change port, modify the configuration file."), CurrentPort);
	
	UE_LOG(LogTemp, Warning, TEXT("Port Configuration: %s"), *InfoText);
	
	FNotificationInfo Info(FText::FromString(FString::Printf(TEXT("Current port: %d. Check log for details."), CurrentPort)));
	Info.ExpireDuration = 5.0f;
	FSlateNotificationManager::Get().AddNotification(Info);
}

int32 FUnrealBlueprintMCPModule::GetSavedPort() const
{
	int32 SavedPort = 8080; // Default
	GConfig->GetInt(TEXT("UnrealBlueprintMCP"), TEXT("ServerPort"), SavedPort, GEditorIni);
	return SavedPort;
}

void FUnrealBlueprintMCPModule::SavePort(int32 Port) const
{
	GConfig->SetInt(TEXT("UnrealBlueprintMCP"), TEXT("ServerPort"), Port, GEditorIni);
	GConfig->Flush(false, GEditorIni);
}

void FUnrealBlueprintMCPModule::CopyToClipboard(const FString& Text) const
{
	FPlatformApplicationMisc::ClipboardCopy(*Text);
}

void FUnrealBlueprintMCPModule::ShowServerInformation() const
{
	if (IsServerRunning())
	{
		int32 Port = JsonRpcServer->GetPort();
		int32 ClientCount = JsonRpcServer->GetConnectedClientCount();
		FDateTime StartTime = JsonRpcServer->GetServerStartTime();
		FTimespan UpTime = FDateTime::Now() - StartTime;
		FString ServerURL = JsonRpcServer->GetServerURL();
		
		FString DetailedInfo = FString::Printf(TEXT("=== MCP Server Information ===\\n")
			TEXT("Status: Running\\n")
			TEXT("Port: %d\\n")
			TEXT("URL: %s\\n")
			TEXT("Start Time: %s\\n")
			TEXT("Uptime: %s\\n")
			TEXT("Connected Clients: %d\\n")
			TEXT("Available Endpoints:\\n")
			TEXT("  - ping\\n")
			TEXT("  - getBlueprints\\n")
			TEXT("  - getActors\\n")
			TEXT("  - resources.list\\n")
			TEXT("  - resources.get\\n")
			TEXT("  - resources.create\\n")
			TEXT("  - tools.*\\n")
			TEXT("  - prompts.*"),
			Port, *ServerURL, *StartTime.ToString(), *UpTime.ToString(), ClientCount);
		
		UE_LOG(LogTemp, Warning, TEXT("%s"), *DetailedInfo);
		
		FNotificationInfo Info(FText::FromString(FString::Printf(TEXT("Server Info - Port: %d, Clients: %d, Uptime: %s\\nSee log for details"), 
			Port, ClientCount, *UpTime.ToString())));
		Info.ExpireDuration = 8.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}
	else
	{
		FString StoppedInfo = TEXT("=== MCP Server Information ===\\n")
			TEXT("Status: Stopped\\n")
			TEXT("Last Used Port: ") + FString::FromInt(GetSavedPort()) + TEXT("\\n")
			TEXT("To start server, use the Start Server option from the menu.");
		
		UE_LOG(LogTemp, Warning, TEXT("%s"), *StoppedInfo);
		
		FNotificationInfo Info(LOCTEXT("ServerStoppedInfo", "Server is stopped. Check log for details."));
		Info.ExpireDuration = 3.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}
}

void FUnrealBlueprintMCPModule::InitializeSettings()
{
	// Bind to settings change events
	UMCPServerSettings::OnSettingsChanged.AddRaw(this, &FUnrealBlueprintMCPModule::OnSettingsChanged);
	UMCPServerSettings::OnApplyServerSettings.AddRaw(this, &FUnrealBlueprintMCPModule::OnApplyServerSettings);
}

void FUnrealBlueprintMCPModule::OnSettingsChanged(const UMCPServerSettings* Settings)
{
	if (Settings)
	{
		UE_LOG(LogTemp, Log, TEXT("MCP Server Settings changed: %s"), *Settings->GetSettingsDisplayString());
		
		// Show notification about settings change
		FNotificationInfo Info(LOCTEXT("SettingsChanged", "MCP Server settings have been updated"));
		Info.ExpireDuration = 2.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}
}

void FUnrealBlueprintMCPModule::OnApplyServerSettings(const UMCPServerSettings* Settings)
{
	if (!Settings)
	{
		return;
	}

	// If server is running, check if restart is needed
	if (IsServerRunning())
	{
		// For now, just show a notification that restart is recommended
		// In a full implementation, you might want to dynamically apply some settings
		FNotificationInfo Info(LOCTEXT("ServerRestartRecommended", "Server restart recommended to apply all settings"));
		Info.ExpireDuration = 5.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
		
		UE_LOG(LogTemp, Warning, TEXT("MCP Server settings applied. Restart recommended for full effect."));
	}
}

void FUnrealBlueprintMCPModule::ShowAdvancedSettingsDialog()
{
	const UMCPServerSettings* Settings = UMCPServerSettings::Get();
	if (!Settings)
	{
		return;
	}

	// For now, show current settings in a notification
	// In a full implementation, you'd create a proper dialog widget
	FString SettingsInfo = FString::Printf(TEXT("Advanced Settings:\\n%s\\n\\nUse Project Settings for detailed configuration."), 
		*Settings->GetSettingsDisplayString());

	FNotificationInfo Info(FText::FromString(SettingsInfo));
	Info.ExpireDuration = 8.0f;
	FSlateNotificationManager::Get().AddNotification(Info);

	UE_LOG(LogTemp, Warning, TEXT("Advanced Settings: %s"), *SettingsInfo);
}

void FUnrealBlueprintMCPModule::ShowSettingsExportImportDialog()
{
	const UMCPServerSettings* Settings = UMCPServerSettings::Get();
	if (!Settings)
	{
		return;
	}

	// Export current settings to JSON and show in log
	FString ExportedJSON = Settings->ExportToJSON();
	
	FString InfoText = TEXT("Settings Export/Import\\n\\nCurrent settings exported to log.\\nSee Output Log for JSON data.\\n\\nUse Project Settings to modify individual settings.");
	
	FNotificationInfo Info(FText::FromString(InfoText));
	Info.ExpireDuration = 5.0f;
	FSlateNotificationManager::Get().AddNotification(Info);

	UE_LOG(LogTemp, Warning, TEXT("MCP Server Settings Export:\\n%s"), *ExportedJSON);
}

void FUnrealBlueprintMCPModule::InitializeUI()
{
	RegisterTabSpawners();
}

void FUnrealBlueprintMCPModule::InitializeCommands()
{
	CommandList = MakeShareable(new FUICommandList);

	const FMCPEditorCommands& Commands = FMCPEditorCommands::Get();

	// Server control commands
	CommandList->MapAction(Commands.StartServer,
		FExecuteAction::CreateRaw(this, &FUnrealBlueprintMCPModule::ExecuteStartServer),
		FCanExecuteAction::CreateRaw(this, &FUnrealBlueprintMCPModule::CanExecuteServerCommands));

	CommandList->MapAction(Commands.StopServer,
		FExecuteAction::CreateRaw(this, &FUnrealBlueprintMCPModule::ExecuteStopServer),
		FCanExecuteAction::CreateRaw(this, &FUnrealBlueprintMCPModule::IsServerRunningForCommands));

	CommandList->MapAction(Commands.RestartServer,
		FExecuteAction::CreateRaw(this, &FUnrealBlueprintMCPModule::ExecuteRestartServer),
		FCanExecuteAction::CreateRaw(this, &FUnrealBlueprintMCPModule::IsServerRunningForCommands));

	CommandList->MapAction(Commands.ToggleServer,
		FExecuteAction::CreateRaw(this, &FUnrealBlueprintMCPModule::ExecuteToggleServer),
		FCanExecuteAction::CreateRaw(this, &FUnrealBlueprintMCPModule::CanExecuteServerCommands));

	// Server information commands
	CommandList->MapAction(Commands.ShowServerStatus,
		FExecuteAction::CreateRaw(this, &FUnrealBlueprintMCPModule::ExecuteShowServerStatus),
		FCanExecuteAction::CreateRaw(this, &FUnrealBlueprintMCPModule::CanExecuteServerCommands));

	CommandList->MapAction(Commands.ShowServerInfo,
		FExecuteAction::CreateRaw(this, &FUnrealBlueprintMCPModule::ExecuteShowServerInfo),
		FCanExecuteAction::CreateRaw(this, &FUnrealBlueprintMCPModule::CanExecuteServerCommands));

	CommandList->MapAction(Commands.CopyServerURL,
		FExecuteAction::CreateRaw(this, &FUnrealBlueprintMCPModule::ExecuteCopyServerURL),
		FCanExecuteAction::CreateRaw(this, &FUnrealBlueprintMCPModule::IsServerRunningForCommands));

	// UI commands
	CommandList->MapAction(Commands.OpenStatusWidget,
		FExecuteAction::CreateRaw(this, &FUnrealBlueprintMCPModule::ExecuteOpenStatusWidget),
		FCanExecuteAction::CreateRaw(this, &FUnrealBlueprintMCPModule::CanExecuteServerCommands));

	CommandList->MapAction(Commands.OpenLogViewer,
		FExecuteAction::CreateRaw(this, &FUnrealBlueprintMCPModule::ExecuteOpenLogViewer),
		FCanExecuteAction::CreateRaw(this, &FUnrealBlueprintMCPModule::CanExecuteServerCommands));

	CommandList->MapAction(Commands.OpenClientTester,
		FExecuteAction::CreateRaw(this, &FUnrealBlueprintMCPModule::ExecuteOpenClientTester),
		FCanExecuteAction::CreateRaw(this, &FUnrealBlueprintMCPModule::CanExecuteServerCommands));

	CommandList->MapAction(Commands.OpenSettings,
		FExecuteAction::CreateRaw(this, &FUnrealBlueprintMCPModule::ExecuteOpenSettings),
		FCanExecuteAction::CreateRaw(this, &FUnrealBlueprintMCPModule::CanExecuteServerCommands));

	// Quick access commands
	CommandList->MapAction(Commands.QuickRestart,
		FExecuteAction::CreateRaw(this, &FUnrealBlueprintMCPModule::ExecuteQuickRestart),
		FCanExecuteAction::CreateRaw(this, &FUnrealBlueprintMCPModule::IsServerRunningForCommands));

	CommandList->MapAction(Commands.QuickTest,
		FExecuteAction::CreateRaw(this, &FUnrealBlueprintMCPModule::ExecuteQuickTest),
		FCanExecuteAction::CreateRaw(this, &FUnrealBlueprintMCPModule::IsServerRunningForCommands));

	CommandList->MapAction(Commands.ExportLogs,
		FExecuteAction::CreateRaw(this, &FUnrealBlueprintMCPModule::ExecuteExportLogs),
		FCanExecuteAction::CreateRaw(this, &FUnrealBlueprintMCPModule::CanExecuteServerCommands));

	CommandList->MapAction(Commands.ClearLogs,
		FExecuteAction::CreateRaw(this, &FUnrealBlueprintMCPModule::ExecuteClearLogs),
		FCanExecuteAction::CreateRaw(this, &FUnrealBlueprintMCPModule::CanExecuteServerCommands));

	// Debug commands
	CommandList->MapAction(Commands.SendTestRequest,
		FExecuteAction::CreateRaw(this, &FUnrealBlueprintMCPModule::ExecuteSendTestRequest),
		FCanExecuteAction::CreateRaw(this, &FUnrealBlueprintMCPModule::IsServerRunningForCommands));

	CommandList->MapAction(Commands.OpenDebugger,
		FExecuteAction::CreateRaw(this, &FUnrealBlueprintMCPModule::ExecuteOpenDebugger),
		FCanExecuteAction::CreateRaw(this, &FUnrealBlueprintMCPModule::CanExecuteServerCommands));

	CommandList->MapAction(Commands.ShowAPIDocumentation,
		FExecuteAction::CreateRaw(this, &FUnrealBlueprintMCPModule::ExecuteShowAPIDocumentation),
		FCanExecuteAction::CreateRaw(this, &FUnrealBlueprintMCPModule::CanExecuteServerCommands));
}

void FUnrealBlueprintMCPModule::RegisterTabSpawners()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(MCPDashboardTabName,
		FOnSpawnTab::CreateRaw(this, &FUnrealBlueprintMCPModule::OnSpawnMCPDashboardTab))
		.SetDisplayName(LOCTEXT("MCPDashboardTabTitle", "MCP Server Dashboard"))
		.SetTooltipText(LOCTEXT("MCPDashboardTabTooltip", "Open the MCP Server Dashboard"))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsDebugCategory())
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.GameSettings"));
}

void FUnrealBlueprintMCPModule::UnregisterTabSpawners()
{
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(MCPDashboardTabName);
}

TSharedRef<SDockTab> FUnrealBlueprintMCPModule::OnSpawnMCPDashboardTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SAssignNew(DashboardWidget, SMCPDashboardWidget)
			.OnDashboardAction(this, &FUnrealBlueprintMCPModule::OnDashboardAction)
		];
}

void FUnrealBlueprintMCPModule::OnDashboardAction(const FString& Action)
{
	UE_LOG(LogTemp, Log, TEXT("Dashboard Action: %s"), *Action);

	// Parse and handle dashboard actions
	if (Action.StartsWith(TEXT("Toolbar:")))
	{
		FString ToolbarAction = Action.RightChop(8); // Remove "Toolbar:" prefix
		OnToolbarAction(ToolbarAction);
	}
	else if (Action.StartsWith(TEXT("Status:")))
	{
		// Handle status widget actions
	}
	else if (Action.StartsWith(TEXT("Log:")))
	{
		// Handle log viewer actions
	}
	else if (Action.StartsWith(TEXT("Client:")))
	{
		// Handle client tester actions
	}
}

void FUnrealBlueprintMCPModule::OnToolbarAction(const FString& Action)
{
	if (Action == TEXT("Start"))
	{
		ExecuteStartServer();
	}
	else if (Action == TEXT("Stop"))
	{
		ExecuteStopServer();
	}
	else if (Action == TEXT("Restart"))
	{
		ExecuteRestartServer();
	}
	else if (Action == TEXT("ShowInfo"))
	{
		ExecuteShowServerInfo();
	}
}

// Command implementations
void FUnrealBlueprintMCPModule::ExecuteStartServer()
{
	if (!IsServerRunning())
	{
		StartMCPServer();
		UpdateDashboardStatus();
		ShowNotification(LOCTEXT("ServerStarted", "MCP Server started"));
	}
}

void FUnrealBlueprintMCPModule::ExecuteStopServer()
{
	if (IsServerRunning())
	{
		StopMCPServer();
		UpdateDashboardStatus();
		ShowNotification(LOCTEXT("ServerStopped", "MCP Server stopped"));
	}
}

void FUnrealBlueprintMCPModule::ExecuteRestartServer()
{
	if (IsServerRunning())
	{
		StopMCPServer();
		StartMCPServer();
		UpdateDashboardStatus();
		ShowNotification(LOCTEXT("ServerRestarted", "MCP Server restarted"));
	}
}

void FUnrealBlueprintMCPModule::ExecuteToggleServer()
{
	if (IsServerRunning())
	{
		ExecuteStopServer();
	}
	else
	{
		ExecuteStartServer();
	}
}

void FUnrealBlueprintMCPModule::ExecuteShowServerStatus()
{
	OnServerStatusClicked();
}

void FUnrealBlueprintMCPModule::ExecuteShowServerInfo()
{
	OnShowServerInfoClicked();
}

void FUnrealBlueprintMCPModule::ExecuteCopyServerURL()
{
	OnCopyServerURLClicked();
}

void FUnrealBlueprintMCPModule::ExecuteOpenStatusWidget()
{
	FGlobalTabmanager::Get()->TryInvokeTab(MCPDashboardTabName);
}

void FUnrealBlueprintMCPModule::ExecuteOpenLogViewer()
{
	FGlobalTabmanager::Get()->TryInvokeTab(MCPDashboardTabName);
}

void FUnrealBlueprintMCPModule::ExecuteOpenClientTester()
{
	FGlobalTabmanager::Get()->TryInvokeTab(MCPDashboardTabName);
}

void FUnrealBlueprintMCPModule::ExecuteOpenSettings()
{
	FModuleManager::LoadModuleChecked<ISettingsModule>("Settings")
		.ShowViewer("Project", "Plugins", "MCP Server");
}

void FUnrealBlueprintMCPModule::ExecuteQuickRestart()
{
	ExecuteRestartServer();
}

void FUnrealBlueprintMCPModule::ExecuteQuickTest()
{
	if (IsServerRunning())
	{
		// Send a simple ping request
		ShowNotification(LOCTEXT("TestRequests", "Sending test request..."));
		// TODO: Implement actual test request sending
	}
}

void FUnrealBlueprintMCPModule::ExecuteExportLogs()
{
	if (DashboardWidget.IsValid() && DashboardWidget->GetLogViewerWidget().IsValid())
	{
		// TODO: Trigger log export through dashboard
		ShowNotification(LOCTEXT("ExportingLogs", "Exporting logs..."));
	}
}

void FUnrealBlueprintMCPModule::ExecuteClearLogs()
{
	if (DashboardWidget.IsValid() && DashboardWidget->GetLogViewerWidget().IsValid())
	{
		DashboardWidget->GetLogViewerWidget()->ClearLogs();
		ShowNotification(LOCTEXT("LogsCleared", "Logs cleared"));
	}
}

void FUnrealBlueprintMCPModule::ExecuteSendTestRequest()
{
	ExecuteQuickTest();
}

void FUnrealBlueprintMCPModule::ExecuteOpenDebugger()
{
	ExecuteOpenClientTester();
}

void FUnrealBlueprintMCPModule::ExecuteShowAPIDocumentation()
{
	FString URL = TEXT("https://spec.modelcontextprotocol.io/specification/");
	FPlatformProcess::LaunchURL(*URL, nullptr, nullptr);
	ShowNotification(LOCTEXT("APIDocsOpened", "Opening MCP API documentation..."));
}

bool FUnrealBlueprintMCPModule::CanExecuteServerCommands() const
{
	return true; // Always allow server commands
}

bool FUnrealBlueprintMCPModule::IsServerRunningForCommands() const
{
	return IsServerRunning();
}

void FUnrealBlueprintMCPModule::UpdateDashboardStatus()
{
	if (DashboardWidget.IsValid() && JsonRpcServer.IsValid())
	{
		bool bRunning = IsServerRunning();
		int32 Port = bRunning ? JsonRpcServer->GetPort() : 0;
		FString URL = bRunning ? JsonRpcServer->GetServerURL() : TEXT("");
		int32 ClientCount = bRunning ? JsonRpcServer->GetConnectedClientCount() : 0;
		FDateTime StartTime = bRunning ? JsonRpcServer->GetServerStartTime() : FDateTime::Now();

		DashboardWidget->UpdateServerStatus(bRunning, Port, URL, ClientCount, StartTime);
	}
}

void FUnrealBlueprintMCPModule::ShowNotification(const FText& Message, bool bIsError)
{
	FMCPNotificationManager& NotificationManager = FMCPNotificationManager::Get();
	EMCPNotificationType Type = bIsError ? EMCPNotificationType::Error : EMCPNotificationType::Info;
	NotificationManager.ShowNotification(Message, Type, 3.0f);
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FUnrealBlueprintMCPModule, UnrealBlueprintMCP)