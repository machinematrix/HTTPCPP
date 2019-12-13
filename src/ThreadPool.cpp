#include "ThreadPool.h"
#include <thread>
#include <mutex>
#include <atomic>
#include <list>
#include <tuple>
#include <condition_variable>
#include <queue>
#include <functional>

class ThreadPool::Impl
{
	std::list<std::tuple<std::atomic<bool>, std::thread>> workers;
	
	std::queue<std::function<TaskCallback>> workQueue;
	std::mutex workMutex; //prevents data races in workStack
	std::condition_variable workAvailable, noWork;

	void workerProcedure(decltype(workers)::iterator myInfo);
public:
	Impl(std::size_t);
	~Impl();

	void addTask(const std::function<TaskCallback>&);
	void waitForTasks();
};

void ThreadPool::Impl::workerProcedure(decltype(workers)::iterator myInfo)
{
	while (std::get<0>(*myInfo).load())
	{
		std::unique_lock<std::mutex> lck(workMutex);

		if (!workQueue.empty()) {
			auto task = workQueue.front();
			workQueue.pop();
			lck.unlock();
			task();

			if(workQueue.empty())
				noWork.notify_all();
		}
		else
			workAvailable.wait(lck, [this, myInfo]() { return !workQueue.empty() || !std::get<0>(*myInfo).load(); });
	}
}

ThreadPool::Impl::Impl(std::size_t workerCount)
{
	for (size_t i = 0; i < workerCount; ++i)
	{
		auto newElem = workers.emplace(workers.end(), true, std::thread());
		std::get<1>(workers.back()) = std::move(std::thread(&Impl::workerProcedure, this, newElem));
	}
}

ThreadPool::Impl::~Impl()
{
	for (auto &workData : workers) {
		std::get<0>(workData).store(false);
	}
	workAvailable.notify_all();
	for (auto &workData : workers) {
		std::get<1>(workData).join();
	}
}

void ThreadPool::Impl::addTask(const std::function<TaskCallback> &task)
{
	std::unique_lock<std::mutex>(workMutex);
	workQueue.emplace(task);
	workAvailable.notify_one();
}

void ThreadPool::Impl::waitForTasks()
{
	std::unique_lock<std::mutex> lck(workMutex);
	noWork.wait(lck, [this]() { return workQueue.empty(); });
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