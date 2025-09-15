#pragma once

#include "CoreMinimal.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Engine/EngineTypes.h"

/** Notification types for different MCP events */
UENUM(BlueprintType)
enum class EMCPNotificationType : uint8
{
	Info,
	Success,
	Warning,
	Error,
	Progress
};

/** Enhanced notification data */
USTRUCT()
struct FMCPNotificationData
{
	GENERATED_BODY()

	FText Title;
	FText Message;
	EMCPNotificationType Type = EMCPNotificationType::Info;
	float Duration = 3.0f;
	bool bShowProgressBar = false;
	float Progress = 0.0f;
	bool bCanBeCanceled = false;
	FString ActionId;

	FMCPNotificationData() = default;

	FMCPNotificationData(const FText& InTitle, const FText& InMessage, EMCPNotificationType InType = EMCPNotificationType::Info)
		: Title(InTitle), Message(InMessage), Type(InType)
	{
	}
};

DECLARE_DELEGATE_OneParam(FOnMCPNotificationAction, const FString&);

/**
 * Enhanced notification manager for MCP plugin with progress support and custom styling
 */
class UNREALBLUEPRINTMCP_API FMCPNotificationManager
{
public:
	/** Get singleton instance */
	static FMCPNotificationManager& Get();

	/** Show notification */
	void ShowNotification(const FMCPNotificationData& NotificationData);

	/** Show simple text notification */
	void ShowNotification(const FText& Message, EMCPNotificationType Type = EMCPNotificationType::Info, float Duration = 3.0f);

	/** Show progress notification */
	TSharedPtr<SNotificationItem> ShowProgressNotification(const FText& Title, const FText& Message, bool bCanCancel = false);

	/** Update progress notification */
	void UpdateProgressNotification(TSharedPtr<SNotificationItem> Notification, float Progress, const FText& Message = FText::GetEmpty());

	/** Complete progress notification */
	void CompleteProgressNotification(TSharedPtr<SNotificationItem> Notification, const FText& CompletionMessage, bool bSuccess = true);

	/** Dismiss notification */
	void DismissNotification(TSharedPtr<SNotificationItem> Notification);

	/** Clear all notifications */
	void ClearAllNotifications();

	/** Server status notifications */
	void ShowServerStartNotification(int32 Port);
	void ShowServerStopNotification();
	void ShowServerErrorNotification(const FString& ErrorMessage);
	void ShowServerRestartNotification();

	/** Client connection notifications */
	void ShowClientConnectedNotification(const FString& ClientInfo);
	void ShowClientDisconnectedNotification(const FString& ClientInfo);

	/** Request/Response notifications */
	void ShowRequestNotification(const FString& Method, const FString& Endpoint);
	void ShowResponseNotification(const FString& Method, float ResponseTime, bool bSuccess);

	/** Settings notifications */
	void ShowSettingsChangedNotification();
	void ShowSettingsAppliedNotification();
	void ShowSettingsErrorNotification(const FString& ErrorMessage);

	/** Set notification action delegate */
	void SetNotificationActionDelegate(const FOnMCPNotificationAction& InDelegate);

private:
	/** Singleton instance */
	static TUniquePtr<FMCPNotificationManager> Instance;

	/** Notification action delegate */
	FOnMCPNotificationAction OnNotificationAction;

	/** Active progress notifications */
	TArray<TSharedPtr<SNotificationItem>> ActiveProgressNotifications;

	/** Get notification icon for type */
	const FSlateBrush* GetNotificationIcon(EMCPNotificationType Type) const;

	/** Get notification color for type */
	FSlateColor GetNotificationColor(EMCPNotificationType Type) const;

	/** Create notification info */
	FNotificationInfo CreateNotificationInfo(const FMCPNotificationData& Data) const;

	/** Handle notification action */
	void HandleNotificationAction(const FString& ActionId);

	/** Constructor */
	FMCPNotificationManager() = default;
};