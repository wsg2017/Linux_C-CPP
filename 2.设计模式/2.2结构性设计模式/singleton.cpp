//单例模式
/*
 * 保证一个类仅有一个实例，并提供一个该实例的全局访问点。  ——《设计模式》GoF 
 > 单例模式的对象通常和应用程序的生命周期是一模一样的，对于用户而言，我们不希望用户去new它或者delete它，
 因此我们需要对用户隐藏它的构造和析构函数（private），我们应用程序退出的时候才会去释放它
*/

//版本一 
/*
问题：
    1.不会调用析构函数，会导致内存泄漏
    2.多线程会出现问题，可能会创建多个_instance
*/
class Singleton {
public:
    static Singleton * GetInstance() {  //静态成员函数，不需要创建类对象
        if (_instance == nullptr) {
            _instance = new Singleton();
        }
        return _instance;
    }
private:
    Singleton(){}; //构造
    ~Singleton(){};
    Singleton(const Singleton &) = delete; //禁止拷⻉构造
    Singleton& operator=(const Singleton&) = delete;//禁止拷贝赋值构造
    Singleton(Singleton &&) = delete;//禁止移动构造
    Singleton& operator=(Singleton &&) = delete;//禁止移动拷贝构造
    static Singleton * _instance;   //静态成员函数只能访问静态成员变量
};

Singleton* Singleton::_instance = nullptr;  //静态成员需要初始化

//版本二
/*
    解决了内存泄漏问题(atexit)，并没有解决线程安全问题
*/
class Singleton {
public:
    static Singleton * GetInstance() {
        if (_instance == nullptr) {
            _instance = new Singleton();        
            atexit(Destructor);     //当程序正常终止时，调用指定的函数 Destructor
            //您可以在任何地方注册你的终止函数，但它会在程序终止的时候(main退出)被调用。
        }
        return _instance;
    }
private:
    static void Destructor() {
        if (nullptr != _instance) { //
            delete _instance;
            _instance = nullptr;
        }
    }
    Singleton(){}; //构造
    ~Singleton(){};
    Singleton(const Singleton &) = delete; //拷⻉构造
    Singleton& operator=(const Singleton&) = delete;//拷贝赋值构造
    Singleton(Singleton &&) = delete;//移动构造
    Singleton& operator=(Singleton &&) = delete;//移动拷贝构造
    static Singleton * _instance;
};
Singleton* Singleton::_instance = nullptr; //静态成员需要初始化
// 还可以使⽤ 内部类，智能指针来解决； 此时还有线程安全问题

//版本三 c++ 98
/*
解决了线程安全问题，但多核cpu会优化执行序，导致问题
*/
#include <mutex>

class Singleton { // 懒汉模式 lazy load
public:
    static Singleton * GetInstance() {
        // std::lock_guard<std::mutex> lock(_mutex); // 3.1 切换线程
        if (_instance == nullptr) {
            std::lock_guard<std::mutex> lock(_mutex); // 3.2 锁加到这，效率更高
            if (_instance == nullptr) { //double check
                _instance = new Singleton();
                // 1. 分配内存
                // 2. 调用构造函数
                // 3. 返回指针
                // 多线程环境下 cpu reorder操作,多核cpu会优化执行序，可能的执行顺序为132，半初始化问题，有指针但数据未被初始化
                atexit(Destructor);
            }
        }
        return _instance;
    }
private:
    static void Destructor() {
        if (nullptr != _instance) {
            delete _instance;
            _instance = nullptr;
        }
    }
    Singleton(){}; //构造
    ~Singleton(){};
    Singleton(const Singleton &) = delete; //拷⻉构造
    Singleton& operator=(const Singleton&) = delete;//拷贝赋值构造
    Singleton(Singleton &&) = delete;//移动构造
    Singleton& operator=(Singleton &&) = delete;//移动拷贝构造
    static Singleton * _instance;
    static std::mutex _mutex;
};
Singleton* Singleton::_instance = nullptr;//静态成员需要初始化
std::mutex Singleton::_mutex; //互斥锁初始化

