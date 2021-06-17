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

	{"rx", Command("Toggle relax settings (misses/failing)", [](Context& ctx) {
		if (ctx.args.size() != 0) {
			bool show_misses = false;
			bool allow_fail = false;

			for (const auto& arg : ctx.args) {
				if (arg == "miss")
					show_misses = true;
				else if (arg == "fail")
					allow_fail = true;
			}

			char* texture_base_ptr = reinterpret_cast<char*>(FindSignature("89 45 b8 81 7d f0 00 00 fe ff 0f 85", 12));
			if (texture_base_ptr == NULL) {
				std::cout << "Please click atleast a single hitcircle before loading." << std::endl;
				return;
			}
			
			char* audio_base_ptr = reinterpret_cast<char*>(FindSignature("8b 48 34 8b 01 8b 40 28 ff 50 10 83 f8 14 0f 8e", 16));
			char* fail_base_ptr = reinterpret_cast<char*>(FindSignature("8b 50 1c 8b 4a 04 8b 7a 08 ff 72 0c 8b d7 ff 15 ?? ?? ?? ?? 83 e0 01 85 c0 0f 8f 82 00 00 00 80 3d", 33));

			bool misses_currently_enabled = *(texture_base_ptr + 10) == 2;
			bool failure_currently_enabled = *(fail_base_ptr + 4) == 2;

			if (show_misses)
				if (!misses_currently_enabled)
					EnableRelaxMisses(texture_base_ptr, audio_base_ptr);
				//else
				//	std::cout << "Misses already enabled." << std::endl;
			else
				if (misses_currently_enabled)
					DisableRelaxMisses(texture_base_ptr, audio_base_ptr);

			if (allow_fail)
				if (!failure_currently_enabled)
					EnableRelaxFailure(fail_base_ptr);
				//else
				//	std::cout << "Failure already enabled." << std::endl;
			else
				if (failure_currently_enabled)
					DisableRelaxFailure(fail_base_ptr);
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
