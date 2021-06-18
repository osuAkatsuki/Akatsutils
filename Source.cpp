// Akatsutils - A program designed for modifying osu! clients connected to Akatsuki.
//              An example usage of this functionality is revealing misses on rx & ap.

#include <functional>
#include <iostream>
#include <unordered_map>
#include <Windows.h>

#include "Utils.h"

#define CMYUI_BUILD 1
#define VERSION "1.0.0"

#define RX_MISS_TEXTURE_SIG "89 45 b8 81 7d f0 00 00 fe ff 0f 85"
#define RX_MISS_AUDIO_SIG "8b 48 34 8b 01 8b 40 28 ff 50 10 83 f8 14 0f 8e"
#define RX_MISS_FAIL_SIG "8b 50 1c 8b 4a 04 8b 7a 08 ff 72 0c 8b d7 ff 15 ?? ?? ?? ?? 83 e0 01 85 c0 0f 8f 82 00 00 00 80 3d"

struct Context {
	const std::string_view& trigger;
	const std::vector<std::string_view> args;

	Context(const std::string_view& _trigger, const std::vector<std::string_view> _args)
		: trigger(_trigger), args(_args) {
	}
};

struct Command {
	std::string_view description;
	std::function<void(Context& ctx)> function;

	Command(std::string_view _description, std::function<void(Context& ctx)> _function)
		: description(_description), function(_function) {
	}
};

// won't change once we have them
char* texture_base_ptr = NULL;
char* audio_base_ptr = NULL;
char* fail_base_ptr = NULL;

// TODO: decorator-like solution?
std::unordered_map<std::string_view, Command> commands {
	{"help", Command("Show available commands", [](Context& ctx) {
		std::cout << "Available commands" << std::endl;
		for (const auto& cmd : commands)
			std::cout << cmd.first << ": " << cmd.second.description << std::endl;
	})},

	{"rx", Command("Toggle relax settings (misses/failing)", [](Context& ctx) {
		if (ctx.args.size() != 0) {
			bool show_misses = false;
			bool allow_fail = false;

			for (const auto& arg : ctx.args) {
				if (arg == "miss" || arg == "m") { show_misses = true; }
				else if (arg == "fail" || arg == "f") { allow_fail = true; }
			}

			if (texture_base_ptr == NULL) {
				// we haven't cached it yet
				texture_base_ptr = reinterpret_cast<char*>(FindSignature(RX_MISS_TEXTURE_SIG, 12));
				if (texture_base_ptr == NULL) {
					std::cout << "Please click atleast a single hitcircle before loading." << std::endl;
					return;
				}

				audio_base_ptr = reinterpret_cast<char*>(FindSignature(RX_MISS_AUDIO_SIG, 16));
				fail_base_ptr = reinterpret_cast<char*>(FindSignature(RX_MISS_FAIL_SIG, 33));
			}

			// check whether we've already overwritten some values
			bool misses_currently_enabled = *(texture_base_ptr + 10) == 2;
			bool failure_currently_enabled = *(fail_base_ptr + 4) == 2;

			if (show_misses) {
				if (!misses_currently_enabled) {
					std::cout << "\x1b[92mEnabling misses\x1b[0m" << std::endl;
					EnableRelaxMisses(texture_base_ptr, audio_base_ptr);
				}
			} else {
				if (misses_currently_enabled) {
					std::cout << "\x1b[91mDisabling misses\x1b[0m" << std::endl;
					DisableRelaxMisses(texture_base_ptr, audio_base_ptr);
				}
			}

			if (allow_fail) {
				if (!failure_currently_enabled) {
					std::cout << "\x1b[92mEnabling failure\x1b[0m" << std::endl;
					EnableRelaxFailure(fail_base_ptr);
				}
			} else {
				if (failure_currently_enabled) {
					std::cout << "\x1b[91mDisabling failure\x1b[0m" << std::endl;
					DisableRelaxFailure(fail_base_ptr);
				}
			}
		} else {
			if (texture_base_ptr != NULL) {
				bool misses_currently_enabled = *(texture_base_ptr + 10) == 2;
				bool failure_currently_enabled = *(fail_base_ptr + 4) == 2;

				if (misses_currently_enabled) {
					std::cout << "\x1b[91mDisabling misses\x1b[0m" << std::endl;
					DisableRelaxMisses(texture_base_ptr, audio_base_ptr);
				}

				if (failure_currently_enabled) {
					std::cout << "\x1b[91mDisabling failure\x1b[0m" << std::endl;
					DisableRelaxFailure(fail_base_ptr);
				}
			}
		}
	})},

#if CMYUI_BUILD
	{"time", Command("Show current in-map time", [](Context& ctx) {
		uintptr_t audio_base = FindSignature("56 83 ec 38 83 3d", 6);
		if (audio_base != NULL) {
			uintptr_t time_ptr = audio_base + 0x2c;
			std::cout << **(uintptr_t**)time_ptr << std::endl;
		} else {
			std::cout << "Failed to find audio base" << std::endl;
		}
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

		if (uintptr_t ptr = FindSignature(fmt_str); ptr != NULL) {
			std::cout << ptr << std::endl;
		} else {
			std::cout << "Not Found" << std::endl;
		}
	})}
#endif
};

