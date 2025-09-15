#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

DECLARE_DELEGATE_OneParam(FOnMCPContextMenuAction, const FString&);

/**
 * Context menu widget for MCP server operations
 */
class UNREALBLUEPRINTMCP_API SMCPContextMenuWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMCPContextMenuWidget) {}
		SLATE_EVENT(FOnMCPContextMenuAction, OnContextMenuAction)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Create context menu for asset browser */
	static TSharedRef<SWidget> CreateAssetContextMenu(bool bServerRunning, const FOnMCPContextMenuAction& OnAction);

	/** Create context menu for content browser */
	static TSharedRef<SWidget> CreateContentBrowserContextMenu(const FOnMCPContextMenuAction& OnAction);

	/** Handle drag and drop of configuration files */
	static bool HandleConfigFileDrop(const FString& FilePath, const FOnMCPContextMenuAction& OnAction);

private:
	/** Context menu action event */
	FOnMCPContextMenuAction OnContextMenuAction;

	/** Create server control menu section */
	static void CreateServerControlSection(FMenuBuilder& MenuBuilder, bool bServerRunning, const FOnMCPContextMenuAction& OnAction);

	/** Create quick actions menu section */
	static void CreateQuickActionsSection(FMenuBuilder& MenuBuilder, const FOnMCPContextMenuAction& OnAction);

	/** Create tools menu section */
	static void CreateToolsSection(FMenuBuilder& MenuBuilder, const FOnMCPContextMenuAction& OnAction);
};