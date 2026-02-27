// Snapshot Games

#include "ListenForLinkServer.h"

#include <corecrt_io.h>
#include <coroutine>
#include <string>
#include <variant>

#include "GoToBPDeveloperSettings.h"
#include "IBehaviorTreeEditor.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Interfaces/IPluginManager.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Toolkits/ToolkitManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Windows/MinWindows.h"

namespace
{
	struct Blocker
	{
		std::coroutine_handle<> _h;

		bool await_ready() const noexcept
		{
			return false;
		}

		void await_suspend(std::coroutine_handle<> handle)
		{
			_h = handle;
		}

		void await_resume() const noexcept
		{
		}

		void Unblock()
		{
			if (_h.address())
			{
				_h.resume();
			}
		}
	};

	struct Coroutine
	{
		struct promise_type
		{
			Coroutine get_return_object()
			{
				return Coroutine(std::coroutine_handle<promise_type>::from_promise(*this));
			}

			std::suspend_never initial_suspend() noexcept
			{
				return std::suspend_never{};
			}

			std::suspend_never final_suspend() noexcept
			{
				return std::suspend_never{};
			}

			void unhandled_exception()
			{
			}

			void return_void()
			{
			}
		};

		std::coroutine_handle<promise_type> handle;

		Coroutine(std::coroutine_handle<promise_type> h) :
			handle(h)
		{
		}
	};

	struct CoroutineContainer
	{
		Coroutine _crt;

		CoroutineContainer(const Coroutine& crt) :
			_crt(crt)
		{
		}

		~CoroutineContainer()
		{
			_crt.handle.destroy();
		}
	};

	std::variant<std::monostate, CoroutineContainer> _crt;
	std::variant<std::monostate, Blocker> _blocker;

	Blocker& GetBlocker()
	{
		if (_blocker.index() == 0)
		{
			_blocker.emplace<Blocker>();
		}
		return std::get<Blocker>(_blocker);
	}

	UObject* FindAssetFromGraph(UObject* graph)
	{
		UPackage* package = graph->GetPackage();
		if (!package)
		{
			return nullptr;
		}

		return package->FindAssetInPackage();
	}

	bool OpenEditorForGraph(const FLinkData& linkData)
	{
		UEdGraph* graph = LoadObject<UEdGraph>(nullptr, *linkData.Graph);
		if (!graph)
		{
			return false;
		}
		
		UEdGraphNode* closest = nullptr;
		auto dist = [&linkData](const UEdGraphNode* node) -> float
		{
			if (!node)
			{
				return 99999999.0f;
			}

			auto nodePos = node->GetPosition();
			auto dist = linkData.Location - nodePos;

			return static_cast<float>(dist.Length());
		};

		for (UEdGraphNode* node : graph->Nodes)
		{
			if (!closest)
			{
				closest = node;
				continue;
			}
			if (dist(closest) > dist(node))
			{
				closest = node;
			}
		}

		UObject* asset = FindAssetFromGraph(graph);
	
		bool opened = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(asset);
		if (!opened)
		{
			return false;
		}
		TSharedPtr<IToolkit> FoundAssetEditor = FToolkitManager::Get().FindEditorForAsset(asset);
		if (!FoundAssetEditor.IsValid())
		{
			return false;
		}

		FoundAssetEditor->BringToolkitToFront();
		FName toolkitName = FoundAssetEditor->GetToolkitFName();
		
		if (FoundAssetEditor->IsBlueprintEditor())
		{
			if (!closest)
			{
				FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(graph);
			}
			else
			{
				FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(closest);
			}
		}
		else if (toolkitName.IsEqual("Behavior Tree"))
		{
			IBehaviorTreeEditor* btEditor = static_cast<IBehaviorTreeEditor*>(FoundAssetEditor.Get());
			if (closest)
			{
				btEditor->FocusAttentionOnNode(closest);
			}
		}
		
		return true;
	}
}


