#!/usr/bin/env python3
"""
Quick Start Example for Unreal Blueprint MCP Integration

This script demonstrates how to use the Unreal Blueprint MCP system
with both automated API calls and Gemini CLI integration examples.

Run this script to see a complete workflow example.
"""

import subprocess
import sys
import time
import json

def print_section(title):
    """Print a formatted section header"""
    print(f"\n{'='*60}")
    print(f"🎯 {title}")
    print(f"{'='*60}\n")

def print_step(step_num, description):
    """Print a formatted step"""
    print(f"📋 Step {step_num}: {description}")
    print("-" * 50)

def run_command(command, description):
    """Run a command and display the result"""
    print(f"💻 Running: {command}")
    try:
        result = subprocess.run(
            command, 
            shell=True, 
            capture_output=True, 
            text=True, 
            timeout=10
        )
        
        if result.returncode == 0:
            print(f"✅ Success!")
            if result.stdout.strip():
                print(f"Output:\n{result.stdout}")
        else:
            print(f"❌ Error (exit code: {result.returncode})")
            if result.stderr.strip():
                print(f"Error: {result.stderr}")
            if result.stdout.strip():
                print(f"Output: {result.stdout}")
    except subprocess.TimeoutExpired:
        print(f"⏰ Command timed out after 10 seconds")
    except Exception as e:
        print(f"❌ Exception: {e}")
    
    print()

def wait_for_user():
    """Wait for user input to continue"""
    input("🔄 Press Enter to continue to the next step...")

