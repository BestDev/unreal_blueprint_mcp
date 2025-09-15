#include "MCPClientTesterWidget.h"
#include "SlateOptMacros.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Views/STableRow.h"
#include "EditorStyleSet.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/FileHelper.h"
#include "Framework/Application/SlateApplication.h"
#include "DesktopPlatformModule.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SMCPClientTesterWidget::Construct(const FArguments& InArgs)
{
	OnClientAction = InArgs._OnClientAction;
	CurrentServerURL = TEXT("http://localhost:8080");

	InitializeMethodList();
	InitializeRequestTemplates();

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(4.0f)
		[
			SNew(SSplitter)
			.Orientation(Orient_Horizontal)
			// Left panel - Request configuration
			+ SSplitter::Slot()
			.Value(0.4f)
			[
				SNew(SVerticalBox)
				// Server URL
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 8.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("MCPClientTester", "ServerURL", "Server URL:"))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 2.0f)
					[
						SAssignNew(ServerURLBox, SEditableTextBox)
						.Text(FText::FromString(CurrentServerURL))
						.HintText(NSLOCTEXT("MCPClientTester", "ServerURLHint", "http://localhost:8080"))
						.OnTextChanged_Lambda([this](const FText& NewText)
						{
							CurrentServerURL = NewText.ToString();
						})
					]
				]
				
				// Method selection
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 8.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Text(NSLOCTEXT("MCPClientTester", "Method", "Method:"))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 2.0f)
						[
							SAssignNew(MethodComboBox, SComboBox<TSharedPtr<FString>>)
							.OptionsSource(&AvailableMethods)
							.OnGenerateWidget_Lambda([](TSharedPtr<FString> Option)
							{
								return SNew(STextBlock).Text(FText::FromString(*Option));
							})
							.OnSelectionChanged(this, &SMCPClientTesterWidget::OnMethodSelectionChanged)
							[
								SNew(STextBlock)
								.Text(this, &SMCPClientTesterWidget::GetCurrentMethodText)
							]
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(8.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Text(NSLOCTEXT("MCPClientTester", "RequestId", "ID:"))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 2.0f)
						[
							SAssignNew(IdTextBox, SEditableTextBox)
							.Text(FText::AsNumber(1))
							.MinDesiredWidth(60.0f)
						]
					]
				]
				
				// Template selection
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 8.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Text(NSLOCTEXT("MCPClientTester", "Template", "Template:"))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 2.0f)
						[
							SAssignNew(TemplateComboBox, SComboBox<TSharedPtr<FString>>)
							.OptionsSource(&RequestTemplates)
							.OnGenerateWidget_Lambda([](TSharedPtr<FString> Option)
							{
								return SNew(STextBlock).Text(FText::FromString(*Option));
							})
							.OnSelectionChanged(this, &SMCPClientTesterWidget::OnTemplateSelectionChanged)
							[
								SNew(STextBlock)
								.Text(this, &SMCPClientTesterWidget::GetCurrentTemplateText)
							]
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(4.0f, 16.0f, 0.0f, 0.0f)
					[
						SNew(SButton)
						.ButtonStyle(FEditorStyle::Get(), "FlatButton.Default")
						.OnClicked(this, &SMCPClientTesterWidget::OnLoadTemplateClicked)
						.ToolTipText(NSLOCTEXT("MCPClientTester", "LoadTemplate", "Load selected template"))
						[
							SNew(STextBlock)
							.Text(NSLOCTEXT("MCPClientTester", "Load", "Load"))
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
						]
					]
				]
				
				// Parameters editor
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("MCPClientTester", "Parameters", "Parameters (JSON):"))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
					]
					+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					.Padding(0.0f, 2.0f, 0.0f, 8.0f)
					[
						SAssignNew(ParamsTextBox, SMultiLineEditableTextBox)
						.Text(FText::FromString(TEXT("{\n\n}")))
						.HintText(NSLOCTEXT("MCPClientTester", "ParamsHint", "Enter JSON parameters here"))
						.IsReadOnly(false)
						.AutoWrapText(true)
					]
				]
				
				// Action buttons
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(SButton)
						.ButtonStyle(FEditorStyle::Get(), "FlatButton.Success")
						.OnClicked(this, &SMCPClientTesterWidget::OnSendRequestClicked)
						.HAlign(HAlign_Center)
						[
							SNew(STextBlock)
							.Text(NSLOCTEXT("MCPClientTester", "SendRequest", "Send Request"))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(4.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SButton)
						.ButtonStyle(FEditorStyle::Get(), "FlatButton.Default")
						.OnClicked(this, &SMCPClientTesterWidget::OnClearResponseClicked)
						[
							SNew(STextBlock)
							.Text(NSLOCTEXT("MCPClientTester", "Clear", "Clear"))
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(4.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SButton)
						.ButtonStyle(FEditorStyle::Get(), "FlatButton.Default")
						.OnClicked(this, &SMCPClientTesterWidget::OnExportHistoryClicked)
						[
							SNew(STextBlock)
							.Text(NSLOCTEXT("MCPClientTester", "Export", "Export"))
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
						]
					]
				]
			]
			
			// Right panel - Response display
			+ SSplitter::Slot()
			.Value(0.6f)
			[
				SNew(SVerticalBox)
				// Response header
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(8.0f, 0.0f, 0.0f, 4.0f)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("MCPClientTester", "Response", "Response:"))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
				]
				
				// Response display (split between raw and tree view)
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				.Padding(8.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SSplitter)
					.Orientation(Orient_Vertical)
					// Raw response
					+ SSplitter::Slot()
					.Value(0.6f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Text(NSLOCTEXT("MCPClientTester", "RawResponse", "Raw Response:"))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
						]
						+ SVerticalBox::Slot()
						.FillHeight(1.0f)
						.Padding(0.0f, 2.0f)
						[
							SAssignNew(ResponseTextBox, SMultiLineEditableTextBox)
							.IsReadOnly(true)
							.AutoWrapText(false)
							.HScrollBar(
								SNew(SScrollBar)
								.Orientation(Orient_Horizontal)
							)
							.VScrollBar(
								SNew(SScrollBar)
								.Orientation(Orient_Vertical)
							)
						]
					]
					
					// Structured response tree
					+ SSplitter::Slot()
					.Value(0.4f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Text(NSLOCTEXT("MCPClientTester", "StructuredResponse", "Structured Response:"))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
						]
						+ SVerticalBox::Slot()
						.FillHeight(1.0f)
						.Padding(0.0f, 2.0f)
						[
							SAssignNew(ResponseTreeView, STreeView<TSharedPtr<FJsonTreeNode>>)
							.TreeItemsSource(&ResponseTreeData)
							.OnGenerateRow(this, &SMCPClientTesterWidget::OnGenerateTreeRow)
							.OnGetChildren(this, &SMCPClientTesterWidget::OnGetTreeChildren)
							.SelectionMode(ESelectionMode::Single)
						]
					]
				]
			]
		]
	];
	
	// Set default selections
	if (MethodComboBox.IsValid() && AvailableMethods.Num() > 0)
	{
		MethodComboBox->SetSelectedItem(AvailableMethods[0]);
	}
	
	if (TemplateComboBox.IsValid() && RequestTemplates.Num() > 0)
	{
		TemplateComboBox->SetSelectedItem(RequestTemplates[0]);
	}
}

