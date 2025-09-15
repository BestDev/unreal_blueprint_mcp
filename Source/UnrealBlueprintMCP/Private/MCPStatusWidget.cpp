#include "MCPStatusWidget.h"
#include "SlateOptMacros.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "EditorStyleSet.h"
#include "Engine/Engine.h"
#include "TimerManager.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SMCPStatusWidget::Construct(const FArguments& InArgs)
{
	OnStatusAction = InArgs._OnStatusAction;

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(8.0f)
		[
			SNew(SVerticalBox)
			// Header with main status
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 8.0f, 0.0f)
				[
					SAssignNew(MainStatusIcon, SImage)
					.Image(this, &SMCPStatusWidget::GetMainStatusIconBrush)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SAssignNew(StatusText, STextBlock)
					.Text_Lambda([this]() -> FText
					{
						return bServerRunning ? 
							NSLOCTEXT("MCPStatus", "Running", "MCP Server Running") :
							NSLOCTEXT("MCPStatus", "Stopped", "MCP Server Stopped");
					})
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
				]
			]
			
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SSeparator)
				.Orientation(Orient_Horizontal)
			]

			// Server details
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 8.0f)
			[
				SNew(SVerticalBox)
				// Port information
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 2.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("MCPStatus", "Port", "Port:"))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SAssignNew(PortText, STextBlock)
						.Text_Lambda([this]() -> FText
						{
							return FText::AsNumber(CurrentPort);
						})
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
					]
				]
				
				// URL information
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 2.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("MCPStatus", "URL", "URL:"))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SAssignNew(URLText, STextBlock)
						.Text_Lambda([this]() -> FText
						{
							return FText::FromString(ServerURL);
						})
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
					]
				]
				
				// Client count
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 2.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("MCPStatus", "Clients", "Clients:"))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SAssignNew(ClientCountText, STextBlock)
						.Text_Lambda([this]() -> FText
						{
							return FText::AsNumber(ConnectedClients);
						})
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
					]
				]
				
				// Uptime
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 2.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("MCPStatus", "Uptime", "Uptime:"))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SAssignNew(UptimeText, STextBlock)
						.Text(this, &SMCPStatusWidget::GetUptimeText)
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
					]
				]
			]
			
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SSeparator)
				.Orientation(Orient_Horizontal)
			]

			// Performance metrics
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 8.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 2.0f)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("MCPStatus", "Performance", "Performance Metrics"))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
				]
				
				// Total requests
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(8.0f, 2.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("MCPStatus", "TotalRequests", "Total Requests:"))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SAssignNew(RequestCountText, STextBlock)
						.Text_Lambda([this]() -> FText
						{
							return FText::AsNumber(TotalRequests);
						})
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
					]
				]
				
				// Average response time
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(8.0f, 2.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("MCPStatus", "AvgResponseTime", "Avg Response:"))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SAssignNew(AvgResponseTimeText, STextBlock)
						.Text_Lambda([this]() -> FText
						{
							return FText::FromString(FString::Printf(TEXT("%.2fms"), AverageResponseTime));
						})
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
					]
				]
				
				// Requests per second
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(8.0f, 2.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("MCPStatus", "RequestsPerSecond", "Requests/sec:"))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SAssignNew(RequestsPerSecondText, STextBlock)
						.Text_Lambda([this]() -> FText
						{
							return FText::AsNumber(CurrentRequestsPerSecond);
						})
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
					]
				]
			]
			
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SSeparator)
				.Orientation(Orient_Horizontal)
			]

			// Recent activity
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(0.0f, 8.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 4.0f)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("MCPStatus", "RecentActivity", "Recent Activity"))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
				]
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						SAssignNew(NetworkActivityList, SVerticalBox)
					]
				]
			]
		]
	];

	// Start uptime timer if server is running
	if (bServerRunning)
	{
		UpdateUptimeDisplay();
	}
}

void SMCPStatusWidget::UpdateStatus(bool bIsRunning, int32 Port, const FString& URL, 
	int32 ClientCount, const FDateTime& StartTime, 
	int32 RequestCount, float AvgResponseTime)
{
	bServerRunning = bIsRunning;
	CurrentPort = Port;
	ServerURL = URL;
	ConnectedClients = ClientCount;
	ServerStartTime = StartTime;
	TotalRequests = RequestCount;
	AverageResponseTime = AvgResponseTime;

	if (bServerRunning && !UptimeUpdateTimer.IsValid())
	{
		// Start uptime update timer
		if (GEngine && GEngine->GetTimerManager())
		{
			GEngine->GetTimerManager()->SetTimer(UptimeUpdateTimer, 
				this, &SMCPStatusWidget::UpdateUptimeDisplay, 1.0f, true);
		}
	}
	else if (!bServerRunning && UptimeUpdateTimer.IsValid())
	{
		// Stop uptime update timer
		if (GEngine && GEngine->GetTimerManager())
		{
			GEngine->GetTimerManager()->ClearTimer(UptimeUpdateTimer);
		}
	}
}

void SMCPStatusWidget::UpdateNetworkStats(int32 RequestsPerSecond, float LatestResponseTime)
{
	CurrentRequestsPerSecond = RequestsPerSecond;
	// Update average response time (simple moving average)
	if (TotalRequests > 0)
	{
		AverageResponseTime = (AverageResponseTime * 0.9f) + (LatestResponseTime * 0.1f);
	}
	else
	{
		AverageResponseTime = LatestResponseTime;
	}
}

void SMCPStatusWidget::AddNetworkActivity(const FString& Method, const FString& Endpoint, float ResponseTime)
{
	if (!NetworkActivityList.IsValid())
	{
		return;
	}

	FString ActivityText = FString::Printf(TEXT("%s %s (%.2fms)"), 
		*Method, *Endpoint, ResponseTime);

	// Add to recent activity list
	RecentActivity.Insert(ActivityText, 0);
	TrimActivityList();

	// Update UI
	NetworkActivityList->ClearChildren();
	for (const FString& Activity : RecentActivity)
	{
		NetworkActivityList->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 1.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Activity))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		];
	}
}

void SMCPStatusWidget::UpdateUptimeDisplay()
{
	// This will trigger the uptime text to update via the lambda
}

const FSlateBrush* SMCPStatusWidget::GetMainStatusIconBrush() const
{
	if (bServerRunning)
	{
		return FEditorStyle::GetBrush("Icons.SuccessWithColor");
	}
	else
	{
		return FEditorStyle::GetBrush("Icons.ErrorWithColor");
	}
}

FText SMCPStatusWidget::GetUptimeText() const
{
	if (!bServerRunning)
	{
		return NSLOCTEXT("MCPStatus", "NotRunning", "Not running");
	}

	FTimespan Uptime = FDateTime::Now() - ServerStartTime;
	if (Uptime.GetTotalDays() >= 1.0)
	{
		return FText::FromString(FString::Printf(TEXT("%dd %02dh %02dm"), 
			Uptime.GetDays(), Uptime.GetHours(), Uptime.GetMinutes()));
	}
	else if (Uptime.GetTotalHours() >= 1.0)
	{
		return FText::FromString(FString::Printf(TEXT("%02dh %02dm %02ds"), 
			Uptime.GetHours(), Uptime.GetMinutes(), Uptime.GetSeconds()));
	}
	else
	{
		return FText::FromString(FString::Printf(TEXT("%02dm %02ds"), 
			Uptime.GetMinutes(), Uptime.GetSeconds()));
	}
}

void SMCPStatusWidget::TrimActivityList()
{
	const int32 MaxEntries = 20;
	if (RecentActivity.Num() > MaxEntries)
	{
		RecentActivity.RemoveAt(MaxEntries, RecentActivity.Num() - MaxEntries);
	}
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION