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

#include <vector>
#include <iostream>
#include <iomanip>
#include <limits>
#include <boost/bind.hpp>

#include "libtorrent/bt_peer_connection.hpp"
#include "libtorrent/session.hpp"
#include "libtorrent/identify_client.hpp"
#include "libtorrent/entry.hpp"
#include "libtorrent/bencode.hpp"
#include "libtorrent/alert_types.hpp"
#include "libtorrent/invariant_check.hpp"
#include "libtorrent/io.hpp"
#include "libtorrent/version.hpp"

using namespace boost::posix_time;
using boost::bind;
using boost::shared_ptr;
using libtorrent::detail::session_impl;

namespace libtorrent
{

	// the names of the extensions to look for in
	// the extensions-message
	const char* bt_peer_connection::extension_names[] =
	{ "", "LT_chat", "LT_metadata", "LT_peer_exchange" };

	const bt_peer_connection::message_handler
	bt_peer_connection::m_message_handler[] =
	{
		&bt_peer_connection::on_choke,
		&bt_peer_connection::on_unchoke,
		&bt_peer_connection::on_interested,
		&bt_peer_connection::on_not_interested,
		&bt_peer_connection::on_have,
		&bt_peer_connection::on_bitfield,
		&bt_peer_connection::on_request,
		&bt_peer_connection::on_piece,
		&bt_peer_connection::on_cancel,
		&bt_peer_connection::on_dht_port,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		&bt_peer_connection::on_extended
	};


	bt_peer_connection::bt_peer_connection(
		detail::session_impl& ses
		, torrent* t
		, shared_ptr<stream_socket> s
		, tcp::endpoint const& remote)
		: peer_connection(ses, t, s, remote)
		, m_state(read_protocol_length)
		, m_supports_extensions(false)
		, m_no_metadata(
			boost::gregorian::date(1970, boost::date_time::Jan, 1)
			, boost::posix_time::seconds(0))
		, m_metadata_request(
			boost::gregorian::date(1970, boost::date_time::Jan, 1)
			, boost::posix_time::seconds(0))
		, m_waiting_metadata_request(false)
		, m_metadata_progress(0)
	{
		INVARIANT_CHECK;

#ifdef TORRENT_VERBOSE_LOGGING
		(*m_logger) << "*** bt_peer_connection\n";
#endif
/*		
		// these numbers are used the first second of connection.
		// then the given upload limits will be applied by running
		// allocate_resources().
		m_ul_bandwidth_quota.min = 10;
		m_ul_bandwidth_quota.max = resource_request::inf;

		if (m_torrent->m_ul_bandwidth_quota.given == resource_request::inf)
		{
			m_ul_bandwidth_quota.given = resource_request::inf;
		}
		else
		{
			// just enough to get started with the handshake and bitmask
			m_ul_bandwidth_quota.given = 400;
		}

		m_dl_bandwidth_quota.min = 10;
		m_dl_bandwidth_quota.max = resource_request::inf;
	
		if (m_torrent->m_dl_bandwidth_quota.given == resource_request::inf)
		{
			m_dl_bandwidth_quota.given = resource_request::inf;
		}
		else
		{
			// just enough to get started with the handshake and bitmask
			m_dl_bandwidth_quota.given = 400;
		}

		assert(m_torrent != 0);

		std::fill(m_peer_id.begin(), m_peer_id.end(), 0);
*/
		// initialize the extension list to zero, since
		// we don't know which extensions the other
		// end supports yet
		std::fill(m_extension_messages, m_extension_messages + num_supported_extensions, 0);

		write_handshake();

		// start in the state where we are trying to read the
		// handshake from the other side
		reset_recv_buffer(1);

		// assume the other end has no pieces
		if (associated_torrent()->ready_for_connections())
			write_bitfield(associated_torrent()->pieces());

		setup_send();
		setup_receive();
	}

	bt_peer_connection::bt_peer_connection(
		detail::session_impl& ses
		, boost::shared_ptr<stream_socket> s)
		: peer_connection(ses, s)
		, m_state(read_protocol_length)
		, m_supports_extensions(false)
		, m_no_metadata(
			boost::gregorian::date(1970, boost::date_time::Jan, 1)
			, boost::posix_time::seconds(0))
		, m_metadata_request(
			boost::gregorian::date(1970, boost::date_time::Jan, 1)
			, boost::posix_time::seconds(0))
		, m_waiting_metadata_request(false)
		, m_metadata_progress(0)
	{
		INVARIANT_CHECK;
/*
		m_remote = m_socket->remote_endpoint();

#ifdef TORRENT_VERBOSE_LOGGING
		assert(m_socket->remote_endpoint() == remote());
		m_logger = m_ses.create_log(remote().address().to_string() + "_"
			+ boost::lexical_cast<std::string>(remote().port()));
		(*m_logger) << "*** INCOMING CONNECTION\n";
#endif


		// upload bandwidth will only be given to connections
		// that are part of a torrent. Since this is an incoming
		// connection, we have to give it some initial bandwidth
		// to send the handshake.
		// after one second, allocate_resources() will be called
		// and the correct bandwidth limits will be set on all
		// connections.

		m_ul_bandwidth_quota.min = 10;
		m_ul_bandwidth_quota.max = resource_request::inf;

		if (m_ses.m_upload_rate == -1)
		{
			m_ul_bandwidth_quota.given = resource_request::inf;
		}
		else
		{
			// just enough to get started with the handshake and bitmask
			m_ul_bandwidth_quota.given = 400;
		}

		m_dl_bandwidth_quota.min = 10;
		m_dl_bandwidth_quota.max = resource_request::inf;
	
		if (m_ses.m_download_rate == -1)
		{
			m_dl_bandwidth_quota.given = resource_request::inf;
		}
		else
		{
			// just enough to get started with the handshake and bitmask
			m_dl_bandwidth_quota.given = 400;
		}

		std::fill(m_peer_id.begin(), m_peer_id.end(), 0);
*/
		// initialize the extension list to zero, since
		// we don't know which extensions the other
		// end supports yet
		std::fill(m_extension_messages, m_extension_messages + num_supported_extensions, 0);

		// we are not attached to any torrent yet.
		// we have to wait for the handshake to see
		// which torrent the connector want's to connect to

		// start in the state where we are trying to read the
		// handshake from the other side
		reset_recv_buffer(1);
		setup_receive();
	}

	bt_peer_connection::~bt_peer_connection()
	{
	}

	void bt_peer_connection::write_handshake()
	{
		INVARIANT_CHECK;

		// add handshake to the send buffer
		const char version_string[] = "BitTorrent protocol";
		const int string_len = sizeof(version_string)-1;

		buffer::interval i = allocate_send_buffer(1 + string_len + 8 + 20 + 20);
		// length of version string
		*i.begin = string_len;
		++i.begin;

		// version string itself
		std::copy(
			version_string
			, version_string + string_len
			, i.begin);
		i.begin += string_len;

		// 8 zeroes
		std::fill(
			i.begin
			, i.begin + 8
			, 0);

		// indicate that we support the DHT messages
		*(i.begin + 7) = 0x01;
		
		// we support extensions
		*(i.begin + 5) = 0x10;

		i.begin += 8;

		// info hash
		sha1_hash const& ih = associated_torrent()->torrent_file().info_hash();
		std::copy(ih.begin(), ih.end(), i.begin);
		i.begin += 20;

		// peer id
		std::copy(
			m_ses.get_peer_id().begin()
			, m_ses.get_peer_id().end()
			, i.begin);
		i.begin += 20;
		assert(i.begin == i.end);

#ifdef TORRENT_VERBOSE_LOGGING
		using namespace boost::posix_time;
		(*m_logger) << to_simple_string(second_clock::universal_time())
			<< " ==> HANDSHAKE\n";
#endif
		setup_send();
	}

