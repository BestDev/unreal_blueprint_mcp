#include "MCPJsonRpcServer.h"
#include "MCPServerSettings.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/Blueprint.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/Event.h" // For FEvent
#include "Misc/FileHelper.h"
#include "Misc/DateTime.h"
#include "Async/Async.h"
#include "Common/TcpSocketBuilder.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Factories/BlueprintFactory.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Kismet2/BlueprintEditorUtils.h"
// Blueprint Graph node includes - Updated for UE 5.6
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_FunctionEntry.h"
#include "Engine/UserDefinedStruct.h"
#include "Engine/SimpleConstructionScript.h"
#include "KismetCompiler.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "HAL/PlatformApplicationMisc.h"

// Static member initialization
int32 FMCPJsonRpcServer::LastUsedPort = 8080;

FMCPJsonRpcServer::FMCPJsonRpcServer()
	: ServerSocket(nullptr)
	, ServerThread(nullptr)
	, ServerPort(8080)
	, bIsRunning(false)
	, bStopRequested(false)
	, ServerStartTime(FDateTime::MinValue())
	, AppliedMaxConnections(10)
	, AppliedTimeoutSeconds(30)
	, bAppliedEnableCORS(false)
	, bAppliedEnableAuth(false)
{
	// Initialize fallback ports
	FallbackPorts = {8080, 8081, 8082, 8083, 8084, 8090, 9000, 9001};
	
	// Apply initial settings from UMCPServerSettings
	const UMCPServerSettings* Settings = UMCPServerSettings::Get();
	if (Settings)
	{
		ApplySettings(Settings);
	}
}

FMCPJsonRpcServer::~FMCPJsonRpcServer()
{
	StopServer();
}

bool FMCPJsonRpcServer::StartServer(int32 Port)
{
	if (bIsRunning)
	{
		LogMessage(TEXT("Server is already running"));
		return false;
	}

	ServerPort = Port;

	// Create server socket
	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SocketSubsystem)
	{
		LogMessage(TEXT("Failed to get socket subsystem"));
		return false;
	}

	ServerSocket = SocketSubsystem->CreateSocket(NAME_Stream, TEXT("MCPJsonRpcServer"), false);
	if (!ServerSocket)
	{
		LogMessage(TEXT("Failed to create server socket"));
		return false;
	}

	// Allow socket reuse
	ServerSocket->SetReuseAddr(true);

	// Bind to port - SECURITY FIX: Only bind to localhost for security
	TSharedRef<FInternetAddr> LocalAddr = SocketSubsystem->CreateInternetAddr();
	bool bIsValid = false;
	LocalAddr->SetIp(TEXT("127.0.0.1"), bIsValid); // localhost only - prevents external network access
	if (!bIsValid)
	{
		LogMessage(TEXT("Failed to set localhost IP address"));
		ServerSocket->Close();
		SocketSubsystem->DestroySocket(ServerSocket);
		ServerSocket = nullptr;
		return false;
	}
	LocalAddr->SetPort(ServerPort);

	if (!ServerSocket->Bind(*LocalAddr))
	{
		LogMessage(FString::Printf(TEXT("Failed to bind to port %d"), ServerPort));
		ServerSocket->Close();
		SocketSubsystem->DestroySocket(ServerSocket);
		ServerSocket = nullptr;
		return false;
	}

	// Start listening
	if (!ServerSocket->Listen(8))
	{
		LogMessage(TEXT("Failed to listen on socket"));
		ServerSocket->Close();
		SocketSubsystem->DestroySocket(ServerSocket);
		ServerSocket = nullptr;
		return false;
	}

	// Start server thread
	ServerThread = FRunnableThread::Create(this, TEXT("MCPJsonRpcServerThread"));
	if (!ServerThread)
	{
		LogMessage(TEXT("Failed to create server thread"));
		ServerSocket->Close();
		SocketSubsystem->DestroySocket(ServerSocket);
		ServerSocket = nullptr;
		return false;
	}

	bIsRunning = true;
	ServerStartTime = FDateTime::Now();
	LastUsedPort = ServerPort;
	LogMessage(FString::Printf(TEXT("Server started on port %d at %s"), ServerPort, *ServerStartTime.ToString()));
	return true;
}

void FMCPJsonRpcServer::StopServer()
{
	if (!bIsRunning)
	{
		return;
	}

	bStopRequested = true;

	// Wait for thread to finish
	if (ServerThread)
	{
		ServerThread->WaitForCompletion();
		delete ServerThread;
		ServerThread = nullptr;
	}

	// Close server socket
	if (ServerSocket)
	{
		ServerSocket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ServerSocket);
		ServerSocket = nullptr;
	}

	bIsRunning = false;
	ServerStartTime = FDateTime::MinValue();
	ConnectedClientCount.Reset();
	LogMessage(TEXT("Server stopped"));
}

bool FMCPJsonRpcServer::Init()
{
	return true;
}

uint32 FMCPJsonRpcServer::Run()
{
	while (!bStopRequested)
	{
		if (!ServerSocket)
		{
			break;
		}

		// Check for pending connections
		bool bHasPendingConnection = false;
		if (ServerSocket->HasPendingConnection(bHasPendingConnection) && bHasPendingConnection)
		{
			// Accept connection
			FSocket* ClientSocket = ServerSocket->Accept(TEXT("MCPJsonRpcClient"));
			if (ClientSocket)
			{
				// Handle client in async task to avoid blocking
				AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [this, ClientSocket]()
				{
					HandleClientConnection(ClientSocket);
				});
			}
		}

		// Sleep for a short time to avoid busy waiting
		FPlatformProcess::Sleep(0.01f);
	}

	return 0;
}

void FMCPJsonRpcServer::Stop()
{
	bStopRequested = true;
}

void FMCPJsonRpcServer::Exit()
{
	// Cleanup
}

void FMCPJsonRpcServer::HandleClientConnection(FSocket* ClientSocket)
{
	if (!ClientSocket)
	{
		return;
	}

	// Increment connected client count
	ConnectedClientCount.Increment();

	// Read HTTP request
	TArray<uint8> ReceivedData;
	uint8 Buffer[1024];
	int32 BytesRead = 0;

	// Configure socket for non-blocking mode with timeout handling
	ClientSocket->SetNonBlocking(true);
	
	// Timeout handling for UE 5.6 compatibility
	double StartTime = FPlatformTime::Seconds();
	const double TimeoutSeconds = 5.0;
	bool bRequestComplete = false;

	while (!bRequestComplete && (FPlatformTime::Seconds() - StartTime) < TimeoutSeconds)
	{
		ESocketConnectionState ConnectionState = ClientSocket->GetConnectionState();
		if (ConnectionState != SCS_Connected)
		{
			break;
		}

		if (ClientSocket->Recv(Buffer, sizeof(Buffer) - 1, BytesRead))
		{
			if (BytesRead > 0)
			{
				ReceivedData.Append(Buffer, BytesRead);
				
				// Check if we have a complete HTTP request (look for double CRLF)
				FString RequestStr = FString::ConstructFromPtrSize((char*)ReceivedData.GetData(), ReceivedData.Num());
				if (RequestStr.Contains(TEXT("\r\n\r\n")))
				{
					bRequestComplete = true;
					break;
				}
			}
		}
		else
		{
			// No data available, sleep briefly to avoid busy waiting
			FPlatformProcess::Sleep(0.001f);
		}
	}

	if (ReceivedData.Num() > 0)
	{
		FString RequestData = FString::ConstructFromPtrSize((char*)ReceivedData.GetData(), ReceivedData.Num());
		FString Response = ProcessHttpRequest(RequestData);

		// Send response
		if (!Response.IsEmpty())
		{
			FTCHARToUTF8 ResponseUTF8(*Response);
			ClientSocket->Send((uint8*)ResponseUTF8.Get(), ResponseUTF8.Length(), BytesRead);
		}
	}

	// Close client socket
	ClientSocket->Close();
	ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ClientSocket);
	
	// Decrement connected client count
	ConnectedClientCount.Decrement();
}

