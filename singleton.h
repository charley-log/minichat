#ifndef SINGLETON_H
#define SINGLETON_H

template <typename T>
class Singleton {
public:
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;

    static T& instance() {
        static T inst;
        return inst;
    }

protected:
    Singleton() = default;
    ~Singleton() = default;
};

#endif