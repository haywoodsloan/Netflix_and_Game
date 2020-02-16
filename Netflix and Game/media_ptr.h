#pragma once
#include <memory>
#include <functional>

template<class T>
class media_ptr : public std::unique_ptr<T, std::function<void(T*)>>
{
public:
	media_ptr(T* ptr) : std::unique_ptr<T, std::function<void(T*)>>(ptr, [](T* ptr)
		{
			(*ptr)->Release();
			delete ptr;
		}
	) {};

	media_ptr() : media_ptr(new T) {};

	T operator->() {
		return this->operator*();
	}

	T* operator&() {
		return this->get();
	}
};

