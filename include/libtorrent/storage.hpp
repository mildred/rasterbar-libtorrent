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

#ifndef TORRENT_STORAGE_HPP_INCLUDE
#define TORRENT_STORAGE_HPP_INCLUDE

#include <vector>
#include <sys/types.h>

#ifdef _MSC_VER
#pragma warning(push, 1)
#endif

#include <boost/function/function2.hpp>
#include <boost/function/function0.hpp>
#include <boost/limits.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/intrusive_ptr.hpp>
#include <boost/unordered_set.hpp>
#include <boost/atomic.hpp>

#ifdef _MSC_VER
#pragma warning(pop)
#endif


#include "libtorrent/piece_picker.hpp"
#include "libtorrent/peer_request.hpp"
#include "libtorrent/hasher.hpp"
#include "libtorrent/config.hpp"
#include "libtorrent/file.hpp"
#include "libtorrent/disk_buffer_holder.hpp"
#include "libtorrent/thread.hpp"
#include "libtorrent/storage_defs.hpp"
#include "libtorrent/allocator.hpp"
#include "libtorrent/file_pool.hpp" // pool_file_status
#include "libtorrent/part_file.hpp"
#include "libtorrent/stat_cache.hpp"
#include "libtorrent/lazy_entry.hpp"
#include "libtorrent/bitfield.hpp"

// OVERVIEW
//
// libtorrent provides a customization point for storage of data. By default,
// (``default_storage``) downloaded files are saved to disk according with the
// general conventions of bittorrent clients, mimicing the original file layout
// when the torrent was created. The libtorrent user may define a custom
// storage to store piece data in a different way.
// 
// A custom storage implementation must derive from and implement the
// storage_interface. You must also provide a function that constructs the
// custom storage object and provide this function to the add_torrent() call
// via add_torrent_params. Either passed in to the constructor or by setting
// the add_torrent_params::storage field.
// 
// This is an example storage implementation that stores all pieces in a
// ``std::map``, i.e. in RAM. It's not necessarily very useful in practice, but
// illustrates the basics of implementing a custom storage.
//
//::
//
//	struct temp_storage : storage_interface
//	{
//		temp_storage(file_storage const& fs) : m_files(fs) {}
//		virtual bool initialize(storage_error& se) { return false; }
//		virtual bool has_any_file() { return false; }
//		virtual int read(char* buf, int slot, int offset, int size)
//		{
//			std::map<int, std::vector<char> >::const_iterator i = m_file_data.find(slot);
//			if (i == m_file_data.end()) return 0;
//			int available = i->second.size() - offset;
//			if (available <= 0) return 0;
//			if (available > size) available = size;
//			memcpy(buf, &i->second[offset], available);
//			return available;
//		}
//		virtual int write(const char* buf, int slot, int offset, int size)
//		{
//			std::vector<char>& data = m_file_data[slot];
//			if (data.size() < offset + size) data.resize(offset + size);
//			std::memcpy(&data[offset], buf, size);
//			return size;
//		}
//		virtual bool rename_file(int file, std::string const& new_name)
//		{ assert(false); return false; }
//		virtual bool move_storage(std::string const& save_path) { return false; }
//		virtual bool verify_resume_data(lazy_entry const& rd, storage_error& error) { return false; }
//		virtual bool write_resume_data(entry& rd) const { return false; }
//		virtual size_type physical_offset(int slot, int offset)
//		{ return slot * m_files.piece_length() + offset; };
//		virtual sha1_hash hash_for_slot(int slot, partial_hash& ph, int piece_size)
//		{
//			int left = piece_size - ph.offset;
//			assert(left >= 0);
//			if (left > 0)
//			{
//				std::vector<char>& data = m_file_data[slot];
//				// if there are padding files, those blocks will be considered
//				// completed even though they haven't been written to the storage.
//				// in this case, just extend the piece buffer to its full size
//				// and fill it with zeroes.
//				if (data.size() < piece_size) data.resize(piece_size, 0);
//				ph.h.update(&data[ph.offset], left);
//			}
//			return ph.h.final();
//		}
//		virtual bool release_files() { return false; }
//		virtual bool delete_files() { return false; }
//	
//		std::map<int, std::vector<char> > m_file_data;
//		file_storage m_files;
//	};
//
//	storage_interface* temp_storage_constructor(storage_params const& params)
//	{
//		return new temp_storage(*params.files);
//	}
namespace libtorrent
{
	class session;
	struct file_pool;
	struct disk_io_job;
	struct disk_buffer_pool;
	struct cache_status;
	namespace aux { struct session_settings; }
	struct cached_piece_entry;

