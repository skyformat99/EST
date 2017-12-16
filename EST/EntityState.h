#pragma once

#include <tuple>
#include <vector>
#include <bitset>
#include <algorithm>
#include <functional>
#include <array>
#include <mutex>
#include <shared_mutex>
#include "MPL.h"

#define property static constexpr auto 


namespace EntityState
{
	//因为辣鸡std::bit不支持高于64位的常量优化
	//所以自己写一个
	template<size_t bits>
	struct bitset
	{
		//参照stl,尽量对齐到寄存器
		using Elem = std::conditional_t<(bits < 32u), uint32_t, uint64_t>;
		property wordbits = (std::ptrdiff_t)(CHAR_BIT * sizeof(Elem));
		property words = (std::ptrdiff_t)(((bits - 1) / wordbits) + 1);
		using data_type = std::array<Elem, words>;
		data_type data;
		template<size_t pos, bool val>
		void draw()
		{
			if constexpr (val) data[pos / wordbits] |= (Elem)1 << pos % wordbits;
			else data[pos / wordbits] &= ~((Elem)1 << pos % wordbits);
		}

		template<size_t pos>
		constexpr bool get() const { return  data[pos / wordbits] & ( (Elem)1 << pos % wordbits ); }

		void reset() { for (std::ptrdiff_t pos = words - 1; pos >= 0; --pos) data[pos] = (Elem)0u; }

		constexpr bool operator==(const bitset& other) const
		{
			for (std::ptrdiff_t pos = words - 1; pos >= 0; --pos)
				if (data[pos] != other.data[pos]) return false;
			return true;
		}

		// (a & b) == b
		constexpr bool contain(const bitset& other) const
		{
			for (std::ptrdiff_t pos = words - 1; pos >= 0; --pos)
				if ((data[pos] & other.data[pos]) != other.data[pos]) return false;
			return true;
		}

		template<size_t bitpos, size_t pos>
		static constexpr Elem make_bit()
		{
			constexpr size_t start = pos * wordbits;
			constexpr size_t end = (pos + 1) * wordbits;
			constexpr bool valid = bitpos >= start && bitpos < end;
			return valid ? ((Elem)1u << (bitpos - start)) : (Elem)0u;
		}
		template<size_t pos, size_t... bitpos>
		static constexpr Elem make_elem(std::index_sequence<bitpos...>) { return (0u | ... | make_bit<bitpos, pos>()); }
		template<typename T, size_t... pos>
		static constexpr data_type make_data(T bp, std::index_sequence<pos...>) { return { {make_elem<pos>(bp)}... }; }
		//编译期: 将指定位置1构造一个bitset
		template<size_t... bitpos> 
		static constexpr bitset make() { return { make_data(std::index_sequence<bitpos...>{}, std::make_index_sequence<words>()) }; }

		constexpr bitset(data_type d) : data(d) {}
		constexpr bitset() : data() {}
	};

	
	struct state;
	struct tag;
	struct global_state;

	template <typename T, typename = void> struct state_type_of {
		using type = state;
	};

	template <typename T>
	struct state_type_of<T, std::void_t<typename T::state_type>> {
		using type = typename T::state_type;
	};



	template<
		typename... Ts
	>
	struct Trait //提取类型信息
	{
		using ARG = MPL::typelist<Ts...>; //状态和标签都是决定实体类型的标记

		template<typename T>
		using is_tag_ = std::disjunction< 
			std::negation<MPL::complete<T>> , //非完整类型
			std::conjunction< MPL::complete<T>, std::is_same<typename state_type_of<T>::type, tag> >>; //完整类型,显式指定type为tag

		template<typename T>
		using is_global_state_ = std::conjunction< MPL::complete<T>, std::is_same<typename state_type_of<T>::type, global_state> >;

		template<typename T>
		using is_state_ = std::conjunction< MPL::complete<T>, std::is_same<typename state_type_of<T>::type, state> >;

		using States = MPL::fliter_t<is_state_, ARG>;
		using Tags = MPL::fliter_t<is_tag_, ARG>;
		using GlobalStates = MPL::fliter_t<is_global_state_, ARG>;

		using Signs = MPL::concat_t<States, Tags>;
		using AllStates = MPL::concat_t<States, GlobalStates>;

		static_assert(MPL::rewrap_t<std::conjunction, MPL::map_t<std::is_pod, MPL::concat_t<States, GlobalStates>>>::value,"state should only be pod."); //进行一次检查


