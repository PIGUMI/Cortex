#pragma once
#include <RmlUi/Core/SystemInterface.h>

/*
* SystemInterface_Cortex
* Rml::SystemInterface の最小実装。全メソッドに既定実装があるため必須ではないが、
* 経過時間とログだけは Cortex 側へ繋いでおく。
*/
class SystemInterface_Cortex : public Rml::SystemInterface
{
public:
	SystemInterface_Cortex();

	double GetElapsedTime() override;
	bool LogMessage(Rml::Log::Type type, const Rml::String& message) override;

private:
	long long m_startTicks = 0;
	long long m_ticksPerSecond = 1;
};
