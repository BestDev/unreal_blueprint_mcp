#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SVerticalBox.h"
#include "Widgets/Layout/SHorizontalBox.h"
#include "Engine/EngineTypes.h"

DECLARE_DELEGATE_OneParam(FOnMCPStatusAction, const FString&);

/**
 * Status panel widget showing detailed MCP server information
 */
class UNREALBLUEPRINTMCP_API SMCPStatusWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMCPStatusWidget) {}
		SLATE_EVENT(FOnMCPStatusAction, OnStatusAction)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Update all status information */
	void UpdateStatus(bool bIsRunning, int32 Port, const FString& URL, 
		int32 ClientCount, const FDateTime& StartTime, 
		int32 RequestCount, float AvgResponseTime);

	/** Update network statistics */
	void UpdateNetworkStats(int32 RequestsPerSecond, float LatestResponseTime);

	/** Add network activity entry */
	void AddNetworkActivity(const FString& Method, const FString& Endpoint, float ResponseTime);

private:
	/** Status action event */
	FOnMCPStatusAction OnStatusAction;

	/** UI Elements */
	TSharedPtr<SImage> MainStatusIcon;
	TSharedPtr<STextBlock> StatusText;
	TSharedPtr<STextBlock> PortText;
	TSharedPtr<STextBlock> URLText;
	TSharedPtr<STextBlock> ClientCountText;
	TSharedPtr<STextBlock> UptimeText;
	TSharedPtr<STextBlock> RequestCountText;
	TSharedPtr<STextBlock> AvgResponseTimeText;
	TSharedPtr<STextBlock> RequestsPerSecondText;
	TSharedPtr<SVerticalBox> NetworkActivityList;

	/** Server state */
	bool bServerRunning = false;
	int32 CurrentPort = 0;
	FString ServerURL;
	int32 ConnectedClients = 0;
	FDateTime ServerStartTime;
	int32 TotalRequests = 0;
	float AverageResponseTime = 0.0f;
	int32 CurrentRequestsPerSecond = 0;
	TArray<FString> RecentActivity;

	/** Timer for updating uptime */
	FTimerHandle UptimeUpdateTimer;

	/** Update uptime display */
	void UpdateUptimeDisplay();

	/** Get status icon brush */
	const FSlateBrush* GetMainStatusIconBrush() const;

	/** Get formatted uptime string */
	FText GetUptimeText() const;

	/** Limit activity list size */
	void TrimActivityList();
};