void SMCPClientTesterWidget::SendRequest(const FMCPRpcRequest& Request)
{
	CurrentRequest = Request;

	// Build JSON payload
	TSharedPtr<FJsonObject> RequestJson = MakeShareable(new FJsonObject);
	RequestJson->SetStringField(TEXT("jsonrpc"), Request.JsonRpcVersion);
	RequestJson->SetStringField(TEXT("method"), Request.Method);
	RequestJson->SetNumberField(TEXT("id"), Request.Id);
	
	if (Request.Params.IsValid())
	{
		RequestJson->SetObjectField(TEXT("params"), Request.Params);
	}

	FString JsonPayload;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonPayload);
	FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer);

	PerformHttpRequest(JsonPayload);
}

void SMCPClientTesterWidget::LoadRequestTemplates()
{
	InitializeRequestTemplates();
}

void SMCPClientTesterWidget::SetServerURL(const FString& URL)
{
	CurrentServerURL = URL;
	if (ServerURLBox.IsValid())
	{
		ServerURLBox->SetText(FText::FromString(URL));
	}
}

void SMCPClientTesterWidget::InitializeMethodList()
{
	AvailableMethods.Empty();
	
	// Add common MCP methods
	AvailableMethods.Add(MakeShareable(new FString(TEXT("ping"))));
	AvailableMethods.Add(MakeShareable(new FString(TEXT("resources/list"))));
	AvailableMethods.Add(MakeShareable(new FString(TEXT("resources/read"))));
	AvailableMethods.Add(MakeShareable(new FString(TEXT("tools/list"))));
	AvailableMethods.Add(MakeShareable(new FString(TEXT("tools/call"))));
	AvailableMethods.Add(MakeShareable(new FString(TEXT("prompts/list"))));
	AvailableMethods.Add(MakeShareable(new FString(TEXT("prompts/get"))));
	AvailableMethods.Add(MakeShareable(new FString(TEXT("getBlueprints"))));
	AvailableMethods.Add(MakeShareable(new FString(TEXT("getActors"))));
	AvailableMethods.Add(MakeShareable(new FString(TEXT("executeBlueprint"))));
}

