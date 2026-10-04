#include "Immersive.h"

#include "AMF.h"
#include "utils/Logger.h"

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <Windows.h>

#include <atomic>
#include <chrono>
#include <format>
#include <mutex>
#include <vector>

namespace immersive
{
	namespace
	{
		// the settings, handed over by Tick
		std::atomic<bool>  g_enabled{ false };
		std::atomic<int>   g_key{ 45 };
		std::atomic<int>   g_button{ 0 };
		std::atomic<bool>  g_hold{ false };
		std::atomic<float> g_seconds{ 0.0F };
		std::atomic<bool>  g_startVisible{ false };

		// the toggle's state: toggled (press mode), held (hold mode), and the display duration's time left
		std::atomic<bool>  g_toggled{ false };
		std::atomic<bool>  g_held{ false };
		std::atomic<float> g_left{ 0.0F };       // seconds left to show (display duration); counted down in play only
		std::atomic<bool>  g_pressed{ false };   // a press to start the display duration, taken by Shown()

		bool g_ihud = false;

		// the binding capture
		std::atomic<int>                      g_armed{ 0 };
		std::atomic<long long>                g_pageDrawn{ 0 };   // steady-clock ms of the page's last draw
		std::atomic<int>                      g_captured{ -1 };
		std::atomic<int>                      g_capturedTarget{ 0 };
		std::mutex                            g_statusLock;
		std::string                           g_status;
		std::chrono::steady_clock::time_point g_capturedAt{};

		long long NowMs()
		{
			return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
		}

		// AMF's own keys (its toggle, Tab, Esc, the arrows, Enter): a binding there would fire every time the menu is used
		bool Reserved(int a_code)
		{
			std::int32_t buf[32]{};
			const auto   n = AMF::ReservedKeys(buf, 32);
			for (std::uint32_t i = 0; i < n && i < 32; ++i) {
				if (buf[i] == a_code) { return true; }
			}
			return false;
		}

		void Press(bool a_down)
		{
			if (g_hold.load()) {
				g_held = a_down;
				return;
			}
			if (!a_down) { return; }
			if (g_seconds.load() > 0.0F) {
				g_pressed = true;   // shown for the display duration, counted from now
			} else {
				g_toggled = !g_toggled.load();
			}
			logger::debug("immersive: toggle pressed ({})", g_seconds.load() > 0.0F ? "display duration" : (g_toggled.load() ? "shown" : "hidden"));
		}

		class InputSink final : public RE::BSTEventSink<RE::InputEvent*>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>*) override
			{
				for (auto* e = a_event ? *a_event : nullptr; e; e = e->next) {
					auto* b = e->AsButtonEvent();
					if (!b) { continue; }
					const auto device = b->GetDevice();
					const bool keyboard = device == RE::INPUT_DEVICE::kKeyboard;
					const bool gamepad = device == RE::INPUT_DEVICE::kGamepad;
					if (!keyboard && !gamepad) { continue; }
					const int code = static_cast<int>(b->GetIDCode());

					// a binding capture: the next press on the armed side, while the page is open
					if (const int armed = g_armed.load(); armed != 0) {
						if (NowMs() - g_pageDrawn.load() > 500) {   // the page closed: drop the arm, never eat a key
							g_armed = 0;
						} else if (b->IsDown() && ((armed == 1 && keyboard) || (armed == 2 && gamepad))) {
							if (keyboard && (Reserved(code) || code == 1 /* Esc */)) {
								std::lock_guard l(g_statusLock);
								g_status = "reserved";
								logger::info("immersive: key {} is reserved for the menu; still waiting", code);
								continue;
							}
							g_captured = code;
							g_capturedTarget = armed;
							g_armed = 0;
							logger::info("immersive: toggle {} bound to {}", keyboard ? "key" : "button", keyboard ? KeyName(code) : ButtonName(code));
							continue;
						}
					}

					if (!g_enabled.load()) { continue; }
					const bool match = (keyboard && code == g_key.load() && code != 0) || (gamepad && code == g_button.load() && code != 0);
					if (!match) { continue; }
					// never in a menu that pauses the game (AMF's, the console, the inventory ...): the key is theirs there
					auto* ui = RE::UI::GetSingleton();
					if ((ui && ui->GameIsPaused()) || AMF::IsMenuOpen()) { continue; }
					if (b->IsDown()) { Press(true); }
					else if (b->IsUp()) { Press(false); }
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};
	}

	void Register()
	{
		static InputSink sink;
		static bool      done = false;
		if (done) { return; }
		if (auto* input = RE::BSInputDeviceManager::GetSingleton()) {
			input->AddEventSink<RE::InputEvent*>(&sink);
			done = true;
			logger::info("immersive: input sink registered");
		} else {
			logger::warn("immersive: no input device manager yet; the HUD toggle key cannot work");
		}
		g_ihud = GetModuleHandleW(L"ImmersiveHUD.dll") != nullptr;
		if (g_ihud) { logger::warn("immersive: ImmersiveHUD is loaded - it drives the same clips' alpha; use one or the other"); }
	}

	void OnGameLoaded()
	{
		g_toggled = g_startVisible.load();
		g_held = false;
		g_left = 0.0F;
		g_pressed = false;
	}

	void Sync(bool a_enabled, int a_key, int a_button, bool a_hold, float a_seconds, bool a_startVisible)
	{
		g_enabled = a_enabled;
		g_key = a_key;
		g_button = a_button;
		g_hold = a_hold;
		g_seconds = a_seconds;
		g_startVisible = a_startVisible;
	}

