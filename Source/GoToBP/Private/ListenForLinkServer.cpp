// Snapshot Games

#include "ListenForLinkServer.h"

#include <coroutine>
#include <string>
#include <variant>

#include "GoToBPDeveloperSettings.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Interfaces/IPluginManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
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
}


void UListenForLinkServer::OnMessageReceived(const FString& Message)
{
	const FString prefix = "param=";
	const FString suffix = "-";
	
	int index = Message.Find(prefix);
	FString payload = Message.RightChop(index + prefix.Len());

	index = payload.Find(suffix, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	payload =  payload.LeftChop(payload.Len() - index);

	TArray<uint8> data;
	FBase64::Decode(payload, data);

	FMemoryReader reader(data);
	FLinkData linkData;
	reader << linkData;

	UEdGraph* graph = LoadObject<UEdGraph>(nullptr, *linkData.Graph);
	if (!graph)
	{
		FNotificationInfo Info(FText::FromString("Couldn't find the graph, you are looking for..."));
		Info.bFireAndForget = true;
		Info.FadeOutDuration = 2.0f;
		Info.ExpireDuration = 3.0f;
		Info.bUseLargeFont = false;
		FSlateNotificationManager::Get().AddNotification(Info);
		return;
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

	if (!closest)
	{
		FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(graph);
	}
	else
	{
		FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(closest);
	}
}

void UListenForLinkServer::Init()
{
	auto crt = [this]() -> Coroutine
	{
		Blocker& blocker = GetBlocker();
		co_await blocker;
		
		ISocketSubsystem* SocketSubsystem = nullptr;
		SocketSubsystem = ISocketSubsystem::Get();
		while (!SocketSubsystem)
		{
			co_await blocker;
			SocketSubsystem = ISocketSubsystem::Get();
		}
		
		FSocket* Sock = SocketSubsystem->CreateSocket("Local Log Socket", "Local Log Socket");
		Sock->SetNonBlocking(true);
		TSharedRef<FInternetAddr> Addr = SocketSubsystem->CreateInternetAddr();
		bool validIP = false;
		Addr->SetIp(TEXT("127.0.0.1"), validIP);
		Addr->SetPort(0);

		bool bound = Sock->Bind(*Addr);
		Sock->GetAddress(Addr.Get());
		
		Sock->Listen(1);
		
		{
			FString scriptPath = GetScriptPath();
			FString command = FString::Format(TEXT("powershell -executionpolicy bypass -File \"{0}\" {1}"), {scriptPath, Addr->GetPort()});
			EnableLinks("gotobp", command);
		}
		
		FString MessageBuff = "";
		FSocket* conn = nullptr;
		while (true)
		{
			while (!conn)
			{
				conn = Sock->Accept("Incoming Link");
				if (!conn)
				{
					co_await blocker;
				}
			}

			auto tryRead = [conn, &MessageBuff](bool& bSuccessful, int32& read)
			{
				constexpr uint16 buffSize = 1024;
				uint8 buff[buffSize + 1] = {};
				bSuccessful = conn->Recv(
					buff,
					buffSize,
					read
				);

				MessageBuff += reinterpret_cast<char*>(buff);
			};

			while (true)
			{
				bool bSuccessful = false;
				int32 read = 0;

				tryRead(bSuccessful, read);
				while (true)
				{
					int32 outIndex = 0;
					bool found = MessageBuff.FindChar('|', outIndex);
					if (!found)
					{
						break;
					}

					FString Message = MessageBuff.Left(outIndex + 1);
					MessageBuff = MessageBuff.RightChop(outIndex + 1);
					OnMessageReceived(Message);
				}

				if (!bSuccessful)
				{
					SocketSubsystem->DestroySocket(conn);
					conn = nullptr;
					break;
				}
				if (read < 0)
				{
					break;
				}
			}
			co_await blocker;
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

	KeyDownHandle = slateApp->OnApplicationPreInputKeyDownListener().AddLambda([this](const FKeyEvent& keyEvent) {
		FKey ActivationKey = UGoToBP::GetActivationKey();
		if (keyEvent.GetKey() == ActivationKey)
		{
			ActivatedAt = FPlatformTime::Seconds();
		}
	});

	MouseDownHandle = slateApp->OnApplicationMousePreInputButtonDownListener().AddLambda([this, slateApp](const FPointerEvent& pointerEvent) {
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
			FVector2f mouse(Location.X, Location.Y);

			FString graphPath = edGraph->GetPathName();

			FLinkData data { graphPath, Location };
			TArray<uint8> buff;
			FMemoryWriter writer(buff);

			writer << data;
			FString payload = FBase64::Encode(buff);
			
			FString link = FString::Format(TEXT("gotobp://loc?param={0}-"), { *payload });
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

void UListenForLinkServer::StopListeningForInput()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication& slateApp = FSlateApplication::Get();
		slateApp.OnApplicationPreInputKeyDownListener().Remove(KeyDownHandle);
		slateApp.OnApplicationMousePreInputButtonDownListener().Remove(MouseDownHandle);
	}
}

bool UListenForLinkServer::EnableLinks(const FString& ProtocolName, const FString& ApplicationPath)
{
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

		FString cmd = FString::Format(TEXT("{0} \"%1\""), {*ApplicationPath});
		std::wstring command = *cmd;

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

FString UListenForLinkServer::GetScriptPath()
{
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT(PLUGIN_NAME));
	FString PluginBaseDir = Plugin->GetBaseDir(); // Folder of the plugin
	FString AbsolutePath = FPaths::ConvertRelativePathToFull(PluginBaseDir);
	FString scriptPath = FPaths::Combine(AbsolutePath, "Scripts/TCPReq.ps1");

	return scriptPath;
}