	boost::optional<piece_block_progress> bt_peer_connection::downloading_piece_progress() const
	{
		buffer::const_interval recv_buffer = receive_buffer();
		// are we currently receiving a 'piece' message?
		if (m_state != read_packet
			|| (recv_buffer.end - recv_buffer.begin) < 9
			|| recv_buffer[0] != msg_piece)
			return boost::optional<piece_block_progress>();

		const char* ptr = recv_buffer.begin + 1;
		peer_request r;
		r.piece = detail::read_int32(ptr);
		r.start = detail::read_int32(ptr);
		r.length = packet_size() - 9;

		// is any of the piece message header data invalid?
		if (!verify_piece(r))
			return boost::optional<piece_block_progress>();

		piece_block_progress p;

		p.piece_index = r.piece;
		p.block_index = r.start / associated_torrent()->block_size();
		p.bytes_downloaded = recv_buffer.end - recv_buffer.begin - 9;
		p.full_block_bytes = r.length;

		return boost::optional<piece_block_progress>(p);
	}


	// message handlers

	// -----------------------------
	// --------- KEEPALIVE ---------
	// -----------------------------

	void bt_peer_connection::on_keepalive()
	{
		INVARIANT_CHECK;

#ifdef TORRENT_VERBOSE_LOGGING
		using namespace boost::posix_time;
		(*m_logger) << to_simple_string(second_clock::universal_time())
			<< " <== KEEPALIVE\n";
#endif
		incoming_keepalive();
	}

	// -----------------------------
	// ----------- CHOKE -----------
	// -----------------------------

	void bt_peer_connection::on_choke(int received)
	{
		INVARIANT_CHECK;

		assert(received > 0);
		if (packet_size() != 1)
			throw protocol_error("'choke' message size != 1");
		m_statistics.received_bytes(0, received);
		if (!packet_finished()) return;

		incoming_choke();
	}

	// -----------------------------
	// ---------- UNCHOKE ----------
	// -----------------------------

	void bt_peer_connection::on_unchoke(int received)
	{
		INVARIANT_CHECK;

		assert(received > 0);
		if (packet_size() != 1)
			throw protocol_error("'unchoke' message size != 1");
		m_statistics.received_bytes(0, received);
		if (!packet_finished()) return;

		incoming_unchoke();
	}

	// -----------------------------
	// -------- INTERESTED ---------
	// -----------------------------

	void bt_peer_connection::on_interested(int received)
	{
		INVARIANT_CHECK;

		assert(received > 0);
		if (packet_size() != 1)
			throw protocol_error("'interested' message size != 1");
		m_statistics.received_bytes(0, received);
		if (!packet_finished()) return;

		incoming_interested();
	}

	// -----------------------------
	// ------ NOT INTERESTED -------
	// -----------------------------

	void bt_peer_connection::on_not_interested(int received)
	{
		INVARIANT_CHECK;

		assert(received > 0);
		if (packet_size() != 1)
			throw protocol_error("'not interested' message size != 1");
		m_statistics.received_bytes(0, received);
		if (!packet_finished()) return;

		incoming_not_interested();
	}

	// -----------------------------
	// ----------- HAVE ------------
	// -----------------------------

	void bt_peer_connection::on_have(int received)
	{
		INVARIANT_CHECK;

		assert(received > 0);
		if (packet_size() != 5)
			throw protocol_error("'have' message size != 5");
		m_statistics.received_bytes(0, received);
		if (!packet_finished()) return;

		buffer::const_interval recv_buffer = receive_buffer();

		const char* ptr = recv_buffer.begin + 1;
		int index = detail::read_int32(ptr);

		incoming_have(index);
	}

	// -----------------------------
	// --------- BITFIELD ----------
	// -----------------------------

	void bt_peer_connection::on_bitfield(int received)
	{
		INVARIANT_CHECK;

		assert(received > 0);
		assert(associated_torrent());
		// if we don't have the metedata, we cannot
		// verify the bitfield size
		if (associated_torrent()->valid_metadata()
			&& packet_size() - 1 != ((int)get_bitfield().size() + 7) / 8)
			throw protocol_error("bitfield with invalid size");

		m_statistics.received_bytes(0, received);
		if (!packet_finished()) return;

		buffer::const_interval recv_buffer = receive_buffer();

		std::vector<bool> bitfield;
		
		if (!associated_torrent()->valid_metadata())
			bitfield.resize((packet_size() - 1) * 8);
		else
			bitfield.resize(get_bitfield().size());

		// if we don't have metadata yet
		// just remember the bitmask
		// don't update the piecepicker
		// (since it doesn't exist yet)
		for (int i = 0; i < (int)bitfield.size(); ++i)
			bitfield[i] = (recv_buffer[1 + (i>>3)] & (1 << (7 - (i&7)))) != 0;
		incoming_bitfield(bitfield);
	}

	// -----------------------------
	// ---------- REQUEST ----------
	// -----------------------------

	void bt_peer_connection::on_request(int received)
	{
		INVARIANT_CHECK;

		assert(received > 0);
		if (packet_size() != 13)
			throw protocol_error("'request' message size != 13");
		m_statistics.received_bytes(0, received);
		if (!packet_finished()) return;

		buffer::const_interval recv_buffer = receive_buffer();

		peer_request r;
		const char* ptr = recv_buffer.begin + 1;
		r.piece = detail::read_int32(ptr);
		r.start = detail::read_int32(ptr);
		r.length = detail::read_int32(ptr);
		
		incoming_request(r);
	}

	// -----------------------------
	// ----------- PIECE -----------
	// -----------------------------

	void bt_peer_connection::on_piece(int received)
	{
		INVARIANT_CHECK;

		assert(received > 0);
		
		buffer::const_interval recv_buffer = receive_buffer();
		int recv_pos = recv_buffer.end - recv_buffer.begin;

		// classify the received data as protocol chatter
		// or data payload for the statistics
		if (recv_pos <= 9)
			// only received protocol data
			m_statistics.received_bytes(0, received);
		else if (recv_pos - received >= 9)
			// only received payload data
			m_statistics.received_bytes(received, 0);
		else
		{
			// received a bit of both
			assert(recv_pos - received < 9);
			assert(recv_pos > 9);
			assert(9 - (recv_pos - received) <= 9);
			m_statistics.received_bytes(
				recv_pos - 9
				, 9 - (recv_pos - received));
		}

		m_last_piece = second_clock::universal_time();
		if (!packet_finished()) return;

		const char* ptr = recv_buffer.begin + 1;
		peer_request p;
		p.piece = detail::read_int32(ptr);
		p.start = detail::read_int32(ptr);
		p.length = packet_size() - 9;

		incoming_piece(p, recv_buffer.begin + 9);
	}

	// -----------------------------
	// ---------- CANCEL -----------
	// -----------------------------

	void bt_peer_connection::on_cancel(int received)
	{
		INVARIANT_CHECK;

		assert(received > 0);
		if (packet_size() != 13)
			throw protocol_error("'cancel' message size != 13");
		m_statistics.received_bytes(0, received);
		if (!packet_finished()) return;

		buffer::const_interval recv_buffer = receive_buffer();

		peer_request r;
		const char* ptr = recv_buffer.begin + 1;
		r.piece = detail::read_int32(ptr);
		r.start = detail::read_int32(ptr);
		r.length = detail::read_int32(ptr);

		incoming_cancel(r);
	}

	// -----------------------------
	// --------- DHT PORT ----------
	// -----------------------------

	void bt_peer_connection::on_dht_port(int received)
	{
		INVARIANT_CHECK;

		assert(received > 0);
		if (packet_size() != 3)
			throw protocol_error("'dht_port' message size != 3");
		m_statistics.received_bytes(0, received);
		if (!packet_finished()) return;

		buffer::const_interval recv_buffer = receive_buffer();

		const char* ptr = recv_buffer.begin + 1;
		int listen_port = detail::read_uint16(ptr);
		
		incoming_dht_port(listen_port);
	}

	// -----------------------------
	// ------ EXTENSION LIST -------
	// -----------------------------
/*
	void bt_peer_connection::on_extension_list(int received)
	{
		INVARIANT_CHECK;

		assert(m_torrent);
		assert(received > 0);
		if (m_packet_size > 100 * 1000)
		{
			// too big extension message, abort
			throw protocol_error("'extensions' message size > 100kB");
		}
		m_statistics.received_bytes(0, received);
		if (m_recv_pos < m_packet_size) return;

		try
		{
			entry e = bdecode(m_recv_buffer.begin()+1, m_recv_buffer.end());
#ifdef TORRENT_VERBOSE_LOGGING
			entry::dictionary_type& extensions = e.dict();
			std::stringstream ext;
			e.print(ext);
			(*m_logger) << ext.str();
#endif

			for (int i = 0; i < num_supported_extensions; ++i)
			{
				entry* f = e.find_key(extension_names[i]);
				if (f)
				{
					m_extension_messages[i] = (int)f->integer();
				}
			}
#ifdef TORRENT_VERBOSE_LOGGING
			(*m_logger) << "supported extensions:\n";
			for (entry::dictionary_type::const_iterator i = extensions.begin();
				i != extensions.end();
				++i)
			{
				(*m_logger) << i->first << "\n";
			}
#endif
		}
		catch(invalid_encoding&)
		{
			throw protocol_error("'extensions' packet contains invalid bencoding");
		}
		catch(type_error&)
		{
			throw protocol_error("'extensions' packet contains incorrect types");
		}
	}
*/
	// -----------------------------
	// --------- EXTENDED ----------
	// -----------------------------

	void bt_peer_connection::on_extended(int received)
	{
		INVARIANT_CHECK;

		assert(received > 0);
		m_statistics.received_bytes(0, received);
		if (packet_size() < 2)
			throw protocol_error("'extended' message smaller than 2 bytes");

		if (associated_torrent() == 0)
			throw protocol_error("'extended' message sent before proper handshake");

		buffer::const_interval recv_buffer = receive_buffer();
		if (recv_buffer.end - recv_buffer.begin < 2) return;

		assert(*recv_buffer.begin == msg_extended);
		++recv_buffer.begin;

		int extended_id = detail::read_uint8(recv_buffer.begin);

		if (extended_id >= 0 && extended_id < num_supported_extensions
			&& !m_ses.m_extension_enabled[extended_id])
			throw protocol_error("'extended' message using disabled extension");

		switch (extended_id)
		{
		case extended_handshake:
			on_extended_handshake(); break;
//		case extended_chat_message:
//			on_chat(); break;
		case extended_metadata_message:
			on_metadata(); break;
		case extended_peer_exchange_message:
			on_peer_exchange(); break;
		default:
			throw protocol_error("unknown extended message id: "
				+ boost::lexical_cast<std::string>(extended_id));
		};
	}

	void bt_peer_connection::on_extended_handshake() try
	{
		if (!packet_finished()) return;

		buffer::const_interval recv_buffer = receive_buffer();
	
		entry root = bdecode(recv_buffer.begin + 2, recv_buffer.end);

#ifdef TORRENT_VERBOSE_LOGGING
		std::stringstream ext;
		root.print(ext);
		(*m_logger) << "<== EXTENDED HANDSHAKE: \n" << ext.str();
#endif

		if (entry* msgs = root.find_key("m"))
		{
			if (msgs->type() == entry::dictionary_t)
			{
				// this must be the initial handshake message
				// lets see if any of our extensions are supported
				// if not, we will signal no extensions support to the upper layer
				for (int i = 1; i < num_supported_extensions; ++i)
				{
					if (entry* f = msgs->find_key(extension_names[i]))
					{
						m_extension_messages[i] = (int)f->integer();
					}
					else
					{
						m_extension_messages[i] = 0;
					}
				}
			}
		}

		// there is supposed to be a remote listen port
		if (entry* listen_port = root.find_key("p"))
		{
			if (listen_port->type() == entry::int_t)
			{
				tcp::endpoint adr((unsigned short)listen_port->integer()
					, remote().address());
				associated_torrent()->get_policy().peer_from_tracker(adr, id());
			}
		}
		// there should be a version too
		// but where do we put that info?
		
		if (entry* client_info = root.find_key("v"))
		{
			if (client_info->type() == entry::string_t)
				m_client_version = client_info->string();
		}
	}
	catch (std::exception& exc)
	{
#ifdef TORRENT_VERBOSE_LOGGIGN
		(*m_logger) << "invalid extended handshake: " << exc.what() << "\n";
#endif
	}

	// -----------------------------
	// ----------- CHAT ------------
	// -----------------------------
/*
	void bt_peer_connection::on_chat()
	{
		if (m_packet_size > 2 * 1024)
			throw protocol_error("CHAT message larger than 2 kB");

		if (m_recv_pos < m_packet_size) return;
		try
		{
			entry d = bdecode(m_recv_buffer.begin()+5, m_recv_buffer.end());
			const std::string& str = d["msg"].string();

			if (m_torrent->alerts().should_post(alert::critical))
			{
				m_torrent->alerts().post_alert(
					chat_message_alert(
						m_torrent->get_handle()
						, m_remote, str));
			}

		}
		catch (invalid_encoding&)
		{
			// TODO: make these non-fatal errors
			// they should just ignore the chat message
			// and report the error via an alert
			throw protocol_error("invalid bencoding in CHAT message");
		}
		catch (type_error&)
		{
			throw protocol_error("invalid types in bencoded CHAT message");
		}
		return;
	}
*/
	// -----------------------------
	// --------- METADATA ----------
	// -----------------------------

	void bt_peer_connection::on_metadata()
	{
		assert(associated_torrent());

		if (packet_size() > 500 * 1024)
			throw protocol_error("metadata message larger than 500 kB");

		if (!packet_finished()) return;

		buffer::const_interval recv_buffer = receive_buffer();
		recv_buffer.begin += 2;
		int type = detail::read_uint8(recv_buffer.begin);

		switch (type)
		{
		case 0: // request
			{
				int start = detail::read_uint8(recv_buffer.begin);
				int size = detail::read_uint8(recv_buffer.begin) + 1;

				if (packet_size() != 5)
				{
					// invalid metadata request
					throw protocol_error("invalid metadata request");
				}

				write_metadata(std::make_pair(start, size));
			}
			break;
		case 1: // data
			{
				if (recv_buffer.end - recv_buffer.begin < 8) return;
				int total_size = detail::read_int32(recv_buffer.begin);
				int offset = detail::read_int32(recv_buffer.begin);
				int data_size = packet_size() - 2 - 9;

				if (total_size > 500 * 1024)
					throw protocol_error("metadata size larger than 500 kB");
				if (total_size <= 0)
					throw protocol_error("invalid metadata size");
				if (offset > total_size || offset < 0)
					throw protocol_error("invalid metadata offset");
				if (offset + data_size > total_size)
					throw protocol_error("invalid metadata message");

				associated_torrent()->metadata_progress(total_size
					, recv_buffer.left() - m_metadata_progress);
				m_metadata_progress = recv_buffer.left();
				if (!packet_finished()) return;

#ifdef TORRENT_VERBOSE_LOGGING
				using namespace boost::posix_time;
				(*m_logger) << to_simple_string(second_clock::universal_time())
					<< " <== METADATA [ tot: " << total_size << " offset: "
					<< offset << " size: " << data_size << " ]\n";
#endif

				m_waiting_metadata_request = false;
				associated_torrent()->received_metadata(recv_buffer.begin, data_size
					, offset, total_size);
				m_metadata_progress = 0;
			}
			break;
		case 2: // have no data
			if (!packet_finished()) return;

			m_no_metadata = second_clock::universal_time();
			if (m_waiting_metadata_request)
				associated_torrent()->cancel_metadata_request(m_last_metadata_request);
			m_waiting_metadata_request = false;
			break;
		default:
			throw protocol_error("unknown metadata extension message: "
				+ boost::lexical_cast<std::string>(type));
		}
		
	}

