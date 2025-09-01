#!/usr/bin/env python3
"""
Unreal Blueprint MCP Client
MCP Server that bridges Claude Code with Unreal Engine Blueprint plugin via JSON-RPC
"""

import asyncio
import json
import logging
import sys
from typing import Any, Dict, List, Optional, Sequence
from urllib.parse import urlparse

import httpx
from mcp.server import Server
from mcp.server.models import InitializationOptions
from mcp.server.stdio import stdio_server
from mcp.types import (
    Resource,
    Tool,
    Prompt,
    TextContent,
    ImageContent,
    EmbeddedResource,
    LoggingLevel
)

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("unreal-blueprint-mcp")

# Default Unreal plugin server configuration
DEFAULT_UNREAL_SERVER_URL = "http://localhost:8080"
UNREAL_SERVER_URL = DEFAULT_UNREAL_SERVER_URL

class UnrealBlueprintMCPServer:
    """MCP Server for Unreal Blueprint integration"""
    
    def __init__(self):
        self.server = Server("unreal-blueprint-mcp")
        self.http_client = httpx.AsyncClient(timeout=30.0)
        self._setup_handlers()
    
    def _setup_handlers(self):
        """Setup all MCP handlers"""
        self._setup_resource_handlers()
        self._setup_tool_handlers()
        self._setup_prompt_handlers()
    
    def _setup_resource_handlers(self):
        """Setup resource handlers"""
        
        @self.server.list_resources()
        async def list_resources() -> List[Resource]:
            """List available Unreal Engine resources"""
            try:
                response = await self.http_client.get(f"{UNREAL_SERVER_URL}/api/resources")
                if response.status_code == 200:
                    data = response.json()
                    resources = []
                    for item in data.get("resources", []):
                        resources.append(Resource(
                            uri=f"unreal://{item['type']}/{item['name']}",
                            name=item["name"],
                            description=item.get("description", f"Unreal {item['type']} resource"),
                            mimeType="application/json"
                        ))
                    return resources
                else:
                    logger.warning(f"Failed to fetch resources: {response.status_code}")
                    return []
            except Exception as e:
                logger.error(f"Error listing resources: {e}")
                return []
        
        @self.server.read_resource()
        async def read_resource(uri: str) -> str:
            """Read a specific Unreal Engine resource"""
            try:
                # Parse URI: unreal://type/name
                parsed = urlparse(uri)
                if parsed.scheme != "unreal":
                    raise ValueError(f"Invalid URI scheme: {parsed.scheme}")
                
                path_parts = parsed.path.strip("/").split("/")
                if len(path_parts) != 2:
                    raise ValueError(f"Invalid URI path: {parsed.path}")
                
                resource_type, resource_name = path_parts
                
                response = await self.http_client.get(
                    f"{UNREAL_SERVER_URL}/api/resources/{resource_type}/{resource_name}"
                )
                
                if response.status_code == 200:
                    return response.text
                else:
                    raise Exception(f"Failed to read resource: {response.status_code}")
                    
            except Exception as e:
                logger.error(f"Error reading resource {uri}: {e}")
                raise
    
    def _setup_tool_handlers(self):
        """Setup tool handlers"""
        
        @self.server.list_tools()
        async def list_tools() -> List[Tool]:
            """List available Unreal Blueprint tools"""
            return [
                Tool(
                    name="create_blueprint",
                    description="Create a new Blueprint class in Unreal Engine",
                    inputSchema={
                        "type": "object",
                        "properties": {
                            "name": {
                                "type": "string",
                                "description": "Blueprint class name"
                            },
                            "parent_class": {
                                "type": "string", 
                                "description": "Parent class (e.g., Actor, Pawn, Character)",
                                "default": "Actor"
                            },
                            "path": {
                                "type": "string",
                                "description": "Content path for the Blueprint",
                                "default": "/Game/Blueprints/"
                            }
                        },
                        "required": ["name"]
                    }
                ),
                Tool(
                    name="add_variable",
                    description="Add a variable to a Blueprint",
                    inputSchema={
                        "type": "object",
                        "properties": {
                            "blueprint_path": {
                                "type": "string",
                                "description": "Path to the Blueprint asset"
                            },
                            "variable_name": {
                                "type": "string",
                                "description": "Name of the variable"
                            },
                            "variable_type": {
                                "type": "string",
                                "description": "Type of the variable (e.g., int, float, string, bool)"
                            },
                            "default_value": {
                                "type": "string",
                                "description": "Default value for the variable",
                                "default": ""
                            },
                            "is_public": {
                                "type": "boolean",
                                "description": "Whether the variable is public",
                                "default": False
                            }
                        },
                        "required": ["blueprint_path", "variable_name", "variable_type"]
                    }
                ),
                Tool(
                    name="add_function",
                    description="Add a function to a Blueprint",
                    inputSchema={
                        "type": "object",
                        "properties": {
                            "blueprint_path": {
                                "type": "string",
                                "description": "Path to the Blueprint asset"
                            },
                            "function_name": {
                                "type": "string",
                                "description": "Name of the function"
                            },
                            "return_type": {
                                "type": "string",
                                "description": "Return type (optional)",
                                "default": "void"
                            },
                            "parameters": {
                                "type": "array",
                                "items": {
                                    "type": "object",
                                    "properties": {
                                        "name": {"type": "string"},
                                        "type": {"type": "string"}
                                    },
                                    "required": ["name", "type"]
                                },
                                "description": "Function parameters",
                                "default": []
                            }
                        },
                        "required": ["blueprint_path", "function_name"]
                    }
                ),
                Tool(
                    name="edit_graph",
                    description="Edit Blueprint graph with nodes and connections",
                    inputSchema={
                        "type": "object",
                        "properties": {
                            "blueprint_path": {
                                "type": "string",
                                "description": "Path to the Blueprint asset"
                            },
                            "graph_type": {
                                "type": "string",
                                "description": "Type of graph (Event, Function, Macro)",
                                "enum": ["Event", "Function", "Macro"],
                                "default": "Event"
                            },
                            "nodes": {
                                "type": "array",
                                "items": {
                                    "type": "object",
                                    "properties": {
                                        "type": {"type": "string"},
                                        "id": {"type": "string"},
                                        "position": {
                                            "type": "object",
                                            "properties": {
                                                "x": {"type": "number"},
                                                "y": {"type": "number"}
                                            }
                                        },
                                        "properties": {"type": "object"}
                                    },
                                    "required": ["type", "id"]
                                },
                                "description": "Nodes to add to the graph"
                            },
                            "connections": {
                                "type": "array",
                                "items": {
                                    "type": "object", 
                                    "properties": {
                                        "from_node": {"type": "string"},
                                        "from_pin": {"type": "string"},
                                        "to_node": {"type": "string"},
                                        "to_pin": {"type": "string"}
                                    },
                                    "required": ["from_node", "from_pin", "to_node", "to_pin"]
                                },
                                "description": "Connections between nodes"
                            }
                        },
                        "required": ["blueprint_path", "nodes"]
                    }
                ),
                Tool(
                    name="list_assets",
                    description="List assets in Unreal Engine content browser",
                    inputSchema={
                        "type": "object",
                        "properties": {
                            "path": {
                                "type": "string",
                                "description": "Content path to search",
                                "default": "/Game/"
                            },
                            "asset_type": {
                                "type": "string",
                                "description": "Filter by asset type (Blueprint, StaticMesh, Material, etc.)",
                                "default": ""
                            }
                        }
                    }
                ),
                Tool(
                    name="get_asset",
                    description="Get detailed information about a specific asset",
                    inputSchema={
                        "type": "object",
                        "properties": {
                            "asset_path": {
                                "type": "string",
                                "description": "Full path to the asset"
                            }
                        },
                        "required": ["asset_path"]
                    }
                ),
                Tool(
                    name="create_asset",
                    description="Create a new asset in Unreal Engine",
                    inputSchema={
                        "type": "object",
                        "properties": {
                            "asset_type": {
                                "type": "string",
                                "description": "Type of asset to create"
                            },
                            "asset_name": {
                                "type": "string",
                                "description": "Name for the new asset"
                            },
                            "asset_path": {
                                "type": "string",
                                "description": "Path where to create the asset",
                                "default": "/Game/"
                            },
                            "properties": {
                                "type": "object",
                                "description": "Asset-specific properties",
                                "default": {}
                            }
                        },
                        "required": ["asset_type", "asset_name"]
                    }
                )
            ]
        
        @self.server.call_tool()
        async def call_tool(name: str, arguments: Dict[str, Any]) -> List[TextContent]:
            """Call a tool on the Unreal Engine server"""
            try:
                # Prepare request payload
                payload = {
                    "tool": name,
                    "arguments": arguments
                }
                
                # Make request to Unreal server
                response = await self.http_client.post(
                    f"{UNREAL_SERVER_URL}/api/tools",
                    json=payload,
                    headers={"Content-Type": "application/json"}
                )
                
                if response.status_code == 200:
                    result = response.json()
                    return [TextContent(
                        type="text",
                        text=json.dumps(result, indent=2)
                    )]
                else:
                    error_msg = f"Tool execution failed: {response.status_code} - {response.text}"
                    logger.error(error_msg)
                    return [TextContent(
                        type="text", 
                        text=f"Error: {error_msg}"
                    )]
                    
            except Exception as e:
                error_msg = f"Error calling tool {name}: {str(e)}"
                logger.error(error_msg)
                return [TextContent(
                    type="text",
                    text=f"Error: {error_msg}"
                )]
    
    def _setup_prompt_handlers(self):
        """Setup prompt handlers"""
        
        @self.server.list_prompts()
        async def list_prompts() -> List[Prompt]:
            """List available Unreal Blueprint prompts"""
            return [
                Prompt(
                    name="blueprint_best_practices",
                    description="Get Blueprint development best practices and guidelines",
                    arguments=[]
                ),
                Prompt(
                    name="performance_tips",
                    description="Get Blueprint performance optimization tips",
                    arguments=[]
                ),
                Prompt(
                    name="node_reference",
                    description="Get reference information for Blueprint nodes",
                    arguments=[
                        {
                            "name": "node_type",
                            "description": "Type of node to get information about",
                            "required": False
                        }
                    ]
                ),
                Prompt(
                    name="troubleshooting",
                    description="Get troubleshooting guide for common Blueprint issues",
                    arguments=[
                        {
                            "name": "issue_type",
                            "description": "Type of issue (compilation, runtime, performance)",
                            "required": False
                        }
                    ]
                )
            ]
        
        @self.server.get_prompt()
        async def get_prompt(name: str, arguments: Dict[str, str]) -> str:
            """Get a specific prompt"""
            try:
                response = await self.http_client.get(
                    f"{UNREAL_SERVER_URL}/api/prompts/{name}",
                    params=arguments
                )
                
                if response.status_code == 200:
                    return response.text
                else:
                    return f"Error retrieving prompt: {response.status_code}"
                    
            except Exception as e:
                logger.error(f"Error getting prompt {name}: {e}")
                return f"Error: {str(e)}"
    
    async def run(self):
        """Run the MCP server"""
        async with stdio_server() as (read_stream, write_stream):
            await self.server.run(
                read_stream,
                write_stream,
                InitializationOptions(
                    server_name="unreal-blueprint-mcp",
                    server_version="1.0.0",
                    capabilities=self.server.get_capabilities(
                        notification_options=None,
                        experimental_capabilities={}
                    )
                )
            )
    
    async def close(self):
        """Close the HTTP client"""
        await self.http_client.aclose()


async def main():
    """Main entry point"""
    server = UnrealBlueprintMCPServer()
    
    try:
        await server.run()
    except KeyboardInterrupt:
        logger.info("Server stopped by user")
    except Exception as e:
        logger.error(f"Server error: {e}")
        raise
    finally:
        await server.close()


if __name__ == "__main__":
    asyncio.run(main())