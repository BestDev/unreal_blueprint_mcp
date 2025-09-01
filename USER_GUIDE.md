# UnrealBlueprintMCP - User Guide & Scenarios

## Quick Start for Different Users

### For Game Developers
**Goal**: Use AI tools to accelerate Blueprint development

1. **Install the plugin** in your game project
2. **Test connectivity** with the ping command
3. **Create Blueprints** using the tools namespace
4. **Get development guidance** from the prompts namespace

### For AI/ML Engineers
**Goal**: Integrate Unreal Engine with AI workflows

1. **Set up HTTP client** in your preferred language
2. **Implement JSON-RPC 2.0** communication
3. **Use resources namespace** to inspect project assets
4. **Automate Blueprint creation** with tools namespace

### For DevOps/Pipeline Engineers
**Goal**: Automate game development workflows

1. **Install in CI/CD environment** (headless Unreal Editor)
2. **Implement asset validation** using resources.list and resources.get
3. **Batch create assets** with automated scripts
4. **Generate reports** on project structure

## Real-World Usage Scenarios

### Scenario 1: AI-Assisted Character Creation

**Use Case**: An AI agent helps create a complete player character system

```python
import requests
import json

class UnrealAIAssistant:
    def __init__(self, server_url="http://localhost:8080"):
        self.server_url = server_url
        self.request_id = 1
    
    def call_api(self, method, params=None):
        payload = {
            "jsonrpc": "2.0",
            "method": method,
            "params": params or {},
            "id": self.request_id
        }
        self.request_id += 1
        
        response = requests.post(self.server_url, json=payload)
        return response.json()
    
    def create_player_character_system(self):
        """Create a complete player character with health and movement"""
        
        # Step 1: Get the detailed guide
        guide = self.call_api("prompts.get", {
            "prompt_name": "create_player_character"
        })
        print("📖 Retrieved character creation guide")
        
        # Step 2: Create the character Blueprint
        character = self.call_api("tools.create_blueprint", {
            "blueprint_name": "BP_AIPlayerCharacter",
            "path": "/Game/Characters",
            "parent_class": "Character"
        })
        
        if "error" in character:
            print(f"❌ Failed to create character: {character['error']}")
            return False
            
        blueprint_path = character["result"]["blueprint_path"]
        print(f"✅ Created character Blueprint: {blueprint_path}")
        
        # Step 3: Add health system variables
        health_vars = [
            {"name": "Health", "type": "float"},
            {"name": "MaxHealth", "type": "float"}, 
            {"name": "bIsDead", "type": "bool"}
        ]
        
        for var in health_vars:
            result = self.call_api("tools.add_variable", {
                "blueprint_path": blueprint_path,
                "variable_name": var["name"],
                "variable_type": var["type"],
                "is_public": True
            })
            print(f"✅ Added variable: {var['name']}")
        
        # Step 4: Add gameplay functions
        functions = ["TakeDamage", "Heal", "Die", "Respawn"]
        
        for func_name in functions:
            result = self.call_api("tools.add_function", {
                "blueprint_path": blueprint_path,
                "function_name": func_name
            })
            print(f"✅ Added function: {func_name}")
        
        # Step 5: Set up basic event graph
        self.call_api("tools.edit_graph", {
            "blueprint_path": blueprint_path,
            "graph_name": "EventGraph",
            "nodes_to_add": [
                {"type": "BeginPlay", "x": 100, "y": 100},
                {"type": "PrintString", "x": 300, "y": 100}
            ]
        })
        print("✅ Added basic event nodes")
        
        return True

# Usage
assistant = UnrealAIAssistant()
assistant.create_player_character_system()
```

### Scenario 2: Project Asset Validation

**Use Case**: Automated validation of Blueprint structure in CI/CD

