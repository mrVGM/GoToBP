// Copyright Epic Games, Inc. All Rights Reserved.

#include "GoToBP.h"

#include <string>

#include "GraphEditorModule.h"
#include "ListenForLinkServer.h"
#include "Interfaces/IPluginManager.h"
#include "Windows/MinWindows.h"

#define LOCTEXT_NAMESPACE "FGoToBPModule"


bool FGoToBPModule::RegisterURLHandler()
{
#if false
	std::wstring ProtocolName = L"gotobp";
	std::wstring baseKey = L"Software\\Classes\\" + ProtocolName;

	HKEY hKey;
	if (RegCreateKeyExW(
		HKEY_CURRENT_USER,
		baseKey.c_str(),
		0, nullptr,
		REG_OPTION_NON_VOLATILE,
		KEY_WRITE,
		nullptr,
		&hKey,
		nullptr) != ERROR_SUCCESS)
	{
		return false;
	}

	// Set default value
	std::wstring description = L"URL:" + ProtocolName + L" Protocol";
	RegSetValueExW(
		hKey,
		nullptr,
		0,
		REG_SZ,
		reinterpret_cast<const BYTE*>(description.c_str()),
		(description.size() + 1) * sizeof(wchar_t));

	// Required empty value
	const wchar_t* empty = L"";
	RegSetValueExW(
		hKey,
		L"URL Protocol",
		0,
		REG_SZ,
		reinterpret_cast<const BYTE*>(empty),
		sizeof(wchar_t));

	RegCloseKey(hKey);

	// Create command subkey
	std::wstring commandKey =
		baseKey + L"\\shell\\open\\command";

	if (RegCreateKeyExW(
		HKEY_CURRENT_USER,
		commandKey.c_str(),
		0, nullptr,
		REG_OPTION_NON_VOLATILE,
		KEY_WRITE,
		nullptr,
		&hKey,
		nullptr) != ERROR_SUCCESS)
	{
		return false;
	}

	std::wstring command =
		L"\"" + ApplicationPath + L"\" \"%1\"";

	RegSetValueExW(
		hKey,
		nullptr,
		0,
		REG_SZ,
		reinterpret_cast<const BYTE*>(command.c_str()),
		(command.size() + 1) * sizeof(wchar_t));

	RegCloseKey(hKey);

#endif
	
	return true;
}

void FGoToBPModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

	Server = NewObject<UListenForLinkServer>();
	Server->AddToRoot();
	Server->Init();

#if false	

	FGraphEditorModule& graphEditor = FModuleManager::LoadModuleChecked<FGraphEditorModule>("GraphEditor");
	graphEditor.GetAllGraphEditorContextMenuExtender()
		.Add(FGraphEditorModule::FGraphEditorMenuExtender_SelectedNode::CreateLambda(
			
	[](
		const TSharedRef<FUICommandList> CommandList,
		const UEdGraph* Graph,
		const UEdGraphNode* Node,
		const UEdGraphPin* Pin,
		bool bT
	) -> TSharedRef<FExtender>
	{
		TSharedRef<FExtender> extender = MakeShared<FExtender>();
		extender->AddMenuExtension(
			"GetLink",
			EExtensionHook::After,
			CommandList,
			FMenuExtensionDelegate::CreateLambda([](FMenuBuilder& MenuBuilder)
			{
				MenuBuilder.AddMenuEntry(
					FText::FromString("Get Link"),
					FText::FromString("Get Link to this Graph"),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateLambda([]()
					{
						
					}))
				);
			})
		);
		return extender;
	}));
#endif
}

void FGoToBPModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	Server->Deinit();
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FGoToBPModule, GoToBP)