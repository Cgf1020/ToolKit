#include "object_pool.h"


ObjectPool::ObjectPool(void* bufferArray, int elementSize, int arraySize) : _buffer(bufferArray), _elementSize(elementSize), _arraySize(arraySize) {
    char* firstAdress = (char*)bufferArray;
    // 记录未使用的索引   每个对象的地址
    for (int i = 0; i < arraySize; i++) {
        _unusedUnits.push_back(&(firstAdress[i * elementSize]));
    }
}

ObjectPool::~ObjectPool() {
}

void* ObjectPool::Applicate() {
    return Applicate(-1);
}

void* ObjectPool::Applicate(int timeout) {
    std::unique_lock<std::mutex> l(_mutex);
    while (_unusedUnits.size() < 1) {

        if (timeout == -1) {
            _cond.wait(l);
        } else if (_cond.wait_for(l, std::chrono::microseconds(timeout)) == std::cv_status::timeout) {
            return nullptr;
        }
    }

    // 返回一个对象的地址
    void* resource = _unusedUnits.back();
    _unusedUnits.pop_back();

    // 已经申请的东西，放入待归还的map中
    _usedUnits[resource] = 1;

    return resource;
}

void ObjectPool::ReturnBack(void* element) 
{
    std::unique_lock<std::mutex> l(_mutex);
    auto iter = _usedUnits.find(element);
    if (iter != _usedUnits.end()) {
        _unusedUnits.push_back(element);
        _usedUnits.erase(iter);
        _cond.notify_one();
    }
}

void ObjectPool::setBeUse(void* element) {
    std::unique_lock<std::mutex> l(_queue_mutex);

    _be_use.push(element);

    _queue_cond.notify_one();
}

void* ObjectPool::getBeUse() {
    return getBeUse(-1);
}

void* ObjectPool::getBeUse(int timeout) {
    std::unique_lock<std::mutex> l(_queue_mutex);
    while (_be_use.size() < 1) {
        if (timeout == -1) {
            _queue_cond.wait(l);
        } else if (_queue_cond.wait_for(l, std::chrono::microseconds(timeout)) == std::cv_status::timeout) {
            return nullptr;
        }
    }
    void* ret = _be_use.front();

    _be_use.pop();

    return ret;
}

int ObjectPool::GetObjectSize() {
    return 0;
}

int ObjectPool::GetObjectCount() {
    return 0;
}

auto ObjectPool::getFreeCount() {

    return _unusedUnits.size();

}