	TORRENT_EXTRA_EXPORT std::vector<std::pair<size_type, std::time_t> > get_filesizes(
		file_storage const& t
		, std::string const& p);

	TORRENT_EXTRA_EXPORT bool match_filesizes(
		file_storage const& t
		, std::string const& p
		, std::vector<std::pair<size_type, std::time_t> > const& sizes
		, bool compact_mode
		, std::string* error = 0);

	TORRENT_EXTRA_EXPORT int bufs_size(file::iovec_t const* bufs, int num_bufs);

	// flags for async_move_storage
	enum move_flags_t
	{
		// replace any files in the destination when copying
		// or moving the storage
		always_replace_files,

		// if any files that we want to copy exist in the destination
		// exist, fail the whole operation and don't perform
		// any copy or move. There is an inherent race condition
		// in this mode. The files are checked for existence before
		// the operation starts. In between the check and performing
		// the copy, the destination files may be created, in which
		// case they are replaced.
		fail_if_exist,

		// if any file exist in the target, take those files instead
		// of the ones we may have in the source.
		dont_replace
	};

	// The storage interface is a pure virtual class that can be implemented to
	// customize how and where data for a torrent is stored. The default storage
	// implementation uses regular files in the filesystem, mapping the files in
	// the torrent in the way one would assume a torrent is saved to disk.
	// Implementing your own storage interface makes it possible to store all
	// data in RAM, or in some optimized order on disk (the order the pieces are
	// received for instance), or saving multifile torrents in a single file in
	// order to be able to take advantage of optimized disk-I/O.
	// 
	// It is also possible to write a thin class that uses the default storage
	// but modifies some particular behavior, for instance encrypting the data
	// before it's written to disk, and decrypting it when it's read again.
	// 
	// The storage interface is based on slots, each slot is 'piece_size' number
	// of bytes. All access is done by writing and reading whole or partial
	// slots. One slot is one piece in the torrent.
	// 
	// libtorrent comes with two built-in storage implementations;
	// ``default_storage`` and ``disabled_storage``. Their constructor functions
	// are called default_storage_constructor() and
	// ``disabled_storage_constructor`` respectively. The disabled storage does
	// just what it sounds like. It throws away data that's written, and it
	// reads garbage. It's useful mostly for benchmarking and profiling purpose.
	//
	struct TORRENT_EXPORT storage_interface
	{
		// hidden
		storage_interface(): m_settings(0) {}


		// This function is called when the storage is to be initialized. The
		// default storage will create directories and empty files at this point.
		// If ``allocate_files`` is true, it will also ``ftruncate`` all files to
		// their target size.
		//
		// If an error occurs, ``storage_error`` should be set to reflect it.
		virtual void initialize(storage_error& ec) = 0;

		// These functions should read and write the data in or to the given
		// ``piece`` at the given ``offset``. It should read or write
		// ``num_bufs`` buffers sequentially, where the size of each buffer is
		// specified in the buffer array ``bufs``. The file::iovec_t type has the
		// following members::
		//
		//	struct iovec_t { void* iov_base; size_t iov_len; };
		//
		// These functions may be called simultaneously from multiple threads.
		// Make sure they are thread safe. The ``file`` in libtorrent is thread
		// safe when it can fall back to ``pread``, ``preadv`` or the windows
		// equivalents. On targets where read operations cannot be thread safe
		// (i.e one has to seek first and then read), only one disk thread is
		// used.
		//
		// Every buffer in ``bufs`` can be assumed to be page aligned and be of a
		// page aligned size, except for the last buffer of the torrent. The
		// allocated buffer can be assumed to fit a fully page aligned number of
		// bytes though. This is useful when reading and writing the last piece
		// of a file in unbuffered mode.
		// 
		// The ``offset`` is aligned to 16 kiB boundries  *most of the time*, but
		// there are rare exceptions when it's not. Specifically if the read
		// cache is disabled/or full and a peer requests unaligned data. Most
		// clients request aligned data.
		// 
		// The number of bytes read or written should be returned, or -1 on
		// error. If there's an error, the ``storage_error`` must be filled out
		// to represent the error that occurred.
		virtual int readv(file::iovec_t const* bufs, int num_bufs
			, int piece, int offset, int flags, storage_error& ec) = 0;
		virtual int writev(file::iovec_t const* bufs, int num_bufs
			, int piece, int offset, int flags, storage_error& ec) = 0;

