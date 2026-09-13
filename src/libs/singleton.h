#pragma once

template <typename T>
class Singleton
{
protected:
	Singleton() { ; }
	virtual ~Singleton() { ; }

	static T* instance;
public:
	static T* Instance()
	{
		if (!instance)
		{
			instance = new T();
			atexit(Destroy);
		}
		return instance;
	}
	static void Destroy()
	{
		if (instance)
		{
			delete instance;
			instance = nullptr;
		}
	}
};

template <typename T>
T* Singleton<T>::instance = nullptr;