		template<typename T> struct is_state { property value = MPL::contain_v<T, States>; };
		template<typename T> struct is_tag { property value = MPL::contain_v<T, Tags>; };
		template<typename T> struct is_global_state { property value = MPL::contain_v<T, GlobalStates>; };

		property stateSize { MPL::size<States>{} };
		property tagSize { MPL::size<Tags>{} };
		property globalStateSize { MPL::size<GlobalStates>{} };

		using Bitset = bitset<stateSize + tagSize>; //计算占用bit数

		template<typename T>
		property sign_index() { return MPL::template index<T, Signs>{}; } //这样便能取到对应的id

		//获取一个标记的bits
		template<typename... Ts>
		struct signBits { inline static constexpr Bitset value{ Bitset::template make<sign_index<Ts>()...>() }; };

		//状态索引,从状态映射到size_t,对应每个状态在storage里面的位置
		//在添加状态之前不会分配状态内存,惰性分配
		//在删除状态之后不会删除状态内存,内存膨胀范围可接受
		template<typename... Ts>
		struct Reference 
		{
			using Types = MPL::typelist<Ts...>;
			size_t ref[sizeof...(Ts)]; //索引每个状态,每个索引有64bit的代价
			Bitset cap; //是否已经"拥有"了一个状态,避免重复分配

			Reference() { cap.reset(); }

			template<typename T>
			inline bool caped() const noexcept { return cap.template get<MPL::index<T, Types>{}>(); }

			template<typename T>
			inline void add(const size_t n) noexcept
			{
				property index { MPL::index<T, Types>{} };
				ref[index] = n; cap.template draw<index, true>();
			}

			template<typename T>
			inline size_t get() const noexcept { return ref[MPL::index<T, Types>{}]; }
		};
		using StateReference = MPL::rewrap_t<Reference, States>;


		//用vector储存同类状态,用tuple组合起来
		//分开储存的策略取决于
		//1,一个system一般只遍历少数状态,把同种状态放在一起提高cache命中
		//2,一个entity一般只有少数状态,节约内存
		//坏处在于Entity的大小膨胀,降低了cache友好度(实际影响需要测试
		struct StateStornge 
		{
			template<typename... Ts>
			using TupleOfVector = std::tuple<std::vector<Ts>...>;
			MPL::rewrap_t<TupleOfVector, States> data;

			template<typename T>
			inline T& ref(size_t index) noexcept { return std::get<std::vector<T>>(data)[index]; }

			template<typename T, typename... Ts>
			inline size_t add(Ts&&... args) noexcept
			{
				std::vector<T>& vec = std::get<std::vector<T>>(data);
				vec.emplace_back(T{std::forward<Ts>(args)...});
				return vec.size() - 1;
			}
		};
	};

	//实体
	template<typename Trait>
	struct Entity
	{
		typename Trait::Bitset bits; //可以认为是一种简单的反射能力
		typename Trait::StateReference compRef; //状态数据索引
		size_t handleIndex; //指向句柄,和句柄形成双向引用
		bool alive = false; //是否存活,用于简易辣鸡回收机制
	};

	struct Handle //句柄
	{
		size_t index; //指向实际数据
		size_t valid = 0; //用于判断失效(因为句柄可能被重用)
	};

	struct simple_for
	{
		template<typename F>
		inline void operator()(F&& f, size_t start, size_t end) { for (size_t i{ start }; i < end; i++) f(i); }
	};

	template<typename... Ts>
	struct StateManager
	{
		using Trait = Trait<Ts...>;
		using Entity = Entity<Trait>;
		typename Trait::StateStornge states; //存放所有状态数据
		std::vector<Entity> entities; //所有的实体数据
		size_t size = 0; //数量
		std::vector<Entity> delayed; //延迟的创建
		std::mutex delayed_mtx;
		//单例,根据暴雪的实践,40%的组件为单例
		MPL::rewrap_t<std::tuple, typename Trait::GlobalStates> globals;
		//因为Entity会因为辣鸡清扫变更位置,所以需要
		//实体句柄,用于作为Entity的确定索引
		std::vector<Handle> handles; 
		using Group = std::pair<typename Trait::Bitset, std::vector<size_t>>;
		//首先明白一个事实,大部分状态只存在于少数实体中
		//所以遍历某个状态的时候可能会有大量未命中
		//于是我们使用分组来进行优化,每个组由一个bits确定,如: G(c1,c2)
		//所有包含一个bits的实体组成一个分组,如: E(c1,c2)会在G(c1)和G(c2)和G(c1,c2)里
		//每次遍历的时候只取最小的子集
		std::vector<Group> groups;

