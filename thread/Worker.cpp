#include "Worker.h"

Worker::Worker(std::function<void()> task)
{
	m_thread = std::thread([this, task]() 
		{
			task();
			m_isFinished = true;
		}
	);
}

Worker::~Worker()
{
	if(m_thread.joinable())
	{
		m_thread.join();
	}
}

bool Worker::IsFinished() const
{
	return m_isFinished.load();
}
