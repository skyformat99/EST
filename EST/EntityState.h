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
	//��Ϊ����std::bit��֧�ָ���64λ�ĳ����Ż�
	//�����Լ�дһ��
	template<size_t bits>
	struct bitset
	{
		//����stl,�������뵽�Ĵ���
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
		//������: ��ָ��λ��1����һ��bitset
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
	struct Trait //��ȡ������Ϣ
	{
		using ARG = MPL::typelist<Ts...>; //״̬�ͱ�ǩ���Ǿ���ʵ�����͵ı��

		template<typename T>
		using is_tag_ = std::disjunction< 
			std::negation<MPL::complete<T>> , //����������
			std::conjunction< MPL::complete<T>, std::is_same<typename state_type_of<T>::type, tag> >>; //��������,��ʽָ��typeΪtag

		template<typename T>
		using is_global_state_ = std::conjunction< MPL::complete<T>, std::is_same<typename state_type_of<T>::type, global_state> >;

		template<typename T>
		using is_state_ = std::conjunction< MPL::complete<T>, std::is_same<typename state_type_of<T>::type, state> >;

		using States = MPL::fliter_t<is_state_, ARG>;
		using Tags = MPL::fliter_t<is_tag_, ARG>;
		using GlobalStates = MPL::fliter_t<is_global_state_, ARG>;

		using Signs = MPL::concat_t<States, Tags>;
		using AllStates = MPL::concat_t<States, GlobalStates>;

		static_assert(MPL::rewrap_t<std::conjunction, MPL::map_t<std::is_pod, MPL::concat_t<States, GlobalStates>>>::value,"state should only be pod."); //����һ�μ��


		template<typename T> struct is_state { property value = MPL::contain_v<T, States>; };
		template<typename T> struct is_tag { property value = MPL::contain_v<T, Tags>; };
		template<typename T> struct is_global_state { property value = MPL::contain_v<T, GlobalStates>; };

		property stateSize { MPL::size<States>{} };
		property tagSize { MPL::size<Tags>{} };
		property globalStateSize { MPL::size<GlobalStates>{} };

		using Bitset = bitset<stateSize + tagSize>; //����ռ��bit��

		template<typename T>
		property sign_index() { return MPL::template index<T, Signs>{}; } //��������ȡ����Ӧ��id

		//��ȡһ����ǵ�bits
		template<typename... Ts>
		struct signBits { inline static constexpr Bitset value{ Bitset::template make<sign_index<Ts>()...>() }; };

		//״̬����,��״̬ӳ�䵽size_t,��Ӧÿ��״̬��storage�����λ��
		//�����״̬֮ǰ�������״̬�ڴ�,���Է���
		//��ɾ��״̬֮�󲻻�ɾ��״̬�ڴ�,�ڴ����ͷ�Χ�ɽ���
		template<typename... Ts>
		struct Reference 
		{
			using Types = MPL::typelist<Ts...>;
			size_t ref[sizeof...(Ts)]; //����ÿ��״̬,ÿ��������64bit�Ĵ���
			Bitset cap; //�Ƿ��Ѿ�"ӵ��"��һ��״̬,�����ظ�����

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


		//��vector����ͬ��״̬,��tuple�������
		//�ֿ�����Ĳ���ȡ����
		//1,һ��systemһ��ֻ��������״̬,��ͬ��״̬����һ�����cache����
		//2,һ��entityһ��ֻ������״̬,��Լ�ڴ�
		//��������Entity�Ĵ�С����,������cache�Ѻö�(ʵ��Ӱ����Ҫ����
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

	//ʵ��
	template<typename Trait>
	struct Entity
	{
		typename Trait::Bitset bits; //������Ϊ��һ�ּ򵥵ķ�������
		typename Trait::StateReference compRef; //״̬��������
		size_t handleIndex; //ָ����,�;���γ�˫������
		bool alive = false; //�Ƿ���,���ڼ����������ջ���
	};

	struct Handle //���
	{
		size_t index; //ָ��ʵ������
		size_t valid = 0; //�����ж�ʧЧ(��Ϊ������ܱ�����)
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
		typename Trait::StateStornge states; //�������״̬����
		std::vector<Entity> entities; //���е�ʵ������
		size_t size = 0; //����
		std::vector<Entity> delayed; //�ӳٵĴ���
		std::mutex delayed_mtx;
		//����,���ݱ�ѩ��ʵ��,40%�����Ϊ����
		MPL::rewrap_t<std::tuple, typename Trait::GlobalStates> globals;
		//��ΪEntity����Ϊ������ɨ���λ��,������Ҫ
		//ʵ����,������ΪEntity��ȷ������
		std::vector<Handle> handles; 
		using Group = std::pair<typename Trait::Bitset, std::vector<size_t>>;
		//��������һ����ʵ,�󲿷�״ֻ̬����������ʵ����
		//���Ա���ĳ��״̬��ʱ����ܻ��д���δ����
		//��������ʹ�÷����������Ż�,ÿ������һ��bitsȷ��,��: G(c1,c2)
		//���а���һ��bits��ʵ�����һ������,��: E(c1,c2)����G(c1)��G(c2)��G(c1,c2)��
		//ÿ�α�����ʱ��ֻȡ��С���Ӽ�
		std::vector<Group> groups;

		StateManager() noexcept
		{
			init_groups(typename Trait::States{});
		}

		//Ĭ��Ϊ���е�����״̬�ֱ𴴽�һ������
		template<typename... Ts>
		void init_groups(MPL::typelist<Ts...>) noexcept
		{
			std::initializer_list<int> _{
				(groups.push_back(Group{ Trait::template signBits<Ts>::value, {} }), 1)...
			}; (void)_;
		}

		//ѡ��һ����С�ķ���
		std::vector<size_t>& get_group(typename Trait::Bitset all) const noexcept
		{
			const size_t groupsize{ groups.size() };
			size_t minSize = 0ull - 1ull; //���ֵ
			std::vector<size_t>* select = nullptr;
			for (size_t i{ 0u }; i < groupsize; i++) //������ɨ��
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

		//����һ������
		template<typename... Ts>
		void create_group() 
		{
			property bits = Trait::template signBits<Ts...>::value;
			groups.push_back(Group{ bits,{} });
			auto& group = groups[groups.size() - 1].second;
			for (size_t i{ 0u }; i < size; i++)
			{
				Entity& e(entities[i]);
				//��Ϊ��ǰ֡�ڵ��������Ԥ֪
				//�ж��Ƿ�Ӧ���Ѿ�������
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

		//������,����������������
		template<typename... Ts>
		struct caller 
		{
			template<typename T>
			decltype(auto) get_helper(Entity& e, StateManager& manager) 
			{
				if constexpr(std::is_same_v<T, Entity&>) //����Handle�������⴦��
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

		//������,������ֻת��ȫ��״̬�ĺ���������
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

		

		//��ȡ״̬ת�ƺ�������Ϣ
		template<typename F, typename... Ts>
		struct transition_trait
		{
			using FunctionTrait = MPL::generic_function_trait<std::decay_t<F>>;
			using ReturnType = typename FunctionTrait::return_type;
			using ArgumentType = typename FunctionTrait::argument_type; //��ȡ��������Ҫ�Ĳ�������
			//���ݺ����������ͺ�ģ�����һ���������
			using DecayArgument = MPL::map_t< decay_and_unwrap_t, ArgumentType >; //��ȥ������
			using ArgumentStates = MPL::intersection_t<DecayArgument, typename Trait::AllStates>;
			using ArgumentSign = MPL::intersection_t<DecayArgument, typename Trait::Signs>; //ȥ��Entity&
			using Signs = MPL::union_t<MPL::typelist<Ts...>, ArgumentSign>; //Ȼ���ģ��������������õ�����б�
		};

		//ʹ�ú���F����״̬ת��
		//����: manager.transit<tag>([](state2& c2, Handle h, state1& c1) { .... });
		//�������� <tag,state1,state2> ��ʵ�����״̬ת��
		template<typename... Ts, typename F, typename loop = simple_for>
		inline void transit(F&& f, loop loop= simple_for{}) noexcept
		{
			using TransitionTrait = transition_trait<F, Ts...>;
			using ArgumentType = typename TransitionTrait::ArgumentType;
			using Signs = typename TransitionTrait::Signs;
			if constexpr(MPL::size<Signs>{} == 0u) 
			{
				if constexpr(MPL::contain_v<Entity&, ArgumentType>) //���ֻ��Ҫʵ��,���������ʵ��
					loop([this, f = std::forward<F>(f)](size_t i)
					{
						Entity& e{ entities[i] };
						MPL::rewrap_t<caller, ArgumentType>{}(e, *this, std::move(f));
					}, 0u, size);
				else //���û�б��,��ôֻ����ȫ��״̬
					MPL::rewrap_t<gcaller, ArgumentType>{}(*this, std::forward<F>(f)); 
			}
			else
			{
				property bits { MPL::rewrap_t<Trait::template signBits, Signs>::value }; //�����б�����bits

				std::vector<size_t>& group{ get_group(bits) };
				size_t groupsize{ group.size() };
				if (groupsize <= size * 2 / 3) 
					loop([this, &group, f = std::forward<F>(f)](size_t i)
					{
						Entity& e{ entities[handles[group[i]].index] };
						if (e.bits.contain(bits))
							MPL::rewrap_t<caller, ArgumentType>{}(e, *this, std::move(f));
					}, 0u, groupsize);
				else //�������̫��,��ֱ�ӱ���Ч�ʸ�(��ʱ�����ʸ�)
					loop([this, f = std::forward<F>(f)](size_t i)
					{
						Entity& e{ entities[i] };
						if (e.bits.contain(bits))
							MPL::rewrap_t<caller, ArgumentType>{}(e, *this, std::move(f));
					}, 0u, size);
			}
		}

		//����
		void grow_to(size_t newCapacity)
		{
			const auto capacity = entities.capacity();
			entities.resize(newCapacity);
			handles.resize(newCapacity);
			for (auto i(capacity); i < newCapacity; i++)
			{
				auto& e(entities[i]);
				e.handleIndex = i; //��ʼ�������ʵ���˫������
				handles[i].index = i;
				e.bits.reset();
				e.bits.reset();
			}
		}

		//��������
		void grow() 
		{
			const auto capacity = entities.capacity(); 
			if (size < capacity) return;
			size_t newCapacity = capacity * 2 + 50;
			grow_to(newCapacity);
		}

		//�½�һ��ʵ��,���ﲢ����Ķ�size
		//���ǰѴ����Ӻ���һ֡��Ч
		//�������̿��ܻ�����delayed����,����ȫ�̼���,����ֱ�ӷ���ʵ��
		template<typename F>
		void create_entity(F&& f) noexcept
		{
			std::unique_lock<std::mutex> ulk(delayed_mtx);
			delayed.emplace_back(Entity{});
			auto& e = delayed.back();
			e.bits.reset(); 
			e.alive = true; 
			//�ӳٴ����Ĳ��ܾ��
			std::forward<F>(f)(e);
		}

		//����ʵ��(��ʱ�Ѿ����µ�ʵ����)
		inline void refresh_entity(Entity& e)
		{
			e.bits.reset(); //�������,����������ȥ���,ֻ��Ҫ���bits
			e.alive = true; //��Ǵ��
			++handles[e.handleIndex].valid; //Ϊÿ��ʵ�����Ψһ���
		}

		inline void kill_entity(Entity& e) const noexcept
		{
			e.alive = false;
			e.bits.reset();
		}

		//ע�ⴴ���Ǳ���ʱ�˵�,��֡�ڴ�����ɱ����ʵ�岢û����������Ч,���Բ�����alive
		inline bool is_alive(const Entity& e) const noexcept { handles[e.handleIndex].index < size; }

		//��ȡ���,��ʵ�Ǿ���ľ��
		inline Handle get_handle(const Entity& e) const noexcept { return Handle{ e.handleIndex, handles[e.handleIndex].valid }; }

		//���ν�����,����ľ��->ʵ��ľ��->ʵ��
		inline Entity& get_entity(const Handle& handle) const noexcept { return entities[handles[handle.index].index]; }

		//����Ƿ���Чȡ����
		//1,ʵ���Ƿ�����,
		//2,����Ƿ��Ѿ�������(ʵ������)
		inline bool is_valid(const Handle& handle) const noexcept { return handles[handle.index].index<size && handles[handle.index].valid == handle.valid; }

		
		template<typename T, bool is_state> //�����flag,ʲô������
		struct add_state_helper{ template<typename... Ts> void operator()(Ts&...) {} };

		template<typename T> //������state
		struct add_state_helper<T, true>
		{
			template<typename... Ts>
			void operator()(StateManager& manager, Entity& e, Ts&&... args)
			{
				if (!e.compRef.template caped<T>()) //Ϊ״̬���Է���
					e.compRef.template add<T>(manager.states.template add<T>(std::forward<Ts>(args)...));
			}
		};

		//���һ��״̬��һ����ǩ
		template<typename T, typename... Ts>
		inline void add(Entity& e, Ts&&... args) noexcept
		{
			auto pre = e.bits;
			e.bits.template draw<Trait::template sign_index<T>(), true>();
			regroup(e, pre);
			add_state_helper<T, Trait::template is_state<T>::value>{}(*this, e, std::forward<Ts>(args)...); //gtmd M$VC û���� constexpr if
		}

		//��ȡһ��״̬������
		template<typename T>
		inline T& ref(Entity& e) noexcept
		{ 
			static_assert(Trait::template is_state<T>::value, "can not 'get' Tag, use 'has' instead."); //ֻ��״̬��ȡ��ֵ
			return states.template ref<T>(e.compRef.template get<T>());
		}

		//��ȡһ��ȫ��״̬������
		template<typename T>
		inline T& ref() noexcept { return std::get<T>(globals); }

		//����һ��״̬��һ����ǩ
		template<typename T>
		inline static void erase(Entity& e) noexcept 
		{ 
			auto pre = e.bits;
			e.bits.draw<Trait::sign_index<T>(), false>(); 
			regroup(e, pre);
		}

		//��ѯһ��״̬��һ����ǩ
		template<typename T>
		inline static bool has(Entity& e) noexcept { return e.bits.get<Trait::sign_index<T>()>(); }

		//��ʵ��ı�Ƿ����ı��ʱ��,��Ҫ���·���
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

		//һ֡,���������ռ�,��ʹ�ô�����Ч
		void tick() 
		{ 
			size_t iD{ 0 }, iA{ size - 1 };
			if (size == 0u) goto complete;
			//������"���"��ʵ���ռ�����,����һ�������ռ�
			while (true) {
				for (; true; ++iD) { //��ǰ�����ҵ�һ��������ʵ��
					if (iD > iA) goto complete; //��������ѭ������goto
					if (!entities[iD].alive) //�ҵ�һ��������ʵ��
					{
						if (!delayed.empty()) //ֱ����ʬ��������
						{
							auto& eD = entities[iD];
							eD = std::move(delayed.back());
							delayed.pop_back();
							++handles[eD.handleIndex].valid;
						}
						else break; //������һ������ʵ�������Ѩ
					}
				}

				for (; true; --iA) { //�Ӻ���ǰ�ҵ�һ������ʵ��,�����Ѩ
					if (entities[iA].alive) break;
					if (iA <= iD) goto complete;
				}
				auto& eA = entities[iA];
				auto& eD = entities[iD];

				std::swap(eA, eD); //��������,���Ѩ
				std::swap(handles[eA.handleIndex], handles[eD.handleIndex]); //��Ϊλ�û�����,���ָ��ҲҪ����

				++iD;
				--iA;
			}
		complete:
			size = iD;
			while (!delayed.empty()) //�½�ʵ��
			{
				grow();
				auto free{ size++ };
				auto& e{ entities[free] };
				e = std::move(delayed.back());
				delayed.pop_back();
				++handles[e.handleIndex].valid;
			}

			//û�б�Ҫ������Щ�ڴ�
			delayed.shrink_to_fit();
		}
	};
}

#undef property