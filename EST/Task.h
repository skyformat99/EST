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

		bool stoped; //��������
		std::condition_variable idle; //�����ź�
		std::mutex mtx; 
		std::condition_variable cond; //ȡ�����ź�
		size_t working; //�������߳�����

		TaskManager() : stoped(false), working(0u)
		{
			//�Զ���������߳���
			size_t nthreads = std::thread::hardware_concurrency();
			nthreads = (nthreads == 0 ? 2 : nthreads);
			//���ɹ����߳�,�ȴ�����������
			for (size_t i{ 0u }; i < nthreads; i++)
			{
				threads.push_back(std::thread([this] {
					while (1)
					{
						std::function<void(void)> task;
						{
							std::unique_lock<std::mutex> ulk(mtx);
							cond.wait(ulk, [this] { return stoped || !tasks.empty(); }); //�ȴ�һ������,��������ʱ��Ҳ��Ҫ����
							if (stoped) //����,ֱ���˳�
								return;
							task = std::move(tasks.back()); //ȡ��һ������
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

		//��֤�߳��˳�
		~TaskManager()
		{
			{
				std::lock_guard<std::mutex> ulk(mtx);
				stoped = true; //֪ͨ���й����߳��˳�
			}
			cond.notify_all();
			size_t nthreads = threads.size();
			for (size_t i{ 0ull }; i < nthreads; i++)
				if (threads[i].joinable()) threads[i].join();
		}

		//ʾ���÷� add_task(add, 1, 2);
		template<typename F, typename... Ts>
		void add_task(F&& f, Ts&&... args)
		{
			std::function<void(void)> task = std::bind(std::forward<F>(f), std::forward<Ts>(args)...); //��װ����
			{
				std::lock_guard<std::mutex> ulk(mtx);
				tasks.emplace_back([task = std::move(task)]() { task(); });
			}
			cond.notify_one(); //�ɷ���һ���߳�
		}

		void wait_idle() //�ȴ������������
		{
			{
				std::lock_guard<std::mutex> lkg(mtx);
				if (0u == working && tasks.empty()) return; //����Ѿ������õ���,ֱ�ӷ���
			}
			std::unique_lock<std::mutex> ulk(mtx); //����ȴ��ź�
			idle.wait(ulk, [this] { return working == 0u && tasks.empty(); });
		}

		inline static TaskManager& get() //����ʽ�򵥵���ģʽ
		{
			static TaskManager This;
			return This;
		}
	};
	

	template<size_t step = 10000u>
	struct parallel_for
	{
		//�ֽ����stepΪ���ȵĶ����װ��Task
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