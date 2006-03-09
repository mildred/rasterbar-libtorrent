/*

Copyright (c) 2003, Arvid Norberg
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

#ifndef TORRENT_BT_PEER_CONNECTION_HPP_INCLUDED
#define TORRENT_BT_PEER_CONNECTION_HPP_INCLUDED

#include <ctime>
#include <algorithm>
#include <vector>
#include <deque>
#include <string>

#include "libtorrent/debug.hpp"

#ifdef _MSC_VER
#pragma warning(push, 1)
#endif

#include <boost/smart_ptr.hpp>
#include <boost/noncopyable.hpp>
#include <boost/array.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/optional.hpp>
#include <boost/cstdint.hpp>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include "libtorrent/buffer.hpp"
#include "libtorrent/peer_connection.hpp"
#include "libtorrent/socket.hpp"
#include "libtorrent/peer_id.hpp"
#include "libtorrent/storage.hpp"
#include "libtorrent/stat.hpp"
#include "libtorrent/alert.hpp"
#include "libtorrent/torrent_handle.hpp"
#include "libtorrent/torrent.hpp"
#include "libtorrent/allocate_resources.hpp"
#include "libtorrent/peer_request.hpp"
#include "libtorrent/piece_block_progress.hpp"
#include "libtorrent/config.hpp"

namespace libtorrent
{
	class torrent;

	namespace detail
	{
		struct session_impl;
	}

	class TORRENT_EXPORT bt_peer_connection
		: public peer_connection
	{
	friend class invariant_access;
	public:

		// this is the constructor where the we are the active part.
		// The peer_conenction should handshake and verify that the
		// other end has the correct id
		bt_peer_connection(
			detail::session_impl& ses
			, torrent* t
			, boost::shared_ptr<stream_socket> s
			, tcp::endpoint const& remote);

		// with this constructor we have been contacted and we still don't
		// know which torrent the connection belongs to
		bt_peer_connection(
			detail::session_impl& ses
			, boost::shared_ptr<stream_socket> s);

		// this function is called once the torrent associated
		// with this peer connection has retrieved the meta-
		// data. If the torrent was spawned with metadata
		// this is called from the constructor.
//		void init();

		~bt_peer_connection();
/*
		// this adds an announcement in the announcement queue
		// it will let the peer know that we have the given piece
		void announce_piece(int index);

		void setup_send();
		void setup_receive();
		
		void fill_send_buffer();
*/		
		// called from the main loop when this connection has any
		// work to do.

		void on_sent(asio::error const& error
			, std::size_t bytes_transferred);
		void on_receive(asio::error const& error
			, std::size_t bytes_transferred);
		
		// tells if this connection has data it want to send
		// and has enough upload bandwidth quota left to send it.
//		bool can_write() const;
//		bool can_read() const;

//		bool is_seed() const;

//		bool has_timed_out() const;

/*
		const peer_id& id() const { return m_peer_id; }
		bool has_piece(int i) const;

		const std::deque<piece_block>& download_queue() const;
		const std::deque<piece_block>& request_queue() const;
		const std::deque<peer_request>& upload_queue() const;
*/
		// returns the block currently being
		// downloaded. And the progress of that
		// block. If the peer isn't downloading
		// a piece for the moment, the boost::optional
		// will be invalid.
		virtual boost::optional<piece_block_progress> downloading_piece_progress() const;
/*
		bool is_interesting() const { return m_interesting; }
		bool is_choked() const { return m_choked; }

		bool is_peer_interested() const { return m_peer_interested; }
		bool has_peer_choked() const { return m_peer_choked; }

		// returns the torrent this connection is a part of
		// may be zero if the connection is an incoming connection
		// and it hasn't received enough information to determine
		// which torrent it should be associated with
		torrent* associated_torrent() const { return m_attached_to_torrent?m_torrent:0; }

		bool verify_piece(const peer_request& p) const;

		const stat& statistics() const { return m_statistics; }
		void add_stat(size_type downloaded, size_type uploaded);

		// is called once every second by the main loop
		void second_tick();

		boost::shared_ptr<stream_socket> get_socket() const { return m_socket; }
		tcp::endpoint const& remote() const { return m_remote; }

		const peer_id& get_peer_id() const { return m_peer_id; }
		const std::vector<bool>& get_bitfield() const;

		// this will cause this peer_connection to be disconnected.
		// what it does is that it puts a reference to it in
		// m_ses.m_disconnect_peer list, which will be scanned in the
		// mainloop to disconnect peers.
		void disconnect();
		bool is_disconnecting() const { return m_disconnecting; }

		// this is called when the connection attempt has succeeded
		// and the peer_connection is supposed to set m_connecting
		// to false, and stop monitor writability
		void on_connection_complete(asio::error const& e);

		// returns true if this connection is still waiting to
		// finish the connection attempt
		bool is_connecting() const { return m_connecting; }

		// returns true if the socket of this peer hasn't been
		// attempted to connect yet (i.e. it's queued for
		// connection attempt).
		bool is_queued() const { return m_queued; }
	
		// called when it's time for this peer_conncetion to actually
		// initiate the tcp connection. This may be postponed until
		// the library isn't using up the limitation of half-open
		// tcp connections.	
		void connect();

		
		// This is called for every peer right after the upload
		// bandwidth has been distributed among them
		// It will reset the used bandwidth to 0 and
		// possibly add or remove the peer's socket
		// from the socket monitor
		void reset_upload_quota();

		// free upload.
		size_type total_free_upload() const;
		void add_free_upload(size_type free_upload);


		// trust management.
		void received_valid_data();
		void received_invalid_data();
		int trust_points() const;

		size_type share_diff() const;
*/
		bool support_extensions() const { return m_supports_extensions; }