		// This function is called when first checking (or re-checking) the
		// storage for a torrent. It should return true if any of the files that
		// is used in this storage exists on disk. If so, the storage will be
		// checked for existing pieces before starting the download.
		// 
		// If an error occurs, ``storage_error`` should be set to reflect it.
		virtual bool has_any_file(storage_error& ec) = 0;

		// change the priorities of files. This is a fenced job and is
		// guaranteed to be the only running function on this storage
		// when called
		virtual void set_file_priority(std::vector<boost::uint8_t> const& prio
			, storage_error& ec) = 0;

		// This function should move all the files belonging to the storage to
		// the new save_path. The default storage moves the single file or the
		// directory of the torrent.
		// 
		// Before moving the files, any open file handles may have to be closed,
		// like ``release_files()``.
		//
		//If an error occurs, ``storage_error`` should be set to reflect it.
		// 
		// returns one of:
		// | no_error = 0
		// | fatal_disk_error = -1
		// | need_full_check = -2
		// | file_exist = -4
		virtual int move_storage(std::string const& save_path, int flags
			, storage_error& ec) = 0;

		// This function should verify the resume data ``rd`` with the files
		// on disk. If the resume data seems to be up-to-date, return true. If
		// not, set ``error`` to a description of what mismatched and return false.
		// 
		// The default storage may compare file sizes and time stamps of the files.
		// 
		// If an error occurs, ``storage_error`` should be set to reflect it.
		// 
		// This function should verify the resume data ``rd`` with the files
		// on disk. If the resume data seems to be up-to-date, return true. If
		// not, set ``error`` to a description of what mismatched and return false.
		virtual bool verify_resume_data(lazy_entry const& rd, storage_error& ec) = 0;

		// This function should fill in resume data, the current state of the
		// storage, in ``rd``. The default storage adds file timestamps and
		// sizes.
		// 
		// Returning ``true`` indicates an error occurred.
		// 
		// If an error occurs, ``storage_error`` should be set to reflect it.
		// 
		virtual void write_resume_data(entry& rd, storage_error& ec) const = 0;

		// This function should release all the file handles that it keeps open
		// to files belonging to this storage. The default implementation just
		// calls file_pool::release_files().
		// 
		// If an error occurs, ``storage_error`` should be set to reflect it.
		// 
		virtual void release_files(storage_error& ec) = 0;

		// Rename file with index ``file`` to the thame ``new_name``.
		// 
		// If an error occurs, ``storage_error`` should be set to reflect it.
		// 
		virtual void rename_file(int index, std::string const& new_filenamem
			, storage_error& ec) = 0;

		// This function should delete all files and directories belonging to
		// this storage.
		// 
		// If an error occurs, ``storage_error`` should be set to reflect it.
		// 
		// The ``disk_buffer_pool`` is used to allocate and free disk buffers. It
		// has the following members::
		// 
		//	struct disk_buffer_pool : boost::noncopyable
		//	{
		//		char* allocate_buffer(char const* category);
		//		void free_buffer(char* buf);
		//
		//		char* allocate_buffers(int blocks, char const* category);
		//		void free_buffers(char* buf, int blocks);
		//
		//		int block_size() const { return m_block_size; }
		//
		//		void release_memory();
		//	};
		// 
		virtual void delete_files(storage_error& ec) = 0;

#ifndef TORRENT_NO_DEPRECATE
		// This function is called each time a file is completely downloaded. The
		// storage implementation can perform last operations on a file. The file
		// will not be opened for writing after this.
		// 
		// ``index`` is the index of the file that completed.
		//	
		//	On windows the default storage implementation clears the sparse file
		//	flag on the specified file.
		//
		//	If an error occurs, ``storage_error`` should be set to reflect it.
		//	
		virtual void finalize_file(int, storage_error&) {}
#endif