//版本四 c++ 11
// volitile
#include <mutex>
#include <atomic>

class Singleton {
public:
    static Singleton * GetInstance() {
        Singleton* tmp = _instance.load(std::memory_order_relaxed); //可以看见其他核心（线程）最新操作的数据
        std::atomic_thread_fence(std::memory_order_acquire);//获取内存屏障
        if (tmp == nullptr) {
            std::lock_guard<std::mutex> lock(_mutex);
            tmp = _instance.load(std::memory_order_relaxed);
            if (tmp == nullptr) {
                tmp = new Singleton;
                std::atomic_thread_fence(std::memory_order_release);//释放内存屏障
                _instance.store(tmp, std::memory_order_relaxed);    //修改的数据让其他核心（线程）可见
                atexit(Destructor);
            }
        }
        return tmp;
    }
private:
    static void Destructor() {
        Singleton* tmp = _instance.load(std::memory_order_relaxed);
        if (nullptr != tmp) {
            delete tmp;
        }
    }
    Singleton(){}; //构造
    ~Singleton(){};
    Singleton(const Singleton &) = delete; //拷⻉构造
    Singleton& operator=(const Singleton&) = delete;//拷贝赋值构造
    Singleton(Singleton &&) = delete;//移动构造
    Singleton& operator=(Singleton &&) = delete;//移动拷贝构造
    static std::atomic<Singleton*> _instance;
    static std::mutex _mutex;
};
std::atomic<Singleton*> Singleton::_instance;//静态成员需要初始化
std::mutex Singleton::_mutex; //互斥锁初始化
// g++ Singleton.cpp -o singleton -std=c++11

//版本五
// c++11 magic static 特性：如果当变量在初始化的时候，并发同时进⼊声明语句，并发线程将会阻塞等待初始化结束。
// c++ effective 作者给出的方法，代码简洁
class Singleton
{
public:
    static Singleton& GetInstance() {
        static Singleton instance;
        return instance;
    }
private:
    Singleton(){}; //构造
    ~Singleton(){};
    Singleton(const Singleton &) = delete; //拷⻉构造
    Singleton& operator=(const Singleton&) = delete;//拷贝赋值构造
    Singleton(Singleton &&) = delete;//移动构造
    Singleton& operator=(Singleton &&) = delete;//移动拷贝构造
};
// 继承 Singleton
// g++ Singleton.cpp -o singleton -std=c++11
/*该版本具备 版本5 所有优点：
1. 利⽤静态局部变量特性，延迟加载；
2. 利⽤静态局部变量特性，系统⾃动回收内存，⾃动调⽤析构函数；
3. 静态局部变量初始化时，没有 new 操作带来的cpu指令reorder操作；
4. c++11 静态局部变量初始化时，具备线程安全；
*/

//版本六
template <typename T>
class Singleton {
public:
    static T& GetInstance() {
        static T instance; // 这⾥要初始化DesignPattern，
        //需要调⽤DesignPattern 构造函数，同时会调⽤⽗类的构造函数。
        return instance;
    }
protected:  //让子类能够得以构造
    virtual ~Singleton() {}
    Singleton() {} // protected修饰构造函数，才能让别⼈继承
private:
    Singleton(const Singleton &) = delete; //拷⻉构造
    Singleton& operator=(const Singleton&) = delete;//拷贝赋值构造
    Singleton(Singleton &&) = delete;//移动构造
    Singleton& operator=(Singleton &&) = delete;//移动拷贝构造
};

class DesignPattern : public Singleton<DesignPattern> {
    friend class Singleton<DesignPattern>; //friend 能让Singleton<T> 访问到 DesignPattern构造函数
private:
    DesignPattern() {}
    ~DesignPattern() {}
};