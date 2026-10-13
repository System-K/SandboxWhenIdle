#include "Papyrus.h"

#include "Manager.h"

namespace Papyrus
{
	namespace
	{
		constexpr auto kScript = "NSA_SandBox"sv;

		bool ForceSandbox(RE::StaticFunctionTag*)
		{
			auto handler = AutoSandboxHandler::GetSingleton();
			handler->ResetSandboxCheck();  // a forced start must not be blocked by an earlier failed check
			return handler->StartSandbox();
		}

		bool StopSandbox(RE::StaticFunctionTag*)
		{
			AutoSandboxHandler::GetSingleton()->StopSandbox();
			return true;
		}

		bool SetFreeCamKeys(RE::StaticFunctionTag*, std::int32_t a_keyboard, std::int32_t a_gamepad)
		{
			AutoSandboxHandler::GetSingleton()->SetFreeCamKeys(static_cast<std::uint32_t>(a_keyboard), static_cast<std::uint32_t>(a_gamepad));
			return true;
		}
	}

	bool Register(RE::BSScript::IVirtualMachine* a_vm)
	{
		if (!a_vm) {
			return false;
		}
		a_vm->RegisterFunction("ForceSandbox"sv, kScript, ForceSandbox);
		a_vm->RegisterFunction("StopSandbox"sv, kScript, StopSandbox);
		a_vm->RegisterFunction("SetFreeCamKeys"sv, kScript, SetFreeCamKeys);
		REX::INFO("Registered papyrus functions for {}", kScript);
		return true;
	}
}
