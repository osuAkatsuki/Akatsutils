// Akatsutils - A program designed for modifying osu! clients connected to Akatsuki.
//              An example usage of this functionality is revealing misses on rx & ap.

#include <functional>
#include <iostream>
#include <unordered_map>
#include <Windows.h>

#include "Utils.h"

#define CMYUI_BUILD 0
#define VERSION "1.0.0"

struct Context {
	const std::string_view& trigger;
	const std::vector<std::string_view> args;

	Context(const std::string_view& _trigger, const std::vector<std::string_view> _args)
		: trigger(_trigger), args(_args) { }
};

struct Command {
	std::string_view description;
	std::function<void(Context& ctx)> function;

	Command(std::string_view _description, std::function<void(Context& ctx)> _function)
		: description(_description), function(_function) { }
};

// TODO: decorator-like solution?
std::unordered_map<std::string_view, Command> commands {
	{"help", Command("Show available commands", [](Context& ctx) {
		for (const auto& cmd : commands)
			std::cout << cmd.first << ": " << cmd.second.description << std::endl;
	})},
#if CMYUI_BUILD
	{"time", Command("Show current in-map time", [](Context& ctx) {
		uintptr_t audio_base = FindSignature("56 83 ec 38 83 3d", 6);
		if (audio_base != NULL) {
			uintptr_t time_ptr = audio_base + 0x2c;
			std::cout << **(uintptr_t**)time_ptr << std::endl;
		} else
			std::cout << "Failed to find audio base" << std::endl;
	})},

	{"find", Command("Search for a given signature", [](Context& ctx) {
		// TODO: deref option(s)

		// should prolly refactor FindPattern
		// so i dont have to do this cursed shit
		std::string fmt_str{};
		for (const auto& arg : ctx.args) {
			fmt_str += arg;
			fmt_str += " ";
		}
		fmt_str.pop_back();
		
		if (uintptr_t ptr = FindSignature(fmt_str); ptr != NULL)
			std::cout << "0x" << std::hex << ptr << std::endl;
		else
			std::cout << "Not Found" << std::endl;
	})},
#endif
	{"rxm", Command("Toggle relax misses", [](Context& ctx) {
		if (ctx.args.size() == 1) {
			char* base_ptr = reinterpret_cast<char*>(FindSignature("89 45 b8 81 7d f0 00 00 fe ff 0f 85", 12));
			if (base_ptr != NULL) {
				char* rx_ptr = base_ptr + 10;
				char* ap_ptr = base_ptr + 23;

				const bool enabled = *rx_ptr == 2 && *ap_ptr == 2;

				// skip the condition by swapping out the compare with 0 to 2, and the jne to je, to make it impossible.
				// more efficient soln it to nop the whole section out & save the bytes for replacement.
				if (ctx.args[0] == "enable") {
					if (enabled) {
						*rx_ptr = 0x02;
						*(rx_ptr + 2) = 0x84;
						*ap_ptr = 0x02;
						*(ap_ptr + 2) = 0x84;
						std::cout << "Misses will be shown." << std::endl;
					}
					else
						std::cout << "No changes made." << std::endl;
				}
				else if (ctx.args[0] == "disable") {
					if (enabled){
						*rx_ptr = 0x00;
						*(rx_ptr + 2) = 0x85;
						*ap_ptr = 0x00;
						*(ap_ptr + 2) = 0x85;
						std::cout << "Misses will be hidden." << std::endl;
					}
					else
						std::cout << "No changes made." << std::endl;
				}
				else if (ctx.args[0] == "check") {
					std::cout << "Misses currently " << (enabled ? "Shown" : "Hidden") << "." << std::endl;
				}
				else
					std::cout << "Unknown subcommand \"" << ctx.args[0] << "\"." << std::endl;
			} else
				std::cout << "Failed to find rxm base" << std::endl;
		} else
			std::cout << "Invalid syntax; correct: !rxm <enable/disable/check>" << std::endl;
	})}
};

void CommandLoop() {
	std::string user_input;

	while (true) {
		std::cout << ">> ";
		std::getline(std::cin, user_input);
		auto cmd_args = ExplodeView(user_input, " ");
		auto cmd_trigger = cmd_args.front();
		cmd_args.erase(cmd_args.begin());

		auto ctx = Context(cmd_trigger, cmd_args);

		if (auto cmd = commands.find(ctx.trigger); cmd != commands.end())
			cmd->second.function(ctx);
		else if (cmd_trigger == "close" || cmd_trigger == "exit" ||
			     cmd_trigger == "c" || cmd_trigger == "e")
			// special condition to break loop
			break;
	}
}

void OnProcessAttach(HINSTANCE hInstance) {
	TakeControl();

	std::cout << "Akatsutils v" VERSION << std::endl;
	CommandLoop();

	ReleaseControl();
	FreeLibraryAndExitThread(hInstance, 0);
}

BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved) { // dll
	switch (dwReason) {
		case DLL_PROCESS_ATTACH:
			DisableThreadLibraryCalls(hInstance);
			CreateThread(nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(OnProcessAttach), hInstance, 0, nullptr);
			break;
		default: break;
	}
	return TRUE;
}

int main() { // exe
	std::cout << "Akatsutils v" VERSION << std::endl;
	CommandLoop();
}
