ACLOCAL_AMFLAGS = -I m4

#DISTCHECK_CONFIGURE_FLAGS = --enable-tests

SUBDIRS = include/libtorrent src examples test bindings tools

DOCS_IMAGES = \
  docs/client_test.png            \
  docs/cwnd.png                   \
  docs/cwnd_thumb.png             \
  docs/delays.png                 \
  docs/delays_thumb.png           \
  docs/disk_access.png            \
  docs/disk_buffer_before_optimization.png  \
  docs/disk_buffer.png            \
  docs/disk_buffer_sample.png     \
  docs/disk_io.png                \
  docs/hacking.html               \
  docs/im_thumb.jpg               \
  docs/merkle_tree.png            \
  docs/our_delay_base.png         \
  docs/our_delay_base_thumb.png   \
  docs/read_disk_buffers.png      \
  docs/session_stats_peers.png    \
  docs/storage.png                \
  docs/todo.html                  \
  docs/write_disk_buffers.png     \
  docs/ziptorrent_thumb.gif       \
  docs/disk_access_elevator.png   \
  docs/disk_access_no_elevator.png\
  docs/ip_id_v4.png               \
  docs/ip_id_v6.png               \
  docs/hash_distribution.png      \
  docs/complete_bit_prefixes.png  \
  docs/troubleshooting.png        \
  docs/troubleshooting_thumb.png  \
  docs/style.css                  \
  docs/base.css                   \
  docs/rst.css                    \
  docs/img/bg.png                 \
  docs/img/blue_bottom.png        \
  docs/img/blue_top.png           \
  docs/img/dotline.gif            \
  docs/img/logo.png               \
  docs/img/minus.gif              \
  docs/img/orange.png

DOCS_PAGES = \
  docs/building.html              \
  docs/client_test.html           \
  docs/contributing.html          \
  docs/dht_extensions.html        \
  docs/dht_rss.html               \
  docs/dht_sec.html               \
  docs/dht_store.html             \
  docs/examples.html              \
  docs/extension_protocol.html    \
  docs/features.html              \
  docs/index.html                 \
  docs/manual-ref.html            \
  docs/projects.html              \
  docs/python_binding.html        \
  docs/tuning.html                \
  docs/settings.rst               \
  docs/stats_counters.rst         \
  docs/troubleshooting.html       \
  docs/udp_tracker_protocol.html  \
  docs/utp.html                   \
  docs/reference-Alerts.html      \
  docs/reference-Bencoding.html   \
  docs/reference-Core.html        \
  docs/reference-Create_Torrents.html \
  docs/reference-Custom_Storage.html \
  docs/reference-Error_Codes.html \
  docs/reference-Filter.html      \
  docs/reference-Plugins.html     \
  docs/reference-RSS.html         \
  docs/reference-Session.html     \
  docs/reference-Settings.html    \
  docs/reference-Storage.html     \
  docs/reference-String.html      \
  docs/reference-Time.html        \
  docs/reference-Utility.html     \
  docs/reference.html

ED25519_SOURCE = \
	ed25519/readme.md \
	ed25519/test.c \
	ed25519/src/fe.h \
	ed25519/src/fixedint.h \
	ed25519/src/ge.h \
	ed25519/src/precomp_data.h \
	ed25519/src/sc.h \
	ed25519/src/sha512.h

EXTRA_DIST = \
  Jamfile \
  Jamroot.jam \
  CMakeLists.txt \
  LICENSE \
  bindings/python/compile_flags  \
  bindings/python/link_flags  \
  libtorrent-rasterbar.pc  \
  libtorrent-rasterbar-cmake.pc  \
  $(DOCS_PAGES) \
  $(DOCS_IMAGES) \
  $(ED25519_SOURCE)

pkgconfigdir   = $(libdir)/pkgconfig
pkgconfig_DATA = libtorrent-rasterbar.pc

