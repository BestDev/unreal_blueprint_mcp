using UnrealBuildTool;

public class UnrealBlueprintMCP : ModuleRules
{
	public UnrealBlueprintMCP(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
		);
				
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
		);
			
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"UnrealEd",
				"ToolMenus",
				"EditorStyle",
				"EditorWidgets",
				"Slate",
				"SlateCore",
				"ApplicationCore",
				"HTTP",
				"Json",
				// Essential Blueprint Graph modules for K2Node classes (UE 5.6 compatible)
				"BlueprintGraph",
				"KismetCompiler",
				// Core editor modules for Blueprint editing
				"AssetTools",
				"AssetRegistry",
				// Settings module for UDeveloperSettings integration
				"DeveloperSettings"
			}
		);
			
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Projects",
				"InputCore",
				"LevelEditor",
				"Networking",
				"Sockets",
				// Blueprint editor support modules
				"ContentBrowser",
				"PropertyEditor",
				// Additional UE 5.6 compatible modules for K2Node functionality
				"Kismet",
				"GraphEditor",
				"KismetWidgets",
				// Settings editor integration
				"Settings"
			}
		);
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
		);
	}
}