		// called periodically (useful for deferred flushing). When returning
		// false, it means no more ticks are necessary. Any disk job submitted
		// will re-enable ticking. The default will always turn ticking back
		// off again.
		virtual bool tick() { return false; }

		// access global session_settings
		aux::session_settings const& settings() const { return *m_settings; }

		// hidden
		virtual ~storage_interface() {}

		// initialized in disk_io_thread::perform_async_job
		aux::session_settings* m_settings;
	};

	// The default implementation of storage_interface. Behaves as a normal
	// bittorrent client. It is possible to derive from this class in order to
	// override some of its behavior, when implementing a custom storage.
	class TORRENT_EXPORT default_storage : public storage_interface, boost::noncopyable
	{
	public:
		// constructs the default_storage based on the give file_storage (fs).
		// ``mapped`` is an optional argument (it may be NULL). If non-NULL it
		// represents the file mappsing that have been made to the torrent before
		// adding it. That's where files are supposed to be saved and looked for
		// on disk. ``save_path`` is the root save folder for this torrent.
		// ``file_pool`` is the cache of file handles that the storage will use.
		// All files it opens will ask the file_pool to open them. ``file_prio``
		// is a vector indicating the priority of files on startup. It may be
		// an empty vector. Any file whose index is not represented by the vector
		// (because the vector is too short) are assumed to have priority 1.
		// this is used to treat files with priority 0 slightly differently.
		default_storage(storage_params const& params);

		// hidden
		~default_storage();

		void set_file_priority(std::vector<boost::uint8_t> const& prio);
#ifndef TORRENT_NO_DEPRECATE
		void finalize_file(int file, storage_error& ec);
#endif
		bool has_any_file(storage_error& ec);
		void set_file_priority(std::vector<boost::uint8_t> const& prio, storage_error& ec);
		void rename_file(int index, std::string const& new_filename, storage_error& ec);
		void release_files(storage_error& ec);
		void delete_files(storage_error& ec);
		void initialize(storage_error& ec);
		int move_storage(std::string const& save_path, int flags, storage_error& ec);
		int sparse_end(int start) const;
		bool verify_resume_data(lazy_entry const& rd, storage_error& error);
		void write_resume_data(entry& rd, storage_error& ec) const;
		bool tick();

		int readv(file::iovec_t const* bufs, int num_bufs
			, int piece, int offset, int flags, storage_error& ec);
		int writev(file::iovec_t const* bufs, int num_bufs
			, int piece, int offset, int flags, storage_error& ec);

		// if the files in this storage are mapped, returns the mapped
		// file_storage, otherwise returns the original file_storage object.
		file_storage const& files() const { return m_mapped_files?*m_mapped_files:m_files; }

		// we need access to these for logging purposes
#if !defined TORRENT_VERBOSE_LOGGING && !defined TORRENT_LOGGING && !defined TORRENT_ERROR_LOGGING
	private:
#endif

		// this identifies a read or write operation
		// so that default_storage::readwritev() knows what to
		// do when it's actually touching the file
		struct fileop
		{
			// file operation
			size_type (file::*op)(size_type file_offset
				, file::iovec_t const* bufs, int num_bufs, error_code& ec, int flags);
			// file open mode (file::read_only, file::write_only etc.)
			// this is used to open the file, but also passed along as the
			// flags argument to the file operation (readv or writev)
			int mode;
			// used for error reporting
			int operation_type;
		};

		void delete_one_file(std::string const& p, error_code& ec);
		int readwritev(file::iovec_t const* bufs, int slot, int offset
			, int num_bufs, fileop const& op, storage_error& ec);

		void need_partfile();

		boost::scoped_ptr<file_storage> m_mapped_files;
		file_storage const& m_files;

		// in order to avoid calling stat() on each file multiple times
		// during startup, cache the results in here, and clear it all
		// out once the torrent starts (to avoid getting stale results)
		// each slot represents the size and timestamp of the file
		mutable stat_cache m_stat_cache;

