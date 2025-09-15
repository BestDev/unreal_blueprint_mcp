# Unreal Blueprint MCP - Phase 2: Advanced Server Control

## Overview

Phase 2 introduces advanced server control features with improved user experience, error handling, and configuration management. This builds upon the basic menu structure from Phase 1 with comprehensive server management capabilities.

## New Features

### 1. 🚀 Enhanced Server Control
- **Smart Port Management**: Automatically tries fallback ports if the preferred port is unavailable
- **Restart Server**: Gracefully restart the server without manual stop/start
- **Fallback Port System**: Supports ports 8080, 8081, 8082, 8083, 8084, 8090, 9000, 9001

### 2. 📊 Advanced Server Information
- **Real-time Status**: Shows current port, connected clients, uptime
- **Detailed Information**: Comprehensive server info with available endpoints
- **Server Start Time**: Tracks when the server was started
- **Connected Client Count**: Real-time monitoring of active connections

### 3. 📋 URL Management
- **Copy Server URL**: One-click copy of server URL to clipboard
- **Dynamic URL Generation**: Automatically generates correct localhost URL with current port

### 4. ⚙️ Configuration Management
- **Port Persistence**: Remembers last used port across editor sessions
- **Configuration Storage**: Uses Unreal Engine's config system for settings
- **Port Configuration Dialog**: Interface for viewing current port settings

### 5. 🛡️ Enhanced Error Handling
- **Port Availability Check**: Validates ports before attempting to bind
- **Detailed Error Messages**: Clear feedback when operations fail
- **Graceful Fallbacks**: Automatic retry with alternative ports
- **Connection Monitoring**: Track client connections and disconnections

### 6. 🎨 Improved Menu Interface
- **Organized Sections**: 
  - Server Control (Start, Stop, Restart)
  - Server Information (Status, Details, Copy URL)
  - Configuration (Port Settings)
- **Visual Icons**: Emoji icons for easy menu navigation
- **Enhanced Tooltips**: Detailed descriptions for each menu option

## Menu Structure

```
MCP Server
├── Server Control
│   ├── 🚀 Start Server
│   ├── ⏹️ Stop Server
│   └── 🔄 Restart Server
├── Server Information  
│   ├── 📊 Server Status
│   ├── ℹ️ Server Information
│   └── 📋 Copy Server URL
└── Configuration
    └── ⚙️ Configure Port
```

## Technical Implementation

### Server Enhancements
- **FMCPJsonRpcServer** class improvements:
  - `StartServerWithFallback()`: Intelligent port selection
  - `IsPortAvailable()`: Port availability checking
  - `RestartServer()`: Graceful server restart
  - `GetServerStartTime()`: Server uptime tracking
  - `GetConnectedClientCount()`: Client connection monitoring
  - `GetServerURL()`: Dynamic URL generation

### Module Enhancements
- **FUnrealBlueprintMCPModule** class improvements:
  - Configuration persistence with `GetSavedPort()` and `SavePort()`
  - Clipboard integration with `CopyToClipboard()`
  - Detailed information display with `ShowServerInformation()`
  - Enhanced notifications with server details

### Error Handling
- **Port Conflict Resolution**: Automatically tries fallback ports
- **Connection Failure Recovery**: Clear error messages and retry suggestions
- **Thread-Safe Operations**: Safe client connection counting
- **Resource Cleanup**: Proper cleanup on server stop/restart

## Configuration

The plugin stores configuration in the editor's INI file:
```ini
[UnrealBlueprintMCP]
ServerPort=8080
```

## Usage Examples

### Starting the Server
1. Navigate to **Main Menu > MCP Server > Server Control > 🚀 Start Server**
2. If port 8080 is unavailable, the system automatically tries fallback ports
3. Success notification shows the actual port used
4. Port preference is saved for future sessions

### Checking Server Status
1. **Main Menu > MCP Server > Server Information > 📊 Server Status**
   - Shows: Port, Connected Clients, Uptime
2. **Main Menu > MCP Server > Server Information > ℹ️ Server Information**  
   - Shows: Detailed info including available endpoints

### Copying Server URL
1. **Main Menu > MCP Server > Server Information > 📋 Copy Server URL**
2. URL is copied to clipboard (e.g., `http://localhost:8080`)
3. Paste directly into MCP client applications

### Restarting the Server
1. **Main Menu > MCP Server > Server Control > 🔄 Restart Server**
2. Server gracefully stops and restarts on the same port
3. Useful for applying configuration changes

## Notifications

All operations provide user feedback via Unreal Engine's notification system:
- ✅ Success notifications (green)
- ⚠️ Warning notifications (yellow) 
- ❌ Error notifications (red)
- ℹ️ Information notifications (blue)

## Thread Safety

- **Client Connection Counting**: Thread-safe counter for accurate client tracking
- **Server State Management**: Atomic operations for server start/stop states
- **Configuration Access**: Thread-safe config file operations

## Backwards Compatibility

Phase 2 maintains full backwards compatibility with Phase 1:
- All existing functionality preserved
- Original menu structure enhanced, not replaced
- Existing server endpoints unchanged
- Configuration migration handled automatically

## Performance Considerations

- **Minimal Overhead**: Client counting adds negligible performance impact
- **Efficient Port Checking**: Quick socket binding tests for port availability
- **Lazy Loading**: Configuration only loaded when needed
- **Memory Management**: Proper cleanup prevents memory leaks

## Future Enhancements (Phase 3+)

Potential areas for future development:
- **Custom Port Range Configuration**: User-defined fallback port ranges
- **Server Metrics Dashboard**: Detailed analytics and monitoring
- **Multiple Server Instances**: Support for running multiple servers
- **SSL/TLS Support**: Secure connections for network deployment
- **Performance Profiling**: Built-in server performance monitoring
- **Plugin Settings UI**: Dedicated settings panel in Project Settings

## Troubleshooting

### Common Issues

**Server Won't Start**
- Check that no other applications are using the target ports
- Verify Windows Firewall isn't blocking the application
- Try manually specifying a different port in configuration

**Can't Copy URL**
- Ensure server is running before attempting to copy URL
- Check that clipboard access is available in the editor

**Configuration Not Saving**
- Verify editor has write permissions to config directory
- Check that config file isn't read-only

### Debug Information

All server operations log detailed information to the Unreal Engine log:
- Server start/stop events with timestamps
- Port binding attempts and results
- Client connection/disconnection events
- Configuration load/save operations

Access logs via **Window > Output Log** or **Window > Developer Tools > Output Log**.

## Conclusion

Phase 2 transforms the basic server control menu into a comprehensive server management solution. With intelligent port management, real-time monitoring, and enhanced user experience, developers can efficiently manage their MCP server integration within Unreal Engine.

The robust error handling and fallback mechanisms ensure reliable operation even in complex development environments where port conflicts are common.