FString FMCPJsonRpcServer::ProcessHttpRequest(const FString& RequestData)
{
	// Simple HTTP parsing - look for JSON content
	TArray<FString> Lines;
	RequestData.ParseIntoArrayLines(Lines);

	if (Lines.Num() == 0)
	{
		return CreateHttpResponse(TEXT("{\"error\":\"Empty request\"}"));
	}

	// Check if it's a POST request
	if (!Lines[0].StartsWith(TEXT("POST")))
	{
		// Return a simple OK response for GET requests
		return CreateHttpResponse(TEXT("{\"status\":\"MCP JSON-RPC Server\",\"version\":\"1.0\"}"));
	}

	// Find the JSON content (after the headers)
	FString JsonContent;
	bool bFoundContent = false;
	for (const FString& Line : Lines)
	{
		if (bFoundContent)
		{
			JsonContent += Line;
		}
		else if (Line.IsEmpty())
		{
			bFoundContent = true;
		}
	}

	if (JsonContent.IsEmpty())
	{
		return CreateHttpResponse(TEXT("{\"error\":\"No JSON content found\"}"));
	}

	// Parse JSON
	TSharedPtr<FJsonObject> JsonRequest;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonContent);
	
	if (!FJsonSerializer::Deserialize(Reader, JsonRequest) || !JsonRequest.IsValid())
	{
		return CreateHttpResponse(TEXT("{\"error\":\"Invalid JSON\"}"));
	}

	// Process JSON-RPC request
	TSharedPtr<FJsonObject> JsonResponse = ProcessJsonRpcRequest(JsonRequest);
	
	// Convert response to string
	FString ResponseContent;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResponseContent);
	FJsonSerializer::Serialize(JsonResponse.ToSharedRef(), Writer);

	return CreateHttpResponse(ResponseContent);
}

TSharedPtr<FJsonObject> FMCPJsonRpcServer::ProcessJsonRpcRequest(TSharedPtr<FJsonObject> Request)
{
	if (!Request.IsValid())
	{
		return CreateErrorResponse(-32600, TEXT("Invalid Request"));
	}

	// Get request ID
	TSharedPtr<FJsonValue> Id = Request->TryGetField(TEXT("id"));

	// Check JSON-RPC version
	FString JsonRpc;
	if (!Request->TryGetStringField(TEXT("jsonrpc"), JsonRpc) || JsonRpc != TEXT("2.0"))
	{
		return CreateErrorResponse(-32600, TEXT("Invalid Request - jsonrpc field must be '2.0'"), Id);
	}

	// Get method name
	FString Method;
	if (!Request->TryGetStringField(TEXT("method"), Method))
	{
		return CreateErrorResponse(-32600, TEXT("Invalid Request - missing method"), Id);
	}

	// Get parameters
	TSharedPtr<FJsonObject> Params = Request->GetObjectField(TEXT("params"));

	// Handle different methods
	TSharedPtr<FJsonObject> Result;
	if (Method == TEXT("ping"))
	{
		Result = HandlePing(Params);
	}
	else if (Method == TEXT("getBlueprints"))
	{
		Result = HandleGetBlueprints(Params);
	}
	else if (Method == TEXT("getActors"))
	{
		Result = HandleGetActors(Params);
	}
	else if (Method == TEXT("resources.list"))
	{
		Result = HandleResourcesList(Params);
	}
	else if (Method == TEXT("resources.get"))
	{
		Result = HandleResourcesGet(Params);
	}
	else if (Method == TEXT("resources.create"))
	{
		Result = HandleResourcesCreate(Params);
	}
	else if (Method == TEXT("tools.create_blueprint"))
	{
		Result = HandleToolsCreateBlueprint(Params);
	}
	else if (Method == TEXT("tools.add_variable"))
	{
		Result = HandleToolsAddVariable(Params);
	}
	else if (Method == TEXT("tools.add_function"))
	{
		Result = HandleToolsAddFunction(Params);
	}
	else if (Method == TEXT("tools.edit_graph"))
	{
		Result = HandleToolsEditGraph(Params);
	}
	else if (Method == TEXT("prompts.list"))
	{
		Result = HandlePromptsList(Params);
	}
	else if (Method == TEXT("prompts.get"))
	{
		Result = HandlePromptsGet(Params);
	}
	else
	{
		return CreateErrorResponse(-32601, TEXT("Method not found"), Id);
	}

	// Create success response
	TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
	Response->SetStringField(TEXT("jsonrpc"), TEXT("2.0"));
	Response->SetObjectField(TEXT("result"), Result);
	if (Id.IsValid())
	{
		Response->SetField(TEXT("id"), Id);
	}

	return Response;
}

TSharedPtr<FJsonObject> FMCPJsonRpcServer::HandlePing(TSharedPtr<FJsonObject> Params)
{
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetStringField(TEXT("status"), TEXT("pong"));
	Result->SetStringField(TEXT("server"), TEXT("UnrealBlueprintMCP"));
	Result->SetStringField(TEXT("version"), TEXT("1.0"));
	return Result;
}

TSharedPtr<FJsonObject> FMCPJsonRpcServer::HandleGetBlueprints(TSharedPtr<FJsonObject> Params)
{
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	TArray<TSharedPtr<FJsonValue>> Blueprints;

	// This is a simplified implementation
	// In a real implementation, you would iterate through Blueprint assets
	TSharedPtr<FJsonObject> SampleBlueprint = MakeShareable(new FJsonObject);
	SampleBlueprint->SetStringField(TEXT("name"), TEXT("SampleBlueprint"));
	SampleBlueprint->SetStringField(TEXT("path"), TEXT("/Game/Blueprints/SampleBlueprint"));
	SampleBlueprint->SetStringField(TEXT("type"), TEXT("Blueprint"));
	
	Blueprints.Add(MakeShareable(new FJsonValueObject(SampleBlueprint)));

	Result->SetArrayField(TEXT("blueprints"), Blueprints);
	Result->SetNumberField(TEXT("count"), Blueprints.Num());
	
	return Result;
}

TSharedPtr<FJsonObject> FMCPJsonRpcServer::HandleGetActors(TSharedPtr<FJsonObject> Params)
{
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	TArray<TSharedPtr<FJsonValue>> Actors;

	// This is a simplified implementation
	// In a real implementation, you would iterate through world actors
	TSharedPtr<FJsonObject> SampleActor = MakeShareable(new FJsonObject);
	SampleActor->SetStringField(TEXT("name"), TEXT("SampleActor"));
	SampleActor->SetStringField(TEXT("class"), TEXT("Actor"));
	SampleActor->SetStringField(TEXT("location"), TEXT("0,0,0"));
	
	Actors.Add(MakeShareable(new FJsonValueObject(SampleActor)));

	Result->SetArrayField(TEXT("actors"), Actors);
	Result->SetNumberField(TEXT("count"), Actors.Num());
	
	return Result;
}

FString FMCPJsonRpcServer::CreateHttpResponse(const FString& Content, const FString& ContentType)
{
	FString Response = TEXT("HTTP/1.1 200 OK\r\n");
	Response += FString::Printf(TEXT("Content-Type: %s\r\n"), *ContentType);
	Response += FString::Printf(TEXT("Content-Length: %d\r\n"), Content.Len());
	
	// Apply CORS headers if enabled in settings
	if (bAppliedEnableCORS)
	{
		const UMCPServerSettings* Settings = UMCPServerSettings::Get();
		if (Settings && Settings->AllowedOrigins.Num() > 0)
		{
			// Use specific allowed origins
			FString AllowedOriginsHeader = FString::Join(Settings->AllowedOrigins, TEXT(", "));
			Response += FString::Printf(TEXT("Access-Control-Allow-Origin: %s\r\n"), *AllowedOriginsHeader);
		}
		else
		{
			// Allow all origins (less secure, but useful for development)
			Response += TEXT("Access-Control-Allow-Origin: *\r\n");
		}
		Response += TEXT("Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n");
		Response += TEXT("Access-Control-Allow-Headers: Content-Type, Authorization\r\n");
		Response += TEXT("Access-Control-Max-Age: 86400\r\n");
	}
	
	// Apply custom headers from settings
	for (const auto& Header : AppliedCustomHeaders)
	{
		Response += FString::Printf(TEXT("%s: %s\r\n"), *Header.Key, *Header.Value);
	}
	
	// Security headers (always applied)
	Response += TEXT("X-Content-Type-Options: nosniff\r\n"); // Security header to prevent MIME type sniffing
	if (!bAppliedEnableCORS)
	{
		Response += TEXT("X-Frame-Options: DENY\r\n"); // Prevent iframe embedding (unless CORS is enabled)
	}
	
	Response += TEXT("\r\n");
	Response += Content;
	return Response;
}