	// -----------------------------
	// ------ PEER EXCHANGE --------
	// -----------------------------

	void bt_peer_connection::on_peer_exchange()
	{
		
	}

	// -----------------------------
	// ------- LISTEN PORT ---------
	// -----------------------------
	// LISTEN PORT extension is deprecated by the new extension handshake
/*
	void bt_peer_connection::on_listen_port()
	{
		using namespace boost::posix_time;
		assert(m_torrent);

		if (m_packet_size != 7)
			throw protocol_error("invalid listen_port message");

		if (is_local())
		{
#ifdef TORRENT_VERBOSE_LOGGING
			(*m_logger) << to_simple_string(second_clock::universal_time())
				<< "<== LISTEN_PORT [ UNEXPECTED ]\n";
#endif
			return;
		}

		const char* ptr = &m_recv_buffer[5];
		unsigned short port = detail::read_uint16(ptr);

#ifdef TORRENT_VERBOSE_LOGGING
		(*m_logger) << to_simple_string(second_clock::universal_time())
				<< "<== LISTEN_PORT [ port: " << port << " ]\n";
#endif

		tcp::endpoint adr = m_remote;
		adr.port(port);
		m_torrent->get_policy().peer_from_tracker(adr, m_peer_id);
	}
*/
	bool bt_peer_connection::has_metadata() const
	{
		using namespace boost::posix_time;
		return second_clock::universal_time() - m_no_metadata > minutes(5);
	}
/*
	void close_socket(boost::shared_ptr<stream_socket> s)
	{
		s->close(asio::ignore_error());
	}

	void bt_peer_connection::disconnect()
	{
		if (m_disconnecting) return;

//		assert((m_ses.m_connections.find(m_socket) != m_ses.m_connections.end())
//			== !m_connecting);

		m_disconnecting = true;
		m_ses.m_selector.post(boost::bind(&close_socket, m_socket));

		if (m_attached_to_torrent)
		{
			assert(m_torrent != 0);
			m_torrent->remove_peer(this);
			m_torrent = 0;
			m_attached_to_torrent = false;
		}
		assert(m_torrent == 0);
		m_ses.close_connection(self());
	}
*/
	bool bt_peer_connection::dispatch_message(int received)
	{
		INVARIANT_CHECK;

		assert(received > 0);

		// this means the connection has been closed already
		if (associated_torrent() == 0) return false;

		buffer::const_interval recv_buffer = receive_buffer();

		int packet_type = recv_buffer[0];
		if (packet_type < 0
			|| packet_type >= num_supported_messages
			|| m_message_handler[packet_type] == 0)
		{
			throw protocol_error("unknown message id: "
				+ boost::lexical_cast<std::string>(packet_type)
				+ " size: " + boost::lexical_cast<std::string>(packet_size()));
		}

		assert(m_message_handler[packet_type] != 0);

		// call the correct handler for this packet type
		(this->*m_message_handler[packet_type])(received);

		if (!packet_finished()) return false;

		return true;
	}

	void bt_peer_connection::write_keepalive()
	{
		INVARIANT_CHECK;

		char buf[] = {0,0,0,0};
		send_buffer(buf, buf + sizeof(buf));
	}

	void bt_peer_connection::write_cancel(peer_request const& r)
	{
		INVARIANT_CHECK;

		assert(associated_torrent()->valid_metadata());

		char buf[] = {0,0,0,13, msg_cancel};

		buffer::interval i = allocate_send_buffer(17);

		std::copy(buf, buf + 5, i.begin);
		i.begin += 5;

		// index
		detail::write_int32(r.piece, i.begin);
		// begin
		detail::write_int32(r.start, i.begin);
		// length
		detail::write_int32(r.length, i.begin);
		assert(i.begin == i.end);

		setup_send();
	}
/*
	void bt_peer_connection::send_block_requests()
	{
		// TODO: calculate the desired request queue each tick instead.
		// TODO: make this constant user-settable
		const int queue_time = 3; // seconds
		// (if the latency is more than this, the download will stall)
		// so, the queue size is 5 * down_rate / 16 kiB (16 kB is the size of each request)
		// the minimum request size is 2 and the maximum is 48
		// the block size doesn't have to be 16. So we first query the torrent for it
		const int block_size = m_torrent->block_size();
		assert(block_size > 0);
		
		int desired_queue_size = static_cast<int>(queue_time
			* statistics().download_rate() / block_size);
		if (desired_queue_size > max_request_queue) desired_queue_size = max_request_queue;
		if (desired_queue_size < min_request_queue) desired_queue_size = min_request_queue;

		if ((int)m_download_queue.size() >= desired_queue_size) return;

		while (!m_request_queue.empty()
			&& (int)m_download_queue.size() < desired_queue_size)
		{
			piece_block block = m_request_queue.front();
			m_request_queue.pop_front();
			m_download_queue.push_back(block);

			int block_offset = block.block_index * m_torrent->block_size();
			int block_size
				= std::min((int)m_torrent->torrent_file().piece_size(block.piece_index)-block_offset,
						m_torrent->block_size());
			assert(block_size > 0);
			assert(block_size <= m_torrent->block_size());

			char buf[] = {0,0,0,13, msg_request};

			buffer::interval i = m_send_buffer.allocate(17);

			std::copy(buf, buf + 5, i.begin);
			i.begin += 5;

			// index
			detail::write_int32(block.piece_index, i.begin);
			// begin
			detail::write_int32(block_offset, i.begin);
			// length
			detail::write_int32(block_size, i.begin);

			assert(i.begin == i.end);
			using namespace boost::posix_time;

#ifdef TORRENT_VERBOSE_LOGGING
			(*m_logger) << to_simple_string(second_clock::universal_time())
				<< " ==> REQUEST [ "
				"piece: " << block.piece_index << " | "
			"b: " << block.block_index << " | "
			"s: " << block_offset << " | "
			"l: " << block_size << " ]\n";

		peer_request r;
		r.piece = block.piece_index;
		r.start = block_offset;
		r.length = block_size;
		assert(verify_piece(r));
#endif
		}
		m_last_piece = second_clock::universal_time();
		send_buffer_updated();

	}
*/	
	void bt_peer_connection::write_request(peer_request const& r)
	{
		INVARIANT_CHECK;

		assert(associated_torrent()->valid_metadata());

		char buf[] = {0,0,0,13, msg_request};

		buffer::interval i = allocate_send_buffer(17);

		std::copy(buf, buf + 5, i.begin);
		i.begin += 5;

		// index
		detail::write_int32(r.piece, i.begin);
		// begin
		detail::write_int32(r.start, i.begin);
		// length
		detail::write_int32(r.length, i.begin);
		assert(i.begin == i.end);

		setup_send();
	}

	void bt_peer_connection::write_metadata(std::pair<int, int> req)
	{
		assert(req.first >= 0);
		assert(req.second > 0);
		assert(req.second <= 256);
		assert(req.first + req.second <= 256);
		assert(associated_torrent());
		INVARIANT_CHECK;

		// abort if the peer doesn't support the metadata extension
		if (!supports_extension(extended_metadata_message)) return;

		if (associated_torrent()->valid_metadata())
		{
			std::pair<int, int> offset
				= req_to_offset(req, (int)associated_torrent()->metadata().size());

			buffer::interval i = allocate_send_buffer(15 + offset.second);

			// yes, we have metadata, send it
			detail::write_uint32(11 + offset.second, i.begin);
			detail::write_uint8(msg_extended, i.begin);
			detail::write_uint8(m_extension_messages[extended_metadata_message]
				, i.begin);
			// means 'data packet'
			detail::write_uint8(1, i.begin);
			detail::write_uint32((int)associated_torrent()->metadata().size(), i.begin);
			detail::write_uint32(offset.first, i.begin);
			std::vector<char> const& metadata = associated_torrent()->metadata();
			std::copy(metadata.begin() + offset.first
				, metadata.begin() + offset.first + offset.second, i.begin);
			i.begin += offset.second;
			assert(i.begin == i.end);
		}
		else
		{
			buffer::interval i = allocate_send_buffer(4 + 3);
			// we don't have the metadata, reply with
			// don't have-message
			detail::write_uint32(1 + 2, i.begin);
			detail::write_uint8(msg_extended, i.begin);
			detail::write_uint8(m_extension_messages[extended_metadata_message]
				, i.begin);
			// means 'have no data'
			detail::write_uint8(2, i.begin);
			assert(i.begin == i.end);
		}
		setup_send();
	}