		// helper function to open a file in the file pool with the right mode
		file_handle open_file(int file, int mode
			, error_code& ec) const;

		std::vector<boost::uint8_t> m_file_priority;
		std::string m_save_path;
		std::string m_part_file_name;
		// the file pool is typically stored in
		// the session, to make all storage
		// instances use the same pool
		file_pool& m_pool;

		// used for skipped files
		boost::scoped_ptr<part_file> m_part_file;

		// this is a bitfield with one bit per file. A bit being set means
		// we've written to that file previously. If we do write to a file
		// whose bit is 0, we set the file size, to make the file allocated
		// on disk (in full allocation mode) and just sparsely allocated in
		// case of sparse allocation mode
		bitfield m_file_created;

		bool m_allocate_files;
	};

	// this storage implementation does not write anything to disk
	// and it pretends to read, and just leaves garbage in the buffers
	// this is useful when simulating many clients on the same machine
	// or when running stress tests and want to take the cost of the
	// disk I/O out of the picture. This cannot be used for any kind
	// of normal bittorrent operation, since it will just send garbage
	// to peers and throw away all the data it downloads. It would end
	// up being banned immediately
	class disabled_storage : public storage_interface, boost::noncopyable
	{
	public:
		disabled_storage(int piece_size) : m_piece_size(piece_size) {}
		bool has_any_file(storage_error&) { return false; }
		void set_file_priority(std::vector<boost::uint8_t> const&, storage_error&) {}
		void rename_file(int, std::string const&, storage_error&) {}
		void release_files(storage_error&) {}
		void delete_files(storage_error&) {}
		void initialize(storage_error&) {}
		int move_storage(std::string const&, int, storage_error&) { return 0; }

		int readv(file::iovec_t const* bufs, int num_bufs, int piece
			, int offset, int flags, storage_error& ec);
		int writev(file::iovec_t const* bufs, int num_bufs, int piece
			, int offset, int flags, storage_error& ec);

		bool verify_resume_data(lazy_entry const&, storage_error&) { return false; }
		void write_resume_data(entry&, storage_error&) const {}

		int m_piece_size;
	};

	// this storage implementation always reads zeroes, and always discards
	// anything written to it
	struct zero_storage : storage_interface
	{
		virtual void initialize(storage_error&) {}

		virtual int readv(file::iovec_t const* bufs, int num_bufs
			, int piece, int offset, int flags, storage_error& ec);
		virtual int writev(file::iovec_t const* bufs, int num_bufs
			, int piece, int offset, int flags, storage_error& ec);

		virtual bool has_any_file(storage_error&) { return false; }
		virtual void set_file_priority(std::vector<boost::uint8_t> const& /* prio */
			, storage_error&) {}
		virtual int move_storage(std::string const& /* save_path */
			, int /* flags */, storage_error&) { return 0; }
		virtual bool verify_resume_data(lazy_entry const& /* rd */, storage_error&)
			{ return false; }
		virtual void write_resume_data(entry&, storage_error&) const {}
		virtual void release_files(storage_error&) {}
		virtual void rename_file(int /* index */
			, std::string const& /* new_filenamem */, storage_error&) {}
		virtual void delete_files(storage_error&) {}
	};

	struct disk_io_thread;

	// implements the disk I/O job fence used by the piece_manager
	// to provide to the disk thread. Whenever a disk job needs
	// exclusive access to the storage for that torrent, it raises
	// the fence, blocking all new jobs, until there are no longer
	// any outstanding jobs on the torrent, then the fence is lowered
	// and it can be performed, along with the backlog of jobs that
	// accrued while the fence was up
	struct TORRENT_EXTRA_EXPORT disk_job_fence
	{
		disk_job_fence();
		~disk_job_fence()
		{
			TORRENT_ASSERT(int(m_outstanding_jobs) == 0);
			TORRENT_ASSERT(m_blocked_jobs.size() == 0);
		}

		// returns one of the fence_* enums.
		// if there are no outstanding jobs on the
		// storage, fence_post_fence is returned, the flush job is expected
		// to be discarded by the caller.
		// fence_post_flush is returned if the fence job was blocked and queued,
		// but the flush job should be posted (i.e. put on the job queue)
		// fence_post_none if both the fence and the flush jobs were queued.
		enum { fence_post_fence = 0, fence_post_flush = 1, fence_post_none = 2 };
		int raise_fence(disk_io_job* fence_job, disk_io_job* flush_job
			, boost::atomic<int>* blocked_counter);
		bool has_fence() const;

