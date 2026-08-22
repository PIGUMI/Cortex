#pragma once
/*
* •Êthread‚ğ—§‚Ä‚»‚±‚Å“n‚³‚ê‚½ˆ—‚ğs‚¤ƒNƒ‰ƒX
*/

#include <thread>
#include <functional>
#include <atomic>

class Worker
{
public:
	Worker(std::function<void()> task);
	~Worker();
	bool IsFinished() const;

private:
	std::atomic<bool> m_isFinished = false;
	std::thread m_thread;
};
