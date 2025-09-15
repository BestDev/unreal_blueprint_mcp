#include "MCPServerSettings.h"
#include "Engine/Engine.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Dom/JsonObject.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Common/TcpSocketBuilder.h"

#if WITH_EDITOR
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#endif

#define LOCTEXT_NAMESPACE "MCPServerSettings"

// Static delegate definitions
UMCPServerSettings::FOnSettingsChanged UMCPServerSettings::OnSettingsChanged;
UMCPServerSettings::FOnApplyServerSettings UMCPServerSettings::OnApplyServerSettings;

UMCPServerSettings::UMCPServerSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Set default values
	ServerPort = 8080;
	bAutoStartServer = false;
	MaxClientConnections = 10;
	ServerTimeoutSeconds = 30;
	bEnableCORS = false;
	LogLevel = EMCPLogLevel::Basic;
	bLogToFile = false;
	LogFilePath = TEXT("Logs/MCPServer.log");
	RequestRateLimit = 0;
	bEnableAuthentication = false;
	APIKey = TEXT("");
	CurrentPreset = EMCPServerPreset::Development;
}

FName UMCPServerSettings::GetCategoryName() const
{
	return TEXT("Plugins");
}

FText UMCPServerSettings::GetSectionText() const
{
	return LOCTEXT("SettingsDisplayName", "MCP Server");
}

FText UMCPServerSettings::GetSectionDescription() const
{
	return LOCTEXT("SettingsDescription", "Configuration settings for the Model Context Protocol (MCP) Server plugin");
}

const UMCPServerSettings* UMCPServerSettings::Get()
{
	return GetDefault<UMCPServerSettings>();
}

UMCPServerSettings* UMCPServerSettings::GetMutable()
{
	return GetMutableDefault<UMCPServerSettings>();
}

#if WITH_EDITOR
void UMCPServerSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Get the property that was changed
	FProperty* Property = PropertyChangedEvent.Property;
	if (!Property)
	{
		return;
	}

	FName PropertyName = Property->GetFName();

	// Validate specific properties
	FString ErrorMessage;
	bool bValidationFailed = false;

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UMCPServerSettings, ServerPort))
	{
		if (!ValidatePort(ServerPort, ErrorMessage))
		{
			bValidationFailed = true;
		}
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UMCPServerSettings, ServerTimeoutSeconds))
	{
		if (!ValidateTimeout(ServerTimeoutSeconds, ErrorMessage))
		{
			bValidationFailed = true;
		}
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UMCPServerSettings, MaxClientConnections))
	{
		if (!ValidateMaxConnections(MaxClientConnections, ErrorMessage))
		{
			bValidationFailed = true;
		}
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UMCPServerSettings, LogFilePath))
	{
		if (bLogToFile && !ValidateLogFilePath(LogFilePath, ErrorMessage))
		{
			bValidationFailed = true;
		}
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UMCPServerSettings, CurrentPreset))
	{
		// Auto-apply preset when changed
		if (CurrentPreset != EMCPServerPreset::Custom)
		{
			ApplyPreset(CurrentPreset);
		}
	}

	// Show validation error if any
	if (bValidationFailed)
	{
		FNotificationInfo Info(FText::FromString(ErrorMessage));
		Info.ExpireDuration = 5.0f;
		Info.bUseLargeFont = false;
		FSlateNotificationManager::Get().AddNotification(Info);
		
		UE_LOG(LogTemp, Warning, TEXT("MCP Server Settings Validation Error: %s"), *ErrorMessage);
	}

	// Check if server restart is required
	bool bNeedsRestart = (PropertyName == GET_MEMBER_NAME_CHECKED(UMCPServerSettings, ServerPort)) ||
						 (PropertyName == GET_MEMBER_NAME_CHECKED(UMCPServerSettings, MaxClientConnections)) ||
						 (PropertyName == GET_MEMBER_NAME_CHECKED(UMCPServerSettings, ServerTimeoutSeconds)) ||
						 (PropertyName == GET_MEMBER_NAME_CHECKED(UMCPServerSettings, bEnableCORS)) ||
						 (PropertyName == GET_MEMBER_NAME_CHECKED(UMCPServerSettings, bEnableAuthentication)) ||
						 (PropertyName == GET_MEMBER_NAME_CHECKED(UMCPServerSettings, APIKey));

	if (bNeedsRestart)
	{
		FNotificationInfo RestartInfo(LOCTEXT("ServerRestartRequired", "Server restart required for changes to take effect"));
		RestartInfo.ExpireDuration = 3.0f;
		FSlateNotificationManager::Get().AddNotification(RestartInfo);
	}

	// Broadcast settings changed
	BroadcastSettingsChanged();

	// Apply settings if server is running
	OnApplyServerSettings.Broadcast(this);

	// Save config
	SaveConfig();
}

