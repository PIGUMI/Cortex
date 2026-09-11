#include "SystemInterface_Cortex.h"
#include <Windows.h>

SystemInterface_Cortex::SystemInterface_Cortex()
{
	LARGE_INTEGER freq{};
	LARGE_INTEGER start{};
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&start);
	m_ticksPerSecond = freq.QuadPart != 0 ? freq.QuadPart : 1;
	m_startTicks = start.QuadPart;
}

double SystemInterface_Cortex::GetElapsedTime()
{
	LARGE_INTEGER now{};
	QueryPerformanceCounter(&now);
	return static_cast<double>(now.QuadPart - m_startTicks) / static_cast<double>(m_ticksPerSecond);
}

bool SystemInterface_Cortex::LogMessage(Rml::Log::Type type, const Rml::String& message)
{
	// デバッグ出力へ流すだけ。Assert/Error でも継続させる (true を返すと処理続行)
	const char* prefix = "[RmlUi] ";
	OutputDebugStringA(prefix);
	OutputDebugStringA(message.c_str());
	OutputDebugStringA("\n");
	return true;
}