```python
class ProjectValidator:
    def __init__(self, server_url="http://localhost:8080"):
        self.server_url = server_url
        self.validation_errors = []
    
    def call_api(self, method, params=None):
        payload = {
            "jsonrpc": "2.0",
            "method": method,
            "params": params or {},
            "id": 1
        }
        response = requests.post(self.server_url, json=payload)
        return response.json()
    
    def validate_project_structure(self):
        """Validate that project follows naming conventions and structure"""
        
        # Check required directories exist
        required_paths = [
            "/Game/Characters",
            "/Game/Blueprints",
            "/Game/UI",
            "/Game/Levels"
        ]
        
        for path in required_paths:
            assets = self.call_api("resources.list", {"path": path})
            if "error" in assets:
                self.validation_errors.append(f"Missing directory: {path}")
            else:
                print(f"✅ Found directory: {path}")
        
        # Validate Blueprint naming conventions
        blueprints = self.call_api("resources.list", {"path": "/Game/Blueprints"})
        
        if "result" in blueprints:
            for asset in blueprints["result"]["assets"]:
                if asset["class"] == "Blueprint":
                    name = asset["name"]
                    if not name.startswith("BP_"):
                        self.validation_errors.append(
                            f"Blueprint naming violation: {name} should start with 'BP_'"
                        )
        
        # Validate character Blueprints have health system
        characters = self.call_api("resources.list", {"path": "/Game/Characters"})
        
        if "result" in characters:
            for asset in characters["result"]["assets"]:
                if asset["class"] == "Blueprint":
                    details = self.call_api("resources.get", {
                        "asset_path": asset["path"]
                    })
                    
                    if "result" in details and "blueprint_details" in details["result"]:
                        variables = details["result"]["blueprint_details"]["variables"]
                        health_vars = [v["name"] for v in variables]
                        
                        required_health_vars = ["Health", "MaxHealth"]
                        missing_vars = [v for v in required_health_vars if v not in health_vars]
                        
                        if missing_vars:
                            self.validation_errors.append(
                                f"Character {asset['name']} missing health variables: {missing_vars}"
                            )
        
        return len(self.validation_errors) == 0
    
    def generate_report(self):
        """Generate validation report"""
        if self.validation_errors:
            print("❌ Project validation failed:")
            for error in self.validation_errors:
                print(f"  - {error}")
            return False
        else:
            print("✅ Project validation passed!")
            return True

# Usage in CI/CD
validator = ProjectValidator()
if validator.validate_project_structure():
    validator.generate_report()
    exit(0)  # Success
else:
    validator.generate_report()
    exit(1)  # Failure
```

### Scenario 3: Interactive Learning Assistant

**Use Case**: Help new developers learn Unreal Engine Blueprint development

