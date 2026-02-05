#ifndef __SERVER_FACTORY_H__
#define __SERVER_FACTORY_H__

#include <type_traits>
#include "server_base.h"

class ServerFactory {
	public:
	template <typename U, class S, typename... Args>
		static S* create(Args&&... args);
};

#include "server_factory.tpp"

#endif