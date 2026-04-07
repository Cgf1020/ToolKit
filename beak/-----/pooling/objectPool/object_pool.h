#pragma once


#include <string>
#include <vector>
#include <queue>
#include <map>
#include <unordered_map>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>


class ObjectPool {
public:
    ObjectPool() = delete;
    /// <param name="bufferArray">对象池的缓冲区，由外部指定，可以理解为一个数组。数组大小需满足bufferSize>=elementSize*arraySize</param>
    /// <param name="elementSize">数组元素大小</param>
    /// <param name="arraySize">数组长度或元素个数</param>
    ObjectPool(void* bufferArray, int elementSize, int arraySize);
    ~ObjectPool();

public:
    /// 申请对象
    // 如果池里对象不足,则会等待，直到有对象才返回。
    void* Applicate();

    /// 申请对象   超时时间，超时后返回null
    void* Applicate(int timeout);

    /// 归还对象
    void ReturnBack(void* element);

    void setBeUse(void* element);

    /// 获取待使用的第一个对象
    void* getBeUse();

    /// 获取待使用的第一个对象
    void* getBeUse(int timeout);

    /// <summary>
    /// 获取对象的大小，即构造方法中的elementSize
    /// </summary>
    /// <returns>对象的大小</returns>
    int GetObjectSize();

    /// <summary>
    /// 获取总的对象数量，即构造方法中的arraySize
    /// </summary>
    /// <returns>总的对象数量</returns>
    int GetObjectCount();

    /// <summary>
    /// 获取空闲的对象数量
    /// return : size_t
    /// </summary>
    auto getFreeCount();

private:
    void* _buffer = NULL;
    int _elementSize;
    int _arraySize;

    // 未使用的对象
    std::vector<void*> _unusedUnits;

    // 使用的对象
    std::unordered_map<void*, int> _usedUnits;

    std::mutex _mutex;
    std::condition_variable _cond;

    //
    std::queue<void*> _be_use;
    std::mutex _queue_mutex;
    std::condition_variable _queue_cond;
};