// EEC.cpp: 定义控制台应用程序的入口点。
//

#include "EntityState.h"
#include "Transition.h"
#include <iostream>
#include <chrono>
#include "SnakeExample.h"

//使用例子

struct complex { ~complex() {} };
using name = const char*; //一个名字状态,任何类型都可以成为状态
struct position { float x, y; }; //一个浮点数状态
struct boy; //纯申明的类型识别为标签
struct girl
{
	using state_type = EntityState::tag; //也可以显式指定为标签
	int anything; //不会储存,tag只作为标记
};
struct room
{
	using state_type = EntityState::global_state; //指定状态为全局状态
	const char* name;
};
using number = size_t;


void entity_state_example()
{
	using namespace EntityState;
	using ExampleManager = StateManager<name, position, boy, girl, room>; //创建一个管理器
	ExampleManager manager;

	//game.ref<room>().name = "bedroom"; 
	manager.transit([](room&r) {r.name = "bedroom"; }); //索引全局状态

	manager.create_entity([&manager](auto& e) //创建一个实体
	{
		manager.add<name>(e, "Jack"); //添加状态,带构造参数
		manager.add<position>(e, 1.f, 1.f);
		manager.add<boy>(e); //添加标签
	}); 
	
	manager.create_entity([&manager](auto& e)
	{
		manager.add<name>(e, "Julia");
		manager.add<position>(e, 7.f, 1.f);
		manager.add<girl>(e);
	});

	//game.create_group<name, position>();
	manager.tick(); //进入下一帧,使得所有修改生效

	//遍历name'和'position状态,并且读取全局状态'room'
	manager.transit([](name& n, position& pos, room& r) { std::cout << n << " is in "<<r.name<<" at: (" << pos.x << "," << pos.y << ");\n"; });
	manager.transit<boy>([](name& n) { std::cout << n << " is a boy;\n"; }); //遍历name状态,并额外指定标签
	manager.transit<girl>([](name& n) { std::cout << n << " is a girl;\n"; });
}

void task_example()
{
	using namespace Task;
	TaskManager& manager = TaskManager::get();
	for (size_t i{ 0u }; i < 10; i++)
		manager.add_task([i] { std::cout << i << " "; });
	manager.wait_idle();
}

void flow_example()
{
	/*构建一个流图
	1 ← 2 ← 3
	↑
	4
	*/
	using namespace Flow;
	FlowGraph<> graph;
	size_t id1 = graph.create_node([] { std::cout << "1\n"; return true; });
	size_t id2 = graph.create_node([] { std::cout << "2\n"; return true; });
	graph.wait(id2, id1);
	size_t id3 = graph.create_node([] { std::cout << "3\n"; return true; });
	graph.wait(id3, id2);
	size_t id4 = graph.create_node([] { std::cout << "4\n"; return true; });
	graph.wait(id4, id1);
	graph.run_once();
}

class Timer {
	std::chrono::high_resolution_clock::time_point start;
	const char *str;

public:
	Timer(const char *str)
		: start(std::chrono::high_resolution_clock::now()), str(str) {
		std::cout << str << " start... \n";
	}
	~Timer() {
		auto end = std::chrono::high_resolution_clock::now();
		std::cout << str << " finished, time used: "
			<< std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
			<< "ms\n\n";
	}
};


struct location
{
	int x, y;
};

struct velocity
{
	int x, y;
};

void performance_example()
{
	using namespace EntityState;
	using namespace Task;
	using namespace Flow;
	using ExampleManager = StateManager<location, velocity>; //创建一个管理器
	ExampleManager manager;
	{
		Timer timer("create 10m entity");
		for (size_t i{ 1u }; i <= 10'000'000u; i++) //这会占用大概1G的内存
		{
			manager.create_entity([&manager](auto& e)
			{ 
				manager.add<location>(e, 0, 0); //添加位置
				manager.add<velocity>(e, 0, 0); //添加速度
			});
			
		}
		manager.tick();
	}
	auto move = [](location& pos, velocity& vel) //根据速度移动位置
	{
		pos.x += vel.x;
		pos.y += vel.y;
	};
	{
		Timer timer("transit");
		manager.transit(move);
	}
	{
		Timer timer("empty tick");
		manager.tick();
	}
	{
		Timer timer("parallel transit (10k per task)");
		manager.transit(move, parallel_for<10'000u>{}); //瞬间完成
		TaskManager::get().wait_idle();
	}
	{
		Timer timer("parallel transit (1m per task)");
		manager.transit(move, parallel_for<1'000'000u>{}); //瞬间完成
		TaskManager::get().wait_idle();
	}
}


template<typename... Ts>
MPL::typelist<Ts...> output{};
#define out(state) return output<state>

void transition_example()
{
	using namespace EntityState;
	using namespace Task;
	using namespace Flow;
	using namespace Transition;
	using ExampleManager = StateManager<location, velocity>; 
	ExampleManager game; //创建一个游戏
	for (size_t i{ 1u }; i <= 10u; i++)
		game.create_entity([&game, i](auto& e) //创建实体
		{
			game.add<location>(e, 0, 0); //添加位置
			game.add<velocity>(e, 1, 1); //添加速度
		});
	//局部状态转移函数
	auto move = [](location& pos, velocity& vel) //输入状态
	{
		pos.x += vel.x; //根据速度移动位置
		pos.y += vel.y;
		out(location); //输出状态
	};
	//没有输出状态的函数被称作View
	auto show = [](location& pos) { std::cout << "(" << pos.x << "," << pos.y << ")" << " "; };
	Function<ExampleManager> transition; //全局状态转移函数
	transition >> move >> show; //定制管线      -> move -> show ->
	transition(game);           //计算世界状态  game -> transition -> newGame
}
#undef out

template<typename... Ts>
struct world
{
	using StateManager = EntityState::StateManager<Ts...>;
	StateManager entities;
	using Transition = Transition::Function<StateManager>;
	Transition transition;

	world() :transition() {}

	void set_transition(Transition&& tf) { transition = std::forward<Transition>(tf); }

	void tick_fixed() { transition(entities); }
};


int _cdecl main()
{
	using namespace EntityState;
	using namespace Task;
	using namespace Flow;
	Example::snake_example();
    return 0;
}