		// called whenever a job completes and is posted back to the
		// main network thread. the tailqueue of jobs will have the
		// backed-up jobs prepended to it in case this resulted in the
		// fence being lowered.
		int job_complete(disk_io_job* j, tailqueue& job_queue);
		int num_outstanding_jobs() const { return m_outstanding_jobs; }

		// if there is a fence up, returns true and adds the job
		// to the queue of blocked jobs
		bool is_blocked(disk_io_job* j);
		
		// the number of blocked jobs
		int num_blocked() const;

	private:
		// when > 0, this storage is blocked for new async
		// operations until all outstanding jobs have completed.
		// at that point, the m_blocked_jobs are issued
		// the count is the number of fence job currently in the queue
		int m_has_fence;

		// when there's a fence up, jobs are queued up in here
		// until the fence is lowered
		tailqueue m_blocked_jobs;

		// the number of disk_io_job objects there are, belonging
		// to this torrent, currently pending, hanging off of
		// cached_piece_entry objects. This is used to determine
		// when the fence can be lowered
		boost::atomic<int> m_outstanding_jobs;

		// must be held when accessing m_has_fence and
		// m_blocked_jobs
		mutable mutex m_mutex;
	};

	// this class keeps track of which pieces, belonging to
	// a specific storage, are in the cache right now. It's
	// used for quickly being able to evict all pieces for a
	// specific torrent
	struct TORRENT_EXTRA_EXPORT storage_piece_set
	{
		void add_piece(cached_piece_entry* p);
		void remove_piece(cached_piece_entry* p);
		bool has_piece(cached_piece_entry* p) const;
		int num_pieces() const { return m_cached_pieces.size(); }
		boost::unordered_set<cached_piece_entry*> const& cached_pieces() const
		{ return m_cached_pieces; }
	private:
		// these are cached pieces belonging to this storage
		boost::unordered_set<cached_piece_entry*> m_cached_pieces;
	};

	class TORRENT_EXTRA_EXPORT piece_manager
		: public boost::enable_shared_from_this<piece_manager>
		, public disk_job_fence
		, public storage_piece_set
		, boost::noncopyable
	{
	friend struct disk_io_thread;
	public:

		piece_manager(
			storage_interface* storage_impl
			, boost::shared_ptr<void> const& torrent
			, file_storage* files);

		~piece_manager();

		file_storage const* files() const { return &m_files; }

		enum return_t
		{
			// return values from check_fastresume, and move_storage
			no_error = 0,
			fatal_disk_error = -1,
			need_full_check = -2,
			disk_check_aborted = -3,
			file_exist = -4
		};

		storage_interface* get_storage_impl() { return m_storage.get(); }

		void write_resume_data(entry& rd, storage_error& ec) const;

#ifdef TORRENT_DEBUG
		void assert_torrent_refcount() const;
#endif
	private:

		// if error is set and return value is 'no_error' or 'need_full_check'
		// the error message indicates that the fast resume data was rejected
		// if 'fatal_disk_error' is returned, the error message indicates what
		// when wrong in the disk access
		int check_fastresume(lazy_entry const& rd, storage_error& error);

		// helper functions for check_fastresume	
		int check_no_fastresume(storage_error& error);
		int check_init_storage(storage_error& error);

#ifdef TORRENT_DEBUG
		std::string name() const { return m_files.name(); }
#endif

#if TORRENT_USE_INVARIANT_CHECKS
		void check_invariant() const;
#endif
		file_storage const& m_files;

		boost::scoped_ptr<storage_interface> m_storage;

		// the reason for this to be a void pointer
		// is to avoid creating a dependency on the
		// torrent. This shared_ptr is here only
		// to keep the torrent object alive until
		// the piece_manager destructs. This is because
		// the torrent_info object is owned by the torrent.
		boost::shared_ptr<void> m_torrent;
	};

}

#endif // TORRENT_STORAGE_HPP_INCLUDED

