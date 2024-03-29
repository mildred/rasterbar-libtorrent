/*

Copyright (c) 2003-2014, Arvid Norberg
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the distribution.
    * Neither the name of the author nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef TORRENT_TRACKER_MANAGER_HPP_INCLUDED
#define TORRENT_TRACKER_MANAGER_HPP_INCLUDED

#include <vector>
#include <string>
#include <list>
#include <utility>
#include <ctime>

#ifdef _MSC_VER
#pragma warning(push, 1)
#endif

#include <boost/shared_ptr.hpp>
#include <boost/cstdint.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/intrusive_ptr.hpp>
#include <boost/tuple/tuple.hpp>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include "libtorrent/config.hpp"
#include "libtorrent/socket.hpp"
#include "libtorrent/address.hpp"
#include "libtorrent/peer_id.hpp"
#include "libtorrent/peer.hpp" // peer_entry
#include "libtorrent/deadline_timer.hpp"
#include "libtorrent/connection_queue.hpp"
#include "libtorrent/intrusive_ptr_base.hpp"
#include "libtorrent/size_type.hpp"
#include "libtorrent/union_endpoint.hpp"
#include "libtorrent/udp_socket.hpp" // for udp_socket_observer
#ifdef TORRENT_USE_OPENSSL
#include <boost/asio/ssl/context.hpp>
#endif

namespace libtorrent
{
	struct request_callback;
	class tracker_manager;
	struct timeout_handler;
	struct tracker_connection;
	namespace aux { struct session_impl; }

	// returns -1 if gzip header is invalid or the header size in bytes
	TORRENT_EXTRA_EXPORT int gzip_header(const char* buf, int size);

	struct TORRENT_EXTRA_EXPORT tracker_request
	{
		tracker_request()
			: downloaded(-1)
			, uploaded(-1)
			, left(-1)
			, corrupt(0)
			, redundant(0)
			, listen_port(0)
			, event(none)
			, kind(announce_request)
			, key(0)
			, num_want(0)
			, send_stats(true)
			, apply_ip_filter(true)
#ifdef TORRENT_USE_OPENSSL
			, ssl_ctx(0)
#endif
		{}

		enum event_t
		{
			none,
			completed,
			started,
			stopped,
			paused
		};

		enum kind_t
		{
			announce_request,
			scrape_request
		};

		std::string url;
		std::string trackerid;

		size_type downloaded;
		size_type uploaded;
		size_type left;
		size_type corrupt;
		size_type redundant;
		boost::uint16_t listen_port;

		// values from event_t
		boost::uint8_t event;

		// values from kind_t
		boost::uint8_t kind;

		boost::uint32_t key;
		int num_want;
		sha1_hash info_hash;
		peer_id pid;
		address bind_ip;

		bool send_stats;
		bool apply_ip_filter;
#ifdef TORRENT_USE_OPENSSL
		boost::asio::ssl::context* ssl_ctx;
#endif
	};

	struct TORRENT_EXTRA_EXPORT request_callback
	{
		friend class tracker_manager;
		request_callback() {}
		virtual ~request_callback() {}
		virtual void tracker_warning(tracker_request const& req
			, std::string const& msg) = 0;
		virtual void tracker_scrape_response(tracker_request const& /*req*/
			, int /*complete*/, int /*incomplete*/, int /*downloads*/
			, int /*downloaders*/) {}
		virtual void tracker_response(
			tracker_request const& req
			, address const& tracker_ip
			, std::list<address> const& ip_list
			, std::vector<peer_entry>& peers
			, int interval
			, int min_interval
			, int complete
			, int incomplete
			, int downloaded
			, address const& external_ip
			, std::string const& trackerid) = 0;
		virtual void tracker_request_error(
			tracker_request const& req
			, int response_code
			, error_code const& ec
			, const std::string& msg
			, int retry_interval) = 0;