TSharedPtr<FJsonObject> FMCPJsonRpcServer::CreateErrorResponse(int32 ErrorCode, const FString& ErrorMessage, TSharedPtr<FJsonValue> Id)
{
	TSharedPtr<FJsonObject> Error = MakeShareable(new FJsonObject);
	Error->SetNumberField(TEXT("code"), ErrorCode);
	Error->SetStringField(TEXT("message"), ErrorMessage);

	TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
	Response->SetStringField(TEXT("jsonrpc"), TEXT("2.0"));
	Response->SetObjectField(TEXT("error"), Error);
	if (Id.IsValid())
	{
		Response->SetField(TEXT("id"), Id);
	}
	else
	{
		Response->SetField(TEXT("id"), MakeShareable(new FJsonValueNull()));
	}

	return Response;
}

void FMCPJsonRpcServer::LogMessage(const FString& Message)
{
	UE_LOG(LogTemp, Warning, TEXT("MCPJsonRpcServer: %s"), *Message);
}

template<typename ReturnType>
ReturnType FMCPJsonRpcServer::ExecuteOnGameThread(TFunction<ReturnType()> Task)
{
	// THREAD SAFETY FIX: All Editor API calls must be executed on Game Thread
	if (IsInGameThread())
	{
		// Already on Game Thread, execute directly with error handling
		try
		{
			return Task();
		}
		catch (...)
		{
			LogMessage(TEXT("Exception caught during Game Thread execution"));
			return ReturnType{}; // Return default-constructed value
		}
	}
	else
	{
		// Marshal to Game Thread and wait for completion
		ReturnType Result{};
		bool bTaskCompleted = false;
		FEvent* CompletionEvent = FPlatformProcess::GetSynchEventFromPool();
		
		AsyncTask(ENamedThreads::GameThread, [&Task, &Result, &bTaskCompleted, CompletionEvent, this]()
		{
			try
			{
				Result = Task();
				bTaskCompleted = true;
			}
			catch (...)
			{
				LogMessage(TEXT("Exception caught during marshaled Game Thread execution"));
				bTaskCompleted = false;
			}
			CompletionEvent->Trigger();
		});
		
		// Wait for completion (with timeout to prevent deadlocks)
		bool bTimedOut = !CompletionEvent->Wait(5000); // 5 second timeout
		FPlatformProcess::ReturnSynchEventToPool(CompletionEvent);
		
		if (bTimedOut)
		{
			LogMessage(TEXT("Game Thread execution timed out"));
		}
		else if (!bTaskCompleted)
		{
			LogMessage(TEXT("Game Thread execution failed"));
		}
		
		return Result;
	}
}

void FMCPJsonRpcServer::ExecuteOnGameThreadAsync(TFunction<void()> Task)
{
	// THREAD SAFETY FIX: Async execution on Game Thread with error handling
	if (IsInGameThread())
	{
		try
		{
			Task();
		}
		catch (...)
		{
			LogMessage(TEXT("Exception caught during async Game Thread execution"));
		}
	}
	else
	{
		AsyncTask(ENamedThreads::GameThread, [Task, this]()
		{
			try
			{
				Task();
			}
			catch (...)
			{
				LogMessage(TEXT("Exception caught during async marshaled Game Thread execution"));
			}
		});
	}
}

TSharedPtr<FJsonObject> FMCPJsonRpcServer::HandleResourcesList(TSharedPtr<FJsonObject> Params)
{
	// THREAD SAFETY FIX: Execute AssetRegistry access on Game Thread
	return ExecuteOnGameThread<TSharedPtr<FJsonObject>>([this, Params]() -> TSharedPtr<FJsonObject>
	{
		TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
		TArray<TSharedPtr<FJsonValue>> Assets;

		// Get path parameter (default to /Game if not provided)
		FString SearchPath = TEXT("/Game");
		if (Params.IsValid() && Params->HasField(TEXT("path")))
		{
			SearchPath = Params->GetStringField(TEXT("path"));
		}

		// Get Asset Registry - Safe to call from Game Thread
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

		// Create filter for the search path
		FARFilter Filter;
		Filter.PackagePaths.Add(FName(*SearchPath));
		Filter.bRecursivePaths = false; // Only immediate children

		// Get assets
		TArray<FAssetData> AssetDataArray;
		AssetRegistry.GetAssets(Filter, AssetDataArray);

		// Convert to JSON
		for (const FAssetData& AssetData : AssetDataArray)
		{
			TSharedPtr<FJsonObject> AssetJson = MakeShareable(new FJsonObject);
			AssetJson->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
			AssetJson->SetStringField(TEXT("path"), AssetData.GetObjectPathString());
			AssetJson->SetStringField(TEXT("class"), AssetData.AssetClassPath.ToString());
			AssetJson->SetStringField(TEXT("package"), AssetData.PackageName.ToString());

			Assets.Add(MakeShareable(new FJsonValueObject(AssetJson)));
		}

		Result->SetArrayField(TEXT("assets"), Assets);
		Result->SetNumberField(TEXT("count"), Assets.Num());
		Result->SetStringField(TEXT("path"), SearchPath);

		return Result;
	});
}

TSharedPtr<FJsonObject> FMCPJsonRpcServer::HandleResourcesGet(TSharedPtr<FJsonObject> Params)
{
	// THREAD SAFETY FIX: Execute asset loading on Game Thread
	return ExecuteOnGameThread<TSharedPtr<FJsonObject>>([this, Params]() -> TSharedPtr<FJsonObject>
	{
		TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);

		if (!Params.IsValid() || !Params->HasField(TEXT("asset_path")))
		{
			Result->SetStringField(TEXT("error"), TEXT("Missing asset_path parameter"));
			return Result;
		}

		FString AssetPath = Params->GetStringField(TEXT("asset_path"));

		// Get Asset Registry - Safe to call from Game Thread
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

		// Find asset
		FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(AssetPath));
		if (!AssetData.IsValid())
		{
			Result->SetStringField(TEXT("error"), TEXT("Asset not found"));
			return Result;
		}

		// Basic asset info
		Result->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
		Result->SetStringField(TEXT("path"), AssetData.GetObjectPathString());
		Result->SetStringField(TEXT("class"), AssetData.AssetClassPath.ToString());
		Result->SetStringField(TEXT("package"), AssetData.PackageName.ToString());

		// Add Blueprint-specific details if it's a Blueprint
		if (AssetData.AssetClassPath == UBlueprint::StaticClass()->GetClassPathName())
		{
			UBlueprint* Blueprint = Cast<UBlueprint>(AssetData.GetAsset()); // Asset loading must be on Game Thread
			if (Blueprint)
			{
				TSharedPtr<FJsonObject> BlueprintDetails = MakeShareable(new FJsonObject);
				
				// Parent class info
				if (Blueprint->ParentClass)
				{
					BlueprintDetails->SetStringField(TEXT("parent_class"), Blueprint->ParentClass->GetName());
				}

				// Variables
				TArray<TSharedPtr<FJsonValue>> Variables;
				for (const FBPVariableDescription& Variable : Blueprint->NewVariables)
				{
					TSharedPtr<FJsonObject> VarJson = MakeShareable(new FJsonObject);
					VarJson->SetStringField(TEXT("name"), Variable.VarName.ToString());
				VarJson->SetStringField(TEXT("type"), Variable.VarType.PinCategory.ToString());
				VarJson->SetBoolField(TEXT("is_public"), Variable.PropertyFlags & CPF_BlueprintVisible);
				Variables.Add(MakeShareable(new FJsonValueObject(VarJson)));
			}
			BlueprintDetails->SetArrayField(TEXT("variables"), Variables);

			// Functions
			TArray<TSharedPtr<FJsonValue>> Functions;
			for (UEdGraph* Graph : Blueprint->FunctionGraphs)
			{
				if (Graph)
				{
					TSharedPtr<FJsonObject> FuncJson = MakeShareable(new FJsonObject);
					FuncJson->SetStringField(TEXT("name"), Graph->GetFName().ToString());
					Functions.Add(MakeShareable(new FJsonValueObject(FuncJson)));
				}
			}
			BlueprintDetails->SetArrayField(TEXT("functions"), Functions);

				Result->SetObjectField(TEXT("blueprint_details"), BlueprintDetails);
			}
		}

		return Result;
	});
}