```python
class BlueprintTutor:
    def __init__(self, server_url="http://localhost:8080"):
        self.server_url = server_url
        self.current_lesson = None
    
    def call_api(self, method, params=None):
        payload = {
            "jsonrpc": "2.0",
            "method": method,
            "params": params or {},
            "id": 1
        }
        response = requests.post(self.server_url, json=payload)
        return response.json()
    
    def show_available_lessons(self):
        """Display all available learning prompts"""
        prompts = self.call_api("prompts.list")
        
        if "result" in prompts:
            print("📚 Available Blueprint Lessons:")
            print("=" * 50)
            
            for i, prompt in enumerate(prompts["result"]["prompts"], 1):
                print(f"{i}. {prompt['name']}")
                print(f"   📖 {prompt['description']}")
                print()
            
            return prompts["result"]["prompts"]
        return []
    
    def start_lesson(self, lesson_name):
        """Start an interactive lesson"""
        lesson = self.call_api("prompts.get", {"prompt_name": lesson_name})
        
        if "result" in lesson:
            self.current_lesson = lesson["result"]
            print(f"🎓 Starting lesson: {lesson_name}")
            print("=" * 60)
            print(self.current_lesson["content"])
            print("=" * 60)
            
            # Offer to create example Blueprint
            create_example = input("\n🤖 Would you like me to create an example Blueprint? (y/n): ")
            
            if create_example.lower() == 'y':
                self.create_lesson_example(lesson_name)
        else:
            print(f"❌ Lesson not found: {lesson_name}")
    
    def create_lesson_example(self, lesson_name):
        """Create practical examples based on the lesson"""
        
        if lesson_name == "create_player_character":
            # Create example player character
            result = self.call_api("tools.create_blueprint", {
                "blueprint_name": "BP_TutorialPlayer",
                "path": "/Game/Tutorial",
                "parent_class": "Character"
            })
            
            if "result" in result:
                print("✅ Created example player character Blueprint")
                
                # Add basic variables
                self.call_api("tools.add_variable", {
                    "blueprint_path": result["result"]["blueprint_path"],
                    "variable_name": "WalkSpeed",
                    "variable_type": "float",
                    "is_public": True
                })
                print("✅ Added WalkSpeed variable")
        
        elif lesson_name == "implement_health_system":
            # Create health system example
            result = self.call_api("tools.create_blueprint", {
                "blueprint_name": "BP_HealthExample",
                "path": "/Game/Tutorial",
                "parent_class": "Actor"
            })
            
            if "result" in result:
                blueprint_path = result["result"]["blueprint_path"]
                
                # Add health variables
                health_vars = [
                    {"name": "Health", "type": "float"},
                    {"name": "MaxHealth", "type": "float"},
                    {"name": "bCanTakeDamage", "type": "bool"}
                ]
                
                for var in health_vars:
                    self.call_api("tools.add_variable", {
                        "blueprint_path": blueprint_path,
                        "variable_name": var["name"],
                        "variable_type": var["type"],
                        "is_public": True
                    })
                
                # Add health functions
                self.call_api("tools.add_function", {
                    "blueprint_path": blueprint_path,
                    "function_name": "TakeDamage"
                })
                
                print("✅ Created health system example Blueprint")
    
    def interactive_mode(self):
        """Start interactive learning session"""
        print("🎮 Welcome to Blueprint Learning Assistant!")
        print("Type 'help' for commands, 'quit' to exit")
        
        while True:
            command = input("\n📝 Enter command: ").strip().lower()
            
            if command == 'quit':
                print("👋 Happy coding!")
                break
            elif command == 'help':
                print("\nAvailable commands:")
                print("- 'lessons': Show available lessons")
                print("- 'start <lesson_name>': Start a specific lesson")
                print("- 'project': Show current project structure")
                print("- 'quit': Exit")
            elif command == 'lessons':
                self.show_available_lessons()
            elif command.startswith('start '):
                lesson_name = command[6:]  # Remove 'start '
                self.start_lesson(lesson_name)
            elif command == 'project':
                self.show_project_structure()
            else:
                print("❓ Unknown command. Type 'help' for available commands.")
    
    def show_project_structure(self):
        """Display current project structure"""
        assets = self.call_api("resources.list", {"path": "/Game"})
        
        if "result" in assets:
            print("📁 Current Project Structure:")
            for asset in assets["result"]["assets"]:
                print(f"  📄 {asset['name']} ({asset['class']})")

# Usage
tutor = BlueprintTutor()
tutor.interactive_mode()
```

### Scenario 4: Batch Asset Creation

**Use Case**: Create multiple game assets from configuration files