bool UMCPServerSettings::CanEditChange(const FProperty* InProperty) const
{
	if (!InProperty)
	{
		return false;
	}

	FName PropertyName = InProperty->GetFName();

	// Conditionally enable/disable properties based on other settings
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UMCPServerSettings, LogFilePath))
	{
		return bLogToFile;
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UMCPServerSettings, AllowedOrigins))
	{
		return bEnableCORS;
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UMCPServerSettings, APIKey))
	{
		return bEnableAuthentication;
	}

	return Super::CanEditChange(InProperty);
}
#endif

bool UMCPServerSettings::ValidateSettings(FString& OutErrorMessage) const
{
	OutErrorMessage.Empty();

	FString TempError;
	
	if (!ValidatePort(ServerPort, TempError))
	{
		OutErrorMessage += TempError + TEXT("\\n");
	}

	if (!ValidateTimeout(ServerTimeoutSeconds, TempError))
	{
		OutErrorMessage += TempError + TEXT("\\n");
	}

	if (!ValidateMaxConnections(MaxClientConnections, TempError))
	{
		OutErrorMessage += TempError + TEXT("\\n");
	}

	if (bLogToFile && !ValidateLogFilePath(LogFilePath, TempError))
	{
		OutErrorMessage += TempError + TEXT("\\n");
	}

	if (bEnableAuthentication && APIKey.IsEmpty())
	{
		OutErrorMessage += TEXT("API Key cannot be empty when authentication is enabled\\n");
	}

	return OutErrorMessage.IsEmpty();
}

bool UMCPServerSettings::IsPortAvailable(int32 Port) const
{
	if (!ValidatePort(Port, FString()))
	{
		return false;
	}

	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SocketSubsystem)
	{
		return false;
	}

	FSocket* TestSocket = SocketSubsystem->CreateSocket(NAME_Stream, TEXT("MCPPortTest"), false);
	if (!TestSocket)
	{
		return false;
	}

	TSharedRef<FInternetAddr> Addr = SocketSubsystem->CreateInternetAddr();
	Addr->SetAnyAddress();
	Addr->SetPort(Port);

	bool bCanBind = TestSocket->Bind(*Addr);
	
	TestSocket->Close();
	SocketSubsystem->DestroySocket(TestSocket);

	return bCanBind;
}

TArray<int32> UMCPServerSettings::GetSuggestedPorts() const
{
	TArray<int32> SuggestedPorts = {8080, 8081, 8082, 8083, 8084, 8090, 9000, 9001, 9002, 9003};
	TArray<int32> AvailablePorts;

	for (int32 Port : SuggestedPorts)
	{
		if (IsPortAvailable(Port))
		{
			AvailablePorts.Add(Port);
		}
	}

	return AvailablePorts;
}

void UMCPServerSettings::ResetToDefaults()
{
	ServerPort = 8080;
	bAutoStartServer = false;
	MaxClientConnections = 10;
	ServerTimeoutSeconds = 30;
	bEnableCORS = false;
	LogLevel = EMCPLogLevel::Basic;
	bLogToFile = false;
	LogFilePath = TEXT("Logs/MCPServer.log");
	RequestRateLimit = 0;
	bEnableAuthentication = false;
	APIKey = TEXT("");
	CurrentPreset = EMCPServerPreset::Development;
	CustomHeaders.Empty();
	AllowedOrigins.Empty();

	SaveConfig();
	BroadcastSettingsChanged();

#if WITH_EDITOR
	FNotificationInfo Info(LOCTEXT("SettingsReset", "MCP Server settings reset to defaults"));
	Info.ExpireDuration = 3.0f;
	FSlateNotificationManager::Get().AddNotification(Info);
#endif
}

void UMCPServerSettings::ApplyPreset(EMCPServerPreset Preset)
{
	int32 PresetPort;
	bool PresetAutoStart;
	EMCPLogLevel PresetLogLevel;
	int32 PresetMaxConnections;
	int32 PresetTimeout;
	bool PresetEnableCORS;

	GetPresetDefaults(Preset, PresetPort, PresetAutoStart, PresetLogLevel, PresetMaxConnections, PresetTimeout, PresetEnableCORS);

	ServerPort = PresetPort;
	bAutoStartServer = PresetAutoStart;
	LogLevel = PresetLogLevel;
	MaxClientConnections = PresetMaxConnections;
	ServerTimeoutSeconds = PresetTimeout;
	bEnableCORS = PresetEnableCORS;
	CurrentPreset = Preset;

	SaveConfig();
	BroadcastSettingsChanged();

#if WITH_EDITOR
	FString PresetName = StaticEnum<EMCPServerPreset>()->GetDisplayNameTextByValue((int64)Preset).ToString();
	FNotificationInfo Info(FText::FromString(FString::Printf(TEXT("Applied %s preset"), *PresetName)));
	Info.ExpireDuration = 3.0f;
	FSlateNotificationManager::Get().AddNotification(Info);
#endif
}

FString UMCPServerSettings::ExportToJSON() const
{
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);

	JsonObject->SetNumberField(TEXT("ServerPort"), ServerPort);
	JsonObject->SetBoolField(TEXT("AutoStartServer"), bAutoStartServer);
	JsonObject->SetNumberField(TEXT("MaxClientConnections"), MaxClientConnections);
	JsonObject->SetNumberField(TEXT("ServerTimeoutSeconds"), ServerTimeoutSeconds);
	JsonObject->SetBoolField(TEXT("EnableCORS"), bEnableCORS);
	JsonObject->SetNumberField(TEXT("LogLevel"), (int32)LogLevel);
	JsonObject->SetBoolField(TEXT("LogToFile"), bLogToFile);
	JsonObject->SetStringField(TEXT("LogFilePath"), LogFilePath);
	JsonObject->SetNumberField(TEXT("RequestRateLimit"), RequestRateLimit);
	JsonObject->SetBoolField(TEXT("EnableAuthentication"), bEnableAuthentication);
	JsonObject->SetStringField(TEXT("APIKey"), APIKey);
	JsonObject->SetNumberField(TEXT("CurrentPreset"), (int32)CurrentPreset);

	// Custom headers
	TSharedPtr<FJsonObject> HeadersObject = MakeShareable(new FJsonObject);
	for (const auto& Header : CustomHeaders)
	{
		HeadersObject->SetStringField(Header.Key, Header.Value);
	}
	JsonObject->SetObjectField(TEXT("CustomHeaders"), HeadersObject);

	// Allowed origins
	TArray<TSharedPtr<FJsonValue>> OriginsArray;
	for (const FString& Origin : AllowedOrigins)
	{
		OriginsArray.Add(MakeShareable(new FJsonValueString(Origin)));
	}
	JsonObject->SetArrayField(TEXT("AllowedOrigins"), OriginsArray);

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

	return OutputString;
}

