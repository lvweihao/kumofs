//
// kumofs
//
// Copyright (C) 2009 FURUHASHI Sadayuki
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.
//
#ifndef SERVER_ZCONNECTION_H__
#define SERVER_ZCONNECTION_H__

#include <zlib.h>
#include <mp/exception.h>
#include "rpc/connection.h"

#ifndef ZCONNECTION_INITIAL_SIZE
#define ZCONNECTION_INITIAL_SIZE (128*1024)
#endif

#ifndef ZCONNECTION_RESERVE_SIZE
#define ZCONNECTION_RESERVE_SIZE (16*1024)
#endif

namespace kumo {
namespace server {


template <typename IMPL>
class zconnection : public mp::wavy::handler {
public:
	zconnection(int fd);
	~zconnection();

	void read_event();
	//void submit_message(rpc::msgobj msg, rpc::auto_zone& z);

private:
	msgpack::unpacker m_pac;
	z_stream m_z;
	char* m_buffer;

private:
	zconnection();
	zconnection(const zconnection&);
};

template <typename IMPL>
zconnection<IMPL>::zconnection(int fd) :
	mp::wavy::handler(fd),
	m_pac(ZCONNECTION_INITIAL_SIZE)
{
	m_buffer = (char*)::malloc(RPC_INITIAL_BUFFER_SIZE);
	if(!m_buffer) {
		throw std::bad_alloc();
	}

	m_z.zalloc = Z_NULL;
	m_z.zfree = Z_NULL;
	m_z.opaque = Z_NULL;

	if(inflateInit(&m_z) != Z_OK) {
		::free(m_buffer);
		throw std::runtime_error(m_z.msg);
	}
}

template <typename IMPL>
zconnection<IMPL>::~zconnection()
{
	inflateEnd(&m_z);
	::free(m_buffer);
}


template <typename IMPL>
void zconnection<IMPL>::read_event()
try {
	ssize_t rl = ::read(fd(), m_buffer, RPC_INITIAL_BUFFER_SIZE);
	if(rl < 0) {
		if(errno == EAGAIN || errno == EINTR) {
			return;
		} else {
			throw std::runtime_error("read error");
		}
	} else if(rl == 0) {
		throw std::runtime_error("connection closed");
	}

	m_z.next_in = (Bytef*)m_buffer;
	m_z.avail_in = rl;

	do {
		if(m_pac.buffer_capacity() < ZCONNECTION_INITIAL_SIZE) {
			m_pac.reserve_buffer(ZCONNECTION_RESERVE_SIZE); // reserve larger buffer
		}

		m_z.next_out = (Bytef*)m_pac.buffer();
		m_z.avail_out = m_pac.buffer_capacity();

		int ret = inflate(&m_z, Z_SYNC_FLUSH);
		if(ret != Z_OK && ret != Z_STREAM_END) {
			throw std::runtime_error("inflate failed");
		}

		m_pac.buffer_consumed( m_pac.buffer_capacity() - m_z.avail_out );

	} while(m_z.avail_in > 0);

	while(m_pac.execute()) {
		rpc::msgobj msg = m_pac.data();
		std::auto_ptr<msgpack::zone> z( m_pac.release_zone() );
		m_pac.reset();
		static_cast<IMPL*>(this)->submit_message(msg, z);
	}

} catch(msgpack::type_error& e) {
	LOG_ERROR("rpc packet: type error");
	throw;
} catch(std::exception& e) {
	LOG_WARN("rpc packet: ", e.what());
	throw;
} catch(...) {
	LOG_ERROR("rpc packet: unknown error");
	throw;
}


}  // namespace server
}  // namespace kumo

#endif  /* server/zconnection.h */