		StateManager() noexcept
		{
			init_groups(typename Trait::States{});
		}

		//默认为所有单独的状态分别创建一个分组
		template<typename... Ts>
		void init_groups(MPL::typelist<Ts...>) noexcept
		{
			std::initializer_list<int> _{
				(groups.push_back(Group{ Trait::template signBits<Ts>::value, {} }), 1)...
			}; (void)_;
		}

		//选择一个最小的分组
		std::vector<size_t>& get_group(typename Trait::Bitset all) const noexcept
		{
			const size_t groupsize{ groups.size() };
			size_t minSize = 0ull - 1ull; //最大值
			std::vector<size_t>* select = nullptr;
			for (size_t i{ 0u }; i < groupsize; i++) //简单线性扫描
			{
				const auto bits = groups[i].first;
				const bool wasInGroup = all.contain(bits);
				if (!wasInGroup) continue;
				auto& group = const_cast<std::vector<size_t>&>(groups[i].second);
				const size_t size = group.size();
				if (size < minSize)
				{
					select = &group;
					minSize = size;
				}
			}
			return (*select);
		}

		//创建一个分组
		template<typename... Ts>
		void create_group() 
		{
			property bits = Trait::template signBits<Ts...>::value;
			groups.push_back(Group{ bits,{} });
			auto& group = groups[groups.size() - 1].second;
			for (size_t i{ 0u }; i < size; i++)
			{
				Entity& e(entities[i]);
				//因为当前帧内的情况不可预知
				//判断是否应该已经在组里
				const bool wasInGroup = ((bits&e.bits) == bits); 
				if (wasInGroup) group.push_back(e.handleIndex);
			}
		}

		template<typename T>
		struct decay_and_unwrap
		{
			property wrapped = false;
			using type = std::decay_t<T>;
		};
		template<typename T>
		struct decay_and_unwrap<MPL::type_t<T>>
		{
			property wrapped = true;
			using type = T;
		};
		template<typename T>
		using decay_and_unwrap_t = typename decay_and_unwrap<T>::type;

		//辅助类,用来给函数填充参数
		template<typename... Ts>
		struct caller 
		{
			template<typename T>
			decltype(auto) get_helper(Entity& e, StateManager& manager) 
			{
				if constexpr(std::is_same_v<T, Entity&>) //对于Handle进行特殊处理
					return e;
				else if constexpr(Trait::template is_global_state<std::decay_t<T>>::value)
					return manager.ref<std::decay_t<T>>();
				else if constexpr(decay_and_unwrap<T>::wrapped)
					return T{};
				else	return manager.ref<std::decay_t<T>>(e); 
			}

			template<typename F>
			void operator()(Entity& e, StateManager& manager, F&& f)
			{
				f(get_helper<Ts>(e, manager)...);
			}
		};

		//辅助类,用来给只转移全局状态的函数填充参数
		template<typename... Ts>
		struct gcaller
		{
			template<typename T>
			decltype(auto) get_helper(StateManager& manager)
			{
				return manager.ref<std::decay_t<T>>();
			}

			template<typename F>
			void operator()(StateManager& manager, F&& f)
			{
				f(get_helper<Ts>(manager)...);
			}
		};

		

		//提取状态转移函数的信息
		template<typename F, typename... Ts>
		struct transition_trait
		{
			using FunctionTrait = MPL::generic_function_trait<std::decay_t<F>>;
			using ReturnType = typename FunctionTrait::return_type;
			using ArgumentType = typename FunctionTrait::argument_type; //提取函数所需要的参数类型
			//根据函数参数类型和模板参数一起计算出标记
			using DecayArgument = MPL::map_t< decay_and_unwrap_t, ArgumentType >; //先去掉引用
			using ArgumentStates = MPL::intersection_t<DecayArgument, typename Trait::AllStates>;
			using ArgumentSign = MPL::intersection_t<DecayArgument, typename Trait::Signs>; //去掉Entity&
			using Signs = MPL::union_t<MPL::typelist<Ts...>, ArgumentSign>; //然后和模板参数做个并集得到标记列表
		};