bool UMCPServerSettings::ImportFromJSON(const FString& JSONString, FString& OutErrorMessage)
{
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JSONString);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		OutErrorMessage = TEXT("Failed to parse JSON data");
		return false;
	}

	// Import basic settings
	if (JsonObject->HasField(TEXT("ServerPort")))
	{
		ServerPort = JsonObject->GetIntegerField(TEXT("ServerPort"));
	}
	if (JsonObject->HasField(TEXT("AutoStartServer")))
	{
		bAutoStartServer = JsonObject->GetBoolField(TEXT("AutoStartServer"));
	}
	if (JsonObject->HasField(TEXT("MaxClientConnections")))
	{
		MaxClientConnections = JsonObject->GetIntegerField(TEXT("MaxClientConnections"));
	}
	if (JsonObject->HasField(TEXT("ServerTimeoutSeconds")))
	{
		ServerTimeoutSeconds = JsonObject->GetIntegerField(TEXT("ServerTimeoutSeconds"));
	}
	if (JsonObject->HasField(TEXT("EnableCORS")))
	{
		bEnableCORS = JsonObject->GetBoolField(TEXT("EnableCORS"));
	}
	if (JsonObject->HasField(TEXT("LogLevel")))
	{
		LogLevel = (EMCPLogLevel)JsonObject->GetIntegerField(TEXT("LogLevel"));
	}
	if (JsonObject->HasField(TEXT("LogToFile")))
	{
		bLogToFile = JsonObject->GetBoolField(TEXT("LogToFile"));
	}
	if (JsonObject->HasField(TEXT("LogFilePath")))
	{
		LogFilePath = JsonObject->GetStringField(TEXT("LogFilePath"));
	}
	if (JsonObject->HasField(TEXT("RequestRateLimit")))
	{
		RequestRateLimit = JsonObject->GetIntegerField(TEXT("RequestRateLimit"));
	}
	if (JsonObject->HasField(TEXT("EnableAuthentication")))
	{
		bEnableAuthentication = JsonObject->GetBoolField(TEXT("EnableAuthentication"));
	}
	if (JsonObject->HasField(TEXT("APIKey")))
	{
		APIKey = JsonObject->GetStringField(TEXT("APIKey"));
	}
	if (JsonObject->HasField(TEXT("CurrentPreset")))
	{
		CurrentPreset = (EMCPServerPreset)JsonObject->GetIntegerField(TEXT("CurrentPreset"));
	}

	// Import custom headers
	if (JsonObject->HasField(TEXT("CustomHeaders")))
	{
		const TSharedPtr<FJsonObject>* HeadersObject;
		if (JsonObject->TryGetObjectField(TEXT("CustomHeaders"), HeadersObject) && HeadersObject->IsValid())
		{
			CustomHeaders.Empty();
			for (const auto& Pair : (*HeadersObject)->Values)
			{
				FString Value;
				if (Pair.Value->TryGetString(Value))
				{
					CustomHeaders.Add(Pair.Key, Value);
				}
			}
		}
	}

	// Import allowed origins
	if (JsonObject->HasField(TEXT("AllowedOrigins")))
	{
		const TArray<TSharedPtr<FJsonValue>>* OriginsArray;
		if (JsonObject->TryGetArrayField(TEXT("AllowedOrigins"), OriginsArray))
		{
			AllowedOrigins.Empty();
			for (const auto& JsonValue : *OriginsArray)
			{
				FString Origin;
				if (JsonValue->TryGetString(Origin))
				{
					AllowedOrigins.Add(Origin);
				}
			}
		}
	}

	// Validate imported settings
	if (!ValidateSettings(OutErrorMessage))
	{
		return false;
	}

	SaveConfig();
	BroadcastSettingsChanged();

	return true;
}

bool UMCPServerSettings::SaveToFile(const FString& FilePath, FString& OutErrorMessage) const
{
	FString JSONString = ExportToJSON();
	FString FullPath = FPaths::IsRelative(FilePath) ? FPaths::ProjectDir() / FilePath : FilePath;

	if (!FFileHelper::SaveStringToFile(JSONString, *FullPath))
	{
		OutErrorMessage = FString::Printf(TEXT("Failed to save settings to file: %s"), *FullPath);
		return false;
	}

	return true;
}

bool UMCPServerSettings::LoadFromFile(const FString& FilePath, FString& OutErrorMessage)
{
	FString FullPath = FPaths::IsRelative(FilePath) ? FPaths::ProjectDir() / FilePath : FilePath;
	FString JSONString;

	if (!FFileHelper::LoadFileToString(JSONString, *FullPath))
	{
		OutErrorMessage = FString::Printf(TEXT("Failed to load settings from file: %s"), *FullPath);
		return false;
	}

	return ImportFromJSON(JSONString, OutErrorMessage);
}

FString UMCPServerSettings::GetServerURL() const
{
	return FString::Printf(TEXT("http://localhost:%d"), ServerPort);
}