	bool Shown(float a_dt, bool a_gameplay)
	{
		if (g_pressed.exchange(false)) { g_left = g_seconds.load(); }
		float left = g_left.load();
		if (left > 0.0F && a_gameplay) {   // the display duration runs in play only (frozen in menus)
			left = std::max(0.0F, left - a_dt);
			g_left = left;
		}
		return g_held.load() || g_toggled.load() || left > 0.0F;
	}

	bool IhudPresent() { return g_ihud; }

	void Arm(Bind a_target)
	{
		g_armed = static_cast<int>(a_target);
		std::lock_guard l(g_statusLock);
		g_status.clear();
	}

	Bind Armed() { return static_cast<Bind>(g_armed.load()); }

	void PageDrawn() { g_pageDrawn = NowMs(); }

	std::string TakeStatus()
	{
		std::lock_guard l(g_statusLock);
		return std::exchange(g_status, {});
	}

	int TakeCaptured(Bind& a_target)
	{
		const int code = g_captured.exchange(-1);
		a_target = static_cast<Bind>(g_capturedTarget.load());
		return code;
	}

	std::string KeyName(int a_code)
	{
		static const std::pair<int, const char*> kNames[] = {
			{ 1, "Esc" }, { 2, "1" }, { 3, "2" }, { 4, "3" }, { 5, "4" }, { 6, "5" }, { 7, "6" }, { 8, "7" }, { 9, "8" }, { 10, "9" }, { 11, "0" },
			{ 12, "-" }, { 13, "=" }, { 14, "Backspace" }, { 15, "Tab" }, { 16, "Q" }, { 17, "W" }, { 18, "E" }, { 19, "R" }, { 20, "T" },
			{ 21, "Y" }, { 22, "U" }, { 23, "I" }, { 24, "O" }, { 25, "P" }, { 26, "[" }, { 27, "]" }, { 28, "Enter" }, { 29, "Left Ctrl" },
			{ 30, "A" }, { 31, "S" }, { 32, "D" }, { 33, "F" }, { 34, "G" }, { 35, "H" }, { 36, "J" }, { 37, "K" }, { 38, "L" }, { 39, ";" },
			{ 40, "'" }, { 41, "`" }, { 42, "Left Shift" }, { 43, "\\" }, { 44, "Z" }, { 45, "X" }, { 46, "C" }, { 47, "V" }, { 48, "B" },
			{ 49, "N" }, { 50, "M" }, { 51, "," }, { 52, "." }, { 53, "/" }, { 54, "Right Shift" }, { 55, "Num *" }, { 56, "Left Alt" },
			{ 57, "Space" }, { 58, "Caps Lock" }, { 59, "F1" }, { 60, "F2" }, { 61, "F3" }, { 62, "F4" }, { 63, "F5" }, { 64, "F6" },
			{ 65, "F7" }, { 66, "F8" }, { 67, "F9" }, { 68, "F10" }, { 69, "Num Lock" }, { 70, "Scroll Lock" }, { 71, "Num 7" }, { 72, "Num 8" },
			{ 73, "Num 9" }, { 74, "Num -" }, { 75, "Num 4" }, { 76, "Num 5" }, { 77, "Num 6" }, { 78, "Num +" }, { 79, "Num 1" }, { 80, "Num 2" },
			{ 81, "Num 3" }, { 82, "Num 0" }, { 83, "Num ." }, { 87, "F11" }, { 88, "F12" }, { 156, "Num Enter" }, { 157, "Right Ctrl" },
			{ 181, "Num /" }, { 184, "Right Alt" }, { 199, "Home" }, { 200, "Up" }, { 201, "Page Up" }, { 203, "Left" }, { 205, "Right" },
			{ 207, "End" }, { 208, "Down" }, { 209, "Page Down" }, { 210, "Insert" }, { 211, "Delete" },
		};
		for (const auto& [c, n] : kNames) {
			if (c == a_code) { return n; }
		}
		return a_code > 0 ? std::format("Key {}", a_code) : std::string{};
	}

	std::string ButtonName(int a_mask)
	{
		static const std::pair<int, const char*> kNames[] = {
			{ 0x0001, "D-pad Up" }, { 0x0002, "D-pad Down" }, { 0x0004, "D-pad Left" }, { 0x0008, "D-pad Right" }, { 0x0010, "Start" },
			{ 0x0020, "Back" }, { 0x0040, "Left Stick" }, { 0x0080, "Right Stick" }, { 0x0100, "LB" }, { 0x0200, "RB" }, { 0x1000, "A" },
			{ 0x2000, "B" }, { 0x4000, "X" }, { 0x8000, "Y" }, { 0x0009, "LT" }, { 0x000A, "RT" },
		};
		for (const auto& [c, n] : kNames) {
			if (c == a_mask) { return n; }
		}
		return a_mask > 0 ? std::format("Button {}", a_mask) : std::string{};
	}

	void Simulate(bool a_down)
	{
		Press(a_down);
	}

	std::string StateJson()
	{
		return std::format(R"({{"enabled":{},"key":{},"button":{},"hold":{},"seconds":{:.1f},"toggled":{},"held":{},"left":{:.2f},"armed":{},"ihud":{}}})",
			g_enabled.load(), g_key.load(), g_button.load(), g_hold.load(), g_seconds.load(), g_toggled.load(), g_held.load(), g_left.load(),
			g_armed.load(), g_ihud);
	}
}
