#include "Manager.h"

// NSA_SimpleAction edition (Actions.dll): sandboxing is started from Papyrus (NSA_SandBox.ForceSandbox)
// instead of the idle camera, uses the NSA packages, tells scripts via mod events and only stops on
// input that is not on the ignore list below.
namespace
{
	void SendModEvent(const char* a_name)
	{
		SKSE::ModCallbackEvent event{ a_name, ""sv, 0.0f, nullptr };
		if (auto source = SKSE::GetModCallbackEventSource()) {
			source->SendEvent(&event);
		}
	}

	// user events that must NOT end sandboxing (menus, camera, map, screenshots, ...)
	constexpr std::array kIgnoredUserEvents{
		"Inventory"sv, "Favorites"sv, "Shout"sv, "Quickload"sv, "Pause"sv, "Journal"sv, "Look"sv, "Tween Menu"sv,
		"Zoom Out"sv, "Zoom In"sv, "Toggle POV"sv, "Wait"sv, "Screenshot"sv, "Console"sv, "Quick Inventory"sv,
		"Accept"sv, "Cancel"sv, "Up"sv, "Right"sv, "Down"sv, "Left"sv, "Quick Magic"sv, "Quick Stats"sv,
		"Quick Map"sv, "Enter"sv, "Click"sv, "PlacePlayerMarker"sv, "PlayerPosition"sv, "LocalMapMoveMode"sv,
		"MapLookMode"sv, "PrevPage"sv, "NextPage"sv, "PrevSubPage"sv, "NextSubPage"sv, "LocalMap"sv, "Item Zoom"sv,
		"Rotate"sv, "TabSwitch"sv, "LeftEquip"sv, "RightEquip"sv, "Cursor"sv, "ChargeItem"sv, "Quicksave"sv
	};
}

void AutoSandboxHandler::SetFreeCamKeys(std::uint32_t a_keyboard, std::uint32_t a_gamepad)
{
	freeCamKeyboard = a_keyboard;
	freeCamGamepad = a_gamepad;
	REX::INFO("FreeCam keys: keyboard {} gamepad {}", a_keyboard, a_gamepad);
}

void AutoSandboxHandler::Register()
{
	if (const auto inputMgr = RE::BSInputDeviceManager::GetSingleton()) {
		inputMgr->AddEventSink<RE::InputEvent*>(this);
		registeredForInput = true;
		REX::INFO("Registered for hotkey event");
	}

	if (auto scripts = RE::ScriptEventSourceHolder::GetSingleton()) {
		scripts->AddEventSink<RE::TESLoadGameEvent>(this);
		REX::INFO("Registered for load game event");
	}

	REX::INFO("Current auto vanity time: {} seconds", "fAutoVanityModeDelay:Camera"_ini.value_or(-1.0f));

	exteriorPackage = RE::TESForm::LookupByEditorID<RE::TESPackage>("NSASandboxExterior");
	interiorPackage = RE::TESForm::LookupByEditorID<RE::TESPackage>("NSASandboxInterior");
	resetRootIdle = RE::TESForm::LookupByEditorID<RE::TESIdleForm>("ResetRoot");

	if (!exteriorPackage) {
		REX::ERROR("Could not find NSASandboxExterior package. Auto sandboxing disabled.");
		skipSandbox = true;
	} else {
		REX::INFO("Found NSASandboxExterior package.");
	}
	if (!interiorPackage) {
		REX::ERROR("Could not find NSASandboxInterior package. Auto sandboxing disabled.");
		skipSandbox = true;
	} else {
		REX::INFO("Found NSASandboxInterior package.");
	}
	if (!resetRootIdle) {
		REX::ERROR("Could not find ResetRoot idle. Auto sandboxing disabled.");
		skipSandbox = true;
	}

	improvedCameraInstalled = REX::W32::GetModuleHandleA("ImprovedCameraSE.dll") != nullptr;
}

bool AutoSandboxHandler::IsAutoSandboxing() const
{
	return autoSandBoxing;
}

bool AutoSandboxHandler::CanSandbox() const
{
	if (!registeredForInput || skipSandbox || sandboxCheckFailed) {
		return false;
	}

	const auto player = RE::PlayerCharacter::GetSingleton();
	if (!player->currentProcess || player->IsDead() || player->IsInCombat() || player->IsSneaking() || player->IsOnMount()) {
		return false;
	}

	const auto controlMap = RE::ControlMap::GetSingleton();
	if (controlMap && (!controlMap->IsMovementControlsEnabled() || !controlMap->IsLookingControlsEnabled())) {
		return false;
	}

	const auto processLists = RE::ProcessLists::GetSingleton();
	if (processLists && processLists->AreHostileActorsNear(nullptr)) {
		return false;
	}

	return true;
}

bool AutoSandboxHandler::StartSandbox()
{
	if (!autoSandBoxing) {
		if (CanSandbox()) {
			if (auto package = GetPackage()) {
				autoSandBoxing = true;

				auto player = RE::PlayerCharacter::GetSingleton();
				player->DrawWeaponMagicHands(false);
				player->SetAIDriven(true);
				player->currentProcess->SetRunOncePackage(nullptr, player);
				player->currentProcess->ComputeLastTimeProcessed();
				player->PutCreatedPackage(package, false, false);
				player->EvaluatePackage();

				SetHeadTracking(true);
				UpdateCameraForSandbox(true);

				SendModEvent("SandboxStarted");
			}
		} else {
			sandboxCheckFailed = true;
		}
	}

	return autoSandBoxing;
}

