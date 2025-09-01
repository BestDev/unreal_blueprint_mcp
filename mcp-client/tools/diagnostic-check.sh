#!/bin/bash
# Unreal Blueprint MCP - Diagnostic Check Script
# Comprehensive system check for troubleshooting

echo "🔍 Unreal Blueprint MCP Diagnostic Check"
echo "========================================"
echo ""

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Helper functions
print_success() {
    echo -e "${GREEN}✅ $1${NC}"
}

print_error() {
    echo -e "${RED}❌ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠️  $1${NC}"
}

print_info() {
    echo -e "${BLUE}ℹ️  $1${NC}"
}

# Check 1: Unreal Engine Server Connection
echo "1. Testing Unreal Engine Connection"
echo "-----------------------------------"
if curl -s --connect-timeout 5 http://localhost:8080 > /dev/null 2>&1; then
    print_success "Unreal Engine server responding on port 8080"
    
    # Test ping endpoint
    response=$(curl -s -X POST http://localhost:8080 \
        -H "Content-Type: application/json" \
        -d '{"jsonrpc":"2.0","method":"ping","id":1}' 2>/dev/null)
    
    if echo "$response" | grep -q "pong"; then
        print_success "JSON-RPC ping successful"
        version=$(echo "$response" | grep -o '"version":"[^"]*"' | cut -d'"' -f4)
        print_info "Server version: $version"
    else
        print_error "JSON-RPC ping failed"
        print_info "Response: $response"
    fi
else
    print_error "Unreal Engine server not responding on port 8080"
    print_info "Make sure Unreal Engine is running with UnrealBlueprintMCP plugin enabled"
    
    # Check if port is occupied by something else
    if netstat -tuln 2>/dev/null | grep -q ":8080 "; then
        print_warning "Port 8080 is occupied by another process"
        print_info "Process using port 8080:"
        netstat -tulpn 2>/dev/null | grep ":8080 " || print_info "Unable to determine process"
    else
        print_info "Port 8080 is available"
    fi
fi
echo ""

# Check 2: Python Environment
echo "2. Testing Python Environment"
echo "-----------------------------"

# Check Python version
python_version=$(python --version 2>&1)
if [[ $? -eq 0 ]]; then
    print_success "Python available: $python_version"
    
    # Check required minimum version (3.7+)
    version_check=$(python -c "import sys; print('ok' if sys.version_info >= (3, 7) else 'old')" 2>/dev/null)
    if [[ "$version_check" == "ok" ]]; then
        print_success "Python version is compatible (3.7+)"
    else
        print_error "Python version too old. Minimum required: 3.7"
    fi
else
    print_error "Python not found in PATH"
    print_info "Please install Python 3.7+ and ensure it's in your PATH"
fi

# Check required packages
echo ""
print_info "Checking required Python packages..."

declare -a packages=("mcp" "httpx" "requests" "asyncio")
all_packages_ok=true

for package in "${packages[@]}"; do
    if python -c "import $package" 2>/dev/null; then
        print_success "$package package available"
    else
        print_error "$package package missing"
        all_packages_ok=false
    fi
done

if [[ "$all_packages_ok" == false ]]; then
    print_info "Install missing packages with: pip install -r requirements.txt"
fi
echo ""

# Check 3: MCP Client
echo "3. Testing MCP Client"
echo "--------------------"

if [[ -f "mcp_client.py" ]]; then
    print_success "mcp_client.py found"
    
    if [[ -r "mcp_client.py" ]]; then
        print_success "mcp_client.py is readable"
        
        # Test MCP client syntax
        if python -m py_compile mcp_client.py 2>/dev/null; then
            print_success "mcp_client.py syntax is valid"
        else
            print_error "mcp_client.py has syntax errors"
            print_info "Run: python -m py_compile mcp_client.py for details"
        fi
        
        # Test MCP client execution (with timeout)
        print_info "Testing MCP client execution..."
        timeout 5 python mcp_client.py > /dev/null 2>&1
        exit_code=$?
        
        if [[ $exit_code -eq 124 ]]; then
            print_success "MCP client started successfully (timed out as expected)"
        elif [[ $exit_code -eq 0 ]]; then
            print_success "MCP client executed successfully"
        else
            print_error "MCP client failed to start (exit code: $exit_code)"
        fi
    else
        print_error "mcp_client.py is not readable"
    fi
else
    print_error "mcp_client.py not found"
    print_info "Ensure you're running this script from the project directory"
fi
echo ""

# Check 4: Configuration Files
echo "4. Checking Configuration Files"
echo "------------------------------"