	void bt_peer_connection::write_metadata_request(std::pair<int, int> req)
	{
		assert(req.first >= 0);
		assert(req.second > 0);
		assert(req.first + req.second <= 256);
		assert(associated_torrent());
		assert(!associated_torrent()->valid_metadata());
		INVARIANT_CHECK;

		int start = req.first;
		int size = req.second;

		// abort if the peer doesn't support the metadata extension
		if (!supports_extension(extended_metadata_message)) return;

#ifdef TORRENT_VERBOSE_LOGGING
		using namespace boost::posix_time;
		(*m_logger) << to_simple_string(second_clock::universal_time())
			<< " ==> METADATA_REQUEST [ start: " << req.first
			<< " size: " << req.second << " ]\n";
#endif

		buffer::interval i = allocate_send_buffer(9);

		detail::write_uint32(1 + 1 + 3, i.begin);
		detail::write_uint8(msg_extended, i.begin);
		detail::write_uint8(m_extension_messages[extended_metadata_message]
			, i.begin);
		// means 'request data'
		detail::write_uint8(0, i.begin);
		detail::write_uint8(start, i.begin);
		detail::write_uint8(size - 1, i.begin);
		assert(i.begin == i.end);
		setup_send();
	}
/*
	void bt_peer_connection::write_chat_message(const std::string& msg)
	{
		INVARIANT_CHECK;

		assert(msg.length() <= 1 * 1024);
		if (!supports_extension(extended_chat_message)) return;

		entry e(entry::dictionary_t);
		e["msg"] = msg;

		std::vector<char> message;
		bencode(std::back_inserter(message), e);

		buffer::interval i = m_send_buffer.allocate(message.size() + 9);

		detail::write_uint32(1 + 4 + (int)message.size(), i.begin);
		detail::write_uint8(msg_extended, i.begin);
		detail::write_int32(m_extension_messages[extended_chat_message], i.begin);
		std::copy(message.begin(), message.end(), i.begin);
		i.begin += message.size();
		assert(i.begin == i.end);
		send_buffer_updated();
	}
*/
	void bt_peer_connection::write_bitfield(std::vector<bool> const& bitfield)
	{
		INVARIANT_CHECK;

		if (associated_torrent()->num_pieces() == 0) return;

#ifdef TORRENT_VERBOSE_LOGGING
		using namespace boost::posix_time;
		(*m_logger) << to_simple_string(second_clock::universal_time())
			<< " ==> BITFIELD ";

		for (int i = 0; i < (int)get_bitfield().size(); ++i)
		{
			if (bitfield[i]) (*m_logger) << "1";
			else (*m_logger) << "0";
		}
		(*m_logger) << "\n";
#endif
		const int packet_size = ((int)bitfield.size() + 7) / 8 + 5;
	
		buffer::interval i = allocate_send_buffer(packet_size);	

		detail::write_int32(packet_size - 4, i.begin);
		detail::write_uint8(msg_bitfield, i.begin);

		std::fill(i.begin, i.end, 0);
		for (int c = 0; c < (int)bitfield.size(); ++c)
		{
			if (bitfield[c])
				i.begin[c >> 3] |= 1 << (7 - (c & 7));
		}
		assert(i.end - i.begin == ((int)bitfield.size() + 7) / 8);
		setup_send();
	}

	void bt_peer_connection::write_extensions()
	{
		INVARIANT_CHECK;

#ifdef TORRENT_VERBOSE_LOGGING
		using namespace boost::posix_time;
		(*m_logger) << to_simple_string(second_clock::universal_time())
			<< " ==> EXTENSIONS\n";
#endif
		assert(m_supports_extensions);

		entry handshake(entry::dictionary_t);
		entry extension_list(entry::dictionary_t);

		for (int i = 1; i < num_supported_extensions; ++i)
		{
			// if this specific extension is disabled
			// just don't add it to the supported set
			if (!m_ses.m_extension_enabled[i]) continue;
			extension_list[extension_names[i]] = i;
		}

		handshake["m"] = extension_list;
		handshake["p"] = m_ses.m_listen_interface.port();
		handshake["v"] = m_ses.m_http_settings.user_agent;

		std::vector<char> msg;
		bencode(std::back_inserter(msg), extended_handshake);

		// make room for message
		buffer::interval i = allocate_send_buffer(6 + msg.size());
		
		// write the length of the message
		detail::write_int32((int)msg.size() + 2, i.begin);
		detail::write_uint8(msg_extended, i.begin);
		// signal handshake message
		detail::write_uint8(extended_handshake, i.begin);

		std::copy(msg.begin(), msg.end(), i.begin);
		i.begin += msg.size();
		assert(i.begin == i.end);

		setup_send();
	}

/*
	void bt_peer_connection::write_extensions()
	{
		INVARIANT_CHECK;

#ifdef TORRENT_VERBOSE_LOGGING
		using namespace boost::posix_time;
		(*m_logger) << to_simple_string(second_clock::universal_time())
			<< " ==> EXTENSIONS\n";
#endif
		assert(m_supports_extensions);

		entry extension_list(entry::dictionary_t);

		for (int i = 0; i < num_supported_extensions; ++i)
		{
			// if this specific extension is disabled
			// just don't add it to the supported set
			if (!m_ses.m_extension_enabled[i]) continue;
			extension_list[extension_names[i]] = i;
		}

		std::vector<char> msg;
		bencode(std::back_inserter(msg), extension_list);

		// make room for message
		buffer::interval i = allocate_send_buffer(5 + msg.size());

		// write the length of the message
		detail::write_int32((int)msg.size() + 1, i.begin);

		detail::write_uint8(msg_extension_list, i.begin);

		std::copy(msg.begin(), msg.end(), i.begin);
		i.begin += msg.size();
		assert(i.begin == i.end);

		setup_send();
	}
*/
	void bt_peer_connection::write_choke()
	{
		INVARIANT_CHECK;

		if (is_choked()) return;
		char msg[] = {0,0,0,1,msg_choke};
		send_buffer(msg, msg + sizeof(msg));
	}

	void bt_peer_connection::write_unchoke()
	{
		INVARIANT_CHECK;

		char msg[] = {0,0,0,1,msg_unchoke};
		send_buffer(msg, msg + sizeof(msg));
	}

	void bt_peer_connection::write_interested()
	{
		INVARIANT_CHECK;

		char msg[] = {0,0,0,1,msg_interested};
		send_buffer(msg, msg + sizeof(msg));
	}

	void bt_peer_connection::write_not_interested()
	{
		INVARIANT_CHECK;

		char msg[] = {0,0,0,1,msg_not_interested};
		send_buffer(msg, msg + sizeof(msg));
	}