def main():
    """Main demonstration function"""
    print("""
🚀 Unreal Blueprint MCP - Quick Start Example
============================================

This script will demonstrate the complete workflow of using
Unreal Blueprint MCP with Claude Code and Gemini CLI.

Prerequisites:
1. Unreal Engine running with UnrealBlueprintMCP plugin enabled
2. Python dependencies installed (pip install -r requirements.txt)
3. Scripts in current directory

Let's get started!
    """)
    
    wait_for_user()
    
    # Step 1: System Check
    print_section("System Diagnostic Check")
    print_step(1, "Running comprehensive system check")
    run_command("./diagnostic-check.sh", "System diagnostic")
    wait_for_user()
    
    # Step 2: Basic API Test
    print_section("Basic API Testing")
    print_step(2, "Testing basic connectivity with Unreal Engine")
    run_command("python unreal-helper.py ping", "Ping test")
    
    print_step(3, "Listing available methods")
    run_command("python unreal-helper.py --list-methods", "Available methods")
    wait_for_user()
    
    # Step 3: Explore Existing Assets
    print_section("Exploring Existing Project Assets")
    print_step(4, "Listing all Blueprints in the project")
    run_command("python unreal-helper.py getBlueprints", "List Blueprints")
    
    print_step(5, "Listing actors in the current world")
    run_command("python unreal-helper.py getActors", "List actors")
    
    print_step(6, "Exploring project resources")
    run_command("python unreal-helper.py resources.list", "List resources")
    wait_for_user()
    
    # Step 4: Create a Simple Blueprint
    print_section("Creating a New Blueprint")
    print_step(7, "Creating a test Blueprint actor")
    
    blueprint_params = {
        "blueprint_name": "QuickStartTestActor",
        "path": "/Game/QuickStartDemo",
        "parent_class": "Actor"
    }
    
    run_command(
        f"python unreal-helper.py tools.create_blueprint '{json.dumps(blueprint_params)}'",
        "Create Blueprint"
    )
    
    print_step(8, "Adding a health variable to the Blueprint")
    
    variable_params = {
        "blueprint_path": "/Game/QuickStartDemo/QuickStartTestActor",
        "variable_name": "Health",
        "variable_type": "float",
        "default_value": "100.0",
        "is_public": True
    }
    
    run_command(
        f"python unreal-helper.py tools.add_variable '{json.dumps(variable_params)}'",
        "Add variable"
    )
    
    print_step(9, "Adding a custom function to the Blueprint")
    
    function_params = {
        "blueprint_path": "/Game/QuickStartDemo/QuickStartTestActor",
        "function_name": "TakeDamage"
    }
    
    run_command(
        f"python unreal-helper.py tools.add_function '{json.dumps(function_params)}'",
        "Add function"
    )
    wait_for_user()
    
    # Step 5: Explore Available Prompts
    print_section("Exploring Game Development Prompts")
    print_step(10, "Listing available game development guides")
    run_command("python unreal-helper.py prompts.list", "List prompts")
    
    print_step(11, "Getting a specific prompt for player character creation")
    run_command(
        "python unreal-helper.py prompts.get '{\"prompt_name\":\"create_player_character\"}'",
        "Get player character prompt"
    )
    wait_for_user()
    
    # Step 6: Full API Test
    print_section("Comprehensive API Testing")
    print_step(12, "Running the complete API test suite")
    run_command("python test_mcp_api.py", "Full API test")
    wait_for_user()
    
    # Step 7: Claude Code Integration Example
    print_section("Claude Code Integration Examples")
    print("""
📝 Now you can use Claude Code with natural language commands:

Example commands you can try in Claude Code:
--------------------------------------------

1. "Create a new player character Blueprint named 'PlayerCharacter' 
   in '/Game/Characters/' with health and speed variables"

2. "List all the Blueprints in my project and show their details"

3. "Add a jump function to the PlayerCharacter Blueprint"

4. "Show me the available game development prompts and get the 
   one for implementing a health system"

5. "Create a collectible item Blueprint with sound and particle effects"

Configuration for Claude Code:
-----------------------------
Make sure your Claude Code MCP configuration includes:

{
  "mcpServers": {
    "unreal-blueprint-mcp": {
      "command": "python",
      "args": ["%s/mcp_client.py"],
      "env": {
        "UNREAL_SERVER_URL": "http://localhost:8080"
      }
    }
  }
}

Replace the path with your actual mcp_client.py location.
    """ % subprocess.check_output("pwd", shell=True, text=True).strip())
    
    wait_for_user()
    
    # Step 8: Gemini CLI Integration Example
    print_section("Gemini CLI Integration Examples")
    print("""
🤖 Gemini CLI Integration Examples:
----------------------------------

You can now use Gemini CLI with the Unreal Engine context.
Here are some example commands:

1. Basic Analysis:
   echo "Analyze my Unreal Engine project structure and suggest improvements" | gemini -f unreal-helper.py

2. Blueprint Review:
   echo "Review the QuickStartTestActor Blueprint we just created and suggest optimizations" | gemini

3. Game Design Consultation:
   echo "I'm making a platformer game. What Blueprints should I create for the core mechanics?" | gemini

4. Performance Analysis:
   echo "Analyze my project for potential performance issues in Blueprint design" | gemini

5. Custom Workflow:
   Create a file with your project context and use it with Gemini:
   
   echo "Project: 3D Platformer
   Current Blueprints: $(python unreal-helper.py getBlueprints --raw)
   
   Based on this project state, what should I implement next?" | gemini

Advanced Integration:
--------------------
You can also create custom scripts that combine both:
1. Use Claude Code to implement features
2. Use Gemini CLI to analyze and optimize
3. Iterate based on AI recommendations
    """)
    
    wait_for_user()
    
    # Final Summary
    print_section("🎉 Quick Start Complete!")
    print("""
Congratulations! You've successfully completed the Unreal Blueprint MCP quick start.

What you've accomplished:
✅ Verified system configuration
✅ Tested API connectivity  
✅ Explored existing project assets
✅ Created a new Blueprint with variables and functions
✅ Explored game development prompts
✅ Learned Claude Code integration patterns
✅ Learned Gemini CLI integration examples

Next Steps:
-----------
1. Try the Claude Code examples with natural language
2. Experiment with Gemini CLI for project analysis
3. Create more complex Blueprints using the tools
4. Explore the game development prompts for guidance
5. Build your own AI-powered development workflows

Resources:
----------
📖 INTEGRATION_GUIDE.md - Complete integration documentation
📖 README.md - Plugin documentation and API reference
📖 USER_GUIDE.md - Step-by-step tutorials
🛠️ unreal-helper.py - Gemini CLI integration script
🔍 diagnostic-check.sh - System diagnostic tool
🧪 test_mcp_api.py - Complete API test suite

Support:
--------
- Run ./diagnostic-check.sh for troubleshooting
- Check the logs in Unreal Engine Output Log
- Review the INTEGRATION_GUIDE.md for detailed setup

Happy developing with AI-powered Unreal Engine workflows! 🚀🎮
    """)

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n\n👋 Quick start cancelled by user. You can run this script again anytime!")
    except Exception as e:
        print(f"\n\n❌ Unexpected error: {e}")
        print("Please check your setup and try again.")