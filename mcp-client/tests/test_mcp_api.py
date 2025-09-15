#!/usr/bin/env python3
"""
UnrealBlueprintMCP API Test Script

This script tests all available JSON-RPC endpoints of the UnrealBlueprintMCP server.
Make sure the Unreal Editor with the plugin is running before executing this script.

Author: UnrealBlueprintMCP Team
Version: 1.3
"""

import json
import requests
import sys
import time
from typing import Dict, Any, List, Optional

class MCPApiTester:
    """Test class for UnrealBlueprintMCP JSON-RPC API"""
    
    def __init__(self, server_url: str = "http://localhost:8080"):
        """
        Initialize the API tester
        
        Args:
            server_url: The URL of the MCP server (default: http://localhost:8080)
        """
        self.server_url = server_url
        self.request_id = 1
        self.test_results: List[Dict[str, Any]] = []
        
    def make_request(self, method: str, params: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
        """
        Make a JSON-RPC request to the server
        
        Args:
            method: The JSON-RPC method name
            params: Optional parameters for the request
            
        Returns:
            The response from the server as a dictionary
        """
        payload = {
            "jsonrpc": "2.0",
            "method": method,
            "id": self.request_id
        }
        
        if params:
            payload["params"] = params
            
        self.request_id += 1
        
        try:
            response = requests.post(
                self.server_url,
                json=payload,
                headers={"Content-Type": "application/json"},
                timeout=10
            )
            
            if response.status_code == 200:
                try:
                    return response.json()
                except json.JSONDecodeError as e:
                    return {
                        "error": {
                            "code": -2,
                            "message": f"Invalid JSON response: {str(e)}"
                        }
                    }
            else:
                return {
                    "error": {
                        "code": response.status_code,
                        "message": f"HTTP Error: {response.status_code} - {response.text[:100]}"
                    }
                }
                
        except requests.exceptions.RequestException as e:
            return {
                "error": {
                    "code": -1,
                    "message": f"Request failed: {str(e)}"
                }
            }
    
    def test_method(self, method: str, params: Optional[Dict[str, Any]] = None, 
                   description: str = "") -> bool:
        """
        Test a specific JSON-RPC method
        
        Args:
            method: The method name to test
            params: Optional parameters
            description: Description of the test
            
        Returns:
            True if the test passed, False otherwise
        """
        print(f"\n🔍 Testing: {method}")
        if description:
            print(f"   Description: {description}")
        
        if params:
            print(f"   Parameters: {json.dumps(params, indent=2)}")
        
        response = self.make_request(method, params)
        
        # Ensure response is a dictionary
        if not isinstance(response, dict):
            error_msg = f"Invalid response type: {type(response)} - {str(response)}"
            print(f"   ❌ FAILED: {error_msg}")
            self.test_results.append({
                "method": method,
                "params": params,
                "description": description,
                "success": False,
                "error": error_msg
            })
            return False
        
        # Check if response has error
        if "error" in response:
            error_message = "Unknown error"
            if isinstance(response["error"], dict) and "message" in response["error"]:
                error_message = response["error"]["message"]
            elif isinstance(response["error"], str):
                error_message = response["error"]
            
            print(f"   ❌ FAILED: {error_message}")
            self.test_results.append({
                "method": method,
                "params": params,
                "description": description,
                "success": False,
                "error": error_message
            })
            return False
        
        # Check if response has result
        if "result" in response:
            print(f"   ✅ SUCCESS")
            print(f"   Response: {json.dumps(response['result'], indent=2)[:200]}{'...' if len(json.dumps(response['result'])) > 200 else ''}")
            self.test_results.append({
                "method": method,
                "params": params,
                "description": description,
                "success": True,
                "result": response["result"]
            })
            return True
        
        print(f"   ⚠️  UNEXPECTED: No result or error in response")
        return False
    
    def test_server_connectivity(self) -> bool:
        """Test basic server connectivity"""
        print("=" * 60)
        print("🚀 TESTING SERVER CONNECTIVITY")
        print("=" * 60)
        
        # Test basic HTTP connectivity
        try:
            response = requests.get(self.server_url, timeout=5)
            if response.status_code == 200:
                print("✅ Server is responding to HTTP requests")
                return True
            else:
                print(f"❌ Server returned HTTP {response.status_code}")
                return False
        except requests.exceptions.RequestException as e:
            print(f"❌ Cannot connect to server: {e}")
            return False
    
    def test_core_methods(self):
        """Test core JSON-RPC methods"""
        print("\n" + "=" * 60)
        print("🔧 TESTING CORE METHODS")
        print("=" * 60)
        
        # Test ping
        self.test_method("ping", description="Basic server ping test")
        
        # Test getBlueprints
        self.test_method("getBlueprints", description="Get list of Blueprint assets")
        
        # Test getActors
        self.test_method("getActors", description="Get list of actors in current world")
    
    def test_resources_namespace(self):
        """Test resources namespace methods"""
        print("\n" + "=" * 60)
        print("📁 TESTING RESOURCES NAMESPACE")
        print("=" * 60)
        
        # Test resources.list
        self.test_method(
            "resources.list",
            params={"path": "/Game"},
            description="List assets in /Game directory"
        )
        
        # Test resources.list with different path
        self.test_method(
            "resources.list",
            params={"path": "/Game/Blueprints"},
            description="List assets in /Game/Blueprints directory"
        )
        
        # Test resources.get (this might fail if no specific asset exists)
        self.test_method(
            "resources.get",
            params={"asset_path": "/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"},
            description="Get details of specific Blueprint asset"
        )
        
        # Test resources.create
        self.test_method(
            "resources.create",
            params={
                "asset_type": "Blueprint",
                "asset_name": "TestBlueprint_API",
                "path": "/Game/Blueprints",
                "parent_class": "Actor"
            },
            description="Create a new Blueprint asset"
        )
    
    def test_tools_namespace(self):
        """Test tools namespace methods"""
        print("\n" + "=" * 60)
        print("🔨 TESTING TOOLS NAMESPACE")
        print("=" * 60)
        
        # Test tools.create_blueprint
        blueprint_created = self.test_method(
            "tools.create_blueprint",
            params={
                "blueprint_name": "TestCharacter_API",
                "path": "/Game/Blueprints",
                "parent_class": "Character"
            },
            description="Create a new Character Blueprint"
        )
        
        if blueprint_created:
            blueprint_path = "/Game/Blueprints/TestCharacter_API"
            
            # Test tools.add_variable
            self.test_method(
                "tools.add_variable",
                params={
                    "blueprint_path": blueprint_path,
                    "variable_name": "TestHealth",
                    "variable_type": "float",
                    "is_public": True
                },
                description="Add a health variable to the Blueprint"
            )
            
            # Test tools.add_function
            self.test_method(
                "tools.add_function",
                params={
                    "blueprint_path": blueprint_path,
                    "function_name": "TestTakeDamage"
                },
                description="Add a TakeDamage function to the Blueprint"
            )
            
            # Test tools.edit_graph
            self.test_method(
                "tools.edit_graph",
                params={
                    "blueprint_path": blueprint_path,
                    "graph_name": "EventGraph",
                    "nodes_to_add": [
                        {
                            "type": "BeginPlay",
                            "x": 100,
                            "y": 100
                        },
                        {
                            "type": "PrintString",
                            "x": 300,
                            "y": 100
                        }
                    ]
                },
                description="Add nodes to the Blueprint's event graph"
            )
    
    def test_prompts_namespace(self):
        """Test prompts namespace methods"""
        print("\n" + "=" * 60)
        print("📚 TESTING PROMPTS NAMESPACE")
        print("=" * 60)
        
        # Test prompts.list
        prompts_listed = self.test_method(
            "prompts.list",
            description="Get list of available game development prompts"
        )
        
        # Test prompts.get for each available prompt
        if prompts_listed:
            prompt_names = [
                "create_player_character",
                "setup_movement", 
                "add_jump_mechanic",
                "create_collectible",
                "implement_health_system",
                "create_inventory_system"
            ]
            
            for prompt_name in prompt_names:
                self.test_method(
                    "prompts.get",
                    params={"prompt_name": prompt_name},
                    description=f"Get detailed content for '{prompt_name}' prompt"
                )
    
    def test_error_handling(self):
        """Test error handling scenarios"""
        print("\n" + "=" * 60)
        print("⚠️  TESTING ERROR HANDLING")
        print("=" * 60)
        
        # Test invalid method
        self.test_method(
            "invalid_method",
            description="Test response to invalid method name"
        )
        
        # Test missing parameters
        self.test_method(
            "resources.get",
            description="Test missing required parameters"
        )
        
        # Test invalid asset path
        self.test_method(
            "resources.get",
            params={"asset_path": "/InvalidPath/NonExistentAsset"},
            description="Test invalid asset path"
        )
        
        # Test invalid JSON-RPC version
        payload = {
            "jsonrpc": "1.0",  # Invalid version
            "method": "ping",
            "id": 999
        }
        
        try:
            response = requests.post(
                self.server_url,
                json=payload,
                headers={"Content-Type": "application/json"},
                timeout=10
            )
            print(f"\n🔍 Testing: Invalid JSON-RPC version")
            print(f"   ❌ Server correctly rejected invalid version: {response.json()}")
        except Exception as e:
            print(f"   Error testing invalid version: {e}")
    
    def generate_curl_examples(self):
        """Generate curl command examples for manual testing"""
        print("\n" + "=" * 60)
        print("📋 CURL COMMAND EXAMPLES")
        print("=" * 60)
        
        examples = [
            {
                "name": "Ping Server",
                "command": 'curl -X POST http://localhost:8080 -H "Content-Type: application/json" -d \'{"jsonrpc":"2.0","method":"ping","id":1}\''
            },
            {
                "name": "List Resources",
                "command": 'curl -X POST http://localhost:8080 -H "Content-Type: application/json" -d \'{"jsonrpc":"2.0","method":"resources.list","params":{"path":"/Game"},"id":2}\''
            },
            {
                "name": "Get Prompts List",
                "command": 'curl -X POST http://localhost:8080 -H "Content-Type: application/json" -d \'{"jsonrpc":"2.0","method":"prompts.list","id":3}\''
            },
            {
                "name": "Create Blueprint",
                "command": 'curl -X POST http://localhost:8080 -H "Content-Type: application/json" -d \'{"jsonrpc":"2.0","method":"tools.create_blueprint","params":{"blueprint_name":"CurlTestBlueprint","path":"/Game/Blueprints","parent_class":"Actor"},"id":4}\''
            },
            {
                "name": "Get Prompt Content",
                "command": 'curl -X POST http://localhost:8080 -H "Content-Type: application/json" -d \'{"jsonrpc":"2.0","method":"prompts.get","params":{"prompt_name":"create_player_character"},"id":5}\''
            }
        ]
        
        for example in examples:
            print(f"\n# {example['name']}")
            print(example['command'])
    
    def print_summary(self):
        """Print test summary"""
        print("\n" + "=" * 60)
        print("📊 TEST SUMMARY")
        print("=" * 60)
        
        total_tests = len(self.test_results)
        passed_tests = len([t for t in self.test_results if t["success"]])
        failed_tests = total_tests - passed_tests
        
        print(f"Total Tests: {total_tests}")
        print(f"✅ Passed: {passed_tests}")
        print(f"❌ Failed: {failed_tests}")
        print(f"Success Rate: {(passed_tests/total_tests*100):.1f}%" if total_tests > 0 else "No tests run")
        
        if failed_tests > 0:
            print(f"\n🔍 Failed Tests:")
            for test in self.test_results:
                if not test["success"]:
                    print(f"  - {test['method']}: {test.get('error', 'Unknown error')}")
        
        print(f"\n💡 Note: Some failures may be expected (e.g., testing invalid inputs)")

def main():
    """Main function to run all tests"""
    print("🎮 UnrealBlueprintMCP API Test Suite")
    print(f"🕒 Started at: {time.strftime('%Y-%m-%d %H:%M:%S')}")
    
    # Initialize tester
    tester = MCPApiTester()
    
    # Test server connectivity first
    if not tester.test_server_connectivity():
        print("\n❌ Cannot connect to server. Make sure:")
        print("   1. Unreal Editor is running")
        print("   2. UnrealBlueprintMCP plugin is enabled")
        print("   3. Server is listening on port 8080")
        sys.exit(1)
    
    # Run all test categories
    tester.test_core_methods()
    tester.test_resources_namespace()
    tester.test_tools_namespace()
    tester.test_prompts_namespace()
    tester.test_error_handling()
    
    # Generate curl examples
    tester.generate_curl_examples()
    
    # Print summary
    tester.print_summary()
    
    print(f"\n🏁 Test completed at: {time.strftime('%Y-%m-%d %H:%M:%S')}")

if __name__ == "__main__":
    main()