void SMCPClientTesterWidget::InitializeRequestTemplates()
{
	RequestTemplates.Empty();
	MethodTemplates.Empty();
	
	RequestTemplates.Add(MakeShareable(new FString(TEXT("Empty"))));
	RequestTemplates.Add(MakeShareable(new FString(TEXT("Ping"))));
	RequestTemplates.Add(MakeShareable(new FString(TEXT("Resources List"))));
	RequestTemplates.Add(MakeShareable(new FString(TEXT("Tools List"))));
	RequestTemplates.Add(MakeShareable(new FString(TEXT("Get Blueprints"))));
	RequestTemplates.Add(MakeShareable(new FString(TEXT("Get Actors"))));
	
	// Define templates
	MethodTemplates.Add(TEXT("Empty"), TEXT("{\n\n}"));
	MethodTemplates.Add(TEXT("Ping"), TEXT("{}"));
	MethodTemplates.Add(TEXT("Resources List"), TEXT("{}"));
	MethodTemplates.Add(TEXT("Tools List"), TEXT("{}"));
	MethodTemplates.Add(TEXT("Get Blueprints"), TEXT("{\n  \"filter\": {\n    \"type\": \"Blueprint\"\n  }\n}"));
	MethodTemplates.Add(TEXT("Get Actors"), TEXT("{\n  \"filter\": {\n    \"class\": \"AActor\"\n  }\n}"));
}

FReply SMCPClientTesterWidget::OnSendRequestClicked()
{
	// Get current values
	FString Method = TEXT("ping");
	if (MethodComboBox.IsValid() && MethodComboBox->GetSelectedItem().IsValid())
	{
		Method = *MethodComboBox->GetSelectedItem();
	}
	
	int32 RequestId = 1;
	if (IdTextBox.IsValid())
	{
		RequestId = FCString::Atoi(*IdTextBox->GetText().ToString());
	}
	
	FString ParamsString = TEXT("{}");
	if (ParamsTextBox.IsValid())
	{
		ParamsString = ParamsTextBox->GetText().ToString();
	}
	
	// Parse parameters JSON
	TSharedPtr<FJsonObject> ParamsJson;
	if (!ParamsString.IsEmpty() && IsValidJson(ParamsString))
	{
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ParamsString);
		FJsonSerializer::Deserialize(Reader, ParamsJson);
	}
	
	// Create and send request
	FMCPRpcRequest Request;
	Request.Method = Method;
	Request.Id = RequestId;
	Request.Params = ParamsJson;
	
	SendRequest(Request);
	
	return FReply::Handled();
}

FReply SMCPClientTesterWidget::OnClearResponseClicked()
{
	if (ResponseTextBox.IsValid())
	{
		ResponseTextBox->SetText(FText::GetEmpty());
	}
	
	ResponseTreeData.Empty();
	if (ResponseTreeView.IsValid())
	{
		ResponseTreeView->RequestTreeRefresh();
	}
	
	return FReply::Handled();
}

FReply SMCPClientTesterWidget::OnLoadTemplateClicked()
{
	if (TemplateComboBox.IsValid() && TemplateComboBox->GetSelectedItem().IsValid())
	{
		FString SelectedTemplate = *TemplateComboBox->GetSelectedItem();
		FString TemplateContent = GetMethodTemplate(SelectedTemplate);
		
		if (ParamsTextBox.IsValid())
		{
			ParamsTextBox->SetText(FText::FromString(TemplateContent));
		}
	}
	
	return FReply::Handled();
}