TSharedPtr<FJsonObject> FMCPJsonRpcServer::HandleResourcesCreate(TSharedPtr<FJsonObject> Params)
{
	// THREAD SAFETY FIX: Execute asset creation on Game Thread
	return ExecuteOnGameThread<TSharedPtr<FJsonObject>>([this, Params]() -> TSharedPtr<FJsonObject>
	{
		TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);

		if (!Params.IsValid())
		{
			Result->SetStringField(TEXT("error"), TEXT("Missing parameters"));
			return Result;
		}

		// Get required parameters
		FString AssetType, AssetName, Path;
		if (!Params->TryGetStringField(TEXT("asset_type"), AssetType) ||
			!Params->TryGetStringField(TEXT("asset_name"), AssetName) ||
			!Params->TryGetStringField(TEXT("path"), Path))
		{
			Result->SetStringField(TEXT("error"), TEXT("Missing required parameters: asset_type, asset_name, path"));
			return Result;
		}

		// Handle Blueprint creation
		if (AssetType == TEXT("Blueprint"))
		{
			// Get Asset Tools - Safe to call from Game Thread
			FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
			IAssetTools& AssetTools = AssetToolsModule.Get();

			// Create Blueprint Factory - Must be on Game Thread
			UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
			Factory->ParentClass = AActor::StaticClass(); // Default to Actor

			// Override parent class if specified
			if (Params->HasField(TEXT("parent_class")))
			{
				FString ParentClassName = Params->GetStringField(TEXT("parent_class"));
				UClass* ParentClass = FindObject<UClass>(nullptr, *ParentClassName);
				if (ParentClass)
				{
					Factory->ParentClass = ParentClass;
				}
			}

			// Create the asset - Must be on Game Thread
			FString PackageName = Path + TEXT("/") + AssetName;
			UObject* NewAsset = AssetTools.CreateAsset(AssetName, Path, UBlueprint::StaticClass(), Factory);

			if (NewAsset)
			{
				Result->SetStringField(TEXT("status"), TEXT("success"));
				Result->SetStringField(TEXT("asset_path"), NewAsset->GetPathName());
			Result->SetStringField(TEXT("asset_name"), AssetName);
			}
			else
			{
				Result->SetStringField(TEXT("error"), TEXT("Failed to create Blueprint asset"));
			}
		}
		else
		{
			Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Asset type '%s' not supported yet"), *AssetType));
		}

		return Result;
	});
}

TSharedPtr<FJsonObject> FMCPJsonRpcServer::HandleToolsCreateBlueprint(TSharedPtr<FJsonObject> Params)
{
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);

	if (!Params.IsValid())
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing parameters"));
		return Result;
	}

	// Get required parameters
	FString BlueprintName, Path, ParentClass;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName) ||
		!Params->TryGetStringField(TEXT("path"), Path) ||
		!Params->TryGetStringField(TEXT("parent_class"), ParentClass))
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing required parameters: blueprint_name, path, parent_class"));
		return Result;
	}

	// Find parent class
	UClass* ParentClassPtr = FindObject<UClass>(nullptr, *ParentClass);
	if (!ParentClassPtr)
	{
		ParentClassPtr = AActor::StaticClass(); // Default to Actor if not found
	}

	// Get Asset Tools
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	IAssetTools& AssetTools = AssetToolsModule.Get();

	// Create Blueprint Factory
	UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
	Factory->ParentClass = ParentClassPtr;

	// Create the Blueprint
	UObject* NewAsset = AssetTools.CreateAsset(BlueprintName, Path, UBlueprint::StaticClass(), Factory);

	if (NewAsset)
	{
		UBlueprint* Blueprint = Cast<UBlueprint>(NewAsset);
		if (Blueprint)
		{
			// Mark Blueprint as modified and regenerate
			FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
			FBlueprintEditorUtils::RefreshAllNodes(Blueprint);

			Result->SetStringField(TEXT("status"), TEXT("success"));
			Result->SetStringField(TEXT("blueprint_path"), Blueprint->GetPathName());
			Result->SetStringField(TEXT("blueprint_name"), BlueprintName);
			Result->SetStringField(TEXT("parent_class"), ParentClassPtr->GetName());
		}
		else
		{
			Result->SetStringField(TEXT("error"), TEXT("Failed to cast created asset to Blueprint"));
		}
	}
	else
	{
		Result->SetStringField(TEXT("error"), TEXT("Failed to create Blueprint asset"));
	}

	return Result;
}

TSharedPtr<FJsonObject> FMCPJsonRpcServer::HandleToolsAddVariable(TSharedPtr<FJsonObject> Params)
{
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);

	if (!Params.IsValid())
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing parameters"));
		return Result;
	}

	// Get required parameters
	FString BlueprintPath, VariableName, VariableType;
	bool bIsPublic = false;
	
	if (!Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath) ||
		!Params->TryGetStringField(TEXT("variable_name"), VariableName) ||
		!Params->TryGetStringField(TEXT("variable_type"), VariableType))
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing required parameters: blueprint_path, variable_name, variable_type"));
		return Result;
	}

	Params->TryGetBoolField(TEXT("is_public"), bIsPublic);

	// Load the Blueprint
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
	if (!Blueprint)
	{
		Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
		return Result;
	}

	// Create variable description
	FBPVariableDescription NewVariable;
	NewVariable.VarName = FName(*VariableName);
	
	// Set variable type based on string
	if (VariableType == TEXT("bool") || VariableType == TEXT("boolean"))
	{
		NewVariable.VarType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
	}
	else if (VariableType == TEXT("int") || VariableType == TEXT("integer"))
	{
		NewVariable.VarType.PinCategory = UEdGraphSchema_K2::PC_Int;
	}
	else if (VariableType == TEXT("float") || VariableType == TEXT("double"))
	{
		NewVariable.VarType.PinCategory = UEdGraphSchema_K2::PC_Real;
		NewVariable.VarType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
	}
	else if (VariableType == TEXT("string"))
	{
		NewVariable.VarType.PinCategory = UEdGraphSchema_K2::PC_String;
	}
	else if (VariableType == TEXT("vector"))
	{
		NewVariable.VarType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		NewVariable.VarType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
	}
	else
	{
		// Default to string if unknown type
		NewVariable.VarType.PinCategory = UEdGraphSchema_K2::PC_String;
	}

	// Set visibility
	if (bIsPublic)
	{
		NewVariable.PropertyFlags |= CPF_BlueprintVisible | CPF_BlueprintReadOnly;
	}

	// Add variable to Blueprint
	Blueprint->NewVariables.Add(NewVariable);
	
	// Mark Blueprint as modified and regenerate
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	FBlueprintEditorUtils::RefreshAllNodes(Blueprint);

	Result->SetStringField(TEXT("status"), TEXT("success"));
	Result->SetStringField(TEXT("variable_name"), VariableName);
	Result->SetStringField(TEXT("variable_type"), VariableType);
	Result->SetBoolField(TEXT("is_public"), bIsPublic);

	return Result;
}