/*
		// a connection is local if it was initiated by us.
		// if it was an incoming connection, it is remote
		bool is_local() const { return m_active; }

		void set_failed() { m_failed = true; }
		bool failed() const { return m_failed; }

#ifdef TORRENT_VERBOSE_LOGGING
		boost::shared_ptr<logger> m_logger;
#endif
*/
		bool supports_extension(extension_index ex) const
		{ return m_extension_messages[ex] > 0; }

		bool has_metadata() const;

		// the message handlers are called
		// each time a recv() returns some new
		// data, the last time it will be called
		// is when the entire packet has been
		// received, then it will no longer
		// be called. i.e. most handlers need
		// to check how much of the packet they
		// have received before any processing
		void on_keepalive();
		void on_choke(int received);
		void on_unchoke(int received);
		void on_interested(int received);
		void on_not_interested(int received);
		void on_have(int received);
		void on_bitfield(int received);
		void on_request(int received);
		void on_piece(int received);
		void on_cancel(int received);
		void on_dht_port(int received);

		void on_extended(int received);

		void on_extended_handshake();
		void on_chat();
		void on_metadata();
		void on_peer_exchange();

		typedef void (bt_peer_connection::*message_handler)(int received);

		// the following functions appends messages
		// to the send buffer
		void write_choke();
		void write_unchoke();
		void write_interested();
		void write_not_interested();
		void write_request(peer_request const& r);
		void write_cancel(peer_request const& r);
		void write_bitfield(std::vector<bool> const& bitfield);
		void write_have(int index);
		void write_piece(peer_request const& r);
		void write_handshake();
		void write_extensions();
		void write_chat_message(const std::string& msg);
		void write_metadata(std::pair<int, int> req);
		void write_metadata_request(std::pair<int, int> req);
		void write_keepalive();
		void write_dht_port(int listen_port);
		void on_connected() {}

#ifndef NDEBUG
		void check_invariant() const;
		boost::posix_time::ptime m_last_choke;
#endif

	private:

		bool dispatch_message(int received);

		// if we don't have all metadata
		// this function will request a part of it
		// from this peer
		void request_metadata();

		enum state
		{
			read_protocol_length = 0,
			read_protocol_string,
			read_info_hash,
			read_peer_id,

			read_packet_size,
			read_packet
		};
		
		std::string m_client_version;

		state m_state;

		// the timeout in seconds
		int m_timeout;

		enum message_type
		{
	// standard messages
			msg_choke = 0,
			msg_unchoke,
			msg_interested,
			msg_not_interested,
			msg_have,
			msg_bitfield,
			msg_request,
			msg_piece,
			msg_cancel,
			msg_dht_port,
	// extension protocol message
			msg_extended = 20,

			num_supported_messages
		};

		static const message_handler m_message_handler[num_supported_messages];

		// this is a queue of ranges that describes
		// where in the send buffer actual payload
		// data is located. This is currently
		// only used to be able to gather statistics
		// seperately on payload and protocol data.
		struct range
		{
			range(int s, int l)
				: start(s)
				, length(l)
			{
				assert(s >= 0);
				assert(l > 0);
			}
			int start;
			int length;
		};
		static bool range_below_zero(const range& r)
		{ return r.start < 0; }
		std::deque<range> m_payloads;

		// this is set to true if the handshake from
		// the peer indicated that it supports the
		// extension protocol
		bool m_supports_extensions;
		bool m_supports_dht_port;

		static const char* extension_names[num_supported_extensions];
		// contains the indices of the extension messages for each extension
		// supported by the other end. A value of <= 0 means that the extension
		// is not supported.
		int m_extension_messages[num_supported_extensions];

		// this is set to the current time each time we get a
		// "I don't have metadata" message.
		boost::posix_time::ptime m_no_metadata;

		// this is set to the time when we last sent
		// a request for metadata to this peer
		boost::posix_time::ptime m_metadata_request;

		// this is set to true when we send a metadata
		// request to this peer, and reset to false when
		// we receive a reply to our request.
		bool m_waiting_metadata_request;

		// if we're waiting for a metadata request
		// this was the request we sent
		std::pair<int, int> m_last_metadata_request;

		// the number of bytes of metadata we have received
		// so far from this per, only counting the current
		// request. Any previously finished requests
		// that have been forwarded to the torrent object
		// do not count.
		int m_metadata_progress;
	};
}

#endif // TORRENT_BT_PEER_CONNECTION_HPP_INCLUDED