void UListenForLinkServer::OnMessageReceived(const FString& Message)
{
	const FString prefix = "path=";
	const FString suffix = "%";

	int index = Message.Find(prefix);
	FString payload = Message.RightChop(index + prefix.Len());

	index = payload.Find(suffix, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	payload = payload.LeftChop(payload.Len() - index);

	TArray<uint8> data;
	FBase64::Decode(payload, data, EBase64Mode::UrlSafe);

	FMemoryReader reader(data);
	FLinkData linkData;
	reader << linkData;

	bool editorOpened = OpenEditorForGraph(linkData);

	if (!editorOpened)
	{
		FNotificationInfo Info(FText::FromString("Couldn't find the graph, you are looking for..."));
		Info.bFireAndForget = true;
		Info.FadeOutDuration = 2.0f;
		Info.ExpireDuration = 3.0f;
		Info.bUseLargeFont = false;
		FSlateNotificationManager::Get().AddNotification(Info);
	}
}

void UListenForLinkServer::Init()
{
	auto crt = [this]() -> Coroutine
	{
		Blocker& blocker = GetBlocker();
		co_await blocker;

		FGuid guid = FGuid::NewGuid();
		FString guidStr = LexToString(guid);
		FString pipeName = FString::Format(TEXT("\\\\.\\pipe\\{0}"),
		{
			*guidStr,
		});
		
		FString command = FString::Format(TEXT("cmd /c echo %1 > {0}"), { pipeName });
		EnableLinks("gotobp", command);

		HANDLE hPipe = CreateNamedPipe(
				*pipeName,
				PIPE_ACCESS_INBOUND | FILE_FLAG_FIRST_PIPE_INSTANCE | FILE_FLAG_OVERLAPPED,
				PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
				1,              // max instances
				4096, 4096,     // out/in buffer sizes
				0,
				nullptr
			);

		FString strBuff;
		while (true)
		{
			{
				OVERLAPPED ov = {};
				bool connected = ConnectNamedPipe(hPipe, &ov);
				if (!connected)
				{
					while (!HasOverlappedIoCompleted(&ov))
					{
						co_await blocker;
					}
				}
			}

			while (true)
			{
				OVERLAPPED ov = {};
				char buff[513] = {};
				BOOL res = ReadFile(
					hPipe,
					buff,
					sizeof(buff) - 1,
					nullptr,
					&ov
				);

				if (res)
				{
					DWORD read = 0;
					GetOverlappedResult(hPipe, &ov, &read, TRUE);
					strBuff += buff;
					UE_LOG(LogTemp, Log, TEXT("ReadFile"));
					OnMessageReceived(strBuff);
					strBuff = "";

					DisconnectNamedPipe(hPipe);
					break;
				}

				while (!HasOverlappedIoCompleted(&ov))
				{
					co_await blocker;
				}

				DWORD read = 0;
				GetOverlappedResult(hPipe, &ov, &read, TRUE);
				strBuff += buff;
			}
		}
	};
	_crt.emplace<CoroutineContainer>(crt());

	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([this](float dt) -> bool
	{
		GetBlocker().Unblock();
		return true;
	}), 1.0f);

	StartListeningForInput();
}

void UListenForLinkServer::Deinit()
{
}

void ShowEditorNotification()
{
	FNotificationInfo Info(FText::FromString("Operation Completed!"));
	Info.bFireAndForget = true;
	Info.FadeOutDuration = 2.0f;
	Info.ExpireDuration = 3.0f;
	Info.bUseLargeFont = false;

	FSlateNotificationManager::Get().AddNotification(Info);
}