TSharedPtr<FJsonObject> FMCPJsonRpcServer::HandleToolsAddFunction(TSharedPtr<FJsonObject> Params)
{
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);

	if (!Params.IsValid())
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing parameters"));
		return Result;
	}

	// Get required parameters
	FString BlueprintPath, FunctionName;
	
	if (!Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath) ||
		!Params->TryGetStringField(TEXT("function_name"), FunctionName))
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing required parameters: blueprint_path, function_name"));
		return Result;
	}

	// Load the Blueprint
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
	if (!Blueprint)
	{
		Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
		return Result;
	}

	// Create new function graph
	UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(
		Blueprint, 
		FName(*FunctionName), 
		UEdGraph::StaticClass(), 
		UEdGraphSchema_K2::StaticClass()
	);

	if (NewGraph)
	{
		// Add to function graphs
		Blueprint->FunctionGraphs.Add(NewGraph);
		
		// Create function entry node
		FGraphNodeCreator<UK2Node_FunctionEntry> EntryNodeCreator(*NewGraph);
		UK2Node_FunctionEntry* EntryNode = EntryNodeCreator.CreateNode();
		EntryNode->CustomGeneratedFunctionName = FName(*FunctionName);
		EntryNodeCreator.Finalize();

		// Mark Blueprint as modified and regenerate
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		FBlueprintEditorUtils::RefreshAllNodes(Blueprint);

		Result->SetStringField(TEXT("status"), TEXT("success"));
		Result->SetStringField(TEXT("function_name"), FunctionName);
		Result->SetStringField(TEXT("graph_name"), NewGraph->GetFName().ToString());
	}
	else
	{
		Result->SetStringField(TEXT("error"), TEXT("Failed to create function graph"));
	}

	return Result;
}

TSharedPtr<FJsonObject> FMCPJsonRpcServer::HandleToolsEditGraph(TSharedPtr<FJsonObject> Params)
{
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);

	if (!Params.IsValid())
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing parameters"));
		return Result;
	}

	// Get required parameters
	FString BlueprintPath, GraphName;
	
	if (!Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath) ||
		!Params->TryGetStringField(TEXT("graph_name"), GraphName))
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing required parameters: blueprint_path, graph_name"));
		return Result;
	}

	// Load the Blueprint
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
	if (!Blueprint)
	{
		Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
		return Result;
	}

	// Find the graph
	UEdGraph* Graph = nullptr;
	if (GraphName == TEXT("EventGraph") || GraphName.IsEmpty())
	{
		Graph = FBlueprintEditorUtils::FindEventGraph(Blueprint);
	}
	else
	{
		// Look for function graph
		for (UEdGraph* FunctionGraph : Blueprint->FunctionGraphs)
		{
			if (FunctionGraph && FunctionGraph->GetFName().ToString() == GraphName)
			{
				Graph = FunctionGraph;
				break;
			}
		}
	}

	if (!Graph)
	{
		Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Graph not found: %s"), *GraphName));
		return Result;
	}

	int32 NodesAdded = 0;
	
	// Handle nodes to add
	const TArray<TSharedPtr<FJsonValue>>* NodesToAdd;
	if (Params->TryGetArrayField(TEXT("nodes_to_add"), NodesToAdd))
	{
		for (const TSharedPtr<FJsonValue>& NodeValue : *NodesToAdd)
		{
			if (TSharedPtr<FJsonObject> NodeObject = NodeValue->AsObject())
			{
				FString NodeType;
				if (NodeObject->TryGetStringField(TEXT("type"), NodeType))
				{
					if (NodeType == TEXT("PrintString"))
					{
						// Create Print String node
						FGraphNodeCreator<UK2Node_CallFunction> NodeCreator(*Graph);
						UK2Node_CallFunction* NewNode = NodeCreator.CreateNode();
						NewNode->FunctionReference.SetExternalMember(
							GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, PrintString),
							UKismetSystemLibrary::StaticClass()
						);
						
						// Set position if provided
						double X = 0, Y = 0;
						if (NodeObject->TryGetNumberField(TEXT("x"), X) && NodeObject->TryGetNumberField(TEXT("y"), Y))
						{
							NewNode->NodePosX = X;
							NewNode->NodePosY = Y;
						}
						
						NodeCreator.Finalize();
						NodesAdded++;
					}
					else if (NodeType == TEXT("BeginPlay"))
					{
						// Create Begin Play event
						FGraphNodeCreator<UK2Node_Event> NodeCreator(*Graph);
						UK2Node_Event* NewNode = NodeCreator.CreateNode();
						// Use the actual function name for BeginPlay event in UE 5.6
						NewNode->EventReference.SetExternalMember(
							FName("ReceiveBeginPlay"),
							AActor::StaticClass()
						);
						NewNode->bOverrideFunction = true;
						
						// Set position if provided
						double X = 0, Y = 0;
						if (NodeObject->TryGetNumberField(TEXT("x"), X) && NodeObject->TryGetNumberField(TEXT("y"), Y))
						{
							NewNode->NodePosX = X;
							NewNode->NodePosY = Y;
						}
						
						NodeCreator.Finalize();
						NodesAdded++;
					}
					// Add more node types as needed
				}
			}
		}
	}

	// Mark Blueprint as modified and regenerate
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	FBlueprintEditorUtils::RefreshAllNodes(Blueprint);

	Result->SetStringField(TEXT("status"), TEXT("success"));
	Result->SetStringField(TEXT("graph_name"), GraphName);
	Result->SetNumberField(TEXT("nodes_added"), NodesAdded);

	return Result;
}

TSharedPtr<FJsonObject> FMCPJsonRpcServer::HandlePromptsList(TSharedPtr<FJsonObject> Params)
{
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	TArray<TSharedPtr<FJsonValue>> Prompts;

	// Create predefined game development prompts
	TArray<TSharedPtr<FJsonObject>> PromptsList = {
		// Player Character Blueprint Creation
		[]() {
			TSharedPtr<FJsonObject> Prompt = MakeShareable(new FJsonObject);
			Prompt->SetStringField(TEXT("name"), TEXT("create_player_character"));
			Prompt->SetStringField(TEXT("description"), TEXT("Step-by-step guide to create a player character Blueprint with basic movement"));
			return Prompt;
		}(),
		
		// Movement System Setup
		[]() {
			TSharedPtr<FJsonObject> Prompt = MakeShareable(new FJsonObject);
			Prompt->SetStringField(TEXT("name"), TEXT("setup_movement"));
			Prompt->SetStringField(TEXT("description"), TEXT("Implementation guide for basic character movement system (WASD controls)"));
			return Prompt;
		}(),
		
		// Jump Mechanic
		[]() {
			TSharedPtr<FJsonObject> Prompt = MakeShareable(new FJsonObject);
			Prompt->SetStringField(TEXT("name"), TEXT("add_jump_mechanic"));
			Prompt->SetStringField(TEXT("description"), TEXT("Guide to add jumping functionality to character controller"));
			return Prompt;
		}(),
		
		// Collectible Item
		[]() {
			TSharedPtr<FJsonObject> Prompt = MakeShareable(new FJsonObject);
			Prompt->SetStringField(TEXT("name"), TEXT("create_collectible"));
			Prompt->SetStringField(TEXT("description"), TEXT("Create collectible items that players can pick up and track"));
			return Prompt;
		}(),
		
		// Health System
		[]() {
			TSharedPtr<FJsonObject> Prompt = MakeShareable(new FJsonObject);
			Prompt->SetStringField(TEXT("name"), TEXT("implement_health_system"));
			Prompt->SetStringField(TEXT("description"), TEXT("Basic health system with damage handling and UI display"));
			return Prompt;
		}(),
		
		// Inventory System
		[]() {
			TSharedPtr<FJsonObject> Prompt = MakeShareable(new FJsonObject);
			Prompt->SetStringField(TEXT("name"), TEXT("create_inventory_system"));
			Prompt->SetStringField(TEXT("description"), TEXT("Simple inventory system for storing and managing items"));
			return Prompt;
		}()
	};

	// Convert to JSON array
	for (const auto& PromptObj : PromptsList)
	{
		Prompts.Add(MakeShareable(new FJsonValueObject(PromptObj)));
	}

	Result->SetArrayField(TEXT("prompts"), Prompts);
	Result->SetNumberField(TEXT("count"), Prompts.Num());

	return Result;
}

