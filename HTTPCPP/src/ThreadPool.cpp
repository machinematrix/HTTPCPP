#include "ThreadPool.h"
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <tuple>
#include <condition_variable>
#include <queue>
#include <functional>

class ThreadPool::Impl
{
	std::atomic<bool> mWorkFlag;
	std::vector<std::thread> mWorkers;
	std::queue<std::function<TaskCallback>> mWorkQueue;
	std::mutex mWorkMutex; //prevents data races in workStack
	std::condition_variable mWorkAvailable, mNoWork;
	unsigned mBusy;

	void workerProcedure();
public:
	Impl(std::size_t);
	~Impl();

	void addTask(const std::function<TaskCallback>&);
	void waitForTasks();
};

void ThreadPool::Impl::workerProcedure()
{
	while (mWorkFlag.load())
	{
		std::unique_lock<std::mutex> lck(mWorkMutex);

		if (!mWorkQueue.empty())
		{
			auto task = mWorkQueue.front();
			mWorkQueue.pop();

			++mBusy;
			lck.unlock();
			task();
			lck.lock();
			--mBusy;

			if(mWorkQueue.empty())
				mNoWork.notify_all();
		}
		else
			mWorkAvailable.wait(lck, [this]() { return !mWorkQueue.empty() || !mWorkFlag.load(); });
	}
}

ThreadPool::Impl::Impl(std::size_t workerCount)
	:mWorkFlag(true)
	,mBusy(0)
{
	for (size_t i = 0; i < workerCount; ++i)
	{
		auto newElem = mWorkers.emplace(mWorkers.end(), std::thread(&Impl::workerProcedure, this));
	}
}

ThreadPool::Impl::~Impl()
{
	mWorkFlag.store(false);
	mWorkAvailable.notify_all();
	for (auto &workData : mWorkers) {
		workData.join();
	}
}

void ThreadPool::Impl::addTask(const std::function<TaskCallback> &task)
{
	std::unique_lock<std::mutex>(workMutex);
	mWorkQueue.emplace(task);
	mWorkAvailable.notify_one();
}

void ThreadPool::Impl::waitForTasks()
{
	std::unique_lock<std::mutex> lck(mWorkMutex);
	mNoWork.wait(lck, [this]() { return mWorkQueue.empty() && !mBusy; });
}

//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

ThreadPool::ThreadPool(size_t workerCount)
	:mThis(new Impl(workerCount))
{}

ThreadPool::~ThreadPool() noexcept = default;

ThreadPool::ThreadPool(ThreadPool&&) noexcept = default;

ThreadPool& ThreadPool::operator=(ThreadPool&&) noexcept = default;

void ThreadPool::addTask(const std::function<TaskCallback> &callback)
{
	mThis->addTask(callback);
}

void ThreadPool::waitForTasks()
{
	mThis->waitForTasks();
}