if [[ -f "config.json" ]]; then
    print_success "config.json found"
    
    if [[ -r "config.json" ]]; then
        print_success "config.json is readable"
        
        # Validate JSON syntax
        if python -m json.tool config.json > /dev/null 2>&1; then
            print_success "config.json has valid JSON syntax"
            
            # Check required fields
            if grep -q "mcpServers" config.json && grep -q "unreal-blueprint-mcp" config.json; then
                print_success "config.json has required MCP server configuration"
                
                # Extract and display server URL
                server_url=$(python -c "import json; data=json.load(open('config.json')); print(data['mcpServers']['unreal-blueprint-mcp']['env']['UNREAL_SERVER_URL'])" 2>/dev/null)
                if [[ -n "$server_url" ]]; then
                    print_info "Configured server URL: $server_url"
                fi
            else
                print_error "config.json missing required MCP configuration"
            fi
        else
            print_error "config.json has invalid JSON syntax"
            print_info "Run: python -m json.tool config.json for details"
        fi
    else
        print_error "config.json is not readable"
    fi
else
    print_error "config.json not found"
fi

if [[ -f "requirements.txt" ]]; then
    print_success "requirements.txt found"
else
    print_warning "requirements.txt not found"
fi
echo ""

# Check 5: Network Connectivity
echo "5. Testing Network Connectivity"
echo "------------------------------"

# Test localhost connectivity
if ping -c 1 localhost > /dev/null 2>&1; then
    print_success "Localhost connectivity OK"
else
    print_error "Localhost connectivity failed"
fi

# Test HTTP connectivity to common ports
declare -a test_ports=(80 443 8080 8081)
for port in "${test_ports[@]}"; do
    if nc -z localhost $port 2>/dev/null; then
        if [[ $port -eq 8080 ]]; then
            print_success "Port $port is open (expected for Unreal server)"
        else
            print_info "Port $port is open"
        fi
    else
        if [[ $port -eq 8080 ]]; then
            print_warning "Port $port is closed (Unreal server should be here)"
        fi
    fi
done
echo ""

# Check 6: File Structure
echo "6. Checking Project File Structure"
echo "---------------------------------"

declare -a expected_files=(
    "mcp_client.py"
    "config.json"
    "requirements.txt"
    "README.md"
    "INTEGRATION_GUIDE.md"
    "test_mcp_api.py"
    "UnrealBlueprintMCP.uplugin"
)

for file in "${expected_files[@]}"; do
    if [[ -f "$file" ]]; then
        print_success "$file exists"
    else
        print_warning "$file missing"
    fi
done

# Check Source directory
if [[ -d "Source" ]]; then
    print_success "Source directory exists"
    
    # Check important source files
    declare -a source_files=(
        "Source/UnrealBlueprintMCP/UnrealBlueprintMCP.Build.cs"
        "Source/UnrealBlueprintMCP/Public/UnrealBlueprintMCP.h"
        "Source/UnrealBlueprintMCP/Private/UnrealBlueprintMCP.cpp"
        "Source/UnrealBlueprintMCP/Public/MCPJsonRpcServer.h"
        "Source/UnrealBlueprintMCP/Private/MCPJsonRpcServer.cpp"
    )
    
    for file in "${source_files[@]}"; do
        if [[ -f "$file" ]]; then
            print_success "$(basename $file) exists"
        else
            print_error "$(basename $file) missing"
        fi
    done
else
    print_error "Source directory missing"
fi
echo ""

# Check 7: System Information
echo "7. System Information"
echo "--------------------"
print_info "Operating System: $(uname -s)"
print_info "Architecture: $(uname -m)"
print_info "Current Directory: $(pwd)"
print_info "User: $(whoami)"
print_info "Date: $(date)"

# Check available tools
echo ""
print_info "Available tools:"
command -v curl >/dev/null 2>&1 && print_success "curl available" || print_warning "curl not available"
command -v netstat >/dev/null 2>&1 && print_success "netstat available" || print_warning "netstat not available" 
command -v nc >/dev/null 2>&1 && print_success "nc (netcat) available" || print_warning "nc (netcat) not available"
command -v python >/dev/null 2>&1 && print_success "python available" || print_error "python not available"
command -v pip >/dev/null 2>&1 && print_success "pip available" || print_warning "pip not available"
echo ""

# Summary
echo "========================================"
echo "🏁 Diagnostic Summary"
echo "========================================"

# Determine overall status
if curl -s --connect-timeout 5 http://localhost:8080 > /dev/null 2>&1 && \
   python -c "import mcp, httpx, requests" 2>/dev/null && \
   [[ -f "mcp_client.py" ]] && [[ -f "config.json" ]]; then
    print_success "System appears to be properly configured!"
    print_info "You should be able to use Unreal Blueprint MCP with Claude Code and Gemini CLI"
else
    print_error "System configuration issues detected"
    print_info "Please review the errors above and follow the troubleshooting guide"
fi

echo ""
print_info "For more help, see INTEGRATION_GUIDE.md"
print_info "To test the API directly, run: python test_mcp_api.py"
echo ""