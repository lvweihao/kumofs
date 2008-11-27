#ifndef RPC_SERVER_H__
#define RPC_SERVER_H__

#include "rpc/session.h"
#include "log/mlogger.h"  // FIXME
#include <mp/pthread.h>
#include <algorithm>
#include <map>

namespace rpc {


class server;

class server_transport : public basic_transport, public connection<server_transport> {
public:
	server_transport(int fd, basic_shared_session& s, transport_manager* mgr);

	virtual ~server_transport();

	typedef std::auto_ptr<msgpack::zone> auto_zone;

	void process_request(method_id method, msgobj param,
			msgid_t msgid, auto_zone& z);

	void process_response(msgobj res, msgobj err,
			msgid_t msgid, auto_zone& z);
};


class peer : public basic_session {
public:
	peer(session_manager* mgr = NULL);
};

typedef mp::shared_ptr<peer> shared_peer;
typedef mp::weak_ptr<peer> weak_peer;


class server : public session_manager, public transport_manager {
public:
	typedef std::auto_ptr<msgpack::zone> auto_zone;
	typedef rpc::msgobj      msgobj;
	typedef rpc::method_id   method_id;
	typedef rpc::msgid_t     msgid_t;
	typedef rpc::shared_zone shared_zone;
	typedef rpc::shared_peer shared_peer;
	typedef rpc::weak_peer   weak_peer;
	typedef rpc::role_type   role_type;

	typedef shared_peer shared_session;
	typedef weak_peer weak_session;

	server();
	virtual ~server();

	virtual void dispatch(
			shared_peer& from, weak_responder response,
			method_id method, msgobj param, shared_zone& life) = 0;

	virtual void dispatch_request(
			basic_shared_session& s, weak_responder response,
			method_id method, msgobj param, shared_zone& life);

public:
	// step timeout count.
	void step_timeout();

	// add accepted connection
	void accepted(int fd);

	// apply function to all connected sessions.
	// F is required to implement
	// void operator() (shared_peer);
	template <typename F>
	void for_each_peer(F f);

protected:
	typedef std::map<void*, basic_weak_session> peers_t;
	peers_t m_peers;

public:
	virtual void transport_lost_notify(basic_shared_session& s);

private:
	server(const server&);
};


inline peer::peer(session_manager* mgr) :
	basic_session(mgr) { }


namespace detail {
	template <typename F>
	struct server_each_peer {
		server_each_peer(F f) : m(f) { }
		inline void operator() (std::pair<void* const, basic_weak_session>& x)
		{
			basic_shared_session s(x.second.lock());
			if(s && !s->is_lost()) {
				m(mp::static_pointer_cast<peer>(s));
			}
		}
	private:
		F m;
		server_each_peer();
	};
}  // namespace detail

template <typename F>
void server::for_each_peer(F f)
{
	detail::server_each_peer<F> e(f);
	std::for_each(m_peers.begin(), m_peers.end(), e);
}


inline void server_transport::process_request(method_id method, msgobj param,
		msgid_t msgid, auto_zone& z)
{
	basic_transport::process_request(method, param, msgid, z);
}

inline void server_transport::process_response(msgobj res, msgobj err,
		msgid_t msgid, auto_zone& z)
{
	basic_transport::process_response(res, err, msgid, z);
}


}  // namespace rpc

#endif /* rpc/client.h */