	void bt_peer_connection::write_have(int index)
	{
		assert(associated_torrent()->valid_metadata());
		assert(index >= 0);
		assert(index < associated_torrent()->torrent_file().num_pieces());
		INVARIANT_CHECK;

		const int packet_size = 9;
		char msg[packet_size] = {0,0,0,5,msg_have};
		char* ptr = msg + 5;
		detail::write_int32(index, ptr);
		send_buffer(msg, msg + packet_size);
	}
/*
	size_type bt_peer_connection::share_diff() const
	{
		float ratio = associated_torrent()->ratio();

		// if we have an infinite ratio, just say we have downloaded
		// much more than we have uploaded. And we'll keep uploading.
		if (ratio == 0.f)
			return std::numeric_limits<size_type>::max();

		return m_free_upload
			+ static_cast<size_type>(m_statistics.total_payload_download() * ratio)
			- m_statistics.total_payload_upload();
	}

	void bt_peer_connection::second_tick()
	{
		INVARIANT_CHECK;

		ptime now(second_clock::universal_time());
		
		// TODO: the timeout should be user-settable
		if (!m_download_queue.empty()
			&& now - m_last_piece > seconds(m_ses.m_settings.piece_timeout))
		{
			// this peer isn't sending the pieces we've
			// requested (this has been observed by BitComet)
			// in this case we'll clear our download queue and
			// re-request the blocks.
#ifdef TORRENT_VERBOSE_LOGGING
			(*m_logger) << to_simple_string(now)
				<< " *** PIECE_REQUESTS TIMED OUT [ " << (int)m_download_queue.size()
				<< " " << to_simple_string(now - m_last_piece) << "] ***\n";
#endif

			piece_picker& picker = associated_torrent()->picker();
			for (std::deque<piece_block>::const_iterator i = m_download_queue.begin()
				, end(m_download_queue.end()); i != end; ++i)
			{
				// since this piece was skipped, clear it and allow it to
				// be requested from other peers
				picker.abort_download(*i);
			}
			for (std::deque<piece_block>::const_iterator i = m_request_queue.begin()
				, end(m_request_queue.end()); i != end; ++i)
			{
				// since this piece was skipped, clear it and allow it to
				// be requested from other peers
				picker.abort_download(*i);
			}

			m_download_queue.clear();
			m_request_queue.clear();
			
			m_assume_fifo = true;

			// this will trigger new picking of pieces
			associated_torrent()->get_policy().unchoked(*this);
		}
	
		// if we don't have any metadata, and this peer
		// supports the request metadata extension
		// and we aren't currently waiting for a request
		// reply. Then, send a request for some metadata.
		if (!associated_torrent()->valid_metadata()
			&& supports_extension(extended_metadata_message)
			&& !m_waiting_metadata_request
			&& has_metadata())
		{
			assert(associated_torrent());
			m_last_metadata_request = associated_torrent()->metadata_request();
			send_metadata_request(m_last_metadata_request);
			m_waiting_metadata_request = true;
			m_metadata_request = now;
		}

		m_statistics.second_tick();
		m_ul_bandwidth_quota.used = std::min(
			(int)ceil(statistics().upload_rate())
			, m_ul_bandwidth_quota.given);

		// If the client sends more data
		// we send it data faster, otherwise, slower.
		// It will also depend on how much data the
		// client has sent us. This is the mean to
		// maintain the share ratio given by m_ratio
		// with all peers.

		if (associated_torrent()->is_seed() || is_choked() || associated_torrent()->ratio() == 0.0f)
		{
			// if we have downloaded more than one piece more
			// than we have uploaded OR if we are a seed
			// have an unlimited upload rate
			if(!m_send_buffer.empty() || (!m_requests.empty() && !is_choked()))
				m_ul_bandwidth_quota.max = resource_request::inf;
			else
				m_ul_bandwidth_quota.max = m_ul_bandwidth_quota.min;
		}
		else
		{
			size_type bias = 0x10000+2*associated_torrent()->block_size() + m_free_upload;

			double break_even_time = 15; // seconds.
			size_type have_uploaded = m_statistics.total_payload_upload();
			size_type have_downloaded = m_statistics.total_payload_download();
			double download_speed = m_statistics.download_rate();

			size_type soon_downloaded =
				have_downloaded + (size_type)(download_speed * break_even_time*1.5);

			if(associated_torrent()->ratio() != 1.f)
				soon_downloaded = (size_type)(soon_downloaded*(double)associated_torrent()->ratio());

			double upload_speed_limit = (soon_downloaded - have_uploaded
				                         + bias) / break_even_time;

			upload_speed_limit = std::min(upload_speed_limit,
				(double)std::numeric_limits<int>::max());

			m_ul_bandwidth_quota.max
				= std::max((int)upload_speed_limit, m_ul_bandwidth_quota.min);
		}
		if (m_ul_bandwidth_quota.given > m_ul_bandwidth_quota.max)
			m_ul_bandwidth_quota.given = m_ul_bandwidth_quota.max;

		if (m_ul_bandwidth_quota.used > m_ul_bandwidth_quota.given)
			m_ul_bandwidth_quota.used = m_ul_bandwidth_quota.given;

		fill_send_buffer();


		size_type diff = share_diff();

		enum { block_limit = 2 }; // how many blocks difference is considered unfair

		// if the peer has been choked, send the current piece
		// as fast as possible
		if (diff > block_limit*m_torrent->block_size() || m_torrent->is_seed() || is_choked())
		{
			// if we have downloaded more than one piece more
			// than we have uploaded OR if we are a seed
			// have an unlimited upload rate
			m_ul_bandwidth_quota.wanted = std::numeric_limits<int>::max();
		}
		else
		{
			float ratio = m_torrent->ratio();
			// if we have downloaded too much, response with an
			// upload rate of 10 kB/s more than we dowlload
			// if we have uploaded too much, send with a rate of
			// 10 kB/s less than we receive
			int bias = 0;
			if (diff > -block_limit*m_torrent->block_size())
			{
				bias = static_cast<int>(m_statistics.download_rate() * ratio) / 2;
				if (bias < 10*1024) bias = 10*1024;
			}
			else
			{
				bias = -static_cast<int>(m_statistics.download_rate() * ratio) / 2;
			}
			m_ul_bandwidth_quota.wanted = static_cast<int>(m_statistics.download_rate()) + bias;

			// the maximum send_quota given our download rate from this peer
			if (m_ul_bandwidth_quota.wanted < 256) m_ul_bandwidth_quota.wanted = 256;
		}

	}

	void bt_peer_connection::fill_send_buffer()
	{
		if (!can_write()) return;

		// only add new piece-chunks if the send buffer is small enough
		// otherwise there will be no end to how large it will be!
		// TODO: the buffer size should probably be dependent on the transfer speed
		while (!m_requests.empty()
			&& ((int)m_send_buffer.size() < associated_torrent()->block_size() * 6)
			&& !m_choked)
		{
			assert(associated_torrent()->valid_metadata());
			peer_request& r = m_requests.front();
			
			assert(r.piece >= 0);
			assert(r.piece < (int)m_have_piece.size());
			assert(associated_torrent() != 0);
			assert(associated_torrent()->have_piece(r.piece));
			assert(r.start + r.length <= associated_torrent()->torrent_file().piece_size(r.piece));
			assert(r.length > 0 && r.start >= 0);

			const int packet_size = 4 + 5 + 4 + r.length;

			buffer::interval i = m_send_buffer.allocate(packet_size);
			
			detail::write_int32(packet_size-4, i.begin);
			detail::write_uint8(msg_piece, i.begin);
			detail::write_int32(r.piece, i.begin);
			detail::write_int32(r.start, i.begin);

			associated_torrent()->filesystem().read(
				i.begin
				, r.piece
				, r.start
				, r.length);
			assert(i.begin + r.length == i.end);
#ifdef TORRENT_VERBOSE_LOGGING
			using namespace boost::posix_time;
			(*m_logger) << to_simple_string(second_clock::universal_time())
				<< " ==> PIECE   [ piece: " << r.piece << " | s: " << r.start << " | l: " << r.length << " ]\n";
#endif

			m_payloads.push_back(range(m_send_buffer.size() - r.length, r.length));
			m_requests.erase(m_requests.begin());

			if (m_requests.empty()
				&& m_num_invalid_requests > 0
				&& is_peer_interested()
				&& !is_seed())
			{
				// this will make the peer clear
				// its download queue and re-request
				// pieces. Hopefully it will not
				// send invalid requests then
				send_choke();
				send_unchoke();
			}
		}
	}
*/
	void bt_peer_connection::write_piece(peer_request const& r)
	{
		const int packet_size = 4 + 5 + 4 + r.length;

		buffer::interval i = allocate_send_buffer(packet_size);
		
		detail::write_int32(packet_size-4, i.begin);
		detail::write_uint8(msg_piece, i.begin);
		detail::write_int32(r.piece, i.begin);
		detail::write_int32(r.start, i.begin);

		associated_torrent()->filesystem().read(
			i.begin, r.piece, r.start, r.length);

		assert(i.begin + r.length == i.end);

		m_payloads.push_back(range(send_buffer_size() - r.length, r.length));
		setup_send();
	}
/*
	void bt_peer_connection::setup_send()
	{
		assert(!m_writing);

		// send the actual buffer
		if (!m_send_buffer.empty())
		{
			int amount_to_send
				= std::min(m_ul_bandwidth_quota.left(), (int)m_send_buffer.size());

			assert(amount_to_send > 0);

			buffer::interval_type send_buffer = m_send_buffer.data();
			// we have data that's scheduled for sending
			int to_send = std::min(int(send_buffer.first.end - send_buffer.first.begin)
				, amount_to_send);

			boost::array<asio::const_buffer, 2> bufs;
			assert(to_send >= 0);
			bufs[0] = asio::buffer(send_buffer.first.begin, to_send);

			to_send = std::min(int(send_buffer.second.end - send_buffer.second.begin)
				, amount_to_send - to_send);
			assert(to_send >= 0);
			bufs[1] = asio::buffer(send_buffer.second.begin, to_send);

			assert(m_ul_bandwidth_quota.left() >= int(buffer_size(bufs[0]) + buffer_size(bufs[1])));
			assert(can_write());
			m_socket->async_write_some(bufs, bind(&bt_peer_connection::on_send_data
				, self(), _1, _2));
			m_writing = true;
			m_last_write_size = amount_to_send;
		}
	}

	void bt_peer_connection::setup_receive()
	{
		if (m_reading) return;
		if (!can_read()) return;

		assert(m_packet_size > 0);
		int max_receive = std::min(
			m_dl_bandwidth_quota.left()
			, m_packet_size - m_recv_pos);

		assert(m_recv_pos >= 0);
		assert(m_packet_size > 0);
		assert(m_dl_bandwidth_quota.left() > 0);
		assert(max_receive > 0);

		assert(can_read());
		m_socket->async_read_some(asio::buffer(&m_recv_buffer[m_recv_pos]
			, max_receive), bind(&bt_peer_connection::on_receive_data, self(), _1, _2));
		m_reading = true;
		m_last_read_size = max_receive;
		m_dl_bandwidth_quota.used += max_receive;
		assert(m_dl_bandwidth_quota.used <= m_dl_bandwidth_quota.given);
	}
*/

