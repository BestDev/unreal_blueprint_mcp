# Unreal Blueprint MCP Plugin - Phase 3: Complete Settings System

## Overview

Phase 3 introduces a comprehensive settings system for the MCP Server plugin, providing full configuration management through Unreal Engine's Project Settings interface. This phase builds upon the basic port configuration from Phase 2 and adds advanced settings management, validation, and export/import capabilities.

## New Features in Phase 3

### 1. UMCPServerSettings Class

**Location**: `Source/UnrealBlueprintMCP/Public/MCPServerSettings.h` and `Private/MCPServerSettings.cpp`

A complete UObject-based settings class that integrates with Unreal Engine's Project Settings system:

- **Base Class**: `UDeveloperSettings` for seamless Project Settings integration
- **Configuration**: Stored in `EditorPerProjectUserSettings` for per-project configuration
- **Blueprint Support**: All settings are Blueprint-accessible with `BlueprintReadOnly`

### 2. Comprehensive Settings Categories

#### Server Configuration
- **Server Port** (1024-65535): Port number for the MCP server with validation
- **Auto Start Server**: Automatically start server when plugin loads
- **Max Client Connections** (1-100): Maximum simultaneous client connections
- **Server Timeout** (5-300 seconds): Connection timeout duration
- **Enable CORS**: Cross-Origin Resource Sharing support for web clients

#### Logging Configuration
- **Log Level**: None, Basic, or Detailed logging verbosity
- **Log to File**: Option to save logs to file
- **Log File Path**: Configurable log file location (when file logging enabled)

#### Advanced Configuration
- **Custom Headers**: Key-value pairs for additional HTTP headers
- **Allowed Origins**: Specific origins for CORS (when CORS enabled)
- **Request Rate Limit**: Maximum requests per second per client
- **Enable Authentication**: API key-based authentication
- **API Key**: Secret key for authentication (password field)

#### Preset Management
- **Current Preset**: Development, Production, Testing, or Custom configurations
- **Preset Auto-Application**: Automatic configuration when preset changes

### 3. Project Settings Integration

**Access Path**: `Edit > Project Settings > Plugins > MCP Server`

The settings appear in a dedicated section within Project Settings with:
- Organized categories for easy navigation
- Conditional property visibility (e.g., log path only shown when file logging enabled)
- Real-time validation with user feedback
- Tooltip documentation for all settings

### 4. Advanced Settings Management

#### Validation System
- **Real-time Validation**: Settings validated as they're changed
- **Error Notifications**: User-friendly error messages for invalid values
- **Port Availability**: Automatic checking of port availability
- **Suggested Ports**: Alternative port suggestions when current port unavailable

#### Configuration Management
- **Reset to Defaults**: One-click restoration of default values
- **Export to JSON**: Complete settings export for backup/sharing
- **Import from JSON**: Settings restoration from exported files
- **File Operations**: Save/load settings to/from files with error handling

#### Preset System
- **Development Preset**: Auto-start enabled, detailed logging, CORS enabled, 5 max connections
- **Production Preset**: Manual start, basic logging, no CORS, 20 max connections  
- **Testing Preset**: Port 9000, detailed logging, 3 max connections, short timeout
- **Custom Preset**: User-defined configuration

### 5. Enhanced Menu System

The plugin menu now includes:
- **🔧 Open Project Settings**: Direct link to MCP Server settings
- **🛠️ Advanced Settings**: Quick view of current configuration
- **📁 Export/Import Settings**: Settings backup and restore functionality
- **⚙️ Configure Port (Legacy)**: Backward compatibility with Phase 2

### 6. Event-Driven Architecture

#### Settings Change Events
- **OnSettingsChanged**: Broadcasted when any setting changes
- **OnApplyServerSettings**: Triggered when settings need server application
- **Real-time Notifications**: User feedback for all configuration changes

#### Server Integration
- **Automatic Settings Application**: Server uses current settings when starting
- **Runtime Configuration**: Some settings applied immediately (headers, CORS)
- **Restart Detection**: Automatic detection of changes requiring server restart

### 7. Server Settings Integration

The `FMCPJsonRpcServer` class now includes:
- **Settings Application**: `ApplySettings()` method for configuration updates
- **Settings Caching**: Local cache of applied settings for performance
- **Dynamic Headers**: CORS and custom headers applied based on settings
- **Security Enhancement**: Conditional security headers based on configuration

## Technical Implementation

