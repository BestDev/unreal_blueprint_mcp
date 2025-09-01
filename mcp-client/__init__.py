"""
UnrealBlueprintMCP Client

AI 모델이 언리얼 엔진의 블루프린트를 자동으로 생성하고 편집할 수 있도록 하는 
Model Context Protocol (MCP) 클라이언트입니다.
"""

__version__ = "1.0.0"
__author__ = "UnrealBlueprintMCP Team"
__description__ = "MCP Client for Unreal Engine Blueprint Automation"

from .src.mcp_client import MCPClient

__all__ = ["MCPClient"]