	// --------------------------
	// RECEIVE DATA
	// --------------------------

	// throws exception when the client should be disconnected
	void bt_peer_connection::on_receive(const asio::error& error
		, std::size_t bytes_transferred)
	{
		INVARIANT_CHECK;

		if (error) return;
	
		buffer::const_interval recv_buffer = receive_buffer();
	
		switch(m_state)
		{
		case read_protocol_length:
		{
			m_statistics.received_bytes(0, bytes_transferred);
			if (!packet_finished()) break;

			int packet_size = recv_buffer[0];

#ifdef TORRENT_VERBOSE_LOGGING
			(*m_logger) << " protocol length: " << packet_size << "\n";
#endif
			if (packet_size > 100 || packet_size <= 0)
			{
				std::stringstream s;
				s << "incorrect protocol length ("
					<< packet_size
					<< ") should be 19.";
				throw std::runtime_error(s.str());
			}
			m_state = read_protocol_string;
			reset_recv_buffer(packet_size);
		}
		break;

		case read_protocol_string:
		{
			m_statistics.received_bytes(0, bytes_transferred);
			if (!packet_finished()) break;

#ifdef TORRENT_VERBOSE_LOGGING
			(*m_logger) << " protocol: '" << std::string(recv_buffer.begin
				, recv_buffer.end) << "'\n";
#endif
			const char protocol_string[] = "BitTorrent protocol";
			if (!std::equal(recv_buffer.begin, recv_buffer.end
				, protocol_string))
			{
				const char cmd[] = "version";
				if (recv_buffer.end - recv_buffer.begin == 7 && std::equal(
					recv_buffer.begin, recv_buffer.end, cmd))
				{
#ifdef TORRENT_VERBOSE_LOGGING
					(*m_logger) << "sending libtorrent version\n";
#endif
					asio::write(*get_socket(), asio::buffer("libtorrent version " LIBTORRENT_VERSION "\n", 27));
					throw std::runtime_error("closing");
				}
#ifdef TORRENT_VERBOSE_LOGGING
				(*m_logger) << "incorrect protocol name\n";
#endif
				std::stringstream s;
				s << "got invalid protocol name: '"
					<< std::string(recv_buffer.begin, recv_buffer.end)
					<< "'";
				throw std::runtime_error(s.str());
			}

			m_state = read_info_hash;
			reset_recv_buffer(28);
		}
		break;

		case read_info_hash:
		{
			m_statistics.received_bytes(0, bytes_transferred);
			if (!packet_finished()) break;

			// the use of this bit collides with Mainline
			// the new way of identifying support for the extensions
			// is in the peer_id

// MassaRoddel
#ifdef TORRENT_VERBOSE_LOGGING	
			for (int i=0; i < 8; ++i)
			{
				for (int j=0; j < 8; ++j)
				{
					if (recv_buffer[i] & (0x80 >> j)) (*m_logger) << "1";
					else (*m_logger) << "0";
				}
			}
			(*m_logger) << "\n";
			if (recv_buffer[7] & 0x01)
				(*m_logger) << "supports DHT port message\n";
			if (recv_buffer[7] & 0x02)
				(*m_logger) << "supports XBT peer exchange message\n";
			if (recv_buffer[5] & 0x10)
				(*m_logger) << "supports LT/uT extensions\n";
#endif

			if ((recv_buffer[5] & 0x10) && m_ses.extensions_enabled())
				m_supports_extensions = true;

			// ok, now we have got enough of the handshake. Is this connection
			// attached to a torrent?
			if (associated_torrent() == 0)
			{
				// now, we have to see if there's a torrent with the
				// info_hash we got from the peer
				sha1_hash info_hash;
				std::copy(recv_buffer.begin + 8, recv_buffer.begin + 28, (char*)info_hash.begin());

				attach_to_torrent(info_hash);
				assert(associated_torrent()->get_policy().has_connection(this));

				// yes, we found the torrent
				// reply with our handshake
				write_handshake();
				write_bitfield(associated_torrent()->pieces());
			}
			else
			{
				// verify info hash
				if (!std::equal(recv_buffer.begin + 8, recv_buffer.begin + 28
					, (const char*)associated_torrent()->torrent_file().info_hash().begin()))
				{
#ifdef TORRENT_VERBOSE_LOGGING
					(*m_logger) << " received invalid info_hash\n";
#endif
					throw std::runtime_error("invalid info-hash in handshake");
				}
			}

			m_state = read_peer_id;
			reset_recv_buffer(20);
#ifdef TORRENT_VERBOSE_LOGGING
			(*m_logger) << " info_hash received\n";
#endif
		}
		break;

		case read_peer_id:
		{
			m_statistics.received_bytes(0, bytes_transferred);
			if (!packet_finished()) break;
			assert(packet_size() == 20);

#ifdef TORRENT_VERBOSE_LOGGING
			{
				peer_id tmp;
				std::copy(recv_buffer.begin, recv_buffer.begin + 20, (char*)tmp.begin());
				std::stringstream s;
				s << "received peer_id: " << tmp << " client: " << identify_client(tmp) << "\n";
				s << "as ascii: ";
				for (peer_id::iterator i = tmp.begin(); i != tmp.end(); ++i)
				{
					if (std::isprint(*i)) s << *i;
					else s << ".";
				}
				s << "\n";
				(*m_logger) << s.str();
			}
#endif
			peer_id pid;
			std::copy(recv_buffer.begin, recv_buffer.begin + 20, (char*)pid.begin());
			set_id(pid);

//			if (std::memcmp(&pid[17], "ext", 3) == 0)
//				m_supports_extensions = true;

			// disconnect if the peer has the same peer-id as ourself
			// since it most likely is ourself then
			if (id() == m_ses.get_peer_id())
				throw std::runtime_error("closing connection to ourself");
					
			// TODO: support extension
//			if (m_supports_extensions) write_extensions();
/*
			if (!m_active)
			{
				m_attached_to_torrent = true;
				assert(m_torrent->get_policy().has_connection(this));
			}
*/
			m_state = read_packet_size;
			reset_recv_buffer(4);
		}
		break;

		case read_packet_size:
		{
			m_statistics.received_bytes(0, bytes_transferred);
			if (!packet_finished()) break;

			const char* ptr = recv_buffer.begin;
			int packet_size = detail::read_int32(ptr);

			// don't accept packets larger than 1 MB
			if (packet_size > 1024*1024 || packet_size < 0)
			{
				// packet too large
				throw std::runtime_error("packet > 1 MB");
			}
					
			if (packet_size == 0)
			{
				incoming_keepalive();
				// keepalive message
				m_state = read_packet_size;
				reset_recv_buffer(4);
			}
			else
			{
				m_state = read_packet;
				reset_recv_buffer(packet_size);
			}
		}
		break;

		case read_packet:
		{
			if (dispatch_message(bytes_transferred))
			{
				m_state = read_packet_size;
				reset_recv_buffer(4);
			}
		}
		break;

		}
	}

/*
	bool bt_peer_connection::can_write() const
	{
		// if we have requests or pending data to be sent or announcements to be made
		// we want to send data
		return ((!m_requests.empty() && !m_choked)
			|| !m_send_buffer.empty())
			&& m_ul_bandwidth_quota.left() > 0
			&& !m_connecting;
	}

	bool bt_peer_connection::can_read() const
	{
		return m_dl_bandwidth_quota.left() > 0 && !m_connecting;
	}

	void bt_peer_connection::connect()
	{
		INVARIANT_CHECK;

#if defined(TORRENT_VERBOSE_LOGGING) || defined(TORRENT_LOGGING)
		(*m_ses.m_logger) << "CONNECTING: " << m_remote.address().to_string() << "\n";
#endif

		m_queued = false;
		assert(m_connecting);
		assert(associated_torrent());
		m_socket->open(asio::ipv4::tcp());
		m_socket->bind(associated_torrent()->get_interface());
		m_socket->async_connect(m_remote
			, bind(&bt_peer_connection::on_connection_complete, self(), _1));

		if (m_torrent->alerts().should_post(alert::debug))
		{
			m_torrent->alerts().post_alert(peer_error_alert(
				m_remote, m_peer_id, "connecting to peer"));
		}
	}
	
	void bt_peer_connection::on_connection_complete(asio::error const& e)
	{
		INVARIANT_CHECK;
		
		if (e == asio::error::operation_aborted) return;

		session_impl::mutex_t::scoped_lock l(m_ses.m_mutex);
		if (e)
		{
#if defined(TORRENT_VERBOSE_LOGGING) || defined(TORRENT_LOGGING)
			(*m_ses.m_logger) << "CONNECTION FAILED: " << m_remote.address().to_string() << "\n";
#endif
			m_ses.connection_failed(m_socket, m_remote, e.what());
//			disconnect();
			return;
		}

		// this means the connection just succeeded

#if defined(TORRENT_VERBOSE_LOGGING) || defined(TORRENT_LOGGING)
		(*m_ses.m_logger) << "COMPLETED: " << m_remote.address().to_string() << "\n";
#endif

		m_connecting = false;
		setup_receive();
		m_ses.connection_completed(self());
	}
*/
	// --------------------------
	// SEND DATA
	// --------------------------

