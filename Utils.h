#pragma once
#ifndef __OSULIB_UTILS_H_
#define __OSULIB_UTILS_H_

#include <chrono>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <Windows.h>

std::vector<std::string_view> ExplodeView(const std::string_view& str, const std::string_view& delim);

auto MakeSignature(const std::string_view& fmt_string);
const uintptr_t FindSignature(const std::string_view& fmt_string, const int ptr_offs = 0);

void EnableRelaxMisses(char* texture_base_ptr, char* audio_base_ptr);
void EnableRelaxFailure(char* fail_base_ptr);

void DisableRelaxMisses(char* texture_base_ptr, char* audio_base_ptr);
void DisableRelaxFailure(char* fail_base_ptr);

// TODO: magnitude fmt for these?

struct StlTimer {
	std::chrono::high_resolution_clock::time_point st;

	StlTimer();
	~StlTimer();
};

struct Win32Timer {
	LARGE_INTEGER st;

	Win32Timer();
	~Win32Timer();
};

struct CpuTimer {
	volatile DWORD64 st;

	CpuTimer();
	~CpuTimer();
};

#endif __OSULIB_UTILS_H_
