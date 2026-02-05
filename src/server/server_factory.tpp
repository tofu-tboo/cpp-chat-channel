
#include "server_factory.h"
#include "server_base.h"

template <typename U, class S, typename... Args>
S* ServerFactory::create(Args&&... args) {
	static_assert(std::is_base_of<ServerBase<U>, S>::value, "S must derive from ServerBase");
	S* server = new S(std::forward<Args>(args)...);
	server->init();

	return server;
}