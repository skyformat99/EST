#pragma once
#include <functional>

namespace Flow
{
	template<typename... TArgs>
	struct FlowGraph
	{
		
		struct Node //组成一个DAG
		{
			std::function<bool(TArgs...)> task;
			//记录需要满足多少个前置条件
			//当前置条件全部满足的时候触发
			size_t ref; 
			size_t ref_; //运行时计数
			std::vector<size_t> then;
		};

		std::function<bool(size_t)> control;

		std::vector<size_t> roots; //从这些节点开始,也就是ref为0的节点

		std::vector<Node> nodes;

		FlowGraph() :FlowGraph([](size_t) {return true; }) {}

		template<typename F>
		FlowGraph(F&& f) : control(std::forward<F>(f)) {}

		template<typename F>
		size_t create_node(F&& f) //创建一个节点
		{
			nodes.push_back(Node{});
			Node& n{ nodes.back() };
			n.ref = 0;
			n.task = std::forward<F>(f);
			size_t h = nodes.size() - 1;
			roots.push_back(h);
			return h;
		}

		//申请一个前置条件
		//注意起始状态不允许有前置条件,这会导致环的形成
		void wait(size_t h, size_t before) 
		{ 
			auto it = std::find(roots.begin(), roots.end(), h);
			if(it!=roots.end()) roots.erase(it);
			nodes[before].then.push_back(h); 
			++nodes[h].ref;
		}

		void run_once(TArgs... args)
		{
			size_t nodeSize = nodes.size();
			for (size_t i{ 0u }; i < nodeSize; i++) nodes[i].ref_ = nodes[i].ref;
			size_t rootSize = roots.size();
			std::vector<size_t> doing_;
			std::vector<size_t> pending_;

			auto *doing = &doing_, *pending = &pending_; 
			for (size_t i{ 0u }; i < rootSize; i++) doing->push_back(roots[i]);
			
			for (size_t i{ 0u }; !doing->empty(); i++)
			{
				size_t doingSize = doing->size();
				for (size_t i{ 0u }; i < doingSize; i++)
				{
					Node& n = nodes[(*doing)[i]];
					size_t thenSize = n.then.size();
					if (!nodes[(*doing)[i]].task(args...)) continue;
					for (size_t i{ 0u }; i < thenSize; i++)
						if (0u == --nodes[n.then[i]].ref_) //满足了所有前置条件,触发
							pending->push_back(n.then[i]);
				}
				if (!control(i)) break; //每一个step会有一个回调
				doing->clear();
				std::swap(doing, pending); //下一个step
			}
		}
	};
}