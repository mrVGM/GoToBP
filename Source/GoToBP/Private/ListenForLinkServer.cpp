// Snapshot Games

#include "ListenForLinkServer.h"

#include <coroutine>
#include <string>
#include <variant>

#include "EditorLevelUtils.h"
#include "GoToBPDeveloperSettings.h"
#include "IBehaviorTreeEditor.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Toolkits/ToolkitManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Windows/WindowsHWrapper.h"

namespace
{
	struct FLinkData
	{
		FString Graph;
		FVector2f Location;
	
		friend FArchive& operator<<(FArchive& Ar, FLinkData& Msg)
		{
			Ar << Msg.Graph;
			Ar << Msg.Location;
			return Ar;
		}
	};
	
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

namespace
{
	void FindClosestNodeInGraph(const FString& GraphName, FVector2f Location, UEdGraph*& OutGraph, UEdGraphNode*& OutClosest)
	{
		OutGraph = nullptr;
		OutClosest = nullptr;
		
		OutGraph = LoadObject<UEdGraph>(nullptr, *GraphName);
		if (!OutGraph)
		{
			return;
		}
		
		auto dist = [&](const UEdGraphNode* node) -> float
		{
			if (!node)
			{
				return 99999999.0f;
			}

			auto nodePos = node->GetPosition();
			auto dist = Location - nodePos;

			return dist.Length();
		};

		for (UEdGraphNode* node : OutGraph->Nodes)
		{
			if (!OutClosest)
			{
				OutClosest = node;
				continue;
			}
			if (dist(OutClosest) > dist(node))
			{
				OutClosest = node;
			}
		}
	}
	
	bool OpenEditorForGraph(const FLinkData& linkData)
	{
		UEdGraph* graph = nullptr;
		UEdGraphNode* closest = nullptr;
		FindClosestNodeInGraph(linkData.Graph, linkData.Location, graph, closest);
		
		if (!graph)
		{
			return false;
		}

		UObject* asset = FindAssetFromGraph(graph);
		{
			UClass* assetClass = asset->GetClass();
			if (assetClass->IsChildOf<UWorld>())
			{
				FWorldContext& editorWorld = GEditor->GetEditorWorldContext();
				UWorld* world = editorWorld.World();

				TArray<UWorld*> allWorlds;
				UEditorLevelUtils::GetWorlds(world, allWorlds, true);

				bool newWorld = true;
				for (UWorld* cur : allWorlds)
				{
					if (cur == asset)
					{
						newWorld = false;
						break;
					}
				}
				if (newWorld)
				{
					GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(asset);
					FindClosestNodeInGraph(linkData.Graph, linkData.Location, graph, closest);
				}

				FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(
					static_cast<bool>(closest)
					? static_cast<UObject*>(closest)
					: static_cast<UObject*>(graph)
				);
				return true;
			}
		}

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
			FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(
				static_cast<bool>(closest)
				? static_cast<UObject*>(closest)
				: static_cast<UObject*>(graph)
			);
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

namespace
{
	bool EnableHyperlinksInWindows(const FString& ProtocolName, const FString& Command)
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
		EnableHyperlinksInWindows("gotobp", command);

		HANDLE hPipe = CreateNamedPipe(
			*pipeName,
			PIPE_ACCESS_INBOUND | FILE_FLAG_FIRST_PIPE_INSTANCE | FILE_FLAG_OVERLAPPED,
			PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
			1,
			4096, 4096,
			0,
			nullptr
		);

		Pipe = hPipe;

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
				constexpr int buffSize = 512;
				char buff[buffSize + 1] = {};
				BOOL res = ReadFile(
					hPipe,
					buff,
					buffSize,
					nullptr,
					&ov
				);

				DWORD read = 0;
				GetOverlappedResult(hPipe, &ov, &read, 1);
				strBuff += buff;
				
				if (res)
				{
					OnMessageReceived(strBuff);
					
					strBuff = "";
					DisconnectNamedPipe(hPipe);
					break;
				}
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
	if (Pipe)
	{
		CloseHandle(Pipe);
	}
	Pipe = nullptr;
}
