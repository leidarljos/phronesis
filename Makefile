# SPDX-License-Identifier: Apache-2.0
#
# Host library (stable C ABI) + CLI + tests + docs.
# Public surface: include/grok-policyd/supervisor.h

VERSION_MAJOR := 0
VERSION_MINOR := 1
VERSION_PATCH := 0
VERSION       := $(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_PATCH)

CC       ?= cc
CFLAGS   ?= -std=c11 -Wall -Wextra -Werror -O2 -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L
CPPFLAGS += -Iinclude -Isrc -Itests
LDFLAGS  ?=
PREFIX   ?= /usr/local

PICFLAGS := -fPIC

CMOCKA_CFLAGS := $(shell pkg-config --cflags cmocka 2>/dev/null)
CMOCKA_LIBS   := $(shell pkg-config --libs cmocka 2>/dev/null)
# cmocka is only required for the test binary (not lib/docs/example).

BUILD    := build
LIB_SRCS := src/paths.c src/action_log.c src/cgroup.c src/policy.c \
	src/supervisor.c src/version.c
LIB_OBJS := $(addprefix $(BUILD)/,$(notdir $(LIB_SRCS:.c=.o)))
LIB_PIC_OBJS := $(addprefix $(BUILD)/pic-,$(notdir $(LIB_SRCS:.c=.o)))

TEST_SRCS := tests/harness.c \
	tests/test_paths.c \
	tests/test_lifecycle.c \
	tests/test_kill_tree.c \
	tests/test_action_log.c \
	tests/test_persist.c \
	tests/test_policy.c \
	tests/test_version.c \
	tests/test_main.c
TEST_OBJS := $(addprefix $(BUILD)/,$(notdir $(TEST_SRCS:.c=.o)))

SONAME     := libgrok_policyd.so.$(VERSION_MAJOR)
REAL_SO    := libgrok_policyd.so.$(VERSION)
STATIC_LIB := $(BUILD)/libgrok_policyd.a
SHARED_LIB := $(BUILD)/$(REAL_SO)

.PHONY: all clean test lib install uninstall example doxygen docs pc

all: lib $(BUILD)/grok-policyd test

lib: $(STATIC_LIB) $(SHARED_LIB) $(BUILD)/libgrok_policyd.so $(BUILD)/$(SONAME) pc

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(BUILD)/pic-%.o: src/%.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(PICFLAGS) -c -o $@ $<

$(BUILD)/%.o: tests/%.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CMOCKA_CFLAGS) -std=c11 -Wall -Wextra -Werror -O2 -D_DEFAULT_SOURCE \
		-Wno-format-truncation -c -o $@ $<

$(STATIC_LIB): $(LIB_OBJS)
	$(AR) rcs $@ $^

$(SHARED_LIB): $(LIB_PIC_OBJS)
	$(CC) -shared -Wl,-soname,$(SONAME) -o $@ $^ $(LDFLAGS)

$(BUILD)/libgrok_policyd.so: $(SHARED_LIB)
	ln -sfn $(REAL_SO) $@

$(BUILD)/$(SONAME): $(SHARED_LIB)
	ln -sfn $(REAL_SO) $@

$(BUILD)/grok-policyd: $(BUILD)/grok-policyd.o $(STATIC_LIB)
	$(CC) $(CFLAGS) -o $@ $(BUILD)/grok-policyd.o $(STATIC_LIB) $(LDFLAGS)

$(BUILD)/supervisor_test: $(TEST_OBJS) $(STATIC_LIB)
	@test -n "$(CMOCKA_LIBS)" || (echo "error: cmocka not found (pkg-config cmocka). Install cmocka-dev / libcmocka-dev." && exit 1)
	$(CC) $(CFLAGS) -o $@ $(TEST_OBJS) $(STATIC_LIB) $(LDFLAGS) $(CMOCKA_LIBS)

$(BUILD)/example_minimal: examples/c/minimal.c $(STATIC_LIB) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ examples/c/minimal.c $(STATIC_LIB) $(LDFLAGS)

example: $(BUILD)/example_minimal

test: $(BUILD)/grok-policyd $(BUILD)/supervisor_test
	@test -n "$(CMOCKA_LIBS)" || (echo "error: cmocka not found (pkg-config cmocka). Install cmocka-dev / libcmocka-dev." && exit 1)
	$(BUILD)/supervisor_test

pc: $(BUILD)/grok-policyd.pc

$(BUILD)/grok-policyd.pc: packaging/grok-policyd.pc.in | $(BUILD)
	sed -e 's|@PREFIX@|$(PREFIX)|g' -e 's|@VERSION@|$(VERSION)|g' \
		$< > $@

install: lib $(BUILD)/grok-policyd
	install -d $(DESTDIR)$(PREFIX)/include/grok-policyd
	install -d $(DESTDIR)$(PREFIX)/lib
	install -d $(DESTDIR)$(PREFIX)/lib/pkgconfig
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 644 include/grok-policyd/supervisor.h \
		$(DESTDIR)$(PREFIX)/include/grok-policyd/
	install -m 644 $(STATIC_LIB) $(DESTDIR)$(PREFIX)/lib/
	install -m 755 $(SHARED_LIB) $(DESTDIR)$(PREFIX)/lib/
	ln -sfn $(REAL_SO) $(DESTDIR)$(PREFIX)/lib/libgrok_policyd.so
	ln -sfn $(REAL_SO) $(DESTDIR)$(PREFIX)/lib/$(SONAME)
	install -m 644 $(BUILD)/grok-policyd.pc \
		$(DESTDIR)$(PREFIX)/lib/pkgconfig/grok-policyd.pc
	install -m 755 $(BUILD)/grok-policyd $(DESTDIR)$(PREFIX)/bin/grok-policyd

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/include/grok-policyd/supervisor.h
	rmdir $(DESTDIR)$(PREFIX)/include/grok-policyd 2>/dev/null || true
	rm -f $(DESTDIR)$(PREFIX)/lib/libgrok_policyd.a
	rm -f $(DESTDIR)$(PREFIX)/lib/libgrok_policyd.so
	rm -f $(DESTDIR)$(PREFIX)/lib/$(SONAME)
	rm -f $(DESTDIR)$(PREFIX)/lib/$(REAL_SO)
	rm -f $(DESTDIR)$(PREFIX)/lib/pkgconfig/grok-policyd.pc
	rm -f $(DESTDIR)$(PREFIX)/bin/grok-policyd

doxygen:
	@command -v doxygen >/dev/null || { echo "doxygen not found"; exit 1; }
	mkdir -p docs/build/doxygen
	cd docs && doxygen Doxyfile

docs: doxygen
	@command -v sphinx-build >/dev/null || { \
		echo "sphinx-build not found — pip install -r docs/requirements.txt"; exit 1; }
	sphinx-build -b html docs/source docs/build/html

clean:
	rm -rf $(BUILD) docs/build

$(BUILD)/grok-policyd.o: include/grok-policyd/supervisor.h
$(BUILD)/supervisor.o: include/grok-policyd/supervisor.h src/internal.h
$(BUILD)/version.o: include/grok-policyd/supervisor.h
$(BUILD)/pic-version.o: include/grok-policyd/supervisor.h
$(BUILD)/paths.o: src/internal.h include/grok-policyd/supervisor.h
$(BUILD)/action_log.o: src/internal.h include/grok-policyd/supervisor.h
$(BUILD)/cgroup.o: src/internal.h include/grok-policyd/supervisor.h
$(BUILD)/policy.o: src/internal.h include/grok-policyd/supervisor.h
$(TEST_OBJS): tests/harness.h include/grok-policyd/supervisor.h
