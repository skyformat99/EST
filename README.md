# EST
EST 框架全称 Entity State Transition ,是一个基于 ECS 模型的 C++17 通用框架.
***
## 目录
* [前置需求](#前置需求)
* [如何编译](#如何编译)
* [核心特征](#核心特征)
* [基本思想](#基本思想)
    * [引入数据驱动编程](#引入数据驱动编程)
    * [什么是数据驱动编程](#什么是数据驱动编程)
    * [这带来了什么](#这带来了什么)
    * [这和EST有什么关系](#这和EST有什么关系)
* [如何使用EST](#如何使用)
    * [EST怎么组成](#EST怎么组成)
    * [EntityState](#EntityState)
    * [Transition](#Transition)
* [为什么使用EST](#为什么使用EST)
* [简单的示例](#简单示例)
* [关于自动并行](#关于自动并行)
***
### 前置需求
* M$VC
* Clang
* 任何其他支持C++17标准的编译器(未测试)
* 不依赖于任何第三方库
***
### 如何编译
* V$用户直接打开项目文件(.sln)编译即可
* 其他的查看[EST.cpp](/EST/EST.cpp)即可
***
### 核心特征
* 数据驱动
    * 读写透明
    * 并行友好
    * 缓存友好
    * 测试友好
    * 策划友好
* 函数式编程
    * 模式匹配
    * 副作用集中
    * 低耦合
* 模板元
    * 高抽象
    * 编译期代价
    * 开源(逃
    * 
***
### 基本思想

##### 引入数据驱动编程
对于游戏的上层,也就是游戏玩法逻辑框架层(gameplay framework),结构是非常的**易变**的
,所以人们经常用一些工具来帮忙进行抽象,比如状态机,AI行为树
,实际上,这种工具都是一种编程模式的实践:**数据驱动编程**.

##### 什么是数据驱动编程
简单来说数据驱动编程就是:程序的逻辑并不是通过硬编码实现的而是由数据和数据结构来决定的,从而在需要改变的程序逻辑的时候只需要改变数据和数据结构而不是代码.

对于数据驱动编程而且,考虑的是尽可能的少编写固定代码.这是 UNIX 哲学之一「提供机制，而不是策略」
##### 这带来了什么
在抛开面向对象后,数据和逻辑分离了,逻辑和逻辑分离了,甚至数据和其他数据分离了,只有逻辑依附在数据上.(当然这种情况有点太夸张了)

所以在数据驱动编程中,一切都是暴露出来的.而控制流和状态的暴露将带来巨大的灵活性.(在[为什么使用EST](#为什么使用EST)展示)
##### 这和EST有什么关系
EST 是一个抽象状态机, 是数据驱动编程的一个实践.


***
### 如何使用EST

##### EST怎么组成
EST 的全称为 Entity State Transition 
* 先分开来讲  
    * State (状态)就是数据
    * Entity (实体)是对象,对,这里还是会有对象,因为人更喜欢观察一个'个体'
    * Transition (转移)就是逻辑了.
* 再讲讲他们的关系
    * State 是'无知'的,它不知道任何东西,他只是一些数据
    * Entity 是一个'个体',它由很多 State (状态)描述
    * Transition 是一些逻辑,它和 State (状态)相互影响
    * 也就是 State <- Entity <- Transition 的依赖关系

从宏观上讲,EST把整个世界抽象成了状态机,由 State (状态)和 Transition (转移)来驱动.
***

EST的接口主要分为两个部分,其实现分别在
* [EntityState](/EST/EntityState.h)
* [Transition](/EST/Transition.h)  

***
### EntityState 

EntityState 提供实体和状态的高效管理

**如何定义 State?** 非常简单
```C++
using name = const char*; //一个名字状态,任何类型都可以成为状态
using number = size_t; //整数状态
struct position { float x, y; }; //一个具名结构体状态
```
**什么是 GlobalState?**  
State 不一定属于实体,也可以属于整个世界,类似于体温于气温的差别,这里我把它单独分别出来(当然世界也可以抽象为一个实体,但我不推荐这么做)  

**那么如何定义一个 GlobalState?** 非常简单
```C++
struct room
{
	using state_type = EntityState::global_state; //指定状态为全局状态
	const char* name;
};
```
**什么是 Flag?**    
State 不一定有一个有意义的值,它可能只是一个类别的标识,比如'人类'  

**如何定义 Flag?**  非常简单
```C++
struct boy; //纯申明的类型识别为标签
struct girl
{
	using state_type = EntityState::tag; //也可以显式指定为标签
	int anything; //不会储存,tag只作为标记
};
```
**如何定义一个 Entity?**   
很抱歉,你不能!  
在 EST 里,Entity 是一个动态的对象.  

**那该怎么做?**   
首先,需要一个世界为容器
```C++
using World = EntityState::StateManager<name, position, boy, girl, room>; //创建一个管理器
World manager;
```
然后,我们可以开始在这个世界里面'造人'了
```C++
auto& e = manager.create_entity() //创建一个实体
manager.add<name>(e, "Jack"); //添加状态,带构造参数
manager.add<position>(e, 1.f, 1.f);
manager.add<boy>(e); //添加标签
```
**注意!** 请不要保存 Entity 的引用,这是一个临时的位置

    如果有需要,应该使用manager.get_handle和manager.get_entity来安全的存取 Entity 的引用   
对 Entity 可进行的的操作一共有
* 添加 State 或 Flag
* 删除 State 或 Flag
* 创建 Entity
* 杀死 Entity  

需要注意的是**杀死Entity不会立即生效!** 要使得他们被清理,你需要  
```C++
manager.tick(); //进入下一帧,使得所有修改生效
```


**那么世界构建好了,我们要如何用逻辑去和它交互呢?**  使用逻辑函数

**如何定义这个逻辑函数?** 非常简单  
任何函数,伪函数,参数要求是**任何状态或Entity**

```C++
auto whos_in = [](name& n, position& pos, room& r) 
{ 
	std::cout << n << " is in " << r.name << " at: (" << pos.x << "," << pos.y << ");\n"; 
};
```

**逻辑函数如何匹配实体?**  使用模式匹配    
观察whos_in的声明  
whos_in (name, position, room)  
其中room是一个 GlobalState,会直接匹配到 game.room  
剩下的whos_in (name, position) 
对所有实体进行模式匹配,假设有三个Entity

* E(name, position)
* D(name)
* F(name, someelse, position)

则 whos_in 会和 E, F 匹配上,而不会和 D 匹配上
在匹配上后,参数将会被对应的状态填补,**特别的,Entity 参数会取得 Entity 本身**   
如whos_in (name, position, room, Entity&) 的 Entity& 会分别匹配为 E 和 F

**如何应用逻辑函数?** 非常简单
```C++
manager.transit(whos_in);
```
可以在应用的时候指定 Tag
```C++
manager.transit<boy>([](name& n) { std::cout << n << " is a boy;\n"; }); //额外指定标签
manager.transit([](name& n, MPL::type_t<girl>) { std::cout << n << " is a girl;\n"; }); //也可以这么指定,但是不推荐
```
***
### Transition 
Transition 模块对零散的转移函数进行高效的管理  

**首先需要注意状态转移函数**  
状态转移函数是特殊的逻辑函数  
你需要定义状态转移函数的输入(const)和输出(mutable--即默认状态)  
```C++
auto move_entity = [](CLocation& loc, const CVelocity& vel)
{
	loc.x = clamp(loc.x + vel.x, 48); 
	loc.y = clamp(loc.y + vel.y, 28);
};
```
特别的,当你需要**创建**状态或者实体的时候,你需要在返回处声明
```C++
auto spawn()
{
	return [this](const CSpawner& sp,const CLocation& loc)
	{
		auto& e = game.create_entity();
		game.add<CLifeTime>(e, sp.life);
		game.add<CLocation>(e, loc.x, loc.y);
		game.add<CAppearance>(e, '*');
		out(Entity);
	};
}
```
其中out的定义为
```C++
template<typename... Ts>
MPL::typelist<Ts...> output{};
#define out(...) return output<__VA_ARGS__>
```
**什么是世界转移函数?**  
世界转移函数能够转移整个世界的状态来推进到新的世界  
世界转移函数由基本的状态转移函数组合而成
```C++
transition(game);
```
**如何构建一个世界转移函数?**
首先定义世界的转移函数
```C++
Transition::Function<Game> transition;
```
然后构建管线,组合状态转移函数
```C++
transition >> move_entity >> ...;
```
也可以用 combine 接口来组合
```C++
transition.combine(move_entity).combine<...>(...)...;
```

***
### 简单的示例
这个实例是一个极简的游戏,玩家控制一条在控制台移动的蛇;

首先我们实现显示的逻辑,即在屏幕上 entity 对应的位置打印出其对应的字符
```C++
struct CAppearance { char v; }; //显示的字符
struct CLocation { int x, y; };
auto draw_entity = [](const CLocation& loc, const CAppearance& ap) { renderer.draw(loc.x, loc.y, ap.v); }; 
```
easy  
接下来实现 entity 的移动,根据速度移动位置即可(还要防止跑出屏幕)
```C++
struct CVelocity { int x, y; }; 
auto move_entity = [](CLocation& loc, const CVelocity& vel)
{
	loc.x = clamp(loc.x + vel.x, 48); 
	loc.y = clamp(loc.y + vel.y, 28);
};
```
简直就是口头叙述的直接翻译!  
接下来让它根据玩家的操作动起来,通过wasd来改变方向,且不能掉头,只能转向
```C++
auto move_input = [](CVelocity& vel) { 
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
};
```
emmm,硬要说难度的话就只有点乘判断垂直了(逃  
这时我们的蛇还只有头没有身体,来加上身体  
**这里的实现思路比较有意思了,我们不把蛇身当做'蛇身',而是当做残影形成的拖尾,残影持续的时间越长,蛇就越长.**  
那么首先实现残影的消散,这里注意了,残影消散意味着要杀掉一个实体,而杀掉一个实体需要调用StateManager提供的接口,但是我们并不想依赖于固定的StateManager实例,于是这里使用模板技巧来倒置依赖
```C++
template<typename Game>
struct Dependent 
{
	using Entity = typename Game::Entity;
	Game& game;
	Dependent(Game& game) :game(game) {}
	...
};
```
然后在这里实现残影的消散, 即倒计时到零的时候杀死实体
```C++
struct CLifeTime { int n; }; //计时死亡
struct Dependent 
{
    ...
    auto life_time() //貌似没办法写成变量
    {
    	return [this](CLifeTime& life, Entity& e)
    	{
    	    if (--life.n < 0) game.kill_entity(e);
        };
    }
    ...
};
```
easy  
最后一个逻辑是生成残影,残影需要三个状态,位置,样子,持续时间
```C++
struct CSpawner { int life; };
struct Dependent 
{
    ...
    auto spawn()
    {
	    return [this](CSpawner& sp, CLocation& loc)
	    {
	    	auto& e = game.create_entity();
	    	game.add<CLifeTime>(e, sp.life);
	    	game.add<CLocation>(e, loc.x, loc.y);
	    	game.add<CAppearance>(e, '*');
	    	out(Entity);
	    };
    }
    ...
};
```
**记得声明实体的创建**  
还有一点需要注意的是,残影会持续 lifetime+1 帧,因为删除会延后一帧  
至此,所有的逻辑都已经实现完成了,是时候构建世界让他们运作起来了  
第一步,构建世界并放置一个"蛇头",它可以根据输入移动(CVelocity,CLocation),它可以显示(CAppearance),它可以生成残影(CSpawner).
```C++
using Game = EntityState::StateManager<CVelocity, CLocation, CAppearance, CLifeTime, CSpawner>;
Game game;
auto& e = game.create_entity([&game](auto& e) //初始化世界
game.add<CVelocity>(e, 0, 0);
game.add<CLocation>(e, 15, 8);
game.add<CAppearance>(e, 'o');
game.add<CSpawner>(e, 5);
```
第二步,拼接组建我们的逻辑  
生成残影要在移动之前,残影消散要在生成残影之后,移动输入需要在移动之前,渲染需要在所有动作之后,嗯,一切都是如此清晰.
```C++
Transition::Function<Game> transition;
Dependent<Game> dependent{ game };
//构建管线
transition >> dependent.spawn() >> dependent.life_time() >> move_input >> move_entity >> draw_entity;
```
最后, 直接循环跑起来就皆大欢喜了.
```C++
while (1) //帧循环
{
	std::this_thread::sleep_for(std::chrono::milliseconds{ 1000 / 20 });
	transition(game);
	renderer.swapchain();
}
```
完整代码见
[SnakeExample](/EST/SnakeExample.h)

***
### 为什么使用EST
从上面的例子中来说
* 对于逻辑(转移)
    * 很多逻辑是**纯函数**,这意味着它可以**方便的进行独立测试**.
        * 比如 assertEq(move_entity({0, 0}, {1, 1}) == {1, 1})
	    * 如 spawn 函数,也能造一个假的 game 来进行测试
	    * 如 input 函数,也容易造一个假的输入在进行测试
    * 所有逻辑都是分开写的,它们互相不知道其他的逻辑,只有我们在最后组合的时候才会考虑它们之间的关系,这意味着**方便的多人协作**.
    * 既然组合逻辑那么容易,那么改变组合也很容易,我们可以在**任何时刻(甚至运行时)拆卸**,安装或者重新排列逻辑.
        * 比如 transition >> dependent.spawn() >> dependent.life_time() >> move_input >> draw_entity; (去掉了 move_entity)  
    那么就不能移动了
        * 比如去掉 dependent.life_time(), 那么残影就不会消失了,蛇变成了无限长.
        * 比如添加一个 change_look 逻辑,就可以每帧改变蛇头的样子.
    * 逻辑对于状态的读写是透明的,这意味着**无锁的自动并行化**
* 对于实体与状态
    * 所有状态都是简单数据,**序列化很轻松**
    * 在实现上,状态都是放在连续内存里的,这意味着 **CPU Cache 友好**
    * 实体由描述它的状态来定义,而逻辑又依附于状态上,那么结果就是,实体的行为由依附的状态来定义,这意味着**实体的行为由描述它的状态的状态来定义** (数据驱动的神威)
    * 状态能够定义实体的行为,也意味着**策划的表格也能轻易定义出行为(逃**
        * 如加上 CSpawner 就能拥有残影
        * 如加上 CLocation 和 CVelocity 就可以根据输入移动(这里获得了两个行为)
    * 而实体的类型又是动态的,这意味着我们可以 **随时改变一个对象的行为**
        * 如加上一个 CBleed 持续掉血
        * 如加上一个 CSpeedUp 移速翻倍
        * 如加上一个 CLifeTime 自动消失

从上面可以看出来EST有着难以想象的灵活性与高性能  
**你若倒戈卸甲,以礼来降,仍不失封侯之位,国安民乐,岂不美哉?**  
**最后祝你.身体健康.**

### 关于自动并行
* 状态输入阻塞相同 State 输出,内部多线程
* 状态输出阻塞相同 State 输入和输出,内部多线程
* 实体创建相当于所有 State(**不包含GlobalState**) 输出,内部单线程
* 状态创建相当于相同状态输出(导致重分配),内部单线程
* 删除状态,杀死实体不阻塞