void UListenForLinkServer::StartListeningForInput()
{
	FSlateApplication* slateApp = &FSlateApplication::Get();

	KeyDownHandle = slateApp->OnApplicationPreInputKeyDownListener().AddLambda([this](const FKeyEvent& keyEvent)
	{
		FKey ActivationKey = UGoToBP::GetActivationKey();
		if (keyEvent.GetKey() == ActivationKey)
		{
			ActivatedAt = FPlatformTime::Seconds();
		}
	});

	MouseDownHandle = slateApp->OnApplicationMousePreInputButtonDownListener().AddLambda([this, slateApp](const FPointerEvent& pointerEvent)
	{
		if (FPlatformTime::Seconds() - ActivatedAt > 1.0f)
		{
			return;
		}

		if (!pointerEvent.GetEffectingButton().GetFName().IsEqual("LeftMouseButton"))
		{
			return;
		}

		TSharedPtr<FSlateUser> user = slateApp->GetUser(pointerEvent);
		FWeakWidgetPath weakWidgetsPath = user->GetLastWidgetsUnderCursor();

		if (!weakWidgetsPath.IsValid())
		{
			return;
		}

		for (int i = 0; i < weakWidgetsPath.Widgets.Num(); ++i)
		{
			TWeakPtr<SWidget> cur = weakWidgetsPath.Widgets[i];
			if (!cur.IsValid())
			{
				continue;
			}

			SWidget* widget = cur.Pin().Get();
			FName type = widget->GetType();

			if (!type.IsEqual("SGraphEditor"))
			{
				continue;
			}

			SGraphEditor* graphEditor = static_cast<SGraphEditor*>(widget);
			UEdGraph* edGraph = graphEditor->GetCurrentGraph();

			FVector2f Location = graphEditor->GetPasteLocation2f();

			FString graphPath = edGraph->GetPathName();

			FLinkData data{graphPath, Location};
			TArray<uint8> buff;
			FMemoryWriter writer(buff);

			writer << data;
			FString payload = FBase64::Encode(buff, EBase64Mode::UrlSafe);

			FString link = FString::Format(TEXT("gotobp://loc?path={0}%"), {*payload});
			UObject* asset = FindAssetFromGraph(edGraph);
			if (asset)
			{
				link = FString::Format(
					TEXT("{0} - {1}"),
					{
						asset->GetName(),
						link
					}
				);
			}
			UE_LOG(LogTemp, Display, TEXT("Location Link: %s"), *link);

			{
				FNotificationInfo Info(FText::FromString("Location Link created and placed in your clipboard!"));
				Info.bFireAndForget = true;
				Info.FadeOutDuration = 2.0f;
				Info.ExpireDuration = 3.0f;
				Info.bUseLargeFont = false;
				FSlateNotificationManager::Get().AddNotification(Info);

				FPlatformApplicationMisc::ClipboardCopy(*link);
			}
		}
	});
}

void UListenForLinkServer::StopListeningForInput() const
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication& slateApp = FSlateApplication::Get();
		slateApp.OnApplicationPreInputKeyDownListener().Remove(KeyDownHandle);
		slateApp.OnApplicationMousePreInputButtonDownListener().Remove(MouseDownHandle);
	}
}

bool UListenForLinkServer::EnableLinks(const FString& ProtocolName, const FString& Command)
{
	FString baseKeyS = TEXT("Software\\Classes\\") + ProtocolName;
	std::wstring baseKey = *baseKeyS;

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

	FString tmp = FString::Format(TEXT("URL:{0} Protocol"), {*ProtocolName});
	// Set default value
	std::wstring description = *tmp;
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
	
	std::wstring command = *Command;
	RegSetValueExW(
		hKey,
		nullptr,
		0,
		REG_SZ,
		reinterpret_cast<const BYTE*>(command.c_str()),
		(command.size() + 1) * sizeof(wchar_t));

	RegCloseKey(hKey);

	return true;
}

FString UListenForLinkServer::GetScriptPath()
{
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT(PLUGIN_NAME));
	FString PluginBaseDir = Plugin->GetBaseDir(); // Folder of the plugin
	FString AbsolutePath = FPaths::ConvertRelativePathToFull(PluginBaseDir);
	FString scriptPath = FPaths::Combine(AbsolutePath, "Scripts/MakeReqToUE.ps1");

	return scriptPath;
}
