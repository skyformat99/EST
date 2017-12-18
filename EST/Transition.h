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

		template<typename T>
		struct decay_and_unwrap
		{
			static constexpr bool value = false;
			using type = std::decay_t<T>;
		};
		template<typename T>
		struct decay_and_unwrap<MPL::type_t<T>>
		{
			static constexpr bool value = true;
			using type = T;
		};
		template<typename T>
		using decay_and_unwrap_t = typename decay_and_unwrap<T>::type;
		template<typename T>
		using is_input = std::disjunction<std::is_const<std::remove_reference_t<T>>, decay_and_unwrap<T>>;
		template<typename T>
		using is_output = std::negation<is_input<T>>;
		//��һ��ת�ƺ���
		template<typename... Ts, typename F, typename loop = Task::parallel_for<10'000>>
		Function& combine(F&& f, loop loop = Task::parallel_for<10'000>{})
		{
			using FunctionTrait = MPL::generic_function_trait<std::decay_t<F>>;
			using ReturnType = typename FunctionTrait::return_type;
			using ArgumentType = typename FunctionTrait::argument_type;
			using ArgumentsInput = MPL::fliter_t<is_input, ArgumentType>;
			using ArgumentInputStates = MPL::intersection_t<MPL::map_t<decay_and_unwrap_t, ArgumentsInput>, typename Trait::AllStates>;
			using Input = MPL::union_t<MPL::typelist<Ts...>, ArgumentInputStates>;
			using ArgumentsOutput = MPL::intersection_t<MPL::map_t<std::decay_t, MPL::fliter_t<is_output, ArgumentType>>, typename Trait::AllStates>;;
			using ReturnCreation = std::conditional_t<std::is_same_v<ReturnType, void>, MPL::typelist<>, ReturnType>;
			using ReturnOutput = std::conditional_t<MPL::contain<typename Trait::Entity, ReturnCreation>::value, typename Trait::States, MPL::intersection_t<ReturnCreation, typename Trait::States>>;
			using Output = MPL::union_t<ReturnOutput, ArgumentsOutput>;

			std::cout << typeid(Input).name() << '\n';
			std::cout << typeid(Output).name() << "\n\n";
			
			size_t id;

			if constexpr(std::is_same_v<ReturnType, void>)
			{
				id = function.create_node([f = std::forward<F>(f), loop]
				(StateManager& manager)
				{
					manager.template transit<Ts...>(std::move(f), loop);
					return true;
				});
			}
			else
			{
				id = function.create_node([f = std::forward<F>(f)]
				(StateManager& manager)
				{
					manager.template transit<Ts...>(std::move(f), EntityState::simple_for{});
					return true;
				});
			}

			MPL::for_types<Input>([&](auto wrapper)
			{
				using type = typename decltype(wrapper)::type;
				if constexpr(!MPL::contain<type, Output>::value)
				{
					constexpr size_t index = MPL::index<type, States>{};
					function.wait(id, out[index]); //in�ȴ�out
					in[index].push_back(id);
				}
			});
			
			MPL::for_types<Output>([&](auto wrapper)
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
