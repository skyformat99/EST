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
    * [这为带来了什么](#这为我们带来了什么)
    * [这和EST有什么关系](#这和EST有什么关系)
    * [EST怎么组成](#EST怎么组成)
* [如何使用EST](#如何使用)
    * [EntityState](#EntityState)
    * [Transition](#Transition)
* [简单的示例](#简单示例)
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
对于游戏的上层,也就是游戏玩法逻辑框架层(gameplay framework),结构是非常的**多变**的
,所以人们经常用一些工具来帮忙进行抽象,比如状态机,AI行为树
,实际上,这种工具都是一种编程模式的实例:**数据驱动编程**.

##### 什么是数据驱动编程
简单来说数据驱动编程就是:程序的逻辑并不是通过硬编码实现的而是由数据和数据结构来决定的,从而在需要改变的程序逻辑的时候只需要改变数据和数据结构而不是代码.

对于数据驱动编程而且,考虑的是尽可能的少编写固定代码.这是 UNIX 哲学之一「提供机制，而不是策略」

##### 这带来了什么
在抛开面向对象后,数据和逻辑分离了,逻辑和逻辑分离了,甚至数据和其他数据分离了,只有逻辑依附在数据上.(当然这种情况有点太夸张了)

所以在数据驱动编程中,一切都是暴露出来的.而控制流和状态的暴露将带来巨大的灵活性.

##### 这和EST有什么关系
EST 是数据驱动编程的一个实践.

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
### 如何使用EST
EST主要分为两个部分
* [EntityState](/EST/EntityState.h)
* [Transition](/EST/Transition.h)  

### EntityState 

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
manager.create_entity([&manager](auto& e) //创建一个实体
{
	manager.add<name>(e, "Jack"); //添加状态,带构造参数
	manager.add<position>(e, 1.f, 1.f);
	manager.add<boy>(e); //添加标签
}); 
```
对 Entity 可进行的的操作一共有
* 添加 State 或 Flag
* 删除 State 或 Flag
* 创建 Entity
* 杀死 Entity  

需要注意的是**杀死和创建Entity不会立即生效!**要使得他们生效,你需要  
```C++
manager.tick(); //进入下一帧,使得所有修改生效
```


**那么世界构建好了,我们要如何用逻辑去和它交互呢?**  使用状态转移函数

**如何定义转移函数?** 非常简单  
任何函数,伪函数

```C++
auto whos_in = [](name& n, position& pos, room& r) 
{ 
	std::cout << n << " is in " << r.name << " at: (" << pos.x << "," << pos.y << ");\n"; 
};
```

**转移函数如何匹配状态?**  使用模式匹配  
观察whos_in的声明  
whos_in (name, position, room)  
其中room是一个 GlobalState,会直接匹配到 GlobalState  
那么剩下  
whos_in (name, position)  
进行模式匹配,假设有两个Entity

* E(name, position)
* D(name)
* F(name, someelse, position)

则 whos_in 会和 E, F 匹配上,而不会和 D 匹配上

**如何应用转移函数?** 非常简单  
```C++
manager.transit(whos_in);
```
可以在转移函数指定 Tag
```C++
manager.transit<boy>([](name& n) { std::cout << n << " is a boy;\n"; }); //额外指定标签
```

### Transition 

Transition 模块对零散的转移函数进行整理  
**有什么不同?**  
你需要定义状态转移函数的输入(通过参数类型隐式的)和输出(通过返回类型显式的)
```C++
auto move_entity = [](CLocation& loc, CVelocity& vel)
{
	loc.x = clamp(loc.x + vel.x, 48); 
	loc.y = clamp(loc.y + vel.y, 28);
	out(CLocation);
};
```
其中out的定义为
```C++
template<typename... Ts>
MPL::typelist<Ts...> output{};
#define out(...) return output<__VA_ARGS__>
```

**如何使用?**
```C++
using Game = EntityState::StateManager<CVelocity, CLocation, CAppearance, CLifeTime, CSpawner>;
Game game;
```
在构建了世界后
```C++
Transition::Function<Game> transition;
```
定义世界的转移函数
然后构建管线,制定逻辑的顺序
```C++
transition >> draw_frame >> dependent.life_time() >> move_input >> move_entity >> dependent.spawn() >> draw_entity;
```
也可以用 combine 函数
```C++
transition.combine(draw_frame);
```

**使用很简单**
```C++
transition(game);
```

### 简单的示例
这个实例中,一条在控制台移动的蛇,其'蛇形'其实是残影构成的
说明TODO
[SnakeExample](/EST/SnakeExample.h)
