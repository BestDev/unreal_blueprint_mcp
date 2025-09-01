#!/usr/bin/env python3
"""
Unreal Engine Helper Script for Gemini CLI Integration

This script provides a simple interface for Gemini CLI to interact with
Unreal Engine through the Blueprint MCP JSON-RPC API.

Usage:
    python unreal-helper.py <method> [params_json]
    python unreal-helper.py ping
    python unreal-helper.py getBlueprints
    python unreal-helper.py tools.create_blueprint '{"blueprint_name":"TestActor","path":"/Game/Test","parent_class":"Actor"}'

Examples:
    # Test connection
    python unreal-helper.py ping
    
    # List all Blueprints
    python unreal-helper.py getBlueprints
    
    # Create a new Blueprint
    python unreal-helper.py tools.create_blueprint '{"blueprint_name":"MyActor","path":"/Game/Blueprints","parent_class":"Actor"}'
    
    # Add a variable to a Blueprint
    python unreal-helper.py tools.add_variable '{"blueprint_path":"/Game/Blueprints/MyActor","variable_name":"Health","variable_type":"float","default_value":"100.0","is_public":true}'
    
    # List project assets
    python unreal-helper.py resources.list '{"path":"/Game/Blueprints"}'
"""

import requests
import json
import sys
import argparse
import time
from typing import Dict, Any, Optional

# Configuration
DEFAULT_UNREAL_SERVER_URL = "http://localhost:8080"
REQUEST_TIMEOUT = 30.0

