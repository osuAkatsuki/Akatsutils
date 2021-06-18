#include "Utils.h"

std::vector<std::string_view> ExplodeView(const std::string_view& str, const std::string_view& delim) {
	std::vector<std::string_view> vec;

	size_t pos = str.find(delim);
	int offs = 0;

	while (pos != std::string::npos) {
		vec.push_back(str.substr(offs, pos - offs));
		offs = pos + 1;
		pos = str.find(delim, offs);
	}

	vec.push_back(str.substr(offs, pos - offs));
	return vec;
}

auto MakeSignature(const std::string_view& fmt_string) {
	// parse the given fmt_string, transforming it into a vector of values & types
	// the only valid types are currently char (x) and unknown (?), subject to change.
	std::vector<unsigned char> fmt_chars;
	std::vector<char> mask;

	size_t pos = fmt_string.find(" ");
	int offs = 0;

	// TODO since it's always 2 bytes we can get a lot better efficiency.
	//      if we instead want to go the generic route, we can use ExplodeView.
	constexpr auto read_byte = [&](std::vector<unsigned char>& sig_bytes, std::vector<char>& mask, std::string_view& s, const int offs) {
		if (s != "??") { // known byte
			mask.push_back('x');
			sig_bytes.push_back(std::stoi(s.data(), nullptr, 16));
		} else { // unknown (wildcard) byte
			mask.push_back('?');
			sig_bytes.push_back(0);
		}
	};

	while (pos != std::string::npos) {
		std::string_view substr = fmt_string.substr(offs, 2);
		read_byte(fmt_chars, mask, substr, offs);
		offs = pos + 1;
		pos = fmt_string.find(" ", offs);
	}

	std::string_view substr = fmt_string.substr(offs, 2);
	read_byte(fmt_chars, mask, substr, offs);
	return std::make_tuple(fmt_chars, mask);
}

const uintptr_t FindSignature(const std::string_view& fmt_string, const int ptr_offs) {
	// parse the given fmt_string creating a vector of bytes & a mask to search for,
	// and use the win32 api to search through the processes' memory allocations.
	auto [sig, mask] = MakeSignature(fmt_string);

	MEMORY_BASIC_INFORMATION32 mbi;
	LPCVOID end_addr = 0;

	while (VirtualQuery(end_addr, reinterpret_cast<PMEMORY_BASIC_INFORMATION>(&mbi), sizeof(mbi)) != 0) {
		end_addr = reinterpret_cast<LPCVOID>(mbi.BaseAddress + mbi.RegionSize);

		if (mbi.State == MEM_COMMIT && mbi.Protect >= PAGE_EXECUTE && mbi.Protect <= PAGE_EXECUTE_WRITECOPY) {
			for (auto i = mbi.BaseAddress; i < reinterpret_cast<size_t>(end_addr) - sig.size(); i++) {
				bool found = true;

				// look for non-matching chars, breaking on the first found.
				for (auto char_offs = 0; char_offs < sig.size(); char_offs++) {
					//if (char_offs > 2)
					//	std::cout << char_offs << std::endl;
					if (mask[char_offs] != '?' && sig[char_offs] != *(unsigned char*)(i + char_offs)) {
						found = false;
						break;
					}
				}

				if (found) {
					return i + ptr_offs;
				}
			}
		}
	}
	return NULL;
}

void EnableRelaxMisses(char* texture_base_ptr, char* audio_base_ptr) {
	// enable miss texture for rx & ap
	char* rx_texture_ptr = texture_base_ptr + 10;
	char* ap_texture_ptr = texture_base_ptr + 23;

	*rx_texture_ptr = 0x02;
	*(rx_texture_ptr + 2) = 0x84;
	*ap_texture_ptr = 0x02;
	*(ap_texture_ptr + 2) = 0x84;

	// enable combobreak audio for rx & ap
	char* rx_audio_ptr = audio_base_ptr + 10;
	char* ap_audio_ptr = audio_base_ptr + 23;

	*rx_audio_ptr = 0x02;
	*(rx_audio_ptr + 2) = 0x84;
	*ap_audio_ptr = 0x02;
	*(ap_audio_ptr + 2) = 0x84;
}

void EnableRelaxFailure(char* fail_base_ptr) {
	// enable failure for rx & ap
	char* rx_fail_ptr = fail_base_ptr + 4;
	char* ap_fail_ptr = fail_base_ptr + 13;

	*rx_fail_ptr = 0x02;
	*(rx_fail_ptr + 1) = 0x74;
	*ap_fail_ptr = 0x02;
	*(ap_fail_ptr + 1) = 0x74;
}

void DisableRelaxMisses(char* texture_base_ptr, char* audio_base_ptr) {
	// disable miss texture for rx & ap
	char* rx_texture_ptr = texture_base_ptr + 10;
	char* ap_texture_ptr = texture_base_ptr + 23;

	*rx_texture_ptr = 0x00;
	*(rx_texture_ptr + 2) = 0x85;
	*ap_texture_ptr = 0x00;
	*(ap_texture_ptr + 2) = 0x85;

	// disable combobreak audio for rx & ap
	char* rx_audio_ptr = audio_base_ptr + 10;
	char* ap_audio_ptr = audio_base_ptr + 23;

	*rx_audio_ptr = 0x00;
	*(rx_audio_ptr + 2) = 0x85;
	*ap_audio_ptr = 0x00;
	*(ap_audio_ptr + 2) = 0x85;
}

void DisableRelaxFailure(char* fail_base_ptr) {
	// disable failure for rx & ap
	char* rx_fail_ptr = fail_base_ptr + 4;
	char* ap_fail_ptr = fail_base_ptr + 13;

	*rx_fail_ptr = 0x00;
	*(rx_fail_ptr + 1) = 0x75;
	*ap_fail_ptr = 0x00;
	*(ap_fail_ptr + 1) = 0x75;
}

StlTimer::StlTimer() {
	st = std::chrono::high_resolution_clock::now();
}

StlTimer::~StlTimer() {
	auto ns_taken = (std::chrono::high_resolution_clock::now() - st).count();
	std::cout << ns_taken << " nsec" << std::endl;
}

Win32Timer::Win32Timer() {
	QueryPerformanceCounter(&st);
}

Win32Timer::~Win32Timer() {
	LARGE_INTEGER et;
	QueryPerformanceCounter(&et);
	auto counts_taken = et.QuadPart - st.QuadPart;
	std::cout << counts_taken << " counts" << std::endl;
}

CpuTimer::CpuTimer() {
	st = __rdtsc();
}

CpuTimer::~CpuTimer() {
	auto cycles_taken = __rdtsc() - st;
	std::cout << cycles_taken << " cycles" << std::endl;
}