	// throws exception when the client should be disconnected
	void bt_peer_connection::on_sent(asio::error const& error
		, std::size_t bytes_transferred)
	{
		INVARIANT_CHECK;

		if (error) return;

		// manage the payload markers
		int amount_payload = 0;
		if (!m_payloads.empty())
		{
			for (std::deque<range>::iterator i = m_payloads.begin();
				i != m_payloads.end(); ++i)
			{
				i->start -= bytes_transferred;
				if (i->start < 0)
				{
					if (i->start + i->length <= 0)
					{
						amount_payload += i->length;
					}
					else
					{
						amount_payload += -i->start;
						i->length -= -i->start;
						i->start = 0;
					}
				}
			}
		}

		// TODO: move the erasing into the loop above
		// remove all payload ranges that has been sent
		m_payloads.erase(
			std::remove_if(m_payloads.begin(), m_payloads.end(), range_below_zero)
			, m_payloads.end());

		assert(amount_payload <= (int)bytes_transferred);
		m_statistics.sent_bytes(amount_payload, bytes_transferred - amount_payload);
	}


#ifndef NDEBUG
	void bt_peer_connection::check_invariant() const
	{
/*
		assert(m_num_pieces == std::count(
			m_have_piece.begin()
			, m_have_piece.end()
			, true));
*/	}
#endif
/*
	bool bt_peer_connection::has_timed_out() const
	{
		using namespace boost::posix_time;

		ptime now(second_clock::universal_time());
		
		// if the socket is still connecting, don't
		// consider it timed out. Because Windows XP SP2
		// may delay connection attempts.
		if (m_connecting) return false;
		
		// if the peer hasn't said a thing for a certain
		// time, it is considered to have timed out
		time_duration d;
		d = second_clock::universal_time() - m_last_receive;
		if (d > seconds(m_timeout)) return true;

		// if the peer hasn't become interested and we haven't
		// become interested in the peer for 10 minutes, it
		// has also timed out.
		time_duration d1;
		time_duration d2;
		d1 = now - m_became_uninterested;
		d2 = now - m_became_uninteresting;
		// than being polled like this
		if (!m_interesting
			&& !m_peer_interested
			&& d1 > minutes(10)
			&& d2 > minutes(10))
		{
			return true;
		}

		return false;
	}


	void bt_peer_connection::keep_alive()
	{
		INVARIANT_CHECK;

		boost::posix_time::time_duration d;
		d = second_clock::universal_time() - m_last_sent;
		if (d.total_seconds() < m_timeout / 2) return;

		// we must either send a keep-alive
		// message or something else.
		if (m_announce_queue.empty())
		{
			char noop[] = {0,0,0,0};
			m_send_buffer.insert(noop, noop + 4);
			m_last_sent = second_clock::universal_time();
#ifdef TORRENT_VERBOSE_LOGGING
			using namespace boost::posix_time;
			(*m_logger) << to_simple_string(second_clock::universal_time())
				<< " ==> NOP\n";
#endif
		}
		else
		{
			for (std::vector<int>::iterator i = m_announce_queue.begin();
				i != m_announce_queue.end(); ++i)
			{
				send_have(*i);
			}
			m_announce_queue.clear();
		}
		send_buffer_updated();
	}

	bool bt_peer_connection::is_seed() const
	{
		// if m_num_pieces == 0, we probably doesn't have the
		// metadata yet.
		return m_num_pieces == (int)m_have_piece.size() && m_num_pieces > 0;
	}
	*/
}