FReply SMCPClientTesterWidget::OnSaveTemplateClicked()
{
	// TODO: Implement template saving functionality
	OnClientAction.ExecuteIfBound(TEXT("SaveTemplate"));
	return FReply::Handled();
}

FReply SMCPClientTesterWidget::OnExportHistoryClicked()
{
	// Open file dialog to export request/response history
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		TArray<FString> OutFileNames;
		FString DefaultFileName = FString::Printf(TEXT("MCP_RequestHistory_%s.json"), 
			*FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
		
		bool bResult = DesktopPlatform->SaveFileDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			TEXT("Export Request History"),
			FPaths::ProjectSavedDir(),
			DefaultFileName,
			TEXT("JSON files (*.json)|*.json"),
			EFileDialogFlags::None,
			OutFileNames
		);
		
		if (bResult && OutFileNames.Num() > 0)
		{
			// Export history
			TArray<FString> HistoryLines;
			HistoryLines.Add(TEXT("{"));
			HistoryLines.Add(TEXT("  \"export_timestamp\": \"") + FDateTime::Now().ToString() + TEXT("\","));
			HistoryLines.Add(TEXT("  \"requests\": ["));
			
			for (int32 i = 0; i < RequestHistory.Num(); ++i)
			{
				const FMCPRpcRequest& Req = RequestHistory[i];
				HistoryLines.Add(FString::Printf(TEXT("    {\"method\": \"%s\", \"id\": %d}%s"), 
					*Req.Method, Req.Id, (i < RequestHistory.Num() - 1) ? TEXT(",") : TEXT("")));
			}
			
			HistoryLines.Add(TEXT("  ]"));
			HistoryLines.Add(TEXT("}"));
			
			FFileHelper::SaveStringArrayToFile(HistoryLines, *OutFileNames[0]);
			OnClientAction.ExecuteIfBound(FString::Printf(TEXT("ExportHistory:%s"), *OutFileNames[0]));
		}
	}
	
	return FReply::Handled();
}

void SMCPClientTesterWidget::OnMethodSelectionChanged(TSharedPtr<FString> SelectedMethod, ESelectInfo::Type SelectInfo)
{
	// Method selection changed - could load appropriate template
}

void SMCPClientTesterWidget::OnTemplateSelectionChanged(TSharedPtr<FString> SelectedTemplate, ESelectInfo::Type SelectInfo)
{
	// Template selection changed
}

TSharedRef<ITableRow> SMCPClientTesterWidget::OnGenerateTreeRow(TSharedPtr<FJsonTreeNode> Node, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FJsonTreeNode>>, OwnerTable)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Node->Key))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT(":")))
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.Padding(2.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Node->Value))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
			.ColorAndOpacity_Lambda([Node]()
			{
				if (Node->Type == TEXT("string"))
					return FSlateColor(FLinearColor::Green);
				else if (Node->Type == TEXT("number"))
					return FSlateColor(FLinearColor::Cyan);
				else if (Node->Type == TEXT("boolean"))
					return FSlateColor(FLinearColor::Yellow);
				else
					return FSlateColor::UseForeground();
			})
		]
	];
}

void SMCPClientTesterWidget::OnGetTreeChildren(TSharedPtr<FJsonTreeNode> Node, TArray<TSharedPtr<FJsonTreeNode>>& OutChildren)
{
	if (Node.IsValid())
	{
		OutChildren = Node->Children;
	}
}