### Class Hierarchy
```cpp
UMCPServerSettings : public UDeveloperSettings
├── Server Configuration Properties
├── Logging Configuration Properties  
├── Advanced Configuration Properties
├── Preset Management Properties
├── Validation Methods
├── Configuration Management Methods
└── Event Broadcasting
```

### Key Methods

#### Settings Management
- `ValidateSettings()`: Complete settings validation with error reporting
- `ResetToDefaults()`: Restore all settings to default values
- `ApplyPreset()`: Apply predefined configuration preset
- `GetSettingsDisplayString()`: Formatted settings summary

#### Import/Export
- `ExportToJSON()`: Serialize current settings to JSON string
- `ImportFromJSON()`: Deserialize settings from JSON with validation
- `SaveToFile()`: Export settings to file with error handling
- `LoadFromFile()`: Import settings from file with validation

#### Utility Functions
- `IsPortAvailable()`: Check if a port is available for use
- `GetSuggestedPorts()`: Get list of alternative available ports
- `GetServerURL()`: Generate formatted server URL
- `RequiresServerRestart()`: Compare settings to detect restart requirements

### Validation Rules

- **Port Range**: 1024-65535 (avoiding system reserved ports)
- **Timeout Range**: 5-300 seconds (reasonable connection timeouts)
- **Max Connections**: 1-100 (resource management)
- **Log File Path**: Directory existence validation and creation
- **API Key**: Required when authentication enabled

### Integration Points

1. **Module Initialization**: Settings delegates bound during startup
2. **Server Startup**: Current settings automatically applied
3. **Settings Changes**: Real-time validation and user feedback
4. **Menu Integration**: Direct Project Settings access
5. **Legacy Support**: Maintains compatibility with Phase 2 port configuration

## Usage Examples

### Accessing Settings in Code
```cpp
// Get read-only settings
const UMCPServerSettings* Settings = UMCPServerSettings::Get();
int32 Port = Settings->ServerPort;

// Get mutable settings (for programmatic changes)
UMCPServerSettings* MutableSettings = UMCPServerSettings::GetMutable();
MutableSettings->ServerPort = 8081;
MutableSettings->SaveConfig();
```

### Settings Validation
```cpp
const UMCPServerSettings* Settings = UMCPServerSettings::Get();
FString ErrorMessage;
if (!Settings->ValidateSettings(ErrorMessage))
{
    UE_LOG(LogTemp, Error, TEXT("Settings validation failed: %s"), *ErrorMessage);
}
```

### Export/Import Settings
```cpp
// Export current settings
const UMCPServerSettings* Settings = UMCPServerSettings::Get();
FString JSONExport = Settings->ExportToJSON();

// Import settings from JSON
UMCPServerSettings* MutableSettings = UMCPServerSettings::GetMutable();
FString ErrorMessage;
if (MutableSettings->ImportFromJSON(JSONExport, ErrorMessage))
{
    UE_LOG(LogTemp, Log, TEXT("Settings imported successfully"));
}
```

## Backward Compatibility

Phase 3 maintains full backward compatibility with Phase 2:
- Legacy port configuration still available through menu
- Existing INI settings automatically migrated
- Phase 2 server control functions unchanged
- API compatibility preserved

## Configuration Files

Settings are stored in:
- **Project Settings**: `Config/DefaultEditorPerProjectUserSettings.ini`
- **Section**: `[/Script/UnrealBlueprintMCP.MCPServerSettings]`
- **Export Format**: JSON for backup and sharing

## Best Practices

1. **Development**: Use Development preset with auto-start enabled
2. **Production**: Use Production preset with authentication enabled
3. **Testing**: Use Testing preset with isolated port and detailed logging
4. **Backup**: Regular export of settings for project sharing
5. **Validation**: Always validate settings after programmatic changes

## Future Enhancements

Phase 3 provides a solid foundation for future features:
- SSL/TLS support configuration
- Plugin-specific logging system
- Advanced authentication methods
- Performance monitoring settings
- Network interface selection
- Service discovery configuration

## Migration Guide

### From Phase 2 to Phase 3

1. **Automatic Migration**: Existing port settings automatically preserved
2. **Enhanced Configuration**: Access new settings through Project Settings
3. **Menu Updates**: New menu options available alongside legacy functions
4. **API Changes**: No breaking changes to existing API

### Settings Migration
```cpp
// Phase 2 legacy method still works
int32 SavedPort = GetSavedPort();

// Phase 3 preferred method
const UMCPServerSettings* Settings = UMCPServerSettings::Get();
int32 Port = Settings->ServerPort;
```

This comprehensive settings system provides complete control over the MCP Server configuration while maintaining ease of use and backward compatibility with previous phases.