TSharedPtr<FJsonObject> FMCPJsonRpcServer::HandlePromptsGet(TSharedPtr<FJsonObject> Params)
{
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);

	if (!Params.IsValid() || !Params->HasField(TEXT("prompt_name")))
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing prompt_name parameter"));
		return Result;
	}

	FString PromptName = Params->GetStringField(TEXT("prompt_name"));

	// Define prompt contents
	TMap<FString, TPair<FString, FString>> PromptDatabase;
	
	PromptDatabase.Add(TEXT("create_player_character"), TPair<FString, FString>(
		TEXT("Step-by-step guide to create a player character Blueprint with basic movement"),
		TEXT("# Creating a Player Character Blueprint\n\n")
		TEXT("## Overview\n")
		TEXT("This guide will help you create a basic player character Blueprint that can move around the world.\n\n")
		TEXT("## Prerequisites\n")
		TEXT("- Unreal Engine project setup\n")
		TEXT("- Basic understanding of Blueprint system\n\n")
		TEXT("## Step 1: Create the Blueprint\n")
		TEXT("1. Right-click in Content Browser\n")
		TEXT("2. Select Blueprint Class\n")
		TEXT("3. Choose 'Character' as parent class\n")
		TEXT("4. Name it 'BP_PlayerCharacter'\n\n")
		TEXT("## Step 2: Set up Input Bindings\n")
		TEXT("1. Go to Edit > Project Settings > Input\n")
		TEXT("2. Add Action Mapping for 'Jump'\n")
		TEXT("3. Add Axis Mappings for 'MoveForward' and 'MoveRight'\n")
		TEXT("4. Bind to appropriate keys (WASD)\n\n")
		TEXT("## Step 3: Implement Movement\n")
		TEXT("1. Open BP_PlayerCharacter Blueprint\n")
		TEXT("2. Go to Event Graph\n")
		TEXT("3. Add Input Action Jump event\n")
		TEXT("4. Connect to Jump function\n")
		TEXT("5. Add Input Axis MoveForward/MoveRight events\n")
		TEXT("6. Connect to Add Movement Input nodes\n\n")
		TEXT("## Step 4: Set up Camera\n")
		TEXT("1. Add Camera Component\n")
		TEXT("2. Add Spring Arm Component\n")
		TEXT("3. Configure camera settings for third-person view\n\n")
		TEXT("## Step 5: Test the Character\n")
		TEXT("1. Set as Default Pawn Class in Game Mode\n")
		TEXT("2. Compile and test movement in Play mode")
	));
	
	PromptDatabase.Add(TEXT("setup_movement"), TPair<FString, FString>(
		TEXT("Implementation guide for basic character movement system (WASD controls)"),
		TEXT("# Basic Movement System Implementation\n\n")
		TEXT("## Overview\n")
		TEXT("Implement WASD movement controls for your character.\n\n")
		TEXT("## Input Setup\n")
		TEXT("### Project Settings > Input\n")
		TEXT("1. **Axis Mappings:**\n")
		TEXT("   - MoveForward: W (Scale 1.0), S (Scale -1.0)\n")
		TEXT("   - MoveRight: D (Scale 1.0), A (Scale -1.0)\n")
		TEXT("   - Turn: Mouse X (Scale 1.0)\n")
		TEXT("   - LookUp: Mouse Y (Scale -1.0)\n\n")
		TEXT("## Blueprint Implementation\n")
		TEXT("### Event Graph Nodes:\n")
		TEXT("1. **InputAxis MoveForward**\n")
		TEXT("   - Connect to 'Add Movement Input'\n")
		TEXT("   - World Direction: Get Actor Forward Vector\n\n")
		TEXT("2. **InputAxis MoveRight**\n")
		TEXT("   - Connect to 'Add Movement Input'\n")
		TEXT("   - World Direction: Get Actor Right Vector\n\n")
		TEXT("3. **InputAxis Turn**\n")
		TEXT("   - Connect to 'Add Controller Yaw Input'\n\n")
		TEXT("4. **InputAxis LookUp**\n")
		TEXT("   - Connect to 'Add Controller Pitch Input'\n\n")
		TEXT("## Character Movement Component Settings\n")
		TEXT("- Max Walk Speed: 600\n")
		TEXT("- Ground Friction: 8.0\n")
		TEXT("- Max Acceleration: 2048\n")
		TEXT("- Air Control: 0.05\n\n")
		TEXT("## Testing\n")
		TEXT("1. Compile Blueprint\n")
		TEXT("2. Test in Play mode\n")
		TEXT("3. Verify smooth movement in all directions")
	));
	
	PromptDatabase.Add(TEXT("add_jump_mechanic"), TPair<FString, FString>(
		TEXT("Guide to add jumping functionality to character controller"),
		TEXT("# Jump Mechanic Implementation\n\n")
		TEXT("## Overview\n")
		TEXT("Add jumping capability to your character with proper physics.\n\n")
		TEXT("## Input Setup\n")
		TEXT("### Project Settings > Input\n")
		TEXT("1. **Action Mapping:**\n")
		TEXT("   - Jump: Spacebar\n")
		TEXT("   - Jump: Gamepad Face Button Bottom\n\n")
		TEXT("## Blueprint Implementation\n")
		TEXT("### Event Graph:\n")
		TEXT("1. **InputAction Jump (Pressed)**\n")
		TEXT("   - Connect to 'Jump' function (inherited from Character)\n\n")
		TEXT("2. **InputAction Jump (Released)**\n")
		TEXT("   - Connect to 'Stop Jumping' function\n\n")
		TEXT("## Character Movement Settings\n")
		TEXT("### Movement Component Properties:\n")
		TEXT("- Jump Z Velocity: 420 (adjust for desired jump height)\n")
		TEXT("- Air Control: 0.05 (allows slight movement in air)\n")
		TEXT("- Gravity Scale: 1.75 (makes jumping feel more responsive)\n")
		TEXT("- Ground Friction: 8.0\n")
		TEXT("- Max Jump Hold Time: 0.0 (instant jump)\n\n")
		TEXT("## Advanced Features (Optional)\n")
		TEXT("### Double Jump Implementation:\n")
		TEXT("1. Add Integer variable 'JumpCount'\n")
		TEXT("2. Override 'Can Jump' function\n")
		TEXT("3. Check if JumpCount < 2\n")
		TEXT("4. Reset JumpCount on landing\n\n")
		TEXT("### Coyote Time:\n")
		TEXT("1. Add Timer for grace period after leaving ground\n")
		TEXT("2. Allow jump for short time after falling\n\n")
		TEXT("## Testing Checklist\n")
		TEXT("- [ ] Character jumps when spacebar pressed\n")
		TEXT("- [ ] Jump height feels appropriate\n")
		TEXT("- [ ] Cannot jump while already in air (unless double jump)\n")
		TEXT("- [ ] Smooth landing animation\n")
		TEXT("- [ ] Works with gamepad input")
	));
	
	PromptDatabase.Add(TEXT("create_collectible"), TPair<FString, FString>(
		TEXT("Create collectible items that players can pick up and track"),
		TEXT("# Collectible Item System\n\n")
		TEXT("## Overview\n")
		TEXT("Create items that players can collect with visual feedback and scoring.\n\n")
		TEXT("## Blueprint Creation\n")
		TEXT("### 1. Create Collectible Blueprint\n")
		TEXT("1. Right-click Content Browser > Blueprint Class\n")
		TEXT("2. Choose 'Actor' as parent class\n")
		TEXT("3. Name it 'BP_Collectible'\n\n")
		TEXT("### 2. Add Components\n")
		TEXT("1. **Static Mesh Component:**\n")
		TEXT("   - Set mesh (sphere, coin, gem, etc.)\n")
		TEXT("   - Scale appropriately\n")
		TEXT("   - Add material with emissive properties\n\n")
		TEXT("2. **Sphere Collision:**\n")
		TEXT("   - Set collision to 'Trigger'\n")
		TEXT("   - Radius: 100-150 units\n")
		TEXT("   - Generate overlap events: True\n\n")
		TEXT("3. **Rotating Movement Component:**\n")
		TEXT("   - Rotation Rate: (0, 0, 90) for Y-axis spin\n\n")
		TEXT("## Blueprint Logic\n")
		TEXT("### Event Graph Implementation:\n")
		TEXT("1. **On Component Begin Overlap:**\n")
		TEXT("   - Check if Other Actor = Player Character\n")
		TEXT("   - Play pickup sound effect\n")
		TEXT("   - Add to player score/inventory\n")
		TEXT("   - Spawn particle effect\n")
		TEXT("   - Destroy actor\n\n")
		TEXT("### Example Logic Flow:\n")
		TEXT("```\n")
		TEXT("Event ActorBeginOverlap\n")
		TEXT("  ↓\n")
		TEXT("Cast to ThirdPersonCharacter\n")
		TEXT("  ↓ (Success)\n")
		TEXT("Play Sound 2D (pickup sound)\n")
		TEXT("  ↓\n")
		TEXT("Spawn Emitter at Location (sparkle effect)\n")
		TEXT("  ↓\n")
		TEXT("Add to Player Score (Custom Event)\n")
		TEXT("  ↓\n")
		TEXT("Destroy Actor\n")
		TEXT("```\n\n")
		TEXT("## Player Integration\n")
		TEXT("### Add to Player Character:\n")
		TEXT("1. Integer variable 'Score' or 'CollectedItems'\n")
		TEXT("2. Custom Event 'AddCollectible'\n")
		TEXT("3. UI update function\n\n")
		TEXT("## Visual Polish\n")
		TEXT("### Material Setup:\n")
		TEXT("- Emissive color for glow effect\n")
		TEXT("- Pulsing animation using Time node\n")
		TEXT("- Transparency for ethereal look\n\n")
		TEXT("### Effects:\n")
		TEXT("- Particle system for pickup feedback\n")
		TEXT("- Sound cue for audio feedback\n")
		TEXT("- UI animation for score display\n\n")
		TEXT("## Testing\n")
		TEXT("1. Place collectibles in level\n")
		TEXT("2. Test collision detection\n")
		TEXT("3. Verify score tracking\n")
		TEXT("4. Check audio/visual feedback")
	));
	
	PromptDatabase.Add(TEXT("implement_health_system"), TPair<FString, FString>(
		TEXT("Basic health system with damage handling and UI display"),
		TEXT("# Health System Implementation\n\n")
		TEXT("## Overview\n")
		TEXT("Create a robust health system with damage, healing, and death mechanics.\n\n")
		TEXT("## Player Character Setup\n")
		TEXT("### Variables to Add:\n")
		TEXT("1. **Health (Float):**\n")
		TEXT("   - Default Value: 100.0\n")
		TEXT("   - Instance Editable: True\n\n")
		TEXT("2. **MaxHealth (Float):**\n")
		TEXT("   - Default Value: 100.0\n")
		TEXT("   - Instance Editable: True\n\n")
		TEXT("3. **bIsDead (Boolean):**\n")
		TEXT("   - Default Value: False\n\n")
		TEXT("## Custom Functions\n")
		TEXT("### 1. TakeDamage Function\n")
		TEXT("**Inputs:** DamageAmount (Float)\n")
		TEXT("**Logic:**\n")
		TEXT("```\n")
		TEXT("If NOT bIsDead:\n")
		TEXT("  Health = Health - DamageAmount\n")
		TEXT("  Clamp Health (0.0 to MaxHealth)\n")
		TEXT("  \n")
		TEXT("  If Health <= 0:\n")
		TEXT("    Set bIsDead = True\n")
		TEXT("    Call HandleDeath()\n")
		TEXT("  \n")
		TEXT("  Update Health UI\n")
		TEXT("  Play Damage Effects\n")
		TEXT("```\n\n")
		TEXT("### 2. HealPlayer Function\n")
		TEXT("**Inputs:** HealAmount (Float)\n")
		TEXT("**Logic:**\n")
		TEXT("```\n")
		TEXT("If NOT bIsDead:\n")
		TEXT("  Health = Health + HealAmount\n")
		TEXT("  Clamp Health (0.0 to MaxHealth)\n")
		TEXT("  Update Health UI\n")
		TEXT("  Play Healing Effects\n")
		TEXT("```\n\n")
		TEXT("### 3. HandleDeath Function\n")
		TEXT("**Logic:**\n")
		TEXT("```\n")
		TEXT("Disable Input\n")
		TEXT("Play Death Animation\n")
		TEXT("Show Death UI/Respawn Options\n")
		TEXT("Optional: Respawn after delay\n")
		TEXT("```\n\n")
		TEXT("## UI Implementation\n")
		TEXT("### Health Bar Widget:\n")
		TEXT("1. Create Widget Blueprint 'WBP_HealthBar'\n")
		TEXT("2. Add Progress Bar component\n")
		TEXT("3. Bind progress to Health/MaxHealth ratio\n")
		TEXT("4. Add to player's viewport on BeginPlay\n\n")
		TEXT("### Update Health Display:\n")
		TEXT("```\n")
		TEXT("Progress Bar Percent = Current Health / Max Health\n")
		TEXT("```\n\n")
		TEXT("## Damage Sources\n")
		TEXT("### Environmental Damage:\n")
		TEXT("1. Create damage volume triggers\n")
		TEXT("2. On overlap, call TakeDamage function\n\n")
		TEXT("### Enemy Damage:\n")
		TEXT("1. Implement in enemy AI behavior\n")
		TEXT("2. Call TakeDamage on successful attack\n\n")
		TEXT("## Testing Checklist\n")
		TEXT("- [ ] Health decreases when taking damage\n")
		TEXT("- [ ] Health UI updates correctly\n")
		TEXT("- [ ] Player dies at 0 health\n")
		TEXT("- [ ] Healing works and doesn't exceed max health\n")
		TEXT("- [ ] Death state prevents further damage\n")
		TEXT("- [ ] Visual/audio feedback works")
	));
	
	PromptDatabase.Add(TEXT("create_inventory_system"), TPair<FString, FString>(
		TEXT("Simple inventory system for storing and managing items"),
		TEXT("# Inventory System Implementation\n\n")
		TEXT("## Overview\n")
		TEXT("Create a flexible inventory system for managing player items.\n\n")
		TEXT("## Data Structure Setup\n")
		TEXT("### 1. Create Item Data Structure\n")
		TEXT("**Blueprint Structure: 'ItemData'**\n")
		TEXT("- ItemName (String): Display name\n")
		TEXT("- ItemID (String): Unique identifier\n")
		TEXT("- ItemIcon (Texture 2D): UI icon\n")
		TEXT("- ItemDescription (String): Item description\n")
		TEXT("- ItemType (Enum): Weapon, Consumable, Key, etc.\n")
		TEXT("- MaxStackSize (Integer): How many can stack\n")
		TEXT("- ItemValue (Integer): Worth/price\n\n")
		TEXT("### 2. Create Inventory Slot Structure\n")
		TEXT("**Blueprint Structure: 'InventorySlot'**\n")
		TEXT("- Item (ItemData): The item data\n")
		TEXT("- Quantity (Integer): How many in stack\n")
		TEXT("- bIsEmpty (Boolean): Slot status\n\n")
		TEXT("## Player Character Integration\n")
		TEXT("### Variables to Add:\n")
		TEXT("1. **Inventory (Array of InventorySlot):**\n")
		TEXT("   - Default size: 20 slots\n")
		TEXT("   - Initialize with empty slots\n\n")
		TEXT("2. **MaxInventorySize (Integer):**\n")
		TEXT("   - Default: 20\n")
		TEXT("   - Instance Editable: True\n\n")
		TEXT("## Core Functions\n")
		TEXT("### 1. AddItem Function\n")
		TEXT("**Inputs:** NewItem (ItemData), Amount (Integer)\n")
		TEXT("**Returns:** Success (Boolean)\n")
		TEXT("**Logic:**\n")
		TEXT("```\n")
		TEXT("1. Check for existing stacks of same item\n")
		TEXT("2. If found and can stack:\n")
		TEXT("   - Add to existing stack\n")
		TEXT("   - Return success\n")
		TEXT("3. If no existing stack:\n")
		TEXT("   - Find first empty slot\n")
		TEXT("   - Add new item\n")
		TEXT("   - Return success/failure\n")
		TEXT("```\n\n")
		TEXT("### 2. RemoveItem Function\n")
		TEXT("**Inputs:** ItemID (String), Amount (Integer)\n")
		TEXT("**Returns:** Success (Boolean)\n")
		TEXT("**Logic:**\n")
		TEXT("```\n")
		TEXT("1. Find item in inventory\n")
		TEXT("2. If found:\n")
		TEXT("   - Reduce quantity\n")
		TEXT("   - If quantity <= 0, clear slot\n")
		TEXT("   - Return success\n")
		TEXT("3. Return failure if not found\n")
		TEXT("```\n\n")
		TEXT("### 3. UseItem Function\n")
		TEXT("**Inputs:** SlotIndex (Integer)\n")
		TEXT("**Logic:**\n")
		TEXT("```\n")
		TEXT("1. Get item from slot\n")
		TEXT("2. Switch on ItemType:\n")
		TEXT("   - Consumable: Apply effect, remove item\n")
		TEXT("   - Weapon: Equip weapon\n")
		TEXT("   - Key: Check for locked doors\n")
		TEXT("```\n\n")
		TEXT("## UI Implementation\n")
		TEXT("### Inventory Widget:\n")
		TEXT("1. Create 'WBP_Inventory' widget\n")
		TEXT("2. Add Uniform Grid Panel for slots\n")
		TEXT("3. Create 'WBP_InventorySlot' for individual slots\n")
		TEXT("4. Bind slot data to display item info\n\n")
		TEXT("### Slot Widget Components:\n")
		TEXT("- Image for item icon\n")
		TEXT("- Text for quantity\n")
		TEXT("- Button for interaction\n")
		TEXT("- Tooltip for item details\n\n")
		TEXT("## Item Pickup Integration\n")
		TEXT("### Modify Collectible System:\n")
		TEXT("```\n")
		TEXT("On Pickup:\n")
		TEXT("  ↓\n")
		TEXT("Get Item Data\n")
		TEXT("  ↓\n")
		TEXT("Call AddItem Function\n")
		TEXT("  ↓\n")
		TEXT("If Success: Destroy pickup\n")
		TEXT("If Failure: Show 'Inventory Full' message\n")
		TEXT("```\n\n")
		TEXT("## Advanced Features\n")
		TEXT("### Item Categories:\n")
		TEXT("- Filter inventory by item type\n")
		TEXT("- Separate tabs for different categories\n\n")
		TEXT("### Drag and Drop:\n")
		TEXT("- Implement slot-to-slot item movement\n")
		TEXT("- Item dropping/deletion\n\n")
		TEXT("### Item Comparison:\n")
		TEXT("- Show stat differences for equipment\n")
		TEXT("- Highlight better/worse items\n\n")
		TEXT("## Testing\n")
		TEXT("1. Test adding items to inventory\n")
		TEXT("2. Verify stacking mechanics\n")
		TEXT("3. Test inventory full scenarios\n")
		TEXT("4. Check UI updates correctly\n")
		TEXT("5. Test item usage functions")
	));

	// Look up the prompt
	if (PromptDatabase.Contains(PromptName))
	{
		TPair<FString, FString> PromptInfo = PromptDatabase[PromptName];
		Result->SetStringField(TEXT("name"), PromptName);
		Result->SetStringField(TEXT("description"), PromptInfo.Key);
		Result->SetStringField(TEXT("content"), PromptInfo.Value);
	}
	else
	{
		Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Prompt not found: %s"), *PromptName));
	}

	return Result;
}

