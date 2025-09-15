#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Networking.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

class UMCPServerSettings;

/**
 * Simple JSON-RPC Server for MCP (Model Context Protocol)
 * 
 * Provides a basic HTTP server that handles JSON-RPC requests.
 * Runs on a separate thread to avoid blocking the editor.
 */
class UNREALBLUEPRINTMCP_API FMCPJsonRpcServer : public FRunnable
{
public:
	FMCPJsonRpcServer();
	virtual ~FMCPJsonRpcServer();

	/** Start the server on the specified port */
	bool StartServer(int32 Port = 8080);

	/** Stop the server */
	void StopServer();

	/** Check if server is running */
	bool IsRunning() const { return bIsRunning; }

	/** Get the current port */
	int32 GetPort() const { return ServerPort; }

	/** Get server start time */
	FDateTime GetServerStartTime() const { return ServerStartTime; }

	/** Get connected client count */
	int32 GetConnectedClientCount() const { return ConnectedClientCount; }

	/** Get server URL */
	FString GetServerURL() const { return FString::Printf(TEXT("http://localhost:%d"), ServerPort); }

	/** Try starting server with alternative port if main port fails */
	bool StartServerWithFallback(int32 PreferredPort = 8080);

	/** Check if port is available */
	static bool IsPortAvailable(int32 Port);

	/** Restart the server */
	bool RestartServer();

	/** Apply settings from UMCPServerSettings */
	void ApplySettings(const UMCPServerSettings* Settings);

	/** Get current settings applied to server */
	FString GetAppliedSettingsString() const;

	// FRunnable interface
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	virtual void Exit() override;

private:
	/** Server socket */
	FSocket* ServerSocket;

	/** Server thread */
	FRunnableThread* ServerThread;

	/** Port number */
	int32 ServerPort;

	/** Running flag */
	FThreadSafeBool bIsRunning;

	/** Stop requested flag */
	FThreadSafeBool bStopRequested;

	/** Server start time */
	FDateTime ServerStartTime;

	/** Connected client count (thread-safe) */
	FThreadSafeCounter ConnectedClientCount;

	/** Last used port for settings persistence */
	static int32 LastUsedPort;

	/** Default fallback ports to try */
	TArray<int32> FallbackPorts;

	/** Applied settings cache */
	int32 AppliedMaxConnections;
	int32 AppliedTimeoutSeconds;
	bool bAppliedEnableCORS;
	bool bAppliedEnableAuth;
	FString AppliedAPIKey;
	TMap<FString, FString> AppliedCustomHeaders;

	/** Handle incoming client connection */
	void HandleClientConnection(FSocket* ClientSocket);

	/** Process HTTP request */
	FString ProcessHttpRequest(const FString& RequestData);

	/** Process JSON-RPC request */
	TSharedPtr<FJsonObject> ProcessJsonRpcRequest(TSharedPtr<FJsonObject> Request);

	/** Handle specific JSON-RPC methods */
	TSharedPtr<FJsonObject> HandleGetBlueprints(TSharedPtr<FJsonObject> Params);
	TSharedPtr<FJsonObject> HandleGetActors(TSharedPtr<FJsonObject> Params);
	TSharedPtr<FJsonObject> HandlePing(TSharedPtr<FJsonObject> Params);

	/** Handle resources namespace methods */
	TSharedPtr<FJsonObject> HandleResourcesList(TSharedPtr<FJsonObject> Params);
	TSharedPtr<FJsonObject> HandleResourcesGet(TSharedPtr<FJsonObject> Params);
	TSharedPtr<FJsonObject> HandleResourcesCreate(TSharedPtr<FJsonObject> Params);

	/** Handle tools namespace methods */
	TSharedPtr<FJsonObject> HandleToolsCreateBlueprint(TSharedPtr<FJsonObject> Params);
	TSharedPtr<FJsonObject> HandleToolsAddVariable(TSharedPtr<FJsonObject> Params);
	TSharedPtr<FJsonObject> HandleToolsAddFunction(TSharedPtr<FJsonObject> Params);
	TSharedPtr<FJsonObject> HandleToolsEditGraph(TSharedPtr<FJsonObject> Params);

	/** Handle prompts namespace methods */
	TSharedPtr<FJsonObject> HandlePromptsList(TSharedPtr<FJsonObject> Params);
	TSharedPtr<FJsonObject> HandlePromptsGet(TSharedPtr<FJsonObject> Params);

	/** Create HTTP response */
	FString CreateHttpResponse(const FString& Content, const FString& ContentType = TEXT("application/json"));

	/** Create JSON-RPC error response */
	TSharedPtr<FJsonObject> CreateErrorResponse(int32 ErrorCode, const FString& ErrorMessage, TSharedPtr<FJsonValue> Id = nullptr);

	/** Log server messages */
	void LogMessage(const FString& Message);

	/** Thread-safe wrapper for editor API calls */
	template<typename ReturnType>
	ReturnType ExecuteOnGameThread(TFunction<ReturnType()> Task);
	
	/** Thread-safe asynchronous execution for editor API calls */
	void ExecuteOnGameThreadAsync(TFunction<void()> Task);
};