std::vector<std::string> exit_triggers = {
	"close", "c",
	"exit", "e",
	"0"
};

void CommandLineInterface() {
	std::string user_input;

	while (true) {
		std::cout << ">> ";
		std::getline(std::cin, user_input);

		if (user_input.empty())
			// ignore empty input
			continue;

		auto cmd_args = ExplodeView(user_input, " ");
		auto cmd_trigger = cmd_args.front();
		cmd_args.erase(cmd_args.begin());

		auto ctx = Context(cmd_trigger, cmd_args);

		if (auto cmd = commands.find(ctx.trigger); cmd != commands.end())
			cmd->second.function(ctx);
		else if (std::find(exit_triggers.begin(), exit_triggers.end(), ctx.trigger) != exit_triggers.end()) {
			// special condition to break loop
			break;
		}
	}
}

void OnProcessAttach(HINSTANCE hInstance) {
	// allocate a console window
	AllocConsole();

	// open standard file streams
	freopen_s(reinterpret_cast<FILE**>(stdin), "CONIN$", "r", stdin);
	freopen_s(reinterpret_cast<FILE**>(stdout), "CONOUT$", "w", stdout);
	freopen_s(reinterpret_cast<FILE**>(stderr), "CONOUT$", "w", stderr);

	// enable virtual terminal sequences for stdout
	auto handle = GetStdHandle(-11);
	DWORD dwMode = 0;
	GetConsoleMode(handle, &dwMode);
	dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	SetConsoleMode(handle, dwMode);

	// set some custom params for the console window
	SetConsoleTitleA("Akatsutils v" VERSION);

	// TODO: resize window?
	//SetConsoleWindowInfo();

	std::cout.setf(std::ios::hex, std::ios::showbase);
	std::cout << "Akatsutils v" VERSION << std::endl;

	CommandLineInterface();

	// close standard file streams
	fclose(stdin);
	fclose(stdout);
	fclose(stderr);

	// free our console window
	FreeConsole();

	FreeLibraryAndExitThread(hInstance, 0);
}

// TODO: reflective injection to pack into 1 exe
// also for longterm, perhaps could do some stuff like
// https://zerosum0x0.blogspot.com/2017/07/threadcontinue-reflective-injection.html
// if i get into making it trying to hide it for fun/learning
BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved) { // dll
	switch (dwReason) {
	case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls(hInstance);
		CreateThread(nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(OnProcessAttach), hInstance, 0, nullptr);
		break;
	default:
		break;
	}
	return TRUE;
}

int main() { // exe, for debugging
	std::cout.setf(std::ios::hex, std::ios::showbase);
	std::cout << "Akatsutils v" VERSION << std::endl;

	CommandLineInterface();
}