```python
import yaml

class AssetBatchCreator:
    def __init__(self, server_url="http://localhost:8080"):
        self.server_url = server_url
    
    def call_api(self, method, params=None):
        payload = {
            "jsonrpc": "2.0", 
            "method": method,
            "params": params or {},
            "id": 1
        }
        response = requests.post(self.server_url, json=payload)
        return response.json()
    
    def create_from_config(self, config_file):
        """Create assets from YAML configuration"""
        
        with open(config_file, 'r') as f:
            config = yaml.safe_load(f)
        
        for asset_config in config.get('assets', []):
            asset_type = asset_config['type']
            
            if asset_type == 'blueprint':
                self.create_blueprint_from_config(asset_config)
            elif asset_type == 'character':
                self.create_character_from_config(asset_config)
    
    def create_blueprint_from_config(self, config):
        """Create Blueprint from configuration"""
        
        # Create the Blueprint
        result = self.call_api("tools.create_blueprint", {
            "blueprint_name": config['name'],
            "path": config['path'],
            "parent_class": config.get('parent_class', 'Actor')
        })
        
        if "error" in result:
            print(f"❌ Failed to create {config['name']}: {result['error']}")
            return
        
        blueprint_path = result["result"]["blueprint_path"]
        print(f"✅ Created Blueprint: {config['name']}")
        
        # Add variables
        for var_config in config.get('variables', []):
            self.call_api("tools.add_variable", {
                "blueprint_path": blueprint_path,
                "variable_name": var_config['name'],
                "variable_type": var_config['type'],
                "is_public": var_config.get('public', False)
            })
            print(f"  ✅ Added variable: {var_config['name']}")
        
        # Add functions
        for func_name in config.get('functions', []):
            self.call_api("tools.add_function", {
                "blueprint_path": blueprint_path,
                "function_name": func_name
            })
            print(f"  ✅ Added function: {func_name}")

# Example config.yaml:
"""
assets:
  - type: blueprint
    name: BP_Collectible
    path: /Game/Items
    parent_class: Actor
    variables:
      - name: Value
        type: int
        public: true
      - name: bCanBeCollected
        type: bool
        public: false
    functions:
      - OnCollected
      - SetValue
      
  - type: blueprint
    name: BP_Enemy
    path: /Game/Characters
    parent_class: Character
    variables:
      - name: Health
        type: float
        public: true
      - name: AttackDamage
        type: float
        public: true
    functions:
      - TakeDamage
      - Attack
      - Die
"""

# Usage
creator = AssetBatchCreator()
creator.create_from_config('game_assets.yaml')
```

## Integration with Development Tools

### VS Code Extension Example

```typescript
// Example VS Code extension integration
import * as vscode from 'vscode';

class UnrealMCPProvider {
    private serverUrl = 'http://localhost:8080';
    
    async createBlueprint(name: string, path: string, parentClass: string) {
        const response = await fetch(this.serverUrl, {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({
                jsonrpc: '2.0',
                method: 'tools.create_blueprint',
                params: {blueprint_name: name, path: path, parent_class: parentClass},
                id: 1
            })
        });
        
        const result = await response.json();
        
        if (result.error) {
            vscode.window.showErrorMessage(`Failed to create Blueprint: ${result.error.message}`);
        } else {
            vscode.window.showInformationMessage(`Blueprint created: ${name}`);
        }
    }
}
```

### Blender Integration Example

```python
# Example Blender addon integration
import bpy
import requests

class UnrealBridgeOperator(bpy.types.Operator):
    bl_idname = "unreal.create_blueprint"
    bl_label = "Create Unreal Blueprint"
    
    def execute(self, context):
        # Get selected objects
        selected = bpy.context.selected_objects
        
        for obj in selected:
            # Create Blueprint for each object
            response = requests.post('http://localhost:8080', json={
                'jsonrpc': '2.0',
                'method': 'tools.create_blueprint',
                'params': {
                    'blueprint_name': f'BP_{obj.name}',
                    'path': '/Game/FromBlender',
                    'parent_class': 'StaticMeshActor'
                },
                'id': 1
            })
            
            if response.json().get('result'):
                self.report({'INFO'}, f"Created Blueprint for {obj.name}")
            else:
                self.report({'ERROR'}, f"Failed to create Blueprint for {obj.name}")
        
        return {'FINISHED'}
```

## Best Practices

### Error Handling
- Always check for error responses
- Implement retry logic for network issues
- Provide meaningful error messages to users

### Performance
- Use batch operations when possible
- Cache responses for repeated queries
- Implement connection pooling for high-frequency use

### Security
- Validate all input parameters
- Use HTTPS in production environments
- Implement authentication for sensitive operations

### Reliability
- Implement health checks
- Use timeouts for all requests
- Handle Unreal Editor restarts gracefully

---

This user guide provides practical examples for integrating UnrealBlueprintMCP into various workflows and development environments.