bool FMCPJsonRpcServer::StartServerWithFallback(int32 PreferredPort)
{
	// Try preferred port first
	if (StartServer(PreferredPort))
	{
		return true;
	}

	LogMessage(FString::Printf(TEXT("Port %d is unavailable, trying fallback ports..."), PreferredPort));

	// Try fallback ports
	for (int32 Port : FallbackPorts)
	{
		if (Port != PreferredPort && IsPortAvailable(Port))
		{
			if (StartServer(Port))
			{
				LogMessage(FString::Printf(TEXT("Server started on fallback port %d"), Port));
				return true;
			}
		}
	}

	LogMessage(TEXT("All fallback ports are unavailable"));
	return false;
}

bool FMCPJsonRpcServer::IsPortAvailable(int32 Port)
{
	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SocketSubsystem)
	{
		return false;
	}

	FSocket* TestSocket = SocketSubsystem->CreateSocket(NAME_Stream, TEXT("PortTest"), false);
	if (!TestSocket)
	{
		return false;
	}

	TSharedRef<FInternetAddr> TestAddr = SocketSubsystem->CreateInternetAddr();
	bool bIsValid = false;
	TestAddr->SetIp(TEXT("127.0.0.1"), bIsValid);
	if (!bIsValid)
	{
		TestSocket->Close();
		SocketSubsystem->DestroySocket(TestSocket);
		return false;
	}
	TestAddr->SetPort(Port);

	bool bPortAvailable = TestSocket->Bind(*TestAddr);
	
	TestSocket->Close();
	SocketSubsystem->DestroySocket(TestSocket);
	
	return bPortAvailable;
}

