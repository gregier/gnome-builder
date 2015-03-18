noinst_PROGRAMS += test-c-parse-helper
TESTS += test-c-parse-helper
test_c_parse_helper_SOURCES = tests/test-c-parse-helper.c libide/c/c-parse-helper.c
test_c_parse_helper_CFLAGS = $(libide_1_0_la_CFLAGS) -I$(top_srcdir)/libide/c/
test_c_parse_helper_LDADD = $(LIBIDE_LIBS)


noinst_PROGRAMS += test-ide-context
TESTS += test-ide-context
test_ide_context_SOURCES = tests/test-ide-context.c
test_ide_context_CFLAGS = \
	$(libide_1_0_la_CFLAGS) \
	-DTEST_DATA_DIR="\"$(top_srcdir)/tests/data\""
test_ide_context_LDADD = libide-1.0.la $(LIBIDE_LIBS)


noinst_PROGRAMS += test-ide-back-forward-list
TESTS += test-ide-back-forward-list
test_ide_back_forward_list_SOURCES = tests/test-ide-back-forward-list.c
test_ide_back_forward_list_CFLAGS = \
	$(libide_1_0_la_CFLAGS) \
	-DTEST_DATA_DIR="\"$(top_srcdir)/tests/data\""
test_ide_back_forward_list_LDADD = libide-1.0.la $(LIBIDE_LIBS)


noinst_PROGRAMS += test-ide-buffer-manager
TESTS += test-ide-buffer-manager
test_ide_buffer_manager_SOURCES = tests/test-ide-buffer-manager.c
test_ide_buffer_manager_CFLAGS = \
	$(libide_1_0_la_CFLAGS) \
	-DTEST_DATA_DIR="\"$(top_srcdir)/tests/data\""
test_ide_buffer_manager_LDADD = libide-1.0.la $(LIBIDE_LIBS)

noinst_PROGRAMS += test-ide-buffer
TESTS += test-ide-buffer
test_ide_buffer_SOURCES = tests/test-ide-buffer.c
test_ide_buffer_CFLAGS = \
	$(libide_1_0_la_CFLAGS) \
	-DTEST_DATA_DIR="\"$(top_srcdir)/tests/data\""
test_ide_buffer_LDADD = libide-1.0.la $(LIBIDE_LIBS)


noinst_PROGRAMS += test-ide-source-view
test_ide_source_view_SOURCES = tests/test-ide-source-view.c
test_ide_source_view_CFLAGS = \
	$(libide_1_0_la_CFLAGS) \
	-DTEST_DATA_DIR="\"$(top_srcdir)/tests/data\""
test_ide_source_view_LDADD = libide-1.0.la $(LIBIDE_LIBS)


EXTRA_DIST += \
	tests/data/project1/configure.ac \
	$(NULL)