		//使用函数F进行状态转移
		//例子: manager.transit<tag>([](state2& c2, Handle h, state1& c1) { .... });
		//遍历满足 <tag,state1,state2> 的实体进行状态转移
		template<typename... Ts, typename F, typename loop = simple_for>
		inline void transit(F&& f, loop loop= simple_for{}) noexcept
		{
			using TransitionTrait = transition_trait<F, Ts...>;
			using ArgumentType = typename TransitionTrait::ArgumentType;
			using Signs = typename TransitionTrait::Signs;
			if constexpr(MPL::size<Signs>{} == 0u) 
			{
				if constexpr(MPL::contain_v<Entity&, ArgumentType>) //如果只需要实体,则遍历所有实体
					loop([this, f = std::forward<F>(f)](size_t i)
					{
						Entity& e{ entities[i] };
						MPL::rewrap_t<caller, ArgumentType>{}(e, *this, std::move(f));
					}, 0u, size);
				else //如果没有标记,那么只处理全局状态
					MPL::rewrap_t<gcaller, ArgumentType>{}(*this, std::forward<F>(f)); 
			}
			else
			{
				property bits { MPL::rewrap_t<Trait::template signBits, Signs>::value }; //根据列表计算出bits

				std::vector<size_t>& group{ get_group(bits) };
				size_t groupsize{ group.size() };
				if (groupsize <= size * 2 / 3) 
					loop([this, &group, f = std::forward<F>(f)](size_t i)
					{
						Entity& e{ entities[handles[group[i]].index] };
						if (e.bits.contain(bits))
							MPL::rewrap_t<caller, ArgumentType>{}(e, *this, std::move(f));
					}, 0u, groupsize);
				else //如果分组太大,则直接遍历效率高(此时命中率高)
					loop([this, f = std::forward<F>(f)](size_t i)
					{
						Entity& e{ entities[i] };
						if (e.bits.contain(bits))
							MPL::rewrap_t<caller, ArgumentType>{}(e, *this, std::move(f));
					}, 0u, size);
			}
		}

		//扩容
		void grow_to(size_t newCapacity)
		{
			const auto capacity = entities.capacity();
			entities.resize(newCapacity);
			handles.resize(newCapacity);
			for (auto i(capacity); i < newCapacity; i++)
			{
				auto& e(entities[i]);
				e.handleIndex = i; //初始化句柄和实体的双向引用
				handles[i].index = i;
				e.bits.reset();
				e.bits.reset();
			}
		}

		//尝试扩容
		void grow() 
		{
			const auto capacity = entities.capacity(); 
			if (size < capacity) return;
			size_t newCapacity = capacity * 2 + 50;
			grow_to(newCapacity);
		}

		//新建一个实体,这里并不会改动size
		//我们把创建延后到下一帧生效
		//创建过程可能会引起delayed扩容,必须全程加锁,不能直接返回实体
		template<typename F>
		void create_entity(F&& f) noexcept
		{
			std::unique_lock<std::mutex> ulk(delayed_mtx);
			delayed.emplace_back(Entity{});
			auto& e = delayed.back();
			e.bits.reset(); 
			e.alive = true; 
			//延迟创建的不管句柄
			std::forward<F>(f)(e);
		}

		//重生实体(此时已经是新的实体了)
		inline void refresh_entity(Entity& e)
		{
			e.bits.reset(); //清空数据,不必真正的去清除,只需要清除bits
			e.alive = true; //标记存活
			++handles[e.handleIndex].valid; //为每个实体分配唯一标号
		}

		inline void kill_entity(Entity& e) const noexcept
		{
			e.alive = false;
			e.bits.reset();
		}

		//注意创建是被延时了的,本帧内创建或杀死的实体并没有真正的生效,所以不能用alive
		inline bool is_alive(const Entity& e) const noexcept { handles[e.handleIndex].index < size; }

		//获取句柄,其实是句柄的句柄
		inline Handle get_handle(const Entity& e) const noexcept { return Handle{ e.handleIndex, handles[e.handleIndex].valid }; }

		//两次解引用,句柄的句柄->实体的句柄->实体
		inline Entity& get_entity(const Handle& handle) const noexcept { return entities[handles[handle.index].index]; }

