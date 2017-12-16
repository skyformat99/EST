#pragma once

#include "Flow.h"
#include "Task.h"

namespace Transition
{

	//整个世界由一堆状态来描述
	//于是可以用状态转移函数来推进世界
	template<typename StateManager>
	struct Function
	{
		//流图,用来处理读写冲突和耦合
		Flow::FlowGraph<StateManager&> function;
		using Trait = typename StateManager::Trait;
		using States = MPL::concat_t<typename Trait::States, typename Trait::GlobalStates>;

		//例如有move: position,speed -> position
		//      bleed:     position -> health
		//这时move和bleed的输入输出有重合,产生了耦合,需要显式管理
		//in out表示当前整体状态的转换即 transition: in... -> out...
		//当发生冲突的时候,直接用等待来避免冲突
		std::array<size_t, MPL::size<States>{}> out; //同时只能一个读
		std::array<std::vector<size_t>, MPL::size<States>{}> in; //同时可以有很多写


		template<typename F>
		Function& operator >> (F&& f)
		{
			return combine(std::forward<F>(f));
		}

		//绑定一个转移函数
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
					function.wait(id, out[index]); //in等待out
					in[index].push_back(id);
				}
			});

			MPL::for_types<output>([&](auto wrapper)
			{
				using type = typename decltype(wrapper)::type;
				constexpr size_t index = MPL::index<type, States>{};
				const size_t size = in[index].size();
				for (size_t i{ 0u }; i < size; i++)
					function.wait(id, in[index][i]); //out等待in
				function.wait(id, out[index]); //out等待out
				in[index].clear();
				out[index] = id;
			});

			return *this;
		}

		Function() :out(), in(), function([](size_t)
		{
			Task::TaskManager::get().wait_idle(); //等待step完成
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
