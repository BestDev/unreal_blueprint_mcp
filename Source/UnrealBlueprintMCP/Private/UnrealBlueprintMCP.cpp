#include "UnrealBlueprintMCP.h"
#include "MCPJsonRpcServer.h"
#include "Core.h"
#include "Modules/ModuleManager.h"
#include "LevelEditor.h"
#include "ToolMenus.h"

static const FName UnrealBlueprintMCPTabName("UnrealBlueprintMCP");

#define LOCTEXT_NAMESPACE "FUnrealBlueprintMCPModule"

void FUnrealBlueprintMCPModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file

	UE_LOG(LogTemp, Warning, TEXT("UnrealBlueprintMCP: Module startup"));

	InitializePlugin();
}

void FUnrealBlueprintMCPModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	UE_LOG(LogTemp, Warning, TEXT("UnrealBlueprintMCP: Module shutdown"));

	CleanupPlugin();
}

void FUnrealBlueprintMCPModule::InitializePlugin()
{
	// Create and start the JSON-RPC server
	JsonRpcServer = MakeShared<FMCPJsonRpcServer>();
	
	if (JsonRpcServer.IsValid())
	{
		bool bServerStarted = JsonRpcServer->StartServer(8080);
		if (bServerStarted)
		{
			UE_LOG(LogTemp, Warning, TEXT("UnrealBlueprintMCP: JSON-RPC Server started on port 8080"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("UnrealBlueprintMCP: Failed to start JSON-RPC Server"));
		}
	}

	RegisterMenuExtensions();
}

void FUnrealBlueprintMCPModule::CleanupPlugin()
{
	UnregisterMenuExtensions();

	// Stop the JSON-RPC server
	if (JsonRpcServer.IsValid())
	{
		JsonRpcServer->StopServer();
		UE_LOG(LogTemp, Warning, TEXT("UnrealBlueprintMCP: JSON-RPC Server stopped"));
	}
}

void FUnrealBlueprintMCPModule::RegisterMenuExtensions()
{
	// Register menu extensions here if needed
	// Note: Removed recursive callback registration that was causing stack overflow
	// TODO: Add actual menu extension implementation when needed
}

void FUnrealBlueprintMCPModule::UnregisterMenuExtensions()
{
	// Unregister menu extensions here if needed
	UToolMenus::UnRegisterStartupCallback(this);
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FUnrealBlueprintMCPModule, UnrealBlueprintMCP)