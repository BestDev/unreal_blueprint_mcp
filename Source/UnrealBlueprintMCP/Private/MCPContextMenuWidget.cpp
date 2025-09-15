#include "MCPContextMenuWidget.h"
#include "SlateOptMacros.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EditorStyleSet.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/FileHelper.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SMCPContextMenuWidget::Construct(const FArguments& InArgs)
{
	OnContextMenuAction = InArgs._OnContextMenuAction;
	
	// This widget is typically not used directly but serves as a utility class
	ChildSlot
	[
		SNullWidget::NullWidget
	];
}

TSharedRef<SWidget> SMCPContextMenuWidget::CreateAssetContextMenu(bool bServerRunning, const FOnMCPContextMenuAction& OnAction)
{
	FMenuBuilder MenuBuilder(true, nullptr);
	
	MenuBuilder.BeginSection("MCPAssetActions", NSLOCTEXT("MCPContextMenu", "AssetActionsSection", "MCP Server"));
	{
		CreateServerControlSection(MenuBuilder, bServerRunning, OnAction);
		CreateQuickActionsSection(MenuBuilder, OnAction);
	}
	MenuBuilder.EndSection();
	
	return MenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SMCPContextMenuWidget::CreateContentBrowserContextMenu(const FOnMCPContextMenuAction& OnAction)
{
	FMenuBuilder MenuBuilder(true, nullptr);
	
	MenuBuilder.BeginSection("MCPContentActions", NSLOCTEXT("MCPContextMenu", "ContentActionsSection", "MCP Tools"));
	{
		CreateQuickActionsSection(MenuBuilder, OnAction);
		CreateToolsSection(MenuBuilder, OnAction);
	}
	MenuBuilder.EndSection();
	
	return MenuBuilder.MakeWidget();
}

bool SMCPContextMenuWidget::HandleConfigFileDrop(const FString& FilePath, const FOnMCPContextMenuAction& OnAction)
{
	// Check if the dropped file is a JSON configuration file
	if (!FilePath.EndsWith(TEXT(".json")) && !FilePath.EndsWith(TEXT(".config")))
	{
		return false;
	}
	
	// Verify file exists
	if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*FilePath))
	{
		return false;
	}
	
	// Read and validate the configuration file
	FString FileContent;
	if (!FFileHelper::LoadFileToString(FileContent, *FilePath))
	{
		return false;
	}
	
	// Basic validation - check if it contains MCP-related configuration
	if (FileContent.Contains(TEXT("ServerPort")) || 
		FileContent.Contains(TEXT("mcp")) || 
		FileContent.Contains(TEXT("server")))
	{
		// Trigger configuration import action
		OnAction.ExecuteIfBound(FString::Printf(TEXT("ImportConfig:%s"), *FilePath));
		return true;
	}
	
	return false;
}

void SMCPContextMenuWidget::CreateServerControlSection(FMenuBuilder& MenuBuilder, bool bServerRunning, const FOnMCPContextMenuAction& OnAction)
{
	if (bServerRunning)
	{
		MenuBuilder.AddMenuEntry(
			NSLOCTEXT("MCPContextMenu", "StopServer", "Stop MCP Server"),
			NSLOCTEXT("MCPContextMenu", "StopServerTooltip", "Stop the MCP JSON-RPC server"),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Icons.Stop"),
			FUIAction(FExecuteAction::CreateLambda([OnAction]()
			{
				OnAction.ExecuteIfBound(TEXT("StopServer"));
			}))
		);
		
		MenuBuilder.AddMenuEntry(
			NSLOCTEXT("MCPContextMenu", "RestartServer", "Restart MCP Server"),
			NSLOCTEXT("MCPContextMenu", "RestartServerTooltip", "Restart the MCP JSON-RPC server"),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Icons.Refresh"),
			FUIAction(FExecuteAction::CreateLambda([OnAction]()
			{
				OnAction.ExecuteIfBound(TEXT("RestartServer"));
			}))
		);
	}
	else
	{
		MenuBuilder.AddMenuEntry(
			NSLOCTEXT("MCPContextMenu", "StartServer", "Start MCP Server"),
			NSLOCTEXT("MCPContextMenu", "StartServerTooltip", "Start the MCP JSON-RPC server"),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Icons.Play"),
			FUIAction(FExecuteAction::CreateLambda([OnAction]()
			{
				OnAction.ExecuteIfBound(TEXT("StartServer"));
			}))
		);
	}
}

void SMCPContextMenuWidget::CreateQuickActionsSection(FMenuBuilder& MenuBuilder, const FOnMCPContextMenuAction& OnAction)
{
	MenuBuilder.AddSubMenu(
		NSLOCTEXT("MCPContextMenu", "QuickActions", "Quick Actions"),
		NSLOCTEXT("MCPContextMenu", "QuickActionsTooltip", "Quick MCP server actions"),
		FNewMenuDelegate::CreateLambda([OnAction](FMenuBuilder& SubMenuBuilder)
		{
			SubMenuBuilder.AddMenuEntry(
				NSLOCTEXT("MCPContextMenu", "ServerStatus", "Show Server Status"),
				NSLOCTEXT("MCPContextMenu", "ServerStatusTooltip", "Display current server status"),
				FSlateIcon(FEditorStyle::GetStyleSetName(), "Icons.Info"),
				FUIAction(FExecuteAction::CreateLambda([OnAction]()
				{
					OnAction.ExecuteIfBound(TEXT("ShowStatus"));
				}))
			);
			
			SubMenuBuilder.AddMenuEntry(
				NSLOCTEXT("MCPContextMenu", "CopyServerURL", "Copy Server URL"),
				NSLOCTEXT("MCPContextMenu", "CopyServerURLTooltip", "Copy server URL to clipboard"),
				FSlateIcon(FEditorStyle::GetStyleSetName(), "Icons.Copy"),
				FUIAction(FExecuteAction::CreateLambda([OnAction]()
				{
					OnAction.ExecuteIfBound(TEXT("CopyURL"));
				}))
			);
			
			SubMenuBuilder.AddMenuEntry(
				NSLOCTEXT("MCPContextMenu", "SendTestRequest", "Send Test Request"),
				NSLOCTEXT("MCPContextMenu", "SendTestRequestTooltip", "Send a test ping request to the server"),
				FSlateIcon(FEditorStyle::GetStyleSetName(), "Icons.TestPassed"),
				FUIAction(FExecuteAction::CreateLambda([OnAction]()
				{
					OnAction.ExecuteIfBound(TEXT("TestRequest"));
				}))
			);
		}),
		false,
		FSlateIcon(FEditorStyle::GetStyleSetName(), "Icons.Toolbar.QuickSettings")
	);
}

void SMCPContextMenuWidget::CreateToolsSection(FMenuBuilder& MenuBuilder, const FOnMCPContextMenuAction& OnAction)
{
	MenuBuilder.AddSubMenu(
		NSLOCTEXT("MCPContextMenu", "MCPTools", "MCP Tools"),
		NSLOCTEXT("MCPContextMenu", "MCPToolsTooltip", "MCP development and debugging tools"),
		FNewMenuDelegate::CreateLambda([OnAction](FMenuBuilder& SubMenuBuilder)
		{
			SubMenuBuilder.AddMenuEntry(
				NSLOCTEXT("MCPContextMenu", "OpenDashboard", "Open Dashboard"),
				NSLOCTEXT("MCPContextMenu", "OpenDashboardTooltip", "Open the MCP server dashboard"),
				FSlateIcon(FEditorStyle::GetStyleSetName(), "Icons.Dashboard"),
				FUIAction(FExecuteAction::CreateLambda([OnAction]()
				{
					OnAction.ExecuteIfBound(TEXT("OpenDashboard"));
				}))
			);
			
			SubMenuBuilder.AddMenuEntry(
				NSLOCTEXT("MCPContextMenu", "OpenClientTester", "Open Client Tester"),
				NSLOCTEXT("MCPContextMenu", "OpenClientTesterTooltip", "Open the JSON-RPC client tester"),
				FSlateIcon(FEditorStyle::GetStyleSetName(), "Icons.TestPlan"),
				FUIAction(FExecuteAction::CreateLambda([OnAction]()
				{
					OnAction.ExecuteIfBound(TEXT("OpenClientTester"));
				}))
			);
			
			SubMenuBuilder.AddMenuEntry(
				NSLOCTEXT("MCPContextMenu", "OpenLogViewer", "Open Log Viewer"),
				NSLOCTEXT("MCPContextMenu", "OpenLogViewerTooltip", "Open the real-time log viewer"),
				FSlateIcon(FEditorStyle::GetStyleSetName(), "Icons.Log"),
				FUIAction(FExecuteAction::CreateLambda([OnAction]()
				{
					OnAction.ExecuteIfBound(TEXT("OpenLogViewer"));
				}))
			);
			
			SubMenuBuilder.AddSeparator();
			
			SubMenuBuilder.AddMenuEntry(
				NSLOCTEXT("MCPContextMenu", "ExportLogs", "Export Logs"),
				NSLOCTEXT("MCPContextMenu", "ExportLogsTooltip", "Export server logs to file"),
				FSlateIcon(FEditorStyle::GetStyleSetName(), "Icons.Export"),
				FUIAction(FExecuteAction::CreateLambda([OnAction]()
				{
					OnAction.ExecuteIfBound(TEXT("ExportLogs"));
				}))
			);
			
			SubMenuBuilder.AddMenuEntry(
				NSLOCTEXT("MCPContextMenu", "ClearLogs", "Clear Logs"),
				NSLOCTEXT("MCPContextMenu", "ClearLogsTooltip", "Clear all server logs"),
				FSlateIcon(FEditorStyle::GetStyleSetName(), "Icons.Delete"),
				FUIAction(FExecuteAction::CreateLambda([OnAction]()
				{
					OnAction.ExecuteIfBound(TEXT("ClearLogs"));
				}))
			);
		}),
		false,
		FSlateIcon(FEditorStyle::GetStyleSetName(), "Icons.Tools")
	);
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION