#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Engine/DeveloperSettings.h"
#include "MCPServerSettings.generated.h"

/**
 * Log Level enumeration for MCP Server
 */
UENUM(BlueprintType)
enum class EMCPLogLevel : uint8
{
	None			UMETA(DisplayName = "None"),
	Basic			UMETA(DisplayName = "Basic"),
	Detailed		UMETA(DisplayName = "Detailed")
};

/**
 * Server Preset enumeration for common configurations
 */
UENUM(BlueprintType)
enum class EMCPServerPreset : uint8
{
	Development		UMETA(DisplayName = "Development"),
	Production		UMETA(DisplayName = "Production"),
	Testing			UMETA(DisplayName = "Testing"),
	Custom			UMETA(DisplayName = "Custom")
};

/**
 * MCP Server Settings class that appears in Project Settings
 * Accessible via Edit > Project Settings > Plugins > MCP Server
 */
UCLASS(config = EditorPerProjectUserSettings, defaultconfig, meta = (DisplayName = "MCP Server"))
class UNREALBLUEPRINTMCP_API UMCPServerSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UMCPServerSettings(const FObjectInitializer& ObjectInitializer);

	// UDeveloperSettings interface
	virtual FName GetCategoryName() const override;
	virtual FText GetSectionText() const override;
	virtual FText GetSectionDescription() const override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual bool CanEditChange(const FProperty* InProperty) const override;
#endif

	/** Get the singleton instance of settings */
	static const UMCPServerSettings* Get();
	
	/** Get the mutable singleton instance of settings */
	static UMCPServerSettings* GetMutable();

	// Server Configuration
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Server Configuration", 
		meta = (DisplayName = "Server Port", ToolTip = "Port number for the MCP server (1024-65535)", ClampMin = "1024", ClampMax = "65535"))
	int32 ServerPort = 8080;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Server Configuration",
		meta = (DisplayName = "Auto Start Server", ToolTip = "Automatically start the server when the plugin is loaded"))
	bool bAutoStartServer = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Server Configuration",
		meta = (DisplayName = "Max Client Connections", ToolTip = "Maximum number of simultaneous client connections", ClampMin = "1", ClampMax = "100"))
	int32 MaxClientConnections = 10;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Server Configuration",
		meta = (DisplayName = "Server Timeout (seconds)", ToolTip = "Connection timeout in seconds", ClampMin = "5", ClampMax = "300"))
	int32 ServerTimeoutSeconds = 30;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Server Configuration",
		meta = (DisplayName = "Enable CORS", ToolTip = "Enable Cross-Origin Resource Sharing for web clients"))
	bool bEnableCORS = false;

	// Logging Configuration
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Logging",
		meta = (DisplayName = "Log Level", ToolTip = "Verbosity level for server logging"))
	EMCPLogLevel LogLevel = EMCPLogLevel::Basic;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Logging",
		meta = (DisplayName = "Log to File", ToolTip = "Save server logs to file"))
	bool bLogToFile = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Logging",
		meta = (DisplayName = "Log File Path", ToolTip = "Path for log file (relative to project directory)", EditCondition = "bLogToFile"))
	FString LogFilePath = TEXT("Logs/MCPServer.log");

	// Advanced Configuration
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Advanced",
		meta = (DisplayName = "Custom Headers", ToolTip = "Additional HTTP headers to include in responses"))
	TMap<FString, FString> CustomHeaders;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Advanced",
		meta = (DisplayName = "Allowed Origins", ToolTip = "List of allowed origins for CORS (empty means all)", EditCondition = "bEnableCORS"))
	TArray<FString> AllowedOrigins;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Advanced",
		meta = (DisplayName = "Request Rate Limit", ToolTip = "Maximum requests per second per client (0 = unlimited)", ClampMin = "0", ClampMax = "1000"))
	int32 RequestRateLimit = 0;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Advanced",
		meta = (DisplayName = "Enable Authentication", ToolTip = "Require authentication for server access"))
	bool bEnableAuthentication = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Advanced",
		meta = (DisplayName = "API Key", ToolTip = "API key for authentication (leave empty to disable)", EditCondition = "bEnableAuthentication", PasswordField = true))
	FString APIKey;

	// Preset Management
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Presets",
		meta = (DisplayName = "Current Preset", ToolTip = "Current configuration preset"))
	EMCPServerPreset CurrentPreset = EMCPServerPreset::Development;

	/** Validation Functions */
	
	/** Check if current settings are valid */
	UFUNCTION(BlueprintCallable, Category = "MCP Server Settings")
	bool ValidateSettings(FString& OutErrorMessage) const;

	/** Check if a port is available for use */
	UFUNCTION(BlueprintCallable, Category = "MCP Server Settings")
	bool IsPortAvailable(int32 Port) const;

	/** Get suggested alternative ports if current port is unavailable */
	UFUNCTION(BlueprintCallable, Category = "MCP Server Settings")
	TArray<int32> GetSuggestedPorts() const;

	/** Configuration Management Functions */
	
	/** Reset all settings to default values */
	UFUNCTION(BlueprintCallable, Category = "MCP Server Settings", meta = (DisplayName = "Reset to Defaults"))
	void ResetToDefaults();

	/** Apply a specific preset configuration */
	UFUNCTION(BlueprintCallable, Category = "MCP Server Settings")
	void ApplyPreset(EMCPServerPreset Preset);

	/** Export current settings to JSON string */
	UFUNCTION(BlueprintCallable, Category = "MCP Server Settings")
	FString ExportToJSON() const;

	/** Import settings from JSON string */
	UFUNCTION(BlueprintCallable, Category = "MCP Server Settings")
	bool ImportFromJSON(const FString& JSONString, FString& OutErrorMessage);

	/** Save current settings to file */
	UFUNCTION(BlueprintCallable, Category = "MCP Server Settings")
	bool SaveToFile(const FString& FilePath, FString& OutErrorMessage) const;

	/** Load settings from file */
	UFUNCTION(BlueprintCallable, Category = "MCP Server Settings")
	bool LoadFromFile(const FString& FilePath, FString& OutErrorMessage);

	/** Event Delegates */
	
	/** Delegate fired when settings change */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnSettingsChanged, const UMCPServerSettings*);
	static FOnSettingsChanged OnSettingsChanged;

	/** Delegate fired when server configuration needs to be applied */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnApplyServerSettings, const UMCPServerSettings*);
	static FOnApplyServerSettings OnApplyServerSettings;

	/** Utility Functions */
	
	/** Get formatted server URL */
	UFUNCTION(BlueprintCallable, Category = "MCP Server Settings")
	FString GetServerURL() const;

	/** Get settings as formatted string for display */
	UFUNCTION(BlueprintCallable, Category = "MCP Server Settings")
	FString GetSettingsDisplayString() const;

	/** Check if settings require server restart */
	UFUNCTION(BlueprintCallable, Category = "MCP Server Settings")
	bool RequiresServerRestart(const UMCPServerSettings* OtherSettings) const;

private:
	/** Internal function to broadcast settings changes */
	void BroadcastSettingsChanged();

	/** Internal function to validate individual settings */
	bool ValidatePort(int32 Port, FString& OutError) const;
	bool ValidateTimeout(int32 Timeout, FString& OutError) const;
	bool ValidateMaxConnections(int32 MaxConnections, FString& OutError) const;
	bool ValidateLogFilePath(const FString& Path, FString& OutError) const;

	/** Internal function to get default values for presets */
	void GetPresetDefaults(EMCPServerPreset Preset, int32& OutPort, bool& OutAutoStart, EMCPLogLevel& OutLogLevel, 
		int32& OutMaxConnections, int32& OutTimeout, bool& OutEnableCORS) const;
};