FString UMCPServerSettings::GetSettingsDisplayString() const
{
	FString LogLevelStr = StaticEnum<EMCPLogLevel>()->GetDisplayNameTextByValue((int64)LogLevel).ToString();
	FString PresetStr = StaticEnum<EMCPServerPreset>()->GetDisplayNameTextByValue((int64)CurrentPreset).ToString();

	return FString::Printf(TEXT("Port: %d, Max Connections: %d, Timeout: %ds, Log Level: %s, Preset: %s"),
		ServerPort, MaxClientConnections, ServerTimeoutSeconds, *LogLevelStr, *PresetStr);
}

bool UMCPServerSettings::RequiresServerRestart(const UMCPServerSettings* OtherSettings) const
{
	if (!OtherSettings)
	{
		return true;
	}

	return (ServerPort != OtherSettings->ServerPort) ||
		   (MaxClientConnections != OtherSettings->MaxClientConnections) ||
		   (ServerTimeoutSeconds != OtherSettings->ServerTimeoutSeconds) ||
		   (bEnableCORS != OtherSettings->bEnableCORS) ||
		   (bEnableAuthentication != OtherSettings->bEnableAuthentication) ||
		   (APIKey != OtherSettings->APIKey);
}

void UMCPServerSettings::BroadcastSettingsChanged()
{
	OnSettingsChanged.Broadcast(this);
}

bool UMCPServerSettings::ValidatePort(int32 Port, FString& OutError) const
{
	if (Port < 1024 || Port > 65535)
	{
		OutError = FString::Printf(TEXT("Port %d is out of valid range (1024-65535)"), Port);
		return false;
	}

	return true;
}

bool UMCPServerSettings::ValidateTimeout(int32 Timeout, FString& OutError) const
{
	if (Timeout < 5 || Timeout > 300)
	{
		OutError = FString::Printf(TEXT("Timeout %d seconds is out of valid range (5-300)"), Timeout);
		return false;
	}

	return true;
}

bool UMCPServerSettings::ValidateMaxConnections(int32 MaxConnections, FString& OutError) const
{
	if (MaxConnections < 1 || MaxConnections > 100)
	{
		OutError = FString::Printf(TEXT("Max connections %d is out of valid range (1-100)"), MaxConnections);
		return false;
	}

	return true;
}

bool UMCPServerSettings::ValidateLogFilePath(const FString& Path, FString& OutError) const
{
	if (Path.IsEmpty())
	{
		OutError = TEXT("Log file path cannot be empty when logging to file is enabled");
		return false;
	}

	FString FullPath = FPaths::IsRelative(Path) ? FPaths::ProjectDir() / Path : Path;
	FString Directory = FPaths::GetPath(FullPath);

	if (!FPaths::DirectoryExists(Directory))
	{
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		if (!PlatformFile.CreateDirectoryTree(*Directory))
		{
			OutError = FString::Printf(TEXT("Cannot create log directory: %s"), *Directory);
			return false;
		}
	}

	return true;
}

void UMCPServerSettings::GetPresetDefaults(EMCPServerPreset Preset, int32& OutPort, bool& OutAutoStart, 
	EMCPLogLevel& OutLogLevel, int32& OutMaxConnections, int32& OutTimeout, bool& OutEnableCORS) const
{
	switch (Preset)
	{
	case EMCPServerPreset::Development:
		OutPort = 8080;
		OutAutoStart = true;
		OutLogLevel = EMCPLogLevel::Detailed;
		OutMaxConnections = 5;
		OutTimeout = 60;
		OutEnableCORS = true;
		break;

	case EMCPServerPreset::Production:
		OutPort = 8080;
		OutAutoStart = false;
		OutLogLevel = EMCPLogLevel::Basic;
		OutMaxConnections = 20;
		OutTimeout = 30;
		OutEnableCORS = false;
		break;

	case EMCPServerPreset::Testing:
		OutPort = 9000;
		OutAutoStart = false;
		OutLogLevel = EMCPLogLevel::Detailed;
		OutMaxConnections = 3;
		OutTimeout = 10;
		OutEnableCORS = true;
		break;

	case EMCPServerPreset::Custom:
	default:
		// Keep current values for custom preset
		OutPort = ServerPort;
		OutAutoStart = bAutoStartServer;
		OutLogLevel = LogLevel;
		OutMaxConnections = MaxClientConnections;
		OutTimeout = ServerTimeoutSeconds;
		OutEnableCORS = bEnableCORS;
		break;
	}
}

#undef LOCTEXT_NAMESPACE