class UnrealEngineAPI:
    """Simple wrapper for Unreal Engine JSON-RPC API"""
    
    def __init__(self, server_url: str = DEFAULT_UNREAL_SERVER_URL):
        self.server_url = server_url
        self.session = requests.Session()
        self.session.headers.update({"Content-Type": "application/json"})
        
    def call_method(self, method: str, params: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
        """Call a JSON-RPC method on the Unreal Engine server"""
        payload = {
            "jsonrpc": "2.0",
            "method": method,
            "id": int(time.time() * 1000)  # Use timestamp as ID
        }
        
        if params:
            payload["params"] = params
            
        try:
            response = self.session.post(
                self.server_url,
                json=payload,
                timeout=REQUEST_TIMEOUT
            )
            
            if response.status_code == 200:
                return response.json()
            else:
                return {
                    "error": {
                        "code": response.status_code,
                        "message": f"HTTP Error: {response.status_code}",
                        "data": response.text
                    }
                }
                
        except requests.exceptions.ConnectionError:
            return {
                "error": {
                    "code": -32001,
                    "message": "Connection failed",
                    "data": f"Unable to connect to Unreal Engine server at {self.server_url}"
                }
            }
        except requests.exceptions.Timeout:
            return {
                "error": {
                    "code": -32002,
                    "message": "Request timeout",
                    "data": f"Request timed out after {REQUEST_TIMEOUT} seconds"
                }
            }
        except Exception as e:
            return {
                "error": {
                    "code": -32003,
                    "message": "Unexpected error",
                    "data": str(e)
                }
            }

def format_output(result: Dict[str, Any], method: str) -> str:
    """Format the output in a user-friendly way"""
    if "error" in result:
        error = result["error"]
        return f"❌ Error: {error['message']}\n   Details: {error.get('data', 'No additional details')}"
    
    if "result" not in result:
        return f"⚠️  Unexpected response format: {json.dumps(result, indent=2)}"
    
    data = result["result"]
    
    # Format specific method responses
    if method == "ping":
        return f"✅ Server responding: {data.get('status', 'unknown')} (version {data.get('version', 'unknown')})"
    
    elif method == "getBlueprints":
        blueprints = data.get("blueprints", [])
        if not blueprints:
            return "📋 No Blueprints found in the project"
        
        output = f"📋 Found {len(blueprints)} Blueprint(s):\n"
        for bp in blueprints:
            output += f"   • {bp.get('name', 'Unknown')} ({bp.get('path', 'Unknown path')})\n"
        return output.rstrip()
    
    elif method == "getActors":
        actors = data.get("actors", [])
        if not actors:
            return "🎭 No actors found in the current world"
        
        output = f"🎭 Found {len(actors)} actor(s) in the world:\n"
        for actor in actors:
            output += f"   • {actor.get('name', 'Unknown')} ({actor.get('class', 'Unknown class')})\n"
        return output.rstrip()
    
    elif method.startswith("resources."):
        if method == "resources.list":
            resources = data.get("resources", [])
            if not resources:
                return "📁 No resources found in the specified path"
            
            output = f"📁 Found {len(resources)} resource(s):\n"
            for resource in resources:
                output += f"   • {resource.get('name', 'Unknown')} ({resource.get('type', 'Unknown type')})\n"
            return output.rstrip()
        
        elif method == "resources.get":
            return f"📄 Resource details:\n{json.dumps(data, indent=2)}"
        
        elif method == "resources.create":
            if data.get("success"):
                return f"✅ Resource created successfully: {data.get('asset_path', 'Unknown path')}"
            else:
                return f"❌ Failed to create resource: {data.get('message', 'Unknown error')}"
    
    elif method.startswith("tools."):
        if data.get("success"):
            return f"✅ Tool operation successful: {data.get('message', 'Operation completed')}"
        else:
            return f"❌ Tool operation failed: {data.get('message', 'Unknown error')}"
    
    elif method.startswith("prompts."):
        if method == "prompts.list":
            prompts = data.get("prompts", [])
            if not prompts:
                return "📚 No prompts available"
            
            output = f"📚 Available prompts ({len(prompts)}):\n"
            for prompt in prompts:
                output += f"   • {prompt.get('name', 'Unknown')} - {prompt.get('description', 'No description')}\n"
            return output.rstrip()
        
        elif method == "prompts.get":
            content = data.get("content", "No content available")
            return f"📚 Prompt content:\n{content}"
    
    # Default formatting for unknown methods
    return json.dumps(data, indent=2)

def print_available_methods():
    """Print available methods and examples"""
    print("""
Available Methods:
==================

Core Methods:
  ping                    - Test server connectivity
  getBlueprints          - List all Blueprint assets
  getActors              - List actors in current world

Resources Namespace:
  resources.list         - List project assets
  resources.get          - Get asset details
  resources.create       - Create new assets

Tools Namespace:
  tools.create_blueprint - Create new Blueprint
  tools.add_variable     - Add variable to Blueprint
  tools.add_function     - Add function to Blueprint
  tools.edit_graph       - Edit Blueprint graph

Prompts Namespace:
  prompts.list          - List available prompts
  prompts.get           - Get prompt content

Examples:
=========

# Test connection
python unreal-helper.py ping

# List Blueprints
python unreal-helper.py getBlueprints

# Create Blueprint
python unreal-helper.py tools.create_blueprint '{"blueprint_name":"TestActor","path":"/Game/Test","parent_class":"Actor"}'

# Add variable
python unreal-helper.py tools.add_variable '{"blueprint_path":"/Game/Test/TestActor","variable_name":"Health","variable_type":"float","is_public":true}'

# List assets in directory
python unreal-helper.py resources.list '{"path":"/Game/Blueprints"}'

# Get available prompts
python unreal-helper.py prompts.list
""")

def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(
        description="Unreal Engine Helper for Gemini CLI Integration",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s ping
  %(prog)s getBlueprints
  %(prog)s tools.create_blueprint '{"blueprint_name":"TestActor","path":"/Game/Test","parent_class":"Actor"}'
        """
    )
    
    parser.add_argument("method", nargs="?", help="JSON-RPC method to call")
    parser.add_argument("params", nargs="?", help="JSON parameters for the method")
    parser.add_argument("--server", default=DEFAULT_UNREAL_SERVER_URL, help="Unreal Engine server URL")
    parser.add_argument("--timeout", type=float, default=REQUEST_TIMEOUT, help="Request timeout in seconds")
    parser.add_argument("--list-methods", action="store_true", help="List available methods and exit")
    parser.add_argument("--raw", action="store_true", help="Output raw JSON response")
    
    args = parser.parse_args()
    
    if args.list_methods:
        print_available_methods()
        return
    
    if not args.method:
        print("❌ Error: Method name is required")
        print("\nUse --list-methods to see available methods")
        print("Example: python unreal-helper.py ping")
        sys.exit(1)
    
    # Parse parameters if provided
    params = None
    if args.params:
        try:
            params = json.loads(args.params)
        except json.JSONDecodeError as e:
            print(f"❌ Error: Invalid JSON parameters: {e}")
            print(f"   Parameters: {args.params}")
            sys.exit(1)
    
    # Initialize API client
    api = UnrealEngineAPI(args.server)
    
    # Update timeout if specified
    global REQUEST_TIMEOUT
    REQUEST_TIMEOUT = args.timeout
    
    # Call the method
    try:
        result = api.call_method(args.method, params)
        
        if args.raw:
            print(json.dumps(result, indent=2))
        else:
            output = format_output(result, args.method)
            print(output)
            
    except KeyboardInterrupt:
        print("\n❌ Operation cancelled by user")
        sys.exit(1)
    except Exception as e:
        print(f"❌ Unexpected error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()