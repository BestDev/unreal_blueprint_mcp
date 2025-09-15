#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Engine/EngineTypes.h"

DECLARE_DELEGATE_OneParam(FOnMCPServerAction, const FString&);

/**
 * Toolbar widget for MCP server status and quick actions
 */
class UNREALBLUEPRINTMCP_API SMCPToolbarWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMCPToolbarWidget) {}
		SLATE_EVENT(FOnMCPServerAction, OnServerAction)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Update server status */
	void UpdateServerStatus(bool bIsRunning, int32 Port, int32 ClientCount);

	/** Update network activity indicator */
	void UpdateNetworkActivity(bool bIsActive);

private:
	/** Server action event */
	FOnMCPServerAction OnServerAction;

	/** UI Elements */
	TSharedPtr<SImage> StatusIcon;
	TSharedPtr<STextBlock> StatusText;
	TSharedPtr<SImage> NetworkActivityIndicator;
	TSharedPtr<SButton> StartStopButton;

	/** Server state */
	bool bServerRunning = false;
	int32 CurrentPort = 0;
	int32 ClientCount = 0;

	/** Button click handlers */
	FReply OnStartStopClicked();
	FReply OnServerInfoClicked();
	FReply OnQuickRestartClicked();

	/** Get status icon brush */
	const FSlateBrush* GetStatusIconBrush() const;
	
	/** Get status text */
	FText GetStatusText() const;
	
	/** Get network activity brush */
	const FSlateBrush* GetNetworkActivityBrush() const;
};