void SMCPClientTesterWidget::BuildJsonTree(TSharedPtr<FJsonObject> JsonObject, TArray<TSharedPtr<FJsonTreeNode>>& OutNodes, const FString& ParentKey)
{
	if (!JsonObject.IsValid())
	{
		return;
	}
	
	for (const auto& Pair : JsonObject->Values)
	{
		TSharedPtr<FJsonTreeNode> Node = MakeShareable(new FJsonTreeNode);
		Node->Key = Pair.Key;
		
		if (Pair.Value->Type == EJson::String)
		{
			Node->Value = Pair.Value->AsString();
			Node->Type = TEXT("string");
		}
		else if (Pair.Value->Type == EJson::Number)
		{
			Node->Value = FString::SanitizeFloat(Pair.Value->AsNumber());
			Node->Type = TEXT("number");
		}
		else if (Pair.Value->Type == EJson::Boolean)
		{
			Node->Value = Pair.Value->AsBool() ? TEXT("true") : TEXT("false");
			Node->Type = TEXT("boolean");
		}
		else if (Pair.Value->Type == EJson::Object)
		{
			Node->Value = TEXT("{ object }");
			Node->Type = TEXT("object");
			BuildJsonTree(Pair.Value->AsObject(), Node->Children, Pair.Key);
		}
		else if (Pair.Value->Type == EJson::Array)
		{
			Node->Value = FString::Printf(TEXT("[ array (%d items) ]"), Pair.Value->AsArray().Num());
			Node->Type = TEXT("array");
		}
		else
		{
			Node->Value = TEXT("null");
			Node->Type = TEXT("null");
		}
		
		OutNodes.Add(Node);
	}
}

void SMCPClientTesterWidget::PerformHttpRequest(const FString& JsonPayload)
{
	FHttpRequestRef Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(CurrentServerURL);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetContentAsString(JsonPayload);
	
	float StartTime = FPlatformTime::Seconds();
	
	Request->OnProcessRequestComplete().BindLambda([this, StartTime](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess)
	{
		float ResponseTime = (FPlatformTime::Seconds() - StartTime) * 1000.0f; // Convert to milliseconds
		
		FString ResponseString = bSuccess && Response.IsValid() ? Response->GetContentAsString() : TEXT("Request failed");
		OnHttpRequestComplete(bSuccess, ResponseString, ResponseTime);
	});
	
	Request->ProcessRequest();
}

void SMCPClientTesterWidget::OnHttpRequestComplete(bool bSuccess, const FString& ResponseString, float ResponseTime)
{
	// Update response text box
	if (ResponseTextBox.IsValid())
	{
		FString FormattedResponse = FormatJson(ResponseString);
		ResponseTextBox->SetText(FText::FromString(FormattedResponse));
	}
	
	// Parse and build tree view
	if (bSuccess && IsValidJson(ResponseString))
	{
		TSharedPtr<FJsonObject> ResponseJson;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseString);
		
		if (FJsonSerializer::Deserialize(Reader, ResponseJson))
		{
			ResponseTreeData.Empty();
			BuildJsonTree(ResponseJson, ResponseTreeData);
			
			if (ResponseTreeView.IsValid())
			{
				ResponseTreeView->RequestTreeRefresh();
			}
		}
	}
	
	// Update history
	LastResponse.bSuccess = bSuccess;
	LastResponse.ResponseTime = ResponseTime;
	LastResponse.RawResponse = ResponseString;
	ResponseHistory.Add(LastResponse);
	
	// Notify action
	OnClientAction.ExecuteIfBound(FString::Printf(TEXT("RequestComplete:%.2fms"), ResponseTime));
}

FString SMCPClientTesterWidget::GetMethodTemplate(const FString& Method) const
{
	if (const FString* Template = MethodTemplates.Find(Method))
	{
		return *Template;
	}
	return TEXT("{\n\n}");
}

bool SMCPClientTesterWidget::IsValidJson(const FString& JsonString) const
{
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	return FJsonSerializer::Deserialize(Reader, JsonObject);
}

FString SMCPClientTesterWidget::FormatJson(const FString& JsonString) const
{
	if (!IsValidJson(JsonString))
	{
		return JsonString;
	}
	
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	
	if (FJsonSerializer::Deserialize(Reader, JsonObject))
	{
		FString FormattedString;
		TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer = 
			TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&FormattedString);
		FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
		return FormattedString;
	}
	
	return JsonString;
}

FText SMCPClientTesterWidget::GetCurrentMethodText() const
{
	if (MethodComboBox.IsValid() && MethodComboBox->GetSelectedItem().IsValid())
	{
		return FText::FromString(*MethodComboBox->GetSelectedItem());
	}
	return FText::FromString(TEXT("ping"));
}

FText SMCPClientTesterWidget::GetCurrentTemplateText() const
{
	if (TemplateComboBox.IsValid() && TemplateComboBox->GetSelectedItem().IsValid())
	{
		return FText::FromString(*TemplateComboBox->GetSelectedItem());
	}
	return FText::FromString(TEXT("Empty"));
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION