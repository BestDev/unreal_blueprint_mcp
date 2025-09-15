#include "MCPEditorCommands.h"

#define LOCTEXT_NAMESPACE "FMCPEditorCommands"

FMCPEditorCommands::FMCPEditorCommands()
	: TCommands<FMCPEditorCommands>(
		TEXT("MCPServer"), // Context name
		NSLOCTEXT("Contexts", "MCPServer", "MCP Server"),
		NAME_None, // Parent context
		FEditorStyle::GetStyleSetName() // Icon Style Set
	)
{
}

void FMCPEditorCommands::RegisterCommands()
{
	// Server control commands
	UI_COMMAND(StartServer, "Start Server", "Start the MCP server", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::M));
	UI_COMMAND(StopServer, "Stop Server", "Stop the MCP server", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::N));
	UI_COMMAND(RestartServer, "Restart Server", "Restart the MCP server", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::R));
	UI_COMMAND(ToggleServer, "Toggle Server", "Toggle MCP server on/off", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::T));

	// Server information commands
	UI_COMMAND(ShowServerStatus, "Show Server Status", "Display current server status", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::S));
	UI_COMMAND(ShowServerInfo, "Show Server Info", "Display detailed server information", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::I));
	UI_COMMAND(CopyServerURL, "Copy Server URL", "Copy server URL to clipboard", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::C));

	// UI commands
	UI_COMMAND(OpenStatusWidget, "Open Status Widget", "Open the server status widget", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::W));
	UI_COMMAND(OpenLogViewer, "Open Log Viewer", "Open the log viewer widget", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::L));
	UI_COMMAND(OpenClientTester, "Open Client Tester", "Open the JSON-RPC client tester", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::J));
	UI_COMMAND(OpenSettings, "Open Settings", "Open MCP server settings", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::O));

	// Quick access commands
	UI_COMMAND(QuickRestart, "Quick Restart", "Quickly restart the server", EUserInterfaceActionType::Button, FInputChord(EKeys::F5));
	UI_COMMAND(QuickTest, "Quick Test", "Send a quick test request", EUserInterfaceActionType::Button, FInputChord(EKeys::F6));
	UI_COMMAND(ExportLogs, "Export Logs", "Export server logs to file", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::E));
	UI_COMMAND(ClearLogs, "Clear Logs", "Clear all server logs", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::X));

	// Debug commands
	UI_COMMAND(SendTestRequest, "Send Test Request", "Send a test request to the server", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::P));
	UI_COMMAND(OpenDebugger, "Open Debugger", "Open the JSON-RPC debugger", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::D));
	UI_COMMAND(ShowAPIDocumentation, "Show API Documentation", "Open API documentation", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::H));
}

#undef LOCTEXT_NAMESPACE