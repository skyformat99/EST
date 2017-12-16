#pragma once

#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <type_traits>
#include <utility>
#include <vector>

namespace Task
{
	struct TaskManager
	{
		std::vector<std::function<void(void)>> tasks;
		std::vector<std::thread> threads;

		bool stoped; //正在析构
		std::condition_variable idle; //闲置信号
		std::mutex mtx; 
		std::condition_variable cond; //取任务信号
		size_t working; //工作的线程数量

		TaskManager() : stoped(false), working(0u)
		{
			//自动分配最大线程数
			size_t nthreads = std::thread::hardware_concurrency();
			nthreads = (nthreads == 0 ? 2 : nthreads);
			//生成工作线程,等待并处理任务
			for (size_t i{ 0u }; i < nthreads; i++)
			{
				threads.push_back(std::thread([this] {
					while (1)
					{
						std::function<void(void)> task;
						{
							std::unique_lock<std::mutex> ulk(mtx);
							cond.wait(ulk, [this] { return stoped || !tasks.empty(); }); //等待一个任务,当析构的时候也需要唤醒
							if (stoped) //析构,直接退出
								return;
							task = std::move(tasks.back()); //取得一个任务
							tasks.pop_back();
							working += 1u;
						}
						task();
						{
							std::lock_guard<std::mutex> lkg(mtx);
							working -= 1u;
							if (working == 0u && tasks.empty()) idle.notify_all();
						}
					}
				}));
			}
		}

		//保证线程退出
		~TaskManager()
		{
			{
				std::lock_guard<std::mutex> ulk(mtx);
				stoped = true; //通知所有工作线程退出
			}
			cond.notify_all();
			size_t nthreads = threads.size();
			for (size_t i{ 0ull }; i < nthreads; i++)
				if (threads[i].joinable()) threads[i].join();
		}

		//示例用法 add_task(add, 1, 2);
		template<typename F, typename... Ts>
		void add_task(F&& f, Ts&&... args)
		{
			std::function<void(void)> task = std::bind(std::forward<F>(f), std::forward<Ts>(args)...); //包装调用
			{
				std::lock_guard<std::mutex> ulk(mtx);
				tasks.emplace_back([task = std::move(task)]() { task(); });
			}
			cond.notify_one(); //派发给一个线程
		}

		void wait_idle() //等待所有任务完成
		{
			{
				std::lock_guard<std::mutex> lkg(mtx);
				if (0u == working && tasks.empty()) return; //如果已经是闲置的了,直接返回
			}
			std::unique_lock<std::mutex> ulk(mtx); //否则等待信号
			idle.wait(ulk, [this] { return working == 0u && tasks.empty(); });
		}

		inline static TaskManager& get() //饿汉式简单单例模式
		{
			static TaskManager This;
			return This;
		}
	};
	

	template<size_t step = 10000u>
	struct parallel_for
	{
		//分解成以step为长度的段落封装成Task
		template<typename F>
		inline void operator()(F&& f, size_t start, size_t end)
		{
			size_t from = start;
			while (from < end)
			{
				size_t to = std::min(end, from + step);
				TaskManager::get().add_task([f = std::forward<F>(f), from, to] { for (size_t i{ from }; i < to; i++) f(i); });
				from = to;
			}
		}
	};

}