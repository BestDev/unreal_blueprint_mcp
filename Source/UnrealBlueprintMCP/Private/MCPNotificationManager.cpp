#include "MCPNotificationManager.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "EditorStyleSet.h"
#include "Engine/Engine.h"

TUniquePtr<FMCPNotificationManager> FMCPNotificationManager::Instance = nullptr;

FMCPNotificationManager& FMCPNotificationManager::Get()
{
	if (!Instance.IsValid())
	{
		Instance = TUniquePtr<FMCPNotificationManager>(new FMCPNotificationManager());
	}
	return *Instance;
}

void FMCPNotificationManager::ShowNotification(const FMCPNotificationData& NotificationData)
{
	FNotificationInfo Info = CreateNotificationInfo(NotificationData);
	
	TSharedPtr<SNotificationItem> NotificationItem = FSlateNotificationManager::Get().AddNotification(Info);
	
	if (NotificationItem.IsValid())
	{
		if (NotificationData.bShowProgressBar)
		{
			NotificationItem->SetCompletionState(SNotificationItem::CS_Pending);
			ActiveProgressNotifications.Add(NotificationItem);
		}
		else
		{
			NotificationItem->SetCompletionState(SNotificationItem::CS_None);
		}
	}
}

void FMCPNotificationManager::ShowNotification(const FText& Message, EMCPNotificationType Type, float Duration)
{
	FMCPNotificationData Data;
	Data.Message = Message;
	Data.Type = Type;
	Data.Duration = Duration;
	
	ShowNotification(Data);
}

TSharedPtr<SNotificationItem> FMCPNotificationManager::ShowProgressNotification(const FText& Title, const FText& Message, bool bCanCancel)
{
	FMCPNotificationData Data;
	Data.Title = Title;
	Data.Message = Message;
	Data.Type = EMCPNotificationType::Progress;
	Data.bShowProgressBar = true;
	Data.bCanBeCanceled = bCanCancel;
	Data.Duration = 0.0f; // Don't auto-expire
	
	FNotificationInfo Info = CreateNotificationInfo(Data);
	TSharedPtr<SNotificationItem> NotificationItem = FSlateNotificationManager::Get().AddNotification(Info);
	
	if (NotificationItem.IsValid())
	{
		NotificationItem->SetCompletionState(SNotificationItem::CS_Pending);
		ActiveProgressNotifications.Add(NotificationItem);
	}
	
	return NotificationItem;
}

void FMCPNotificationManager::UpdateProgressNotification(TSharedPtr<SNotificationItem> Notification, float Progress, const FText& Message)
{
	if (!Notification.IsValid())
	{
		return;
	}
	
	// Update progress (0.0 to 1.0)
	Progress = FMath::Clamp(Progress, 0.0f, 1.0f);
	
	// Update message if provided
	if (!Message.IsEmpty())
	{
		Notification->SetText(Message);
	}
	
	// Update completion state based on progress
	if (Progress >= 1.0f)
	{
		Notification->SetCompletionState(SNotificationItem::CS_Success);
	}
	else
	{
		Notification->SetCompletionState(SNotificationItem::CS_Pending);
	}
}

void FMCPNotificationManager::CompleteProgressNotification(TSharedPtr<SNotificationItem> Notification, const FText& CompletionMessage, bool bSuccess)
{
	if (!Notification.IsValid())
	{
		return;
	}
	
	// Update final message
	if (!CompletionMessage.IsEmpty())
	{
		Notification->SetText(CompletionMessage);
	}
	
	// Set completion state
	SNotificationItem::ECompletionState CompletionState = bSuccess ? 
		SNotificationItem::CS_Success : 
		SNotificationItem::CS_Fail;
	
	Notification->SetCompletionState(CompletionState);
	
	// Remove from active progress notifications
	ActiveProgressNotifications.RemoveAll([Notification](const TWeakPtr<SNotificationItem>& WeakNotification)
	{
		return WeakNotification.Pin() == Notification;
	});
	
	// Auto-expire after completion
	Notification->ExpireAndFadeout();
}

void FMCPNotificationManager::DismissNotification(TSharedPtr<SNotificationItem> Notification)
{
	if (Notification.IsValid())
	{
		Notification->SetCompletionState(SNotificationItem::CS_None);
		Notification->ExpireAndFadeout();
		
		// Remove from active notifications
		ActiveProgressNotifications.RemoveAll([Notification](const TWeakPtr<SNotificationItem>& WeakNotification)
		{
			return WeakNotification.Pin() == Notification;
		});
	}
}

void FMCPNotificationManager::ClearAllNotifications()
{
	// Clear all active progress notifications
	for (auto& WeakNotification : ActiveProgressNotifications)
	{
		if (TSharedPtr<SNotificationItem> Notification = WeakNotification.Pin())
		{
			Notification->SetCompletionState(SNotificationItem::CS_None);
			Notification->ExpireAndFadeout();
		}
	}
	
	ActiveProgressNotifications.Empty();
}

void FMCPNotificationManager::ShowServerStartNotification(int32 Port)
{
	FText Message = FText::FromString(FString::Printf(TEXT("MCP Server started successfully on port %d"), Port));
	ShowNotification(Message, EMCPNotificationType::Success, 3.0f);
}

void FMCPNotificationManager::ShowServerStopNotification()
{
	FText Message = NSLOCTEXT("MCPNotifications", "ServerStopped", "MCP Server stopped");
	ShowNotification(Message, EMCPNotificationType::Info, 2.0f);
}

void FMCPNotificationManager::ShowServerErrorNotification(const FString& ErrorMessage)
{
	FText Message = FText::FromString(FString::Printf(TEXT("MCP Server Error: %s"), *ErrorMessage));
	ShowNotification(Message, EMCPNotificationType::Error, 5.0f);
}

void FMCPNotificationManager::ShowServerRestartNotification()
{
	FText Message = NSLOCTEXT("MCPNotifications", "ServerRestarted", "MCP Server restarted successfully");
	ShowNotification(Message, EMCPNotificationType::Success, 3.0f);
}

void FMCPNotificationManager::ShowClientConnectedNotification(const FString& ClientInfo)
{
	FText Message = FText::FromString(FString::Printf(TEXT("Client connected: %s"), *ClientInfo));
	ShowNotification(Message, EMCPNotificationType::Info, 2.0f);
}

void FMCPNotificationManager::ShowClientDisconnectedNotification(const FString& ClientInfo)
{
	FText Message = FText::FromString(FString::Printf(TEXT("Client disconnected: %s"), *ClientInfo));
	ShowNotification(Message, EMCPNotificationType::Info, 2.0f);
}

void FMCPNotificationManager::ShowRequestNotification(const FString& Method, const FString& Endpoint)
{
	FText Message = FText::FromString(FString::Printf(TEXT("Processing %s request to %s"), *Method, *Endpoint));
	ShowNotification(Message, EMCPNotificationType::Info, 1.0f);
}

void FMCPNotificationManager::ShowResponseNotification(const FString& Method, float ResponseTime, bool bSuccess)
{
	EMCPNotificationType Type = bSuccess ? EMCPNotificationType::Success : EMCPNotificationType::Warning;
	FString StatusText = bSuccess ? TEXT("completed") : TEXT("failed");
	
	FText Message = FText::FromString(FString::Printf(TEXT("%s %s (%.2fms)"), 
		*Method, *StatusText, ResponseTime));
	ShowNotification(Message, Type, 2.0f);
}

void FMCPNotificationManager::ShowSettingsChangedNotification()
{
	FText Message = NSLOCTEXT("MCPNotifications", "SettingsChanged", "MCP Server settings updated");
	ShowNotification(Message, EMCPNotificationType::Info, 2.0f);
}

void FMCPNotificationManager::ShowSettingsAppliedNotification()
{
	FText Message = NSLOCTEXT("MCPNotifications", "SettingsApplied", "MCP Server settings applied successfully");
	ShowNotification(Message, EMCPNotificationType::Success, 2.0f);
}

void FMCPNotificationManager::ShowSettingsErrorNotification(const FString& ErrorMessage)
{
	FText Message = FText::FromString(FString::Printf(TEXT("Settings Error: %s"), *ErrorMessage));
	ShowNotification(Message, EMCPNotificationType::Error, 4.0f);
}

void FMCPNotificationManager::SetNotificationActionDelegate(const FOnMCPNotificationAction& InDelegate)
{
	OnNotificationAction = InDelegate;
}

const FSlateBrush* FMCPNotificationManager::GetNotificationIcon(EMCPNotificationType Type) const
{
	switch (Type)
	{
		case EMCPNotificationType::Success:
			return FEditorStyle::GetBrush("NotificationList.SuccessImage");
		case EMCPNotificationType::Warning:
			return FEditorStyle::GetBrush("NotificationList.WarningImage");
		case EMCPNotificationType::Error:
			return FEditorStyle::GetBrush("NotificationList.FailImage");
		case EMCPNotificationType::Progress:
			return FEditorStyle::GetBrush("NotificationList.DefaultMessage");
		case EMCPNotificationType::Info:
		default:
			return FEditorStyle::GetBrush("NotificationList.DefaultMessage");
	}
}

FSlateColor FMCPNotificationManager::GetNotificationColor(EMCPNotificationType Type) const
{
	switch (Type)
	{
		case EMCPNotificationType::Success:
			return FSlateColor(FLinearColor::Green);
		case EMCPNotificationType::Warning:
			return FSlateColor(FLinearColor::Yellow);
		case EMCPNotificationType::Error:
			return FSlateColor(FLinearColor::Red);
		case EMCPNotificationType::Progress:
			return FSlateColor(FLinearColor::Blue);
		case EMCPNotificationType::Info:
		default:
			return FSlateColor::UseForeground();
	}
}

FNotificationInfo FMCPNotificationManager::CreateNotificationInfo(const FMCPNotificationData& Data) const
{
	FNotificationInfo Info(Data.Message);
	
	// Set basic properties
	Info.ExpireDuration = Data.Duration;
	Info.bUseLargeFont = false;
	Info.bFireAndForget = !Data.bShowProgressBar;
	Info.bUseThrobber = Data.bShowProgressBar;
	
	// Set icon and color based on type
	Info.Image = GetNotificationIcon(Data.Type);
	
	// Add title if provided
	if (!Data.Title.IsEmpty())
	{
		// For notifications with titles, we combine them
		FText CombinedText = FText::FromString(FString::Printf(TEXT("%s: %s"), 
			*Data.Title.ToString(), *Data.Message.ToString()));
		Info.Text = CombinedText;
	}
	else
	{
		Info.Text = Data.Message;
	}
	
	// Add action buttons if needed
	if (Data.bCanBeCanceled)
	{
		Info.ButtonDetails.Add(FNotificationButtonInfo(
			NSLOCTEXT("MCPNotifications", "Cancel", "Cancel"),
			NSLOCTEXT("MCPNotifications", "CancelTooltip", "Cancel this operation"),
			FSimpleDelegate::CreateLambda([this, ActionId = Data.ActionId]()
			{
				HandleNotificationAction(ActionId);
			})
		));
	}
	
	return Info;
}

void FMCPNotificationManager::HandleNotificationAction(const FString& ActionId)
{
	OnNotificationAction.ExecuteIfBound(ActionId);
}