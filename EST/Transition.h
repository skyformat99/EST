#pragma once

#include "Flow.h"
#include "Task.h"

namespace Transition
{

	//����������һ��״̬������
	//���ǿ�����״̬ת�ƺ������ƽ�����
	template<typename StateManager>
	struct Function
	{
		//��ͼ,���������д��ͻ�����
		Flow::FlowGraph<StateManager&> function;
		using Trait = typename StateManager::Trait;
		using States = MPL::concat_t<typename Trait::States, typename Trait::GlobalStates>;

		//������move: position,speed -> position
		//      bleed:     position -> health
		//��ʱmove��bleed������������غ�,���������,��Ҫ��ʽ����
		//in out��ʾ��ǰ����״̬��ת���� transition: in... -> out...
		//��������ͻ��ʱ��,ֱ���õȴ��������ͻ
		std::array<size_t, MPL::size<States>{}> out; //ͬʱֻ��һ����
		std::array<std::vector<size_t>, MPL::size<States>{}> in; //ͬʱ�����кܶ�д


		template<typename F>
		Function& operator >> (F&& f)
		{
			return combine(std::forward<F>(f));
		}

		//��һ��ת�ƺ���
		template<typename... Ts, typename F, typename loop = Task::parallel_for<10'000>>
		Function& combine(F&& f, loop loop = Task::parallel_for<10'000>{})
		{
			size_t id = function.create_node([f = std::forward<F>(f), loop]
			(StateManager& manager) 
			{ 
				manager.template transit<Ts...>(std::move(f), loop);
				return true; 
			});
			using input = typename StateManager::template transition_trait<F, Ts...>::ArgumentStates;
			using ReturnType = typename StateManager::template transition_trait<F, Ts...>::ReturnType;

			using output = std::conditional_t<std::is_same_v<ReturnType, void>, MPL::typelist<>, ReturnType>;
			MPL::for_types<input>([&](auto wrapper)
			{
				using type = typename decltype(wrapper)::type;
				if constexpr(!MPL::contain<type, output>::value)
				{
					constexpr size_t index = MPL::index<type, States>{};
					function.wait(id, out[index]); //in�ȴ�out
					in[index].push_back(id);
				}
			});

			MPL::for_types<output>([&](auto wrapper)
			{
				using type = typename decltype(wrapper)::type;
				constexpr size_t index = MPL::index<type, States>{};
				const size_t size = in[index].size();
				for (size_t i{ 0u }; i < size; i++)
					function.wait(id, in[index][i]); //out�ȴ�in
				function.wait(id, out[index]); //out�ȴ�out
				in[index].clear();
				out[index] = id;
			});

			return *this;
		}

		Function() :out(), in(), function([](size_t)
		{
			Task::TaskManager::get().wait_idle(); //�ȴ�step���
			return true;
		})

		{
			function.create_node([](StateManager& entities)
			{
				entities.tick();
				return true;
			});
		}

		void operator()(StateManager& entites) { function.run_once(entites); }
	};
}
