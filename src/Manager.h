#pragma once

class AutoSandboxHandler :
	public REX::TSingleton<AutoSandboxHandler>,
	public RE::BSTEventSink<RE::InputEvent*>,
	public RE::BSTEventSink<RE::TESLoadGameEvent>
{
public:
	void Register();

	bool IsAutoSandboxing() const;
	bool CanSandbox() const;
	bool StartSandbox();
	void StopSandbox();

	void ResetSandboxCheck();

	void UpdateCameraForSandbox(bool a_enable);
	void SetHeadTracking(bool a_enable);

	RE::TESPackage* GetPackage();

	// How to sit sleep meditate and fall... anywhere (NSA_SimpleAction): free-camera keys reported by its MCM
	void SetFreeCamKeys(std::uint32_t a_keyboard, std::uint32_t a_gamepad);

private:
	RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_evn, RE::BSTEventSource<RE::InputEvent*>*) override;
	RE::BSEventNotifyControl ProcessEvent(const RE::TESLoadGameEvent* a_evn, RE::BSTEventSource<RE::TESLoadGameEvent>*) override;

	// members
	std::uint32_t    freeCamKeyboard{ 0 };  // 0 = not set
	std::uint32_t    freeCamGamepad{ 0 };
	bool             autoSandBoxing{ false };
	bool             sandboxCheckFailed{ false };
	bool             registeredForInput{ false };
	bool             isInFirstPerson{ false };
	bool             headTrackingState{ false };
	bool             isNPCMode{ false };
	bool             improvedCameraInstalled{ false };
	bool             skipSandbox{ false };
	float            currentZoom;
	float            targetZoom;
	RE::TESPackage*  exteriorPackage{ nullptr };
	RE::TESPackage*  interiorPackage{ nullptr };
	RE::TESIdleForm* resetRootIdle{ nullptr };
};
