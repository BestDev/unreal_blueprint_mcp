#include "MCPDashboardWidget.h"
#include "SlateOptMacros.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/Docking/LayoutService.h"
#include "EditorStyleSet.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

const FName SMCPDashboardWidget::StatusTabId(TEXT("MCPStatus"));
const FName SMCPDashboardWidget::LogViewerTabId(TEXT("MCPLogViewer"));
const FName SMCPDashboardWidget::ClientTesterTabId(TEXT("MCPClientTester"));

void SMCPDashboardWidget::Construct(const FArguments& InArgs)
{
	OnDashboardAction = InArgs._OnDashboardAction;

	// Create tab manager
	TabManager = FGlobalTabmanager::Get()->NewTabManager(SharedThis(this));
	RegisterTabSpawners();

	// Create layout
	TabLayout = CreateDefaultLayout();

	ChildSlot
	[
		SNew(SVerticalBox)
		// Toolbar at the top
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.0f)
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(4.0f)
			[
				SAssignNew(ToolbarWidget, SMCPToolbarWidget)
				.OnServerAction(this, &SMCPDashboardWidget::OnToolbarAction)
			]
		]
		
		// Main content area with tabs
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(4.0f, 0.0f, 4.0f, 4.0f)
		[
			TabManager->RestoreFrom(TabLayout.ToSharedRef(), TSharedPtr<SWindow>()).ToSharedRef()
		]
	];
}

void SMCPDashboardWidget::UpdateServerStatus(bool bIsRunning, int32 Port, const FString& URL, 
	int32 ClientCount, const FDateTime& StartTime)
{
	bServerRunning = bIsRunning;
	CurrentPort = Port;
	ServerURL = URL;

	// Update toolbar
	if (ToolbarWidget.IsValid())
	{
		ToolbarWidget->UpdateServerStatus(bIsRunning, Port, ClientCount);
	}

	// Update status widget
	if (StatusWidget.IsValid())
	{
		StatusWidget->UpdateStatus(bIsRunning, Port, URL, ClientCount, StartTime, 0, 0.0f);
	}

	// Update client tester server URL
	if (ClientTesterWidget.IsValid() && bIsRunning)
	{
		ClientTesterWidget->SetServerURL(URL);
	}
}

void SMCPDashboardWidget::AddLogEntry(const FString& Level, const FString& Category, const FString& Message)
{
	if (LogViewerWidget.IsValid())
	{
		LogViewerWidget->AddLogEntry(Level, Category, Message);
	}
}

void SMCPDashboardWidget::AddNetworkActivity(const FString& Method, const FString& Endpoint, float ResponseTime)
{
	if (StatusWidget.IsValid())
	{
		StatusWidget->AddNetworkActivity(Method, Endpoint, ResponseTime);
	}

	// Also update toolbar network activity indicator
	if (ToolbarWidget.IsValid())
	{
		ToolbarWidget->UpdateNetworkActivity(true);
		
		// Reset network indicator after a short delay
		FTimerHandle TimerHandle;
		if (GEngine && GEngine->GetTimerManager())
		{
			GEngine->GetTimerManager()->SetTimer(TimerHandle, [this]()
			{
				if (ToolbarWidget.IsValid())
				{
					ToolbarWidget->UpdateNetworkActivity(false);
				}
			}, 0.5f, false);
		}
	}
}

void SMCPDashboardWidget::UpdateNetworkStats(int32 RequestsPerSecond, float LatestResponseTime)
{
	if (StatusWidget.IsValid())
	{
		StatusWidget->UpdateNetworkStats(RequestsPerSecond, LatestResponseTime);
	}
}

void SMCPDashboardWidget::SetServerURL(const FString& URL)
{
	ServerURL = URL;
	if (ClientTesterWidget.IsValid())
	{
		ClientTesterWidget->SetServerURL(URL);
	}
}

void SMCPDashboardWidget::OnToolbarAction(const FString& Action)
{
	OnDashboardAction.ExecuteIfBound(FString::Printf(TEXT("Toolbar:%s"), *Action));
}

void SMCPDashboardWidget::OnStatusAction(const FString& Action)
{
	OnDashboardAction.ExecuteIfBound(FString::Printf(TEXT("Status:%s"), *Action));
}

void SMCPDashboardWidget::OnLogAction(const FString& Action)
{
	OnDashboardAction.ExecuteIfBound(FString::Printf(TEXT("Log:%s"), *Action));
}

void SMCPDashboardWidget::OnClientAction(const FString& Action)
{
	OnDashboardAction.ExecuteIfBound(FString::Printf(TEXT("Client:%s"), *Action));
}

void SMCPDashboardWidget::RegisterTabSpawners()
{
	TabManager->RegisterTabSpawner(StatusTabId, FOnSpawnTab::CreateSP(this, &SMCPDashboardWidget::SpawnStatusTab))
		.SetDisplayName(NSLOCTEXT("MCPDashboard", "StatusTabLabel", "Server Status"))
		.SetTooltipText(NSLOCTEXT("MCPDashboard", "StatusTabTooltip", "Shows detailed server status and performance metrics"));

	TabManager->RegisterTabSpawner(LogViewerTabId, FOnSpawnTab::CreateSP(this, &SMCPDashboardWidget::SpawnLogViewerTab))
		.SetDisplayName(NSLOCTEXT("MCPDashboard", "LogViewerTabLabel", "Log Viewer"))
		.SetTooltipText(NSLOCTEXT("MCPDashboard", "LogViewerTabTooltip", "Real-time server log viewer"));

	TabManager->RegisterTabSpawner(ClientTesterTabId, FOnSpawnTab::CreateSP(this, &SMCPDashboardWidget::SpawnClientTesterTab))
		.SetDisplayName(NSLOCTEXT("MCPDashboard", "ClientTesterTabLabel", "Client Tester"))
		.SetTooltipText(NSLOCTEXT("MCPDashboard", "ClientTesterTabTooltip", "JSON-RPC client tester and debugger"));
}

TSharedRef<FTabManager::FLayout> SMCPDashboardWidget::CreateDefaultLayout()
{
	return FTabManager::NewLayout("MCPDashboardLayout_v1.0")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Horizontal)
			->Split
			(
				// Left side - Status panel
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Vertical)
				->SetSizeCoefficient(0.3f)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(1.0f)
					->AddTab(StatusTabId, ETabState::OpenedTab)
				)
			)
			->Split
			(
				// Right side - Log viewer and client tester
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Vertical)
				->SetSizeCoefficient(0.7f)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.4f)
					->AddTab(LogViewerTabId, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.6f)
					->AddTab(ClientTesterTabId, ETabState::OpenedTab)
				)
			)
		);
}

TSharedRef<SDockTab> SMCPDashboardWidget::SpawnStatusTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::PanelTab)
		.Label(NSLOCTEXT("MCPDashboard", "StatusTabLabel", "Server Status"))
		[
			SAssignNew(StatusWidget, SMCPStatusWidget)
			.OnStatusAction(this, &SMCPDashboardWidget::OnStatusAction)
		];
}

TSharedRef<SDockTab> SMCPDashboardWidget::SpawnLogViewerTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::PanelTab)
		.Label(NSLOCTEXT("MCPDashboard", "LogViewerTabLabel", "Log Viewer"))
		[
			SAssignNew(LogViewerWidget, SMCPLogViewerWidget)
			.OnLogAction(this, &SMCPDashboardWidget::OnLogAction)
		];
}

TSharedRef<SDockTab> SMCPDashboardWidget::SpawnClientTesterTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::PanelTab)
		.Label(NSLOCTEXT("MCPDashboard", "ClientTesterTabLabel", "Client Tester"))
		[
			SAssignNew(ClientTesterWidget, SMCPClientTesterWidget)
			.OnClientAction(this, &SMCPDashboardWidget::OnClientAction)
		];
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION