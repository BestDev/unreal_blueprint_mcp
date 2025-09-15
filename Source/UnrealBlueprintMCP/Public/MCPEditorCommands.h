#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "Framework/Commands/UICommandInfo.h"
#include "EditorStyleSet.h"

/**
 * MCP Editor command definitions for keyboard shortcuts and UI actions
 */
class UNREALBLUEPRINTMCP_API FMCPEditorCommands : public TCommands<FMCPEditorCommands>
{
public:
	FMCPEditorCommands();

	/** Initialize commands */
	virtual void RegisterCommands() override;

	/** Server control commands */
	TSharedPtr<FUICommandInfo> StartServer;
	TSharedPtr<FUICommandInfo> StopServer;
	TSharedPtr<FUICommandInfo> RestartServer;
	TSharedPtr<FUICommandInfo> ToggleServer;

	/** Server information commands */
	TSharedPtr<FUICommandInfo> ShowServerStatus;
	TSharedPtr<FUICommandInfo> ShowServerInfo;
	TSharedPtr<FUICommandInfo> CopyServerURL;

	/** UI commands */
	TSharedPtr<FUICommandInfo> OpenStatusWidget;
	TSharedPtr<FUICommandInfo> OpenLogViewer;
	TSharedPtr<FUICommandInfo> OpenClientTester;
	TSharedPtr<FUICommandInfo> OpenSettings;

	/** Quick access commands */
	TSharedPtr<FUICommandInfo> QuickRestart;
	TSharedPtr<FUICommandInfo> QuickTest;
	TSharedPtr<FUICommandInfo> ExportLogs;
	TSharedPtr<FUICommandInfo> ClearLogs;

	/** Debug commands */
	TSharedPtr<FUICommandInfo> SendTestRequest;
	TSharedPtr<FUICommandInfo> OpenDebugger;
	TSharedPtr<FUICommandInfo> ShowAPIDocumentation;
};