#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Views/STreeView.h"
#include "Widgets/Input/SComboBox.h"
#include "Dom/JsonObject.h"
#include "Engine/EngineTypes.h"

/** JSON-RPC request data structure */
USTRUCT()
struct FMCPRpcRequest
{
	GENERATED_BODY()

	FString Method;
	TSharedPtr<FJsonObject> Params;
	FString JsonRpcVersion = TEXT("2.0");
	int32 Id = 1;

	FMCPRpcRequest() = default;

	FMCPRpcRequest(const FString& InMethod)
		: Method(InMethod)
		, Params(MakeShareable(new FJsonObject))
	{
	}
};

/** JSON-RPC response data structure */
USTRUCT()
struct FMCPRpcResponse
{
	GENERATED_BODY()

	bool bSuccess = false;
	TSharedPtr<FJsonObject> Result;
	TSharedPtr<FJsonObject> Error;
	int32 Id = 0;
	FString RawResponse;
	float ResponseTime = 0.0f;

	FMCPRpcResponse() = default;
};

/** Tree node for JSON data display */
struct FJsonTreeNode
{
	FString Key;
	FString Value;
	FString Type;
	TArray<TSharedPtr<FJsonTreeNode>> Children;
	bool bExpanded = false;

	FJsonTreeNode(const FString& InKey = TEXT(""), const FString& InValue = TEXT(""), const FString& InType = TEXT(""))
		: Key(InKey), Value(InValue), Type(InType)
	{
	}
};

DECLARE_DELEGATE_OneParam(FOnMCPClientAction, const FString&);

/**
 * MCP client tester widget for debugging JSON-RPC calls
 */
class UNREALBLUEPRINTMCP_API SMCPClientTesterWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMCPClientTesterWidget) {}
		SLATE_EVENT(FOnMCPClientAction, OnClientAction)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Send JSON-RPC request */
	void SendRequest(const FMCPRpcRequest& Request);

	/** Load predefined request templates */
	void LoadRequestTemplates();

	/** Set server URL */
	void SetServerURL(const FString& URL);

private:
	/** Client action event */
	FOnMCPClientAction OnClientAction;

	/** UI Elements */
	TSharedPtr<SEditableTextBox> ServerURLBox;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> MethodComboBox;
	TSharedPtr<SMultiLineEditableTextBox> ParamsTextBox;
	TSharedPtr<SMultiLineEditableTextBox> ResponseTextBox;
	TSharedPtr<STreeView<TSharedPtr<FJsonTreeNode>>> ResponseTreeView;
	TSharedPtr<SEditableTextBox> IdTextBox;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> TemplateComboBox;

	/** Data */
	TArray<TSharedPtr<FString>> AvailableMethods;
	TArray<TSharedPtr<FString>> RequestTemplates;
	TArray<TSharedPtr<FJsonTreeNode>> ResponseTreeData;
	FString CurrentServerURL;
	TMap<FString, FString> MethodTemplates;

	/** Request history */
	TArray<FMCPRpcRequest> RequestHistory;
	TArray<FMCPRpcResponse> ResponseHistory;

	/** Current request/response */
	FMCPRpcRequest CurrentRequest;
	FMCPRpcResponse LastResponse;

	/** Initialize method list */
	void InitializeMethodList();

	/** Initialize request templates */
	void InitializeRequestTemplates();

	/** Send button clicked */
	FReply OnSendRequestClicked();

	/** Clear response button clicked */
	FReply OnClearResponseClicked();

	/** Load template button clicked */
	FReply OnLoadTemplateClicked();

	/** Save template button clicked */
	FReply OnSaveTemplateClicked();

	/** Export history button clicked */
	FReply OnExportHistoryClicked();

	/** Method selection changed */
	void OnMethodSelectionChanged(TSharedPtr<FString> SelectedMethod, ESelectInfo::Type SelectInfo);

	/** Template selection changed */
	void OnTemplateSelectionChanged(TSharedPtr<FString> SelectedTemplate, ESelectInfo::Type SelectInfo);

	/** Generate tree row for JSON display */
	TSharedRef<ITableRow> OnGenerateTreeRow(TSharedPtr<FJsonTreeNode> Node, const TSharedRef<STableViewBase>& OwnerTable);

	/** Get children for tree node */
	void OnGetTreeChildren(TSharedPtr<FJsonTreeNode> Node, TArray<TSharedPtr<FJsonTreeNode>>& OutChildren);

	/** Build JSON tree from response */
	void BuildJsonTree(TSharedPtr<FJsonObject> JsonObject, TArray<TSharedPtr<FJsonTreeNode>>& OutNodes, const FString& ParentKey = TEXT(""));

	/** Perform HTTP request */
	void PerformHttpRequest(const FString& JsonPayload);

	/** Handle HTTP response */
	void OnHttpRequestComplete(bool bSuccess, const FString& ResponseString, float ResponseTime);

	/** Get method template */
	FString GetMethodTemplate(const FString& Method) const;

	/** Validate JSON string */
	bool IsValidJson(const FString& JsonString) const;

	/** Format JSON string */
	FString FormatJson(const FString& JsonString) const;

	/** Get current method text */
	FText GetCurrentMethodText() const;

	/** Get current template text */
	FText GetCurrentTemplateText() const;
};