		//句柄是否有效取决于
		//1,实体是否已死,
		//2,句柄是否已经被重用(实体已死)
		inline bool is_valid(const Handle& handle) const noexcept { return handles[handle.index].index<size && handles[handle.index].valid == handle.valid; }

		
		template<typename T, bool is_state> //如果是flag,什么都不做
		struct add_state_helper{ template<typename... Ts> void operator()(Ts&...) {} };

		template<typename T> //否则是state
		struct add_state_helper<T, true>
		{
			template<typename... Ts>
			void operator()(StateManager& manager, Entity& e, Ts&&... args)
			{
				if (!e.compRef.template caped<T>()) //为状态惰性分配
					e.compRef.template add<T>(manager.states.template add<T>(std::forward<Ts>(args)...));
			}
		};

		//添加一个状态或一个标签
		template<typename T, typename... Ts>
		inline void add(Entity& e, Ts&&... args) noexcept
		{
			auto pre = e.bits;
			e.bits.template draw<Trait::template sign_index<T>(), true>();
			regroup(e, pre);
			add_state_helper<T, Trait::template is_state<T>::value>{}(*this, e, std::forward<Ts>(args)...); //gtmd M$VC 没法用 constexpr if
		}

		//获取一个状态的引用
		template<typename T>
		inline T& ref(Entity& e) noexcept
		{ 
			static_assert(Trait::template is_state<T>::value, "can not 'get' Tag, use 'has' instead."); //只有状态能取得值
			return states.template ref<T>(e.compRef.template get<T>());
		}

		//获取一个全局状态的引用
		template<typename T>
		inline T& ref() noexcept { return std::get<T>(globals); }

		//擦除一个状态或一个标签
		template<typename T>
		inline static void erase(Entity& e) noexcept 
		{ 
			auto pre = e.bits;
			e.bits.draw<Trait::sign_index<T>(), false>(); 
			regroup(e, pre);
		}

		//查询一个状态或一个标签
		template<typename T>
		inline static bool has(Entity& e) noexcept { return e.bits.get<Trait::sign_index<T>()>(); }

		//当实体的标记发生改变的时候,需要更新分组
		void regroup(Entity& e, typename Trait::Bitset pre)
		{
			const auto groupsize{ groups.size() };
			for (size_t i{ 0u }; i < groupsize; i++)
			{
				const auto bits = groups[i].first;

				const bool wasInGroup = pre.contain(bits);
				const bool isEnteringGroup = e.bits.contain(bits);
				const bool isLeavingGroup = !isEnteringGroup;

				auto& group = groups[i].second;
				if (!wasInGroup && isEnteringGroup)
				{
					group.push_back(e.handleIndex);
				}
				else if (wasInGroup && isLeavingGroup)
				{
					group.erase(std::find(group.begin(), group.end(), e.handleIndex));
				}
			}
		}

		//一帧,进行辣鸡收集,并使得创建生效
		void tick() 
		{ 
			size_t iD{ 0 }, iA{ size - 1 };
			if (size == 0u) goto complete;
			//把所有"存活"的实体收集起来,进行一次辣鸡收集
			while (true) {
				for (; true; ++iD) { //从前往后找到一个死亡的实体
					if (iD > iA) goto complete; //跳出两层循环用了goto
					if (!entities[iD].alive) //找到一个死亡的实体
					{
						if (!delayed.empty()) //直接在尸体上重生
						{
							auto& eD = entities[iD];
							eD = std::move(delayed.back());
							delayed.pop_back();
							++handles[eD.handleIndex].valid;
						}
						else break; //否则用一个存活的实体来填补空穴
					}
				}

				for (; true; --iA) { //从后往前找到一个存活的实体,以填补空穴
					if (entities[iA].alive) break;
					if (iA <= iD) goto complete;
				}
				auto& eA = entities[iA];
				auto& eD = entities[iD];

				std::swap(eA, eD); //交换他们,填补空穴
				std::swap(handles[eA.handleIndex], handles[eD.handleIndex]); //因为位置互换了,句柄指向也要互换

				++iD;
				--iA;
			}
		complete:
			size = iD;
			while (!delayed.empty()) //新建实体
			{
				grow();
				auto free{ size++ };
				auto& e{ entities[free] };
				e = std::move(delayed.back());
				delayed.pop_back();
				++handles[e.handleIndex].valid;
			}

			//没有必要保留这些内存
			delayed.shrink_to_fit();
		}
	};
}

#undef property