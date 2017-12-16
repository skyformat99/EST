#pragma once
#include <vector>
#include <Windows.h>
#include "EntityState.h"
#include "Transition.h"
namespace Example
{
	struct Renderer {
		std::vector<char> deferred;
		std::vector<char> content;
		size_t access(size_t x, size_t y) { return (height - y - 1) * width + x; }
		size_t height, width;
		Renderer(size_t h, size_t w) {
			height = h;
			width = w;
			deferred.resize(h * w);
			content.resize(h * w);
			memset(deferred.data(), ' ', deferred.size() * sizeof(char));
			memset(content.data(), ' ', content.size() * sizeof(char));
			HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
			CONSOLE_CURSOR_INFO CursorInfo;
			GetConsoleCursorInfo(handle, &CursorInfo);
			CursorInfo.bVisible = false;
			SetConsoleCursorInfo(handle, &CursorInfo);
		}
		void draw(size_t x, size_t y, char v) { deferred[access(x, y)] = v; }
		void swapchain()
		{
			HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
			for (size_t x = 0u; x < width; x++)
				for (size_t y = 0u; y < height; y++) {
					char v;
					if (dirty(x, y, v)) {
						SetConsoleCursorPosition(hOut, { (SHORT)x * 2, (SHORT)y });
						putchar(v);
					}
				}
			std::swap(deferred, content);
			memset(deferred.data(), ' ', deferred.size() * sizeof(char));
		}
		bool dirty(size_t x, size_t y, char &v) {
			size_t pos = access(x, y);
			if (deferred[pos] != content[pos]) {
				v = deferred[pos];
				return true;
			}
			return false;
		}
	};
	template<typename F>
	void for_inputs(F f)
	{
		HANDLE handle = GetStdHandle(STD_INPUT_HANDLE);
		INPUT_RECORD records[128];
		DWORD nReads;
		DWORD nEvents;
		GetNumberOfConsoleInputEvents(handle, &nEvents);
		if (nEvents > 0u)
		{
			ReadConsoleInput(handle, records, 128, &nReads);
			for (size_t i = 0; i < nReads; i++) {
				if (records[i].EventType == KEY_EVENT) {
					auto Event = records[i].Event.KeyEvent;
					if (!Event.bKeyDown) continue;
					f(Event.uChar.AsciiChar);
				}
			}
		}
	}
	int clamp(int x, int limit)
	{
		while (x < 0) x += limit;
		return x % limit;
	}

	template<typename... Ts>
	MPL::typelist<Ts...> output{};
#define out(...) return output<__VA_ARGS__>

	//logic

	struct CAppearance { char v; };
	struct CVelocity { int x, y; };
	struct CLocation { int x, y; };
	struct CLifeTime { int n; };
	struct CSpawner { int life; };

	static Renderer renderer{ 28, 48 };
	//虽然将状态向外输出,但内部状态不被影响(信息守恒),也算是纯函数
	auto draw_entity = [](CLocation& loc, CAppearance& ap) { renderer.draw(loc.x, loc.y, ap.v); }; 
	auto draw_frame = [] { renderer.swapchain(); };
	//非纯函数,从外部读取状态(信息不守恒),需要特殊注意
	auto move_input= [](CVelocity& vel) { 
		CVelocity newVel = vel;
		for_inputs([&newVel](char in)
		{
			switch (in)
			{
			case 'a': newVel = { -1, 0 }; break;
			case 'd': newVel = { 1, 0 }; break;
			case 'w': newVel = { 0, -1 }; break;
			case 's': newVel = { 0, 1 }; break;
			}
		});
		if ((vel.x * newVel.x + vel.y * newVel.y) == 0) vel = newVel; //只能转向
		out(CVelocity);
	};
	auto move_entity = [](CLocation& loc, CVelocity& vel)
	{
		loc.x = clamp(loc.x + vel.x, 48); 
		loc.y = clamp(loc.y + vel.y, 28);
		out(CLocation);
	};
	//当涉及到实体的时候,会对Game产生依赖
	//利用这个模板技巧可以倒置依赖
	template<typename Game>
	struct Dependent 
	{
		using Entity = typename Game::Entity;
		Game& game;
		Dependent(Game& game) :game(game) {}
		auto life_time() //貌似没办法写成变量
		{
			return [this](CLifeTime& life, Entity& e)
			{
				if (--life.n < 0) game.kill_entity(e);
				out(CLifeTime);
			};
		}
		auto spawn()
		{
			return [this](CSpawner& sp, CLocation& loc)
			{
				game.create_entity([&](Entity& e)
				{
					game.add<CLifeTime>(e, sp.life);
					game.add<CLocation>(e, loc.x, loc.y);
					game.add<CAppearance>(e, '*');
				});
				out(CLifeTime, CLocation, CAppearance);
			};
		}
	};

	void snake_example()
	{
		using Game = EntityState::StateManager<CVelocity, CLocation, CAppearance, CLifeTime, CSpawner>;
		Game game;
		game.create_entity([&game](auto& e) //初始化世界
		{
			game.add<CVelocity>(e, 0, 0);
			game.add<CLocation>(e, 15, 8);
			game.add<CAppearance>(e, 'o');
			game.add<CSpawner>(e, 5);
		});
		
		Transition::Function<Game> transition;
		Dependent<Game> dependent{ game };
		//构建管线
		transition >> draw_frame >> dependent.life_time() >> move_input >> move_entity >> dependent.spawn() >> draw_entity;
		while (1) //帧循环
		{
			std::this_thread::sleep_for(std::chrono::milliseconds{ 1000 / 20 });
			transition(game);
		}
	}
#undef out
}