bool FMCPJsonRpcServer::RestartServer()
{
	int32 CurrentPort = ServerPort;
	
	if (IsRunning())
	{
		StopServer();
		// Wait a moment for cleanup
		FPlatformProcess::Sleep(0.5f);
	}
	
	return StartServer(CurrentPort);
}

void FMCPJsonRpcServer::ApplySettings(const UMCPServerSettings* Settings)
{
	if (!Settings)
	{
		return;
	}

	// Cache applied settings
	AppliedMaxConnections = Settings->MaxClientConnections;
	AppliedTimeoutSeconds = Settings->ServerTimeoutSeconds;
	bAppliedEnableCORS = Settings->bEnableCORS;
	bAppliedEnableAuth = Settings->bEnableAuthentication;
	AppliedAPIKey = Settings->APIKey;
	AppliedCustomHeaders = Settings->CustomHeaders;

	// Apply port if server is not running
	if (!IsRunning())
	{
		ServerPort = Settings->ServerPort;
	}

	UE_LOG(LogTemp, Log, TEXT("MCP Server: Applied settings - Port: %d, MaxConnections: %d, Timeout: %ds, CORS: %s, Auth: %s"),
		Settings->ServerPort, AppliedMaxConnections, AppliedTimeoutSeconds,
		bAppliedEnableCORS ? TEXT("Enabled") : TEXT("Disabled"),
		bAppliedEnableAuth ? TEXT("Enabled") : TEXT("Disabled"));
}

FString FMCPJsonRpcServer::GetAppliedSettingsString() const
{
	return FString::Printf(TEXT("Applied Settings - Port: %d, Max Connections: %d, Timeout: %ds, CORS: %s, Auth: %s, Custom Headers: %d"),
		ServerPort, AppliedMaxConnections, AppliedTimeoutSeconds,
		bAppliedEnableCORS ? TEXT("Enabled") : TEXT("Disabled"),
		bAppliedEnableAuth ? TEXT("Enabled") : TEXT("Disabled"),
		AppliedCustomHeaders.Num());
}