#if defined TORRENT_VERBOSE_LOGGING || defined TORRENT_LOGGING || defined TORRENT_ERROR_LOGGING
		virtual void debug_log(const char* fmt, ...) const = 0;
#endif
	};

	struct TORRENT_EXTRA_EXPORT timeout_handler
		: intrusive_ptr_base<timeout_handler>
		, boost::noncopyable
	{
		timeout_handler(io_service& str);

		void set_timeout(int completion_timeout, int read_timeout);
		void restart_read_timeout();
		void cancel();
		bool cancelled() const { return m_abort; }

		virtual void on_timeout(error_code const& ec) = 0;
		virtual ~timeout_handler() {}

		io_service& get_io_service() { return m_timeout.get_io_service(); }

	private:
	
		void timeout_callback(error_code const&);

		boost::intrusive_ptr<timeout_handler> self()
		{ return boost::intrusive_ptr<timeout_handler>(this); }

		int m_completion_timeout;

		typedef mutex mutex_t;
		mutable mutex_t m_mutex;

		// used for timeouts
		// this is set when the request has been sent
		ptime m_start_time;

		// this is set every time something is received
		ptime m_read_time;

		// the asio async operation
		deadline_timer m_timeout;
		
		int m_read_timeout;

		bool m_abort;
	};

	struct TORRENT_EXTRA_EXPORT tracker_connection
		: timeout_handler
	{
		tracker_connection(tracker_manager& man
			, tracker_request const& req
			, io_service& ios
			, boost::weak_ptr<request_callback> r);

		boost::shared_ptr<request_callback> requester() const;
		virtual ~tracker_connection() {}

		tracker_request const& tracker_req() const { return m_req; }

		void fail(error_code const& ec, int code = -1, char const* msg = ""
			, int interval = 0, int min_interval = 0);
		virtual void start() = 0;
		virtual void close();
		address const& bind_interface() const { return m_req.bind_ip; }
		void sent_bytes(int bytes);
		void received_bytes(int bytes);
		virtual bool on_receive(error_code const&, udp::endpoint const&
			, char const* /* buf */, int /* size */) { return false; }
		virtual bool on_receive_hostname(error_code const&
			, char const* /* hostname */
			, char const* /* buf */, int /* size */) { return false; }

		boost::intrusive_ptr<tracker_connection> self()
		{ return boost::intrusive_ptr<tracker_connection>(this); }

	private:

		const tracker_request m_req;

	protected:

		void fail_impl(error_code const& ec, int code = -1, std::string msg = std::string()
			, int interval = 0, int min_interval = 0);

		boost::weak_ptr<request_callback> m_requester;

		tracker_manager& m_man;
	};

	class TORRENT_EXTRA_EXPORT tracker_manager: public udp_socket_observer, boost::noncopyable
	{
	public:

		tracker_manager(aux::session_impl& ses)
			: m_ses(ses)
			, m_abort(false) {}
		~tracker_manager();

		void queue_request(
			io_service& ios
			, connection_queue& cc
			, tracker_request r
			, std::string const& auth
			, boost::weak_ptr<request_callback> c
				= boost::weak_ptr<request_callback>());
		void abort_all_requests(bool all = false);

		void remove_request(tracker_connection const*);
		bool empty() const;
		int num_requests() const;

		void sent_bytes(int bytes);
		void received_bytes(int bytes);

		virtual bool incoming_packet(error_code const& e, udp::endpoint const& ep
			, char const* buf, int size);

		// this is only used for SOCKS packets, since
		// they may be addressed to hostname
		virtual bool incoming_packet(error_code const& e, char const* hostname
			, char const* buf, int size);
		
	private:

		typedef mutex mutex_t;
		mutable mutex_t m_mutex;

		typedef std::list<boost::intrusive_ptr<tracker_connection> >
			tracker_connections_t;
		tracker_connections_t m_connections;
		aux::session_impl& m_ses;
		bool m_abort;
	};
}

#endif // TORRENT_TRACKER_MANAGER_HPP_INCLUDED

