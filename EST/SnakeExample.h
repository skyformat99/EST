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
		void set(size_t x, size_t y, char v) { deferred[access(x, y)] = v; }
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


	template<typename... Ts>
	MPL::typelist<Ts...> output{};
#define out(...) return output<__VA_ARGS__>

	struct CAppearance { char v; };
	struct CVelocity { int x, y; };
	struct CLocation { int x, y; };
	struct CLifeTime { int n; };
	struct CSpawner { int life; };

	static Renderer renderer{ 14, 24 };
	auto draw_entity = [](CLocation& loc, CAppearance& ap) { renderer.set(loc.x, loc.y, ap.v); };
	auto draw_frame = [] { renderer.swapchain(); };
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
		loc.x += vel.x;
		loc.y += vel.y;
		out(CLocation);
	};
	void snake_example()
	{
		using Game = EntityState::StateManager<CVelocity, CLocation, CAppearance, CLifeTime, CSpawner>;
		Game game;
		using Entity = Game::Entity;
		game.create_entity([&game](auto& e)
		{
			game.add<CVelocity>(e, 0, 0);
			game.add<CLocation>(e, 15, 8);
			game.add<CAppearance>(e, 'o');
			game.add<CSpawner>(e, 5);
		});
		auto life_time = [&game](CLifeTime& life, Entity& e) //非纯函数,涉及状态的增减
		{
			if (--life.n < 0) game.kill_entity(e);
			out(CLifeTime);
		};
		auto spawn = [&game](CSpawner& sp, CLocation& loc)
		{
			game.create_entity([&](Entity& e)
			{
				game.add<CLifeTime>(e, sp.life);
				game.add<CLocation>(e, loc.x, loc.y);
				game.add<CAppearance>(e, '*');
			});
			out(CLifeTime, CLocation, CAppearance);
		};
		Transition::Function<Game> transition;
		
		transition >> draw_frame >> life_time >> move_input >> move_entity >> spawn >> draw_entity;
		while (1) {
			std::this_thread::sleep_for(std::chrono::milliseconds{ 1000 / 10 });
			transition(game);
		}
	}
#undef out
}