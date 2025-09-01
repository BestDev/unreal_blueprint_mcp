#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FMCPJsonRpcServer;

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

	/** Initialize the plugin UI */
	void InitializePlugin();

	/** Cleanup plugin resources */
	void CleanupPlugin();

	/** Register menu extensions */
	void RegisterMenuExtensions();

	/** Unregister menu extensions */
	void UnregisterMenuExtensions();
};