void AutoSandboxHandler::StopSandbox()
{
	autoSandBoxing = false;

	auto player = RE::PlayerCharacter::GetSingleton();
	player->StopInteractingQuick(true);
	if (auto currentProcess = player->currentProcess) {
		currentProcess->StopCurrentIdle(player, true);
		currentProcess->PlayIdle(player, resetRootIdle, nullptr);

		currentProcess->AddToProcedureIndexRunning(player, 1);
		currentProcess->SetRunOncePackage(nullptr, player);
	}
	player->SetAIDriven(false);

	SetHeadTracking(false);
	UpdateCameraForSandbox(false);

	ResetSandboxCheck();

	SendModEvent("SandboxStopped");
}

void AutoSandboxHandler::ResetSandboxCheck()
{
	sandboxCheckFailed = false;
}

void AutoSandboxHandler::UpdateCameraForSandbox(bool a_enable)
{
	auto playerCamera = RE::PlayerCamera::GetSingleton();
	if (a_enable) {
		isInFirstPerson = !improvedCameraInstalled && playerCamera->IsInFirstPerson();  // don't switch if IC is installed
		if (isInFirstPerson) {
			playerCamera->PushCameraState(RE::CameraState::kThirdPerson);
			if (auto tps = skyrim_cast<RE::ThirdPersonState*>(RE::PlayerCamera::GetSingleton()->currentState.get())) {
				// default tps is too zoomed in
				currentZoom = tps->currentZoomOffset;
				targetZoom = tps->targetZoomOffset;
				if (currentZoom < 0.8f) {
					tps->currentZoomOffset = 0.8f;
				}
				if (targetZoom < 0.8f) {
					tps->targetZoomOffset = 0.8f;
				}
			}
		}
		RE::SendHUDMessage::PushHUDMode("AutoVanity");
	} else {
		if (isInFirstPerson) {
			playerCamera->ForceFirstPerson();
		}
		playerCamera->idleTimer = 0.0f;
		RE::SendHUDMessage::PopHUDMode("AutoVanity");
	}
}

void AutoSandboxHandler::SetHeadTracking(bool a_enable)
{
	auto player = RE::PlayerCharacter::GetSingleton();

	if (a_enable) {
		headTrackingState = player->actorState2.headTracking;
		player->GetGraphVariableBool("IsNPC", isNPCMode);

		player->actorState2.headTracking = true;
		player->SetGraphVariableBool("IsNPC", true);
	} else {
		player->actorState2.headTracking = headTrackingState;
		player->SetGraphVariableBool("IsNPC", isNPCMode);
	}
}

RE::TESPackage* AutoSandboxHandler::GetPackage()
{
	auto parentCell = RE::PlayerCharacter::GetSingleton()->GetParentCell();
	return parentCell && parentCell->IsInteriorCell() ? interiorPackage : exteriorPackage;
}

RE::BSEventNotifyControl AutoSandboxHandler::ProcessEvent(RE::InputEvent* const* a_evn, RE::BSTEventSource<RE::InputEvent*>*)
{
	if (!a_evn || !IsAutoSandboxing()) {
		return RE::BSEventNotifyControl::kContinue;
	}

	const auto player = RE::PlayerCharacter::GetSingleton();
	const auto sitState = player->GetSitSleepState();
	const bool sitting = sitState == RE::SIT_SLEEP_STATE::kIsSitting || sitState == RE::SIT_SLEEP_STATE::kWantToStand;

	for (auto event = *a_evn; event; event = event->next) {
		const auto buttonEvent = event->AsButtonEvent();
		if (!buttonEvent || !buttonEvent->IsDown()) {
			continue;
		}

		const auto key = buttonEvent->GetIDCode();
		switch (buttonEvent->GetDevice()) {
		case RE::INPUT_DEVICE::kKeyboard:
			// free-cam key, Esc (0x01), A (0x1E), D (0x20), left/right Alt (0x38/0xB8)
			if ((freeCamKeyboard != 0 && key == freeCamKeyboard) || key == 0x01 || key == 0x1E || key == 0x20 || key == 0x38 || key == 0xB8) {
				continue;
			}
			break;
		case RE::INPUT_DEVICE::kGamepad:
			// free-cam button, d-pad up/down (0x1/0x2), start (0x10)
			if ((freeCamGamepad != 0 && key == freeCamGamepad) || key == 0x10 || key == 0x1 || key == 0x2) {
				continue;
			}
			break;
		default:
			break;
		}

		const std::string_view userEvent = buttonEvent->QUserEvent().c_str();
		if (std::ranges::find(kIgnoredUserEvents, userEvent) != kIgnoredUserEvents.end()) {
			continue;
		}
		if (userEvent == "Activate"sv && sitting) {
			continue;
		}

		StopSandbox();
		break;
	}

	return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl AutoSandboxHandler::ProcessEvent(const RE::TESLoadGameEvent* a_evn, RE::BSTEventSource<RE::TESLoadGameEvent>*)
{
	if (!a_evn) {
		return RE::BSEventNotifyControl::kContinue;
	}

	if (IsAutoSandboxing()) {
		StopSandbox();
	}

	ResetSandboxCheck();

	return RE::BSEventNotifyControl::kContinue;
}
