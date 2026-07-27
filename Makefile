# SPDX-License-Identifier: Apache-2.0
#
# Host library (stable C ABI) + CLI + tests + docs.
# Public surface: include/grok-policyd/supervisor.h

# Single source: VERSION + API_VERSION (see scripts/check-version.sh)
VERSION       := $(shell tr -d '[:space:]' < VERSION)
VERSION_MAJOR := $(word 1,$(subst ., ,$(VERSION)))
VERSION_MINOR := $(word 2,$(subst ., ,$(VERSION)))
VERSION_PATCH := $(word 3,$(subst ., ,$(VERSION)))
API_VERSION   := $(shell tr -d '[:space:]' < API_VERSION)

CC       ?= cc
CFLAGS   ?= -std=c11 -Wall -Wextra -Werror -O2 -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L
CPPFLAGS += -Iinclude -Isrc -Itests
LDFLAGS  ?=
PREFIX   ?= /usr/local

PICFLAGS := -fPIC

CMOCKA_CFLAGS := $(shell pkg-config --cflags cmocka 2>/dev/null)
CMOCKA_LIBS   := $(shell pkg-config --libs cmocka 2>/dev/null)
# cmocka is only required for the test binary (not lib/docs/example).
# nng: Cap'n peer transport (req/rep over ipc://). Prefer pkg-config; conda may only provide -lnng.
NNG_CFLAGS := $(shell pkg-config --cflags nng 2>/dev/null)
NNG_LIBS   := $(shell pkg-config --libs nng 2>/dev/null)
ifeq ($(NNG_LIBS),)
NNG_LIBS := -lnng
endif

BUILD    := build
LIB_SRCS := src/paths.c src/unix_dir.c src/unix_sock.c src/action_log.c \
	src/cgroup.c src/policy.c src/supervisor.c src/version.c
WIRE_SRCS := src/wire/frame.c src/wire/capnp_min.c src/wire/serve.c
WIRE_OBJS := $(addprefix $(BUILD)/wire-,$(notdir $(WIRE_SRCS:.c=.o)))
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
	tests/test_wire_serve.c \
	tests/test_main.c
TEST_OBJS := $(addprefix $(BUILD)/,$(notdir $(TEST_SRCS:.c=.o)))

SONAME     := libgrok_policyd.so.$(VERSION_MAJOR)
REAL_SO    := libgrok_policyd.so.$(VERSION)
STATIC_LIB := $(BUILD)/libgrok_policyd.a
SHARED_LIB := $(BUILD)/$(REAL_SO)

.PHONY: all clean test lib install uninstall example doxygen docs pc check-version test-wire

all: lib $(BUILD)/grok-policyd test

lib: check-version $(STATIC_LIB) $(SHARED_LIB) $(BUILD)/libgrok_policyd.so $(BUILD)/$(SONAME) pc

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

$(BUILD)/wire-%.o: src/wire/%.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(NNG_CFLAGS) -c -o $@ $<

$(BUILD)/grok-policyd: $(BUILD)/grok-policyd.o $(WIRE_OBJS) $(STATIC_LIB)
	$(CC) $(CFLAGS) -o $@ $(BUILD)/grok-policyd.o $(WIRE_OBJS) $(STATIC_LIB) $(LDFLAGS) $(NNG_LIBS)

$(BUILD)/supervisor_test: $(TEST_OBJS) $(WIRE_OBJS) $(STATIC_LIB)
	@test -n "$(CMOCKA_LIBS)" || (echo "error: cmocka not found (pkg-config cmocka). Use pixi install --locked (provides cmocka), or install cmocka-dev / libcmocka-dev." && exit 1)
	$(CC) $(CFLAGS) -o $@ $(TEST_OBJS) $(WIRE_OBJS) $(STATIC_LIB) $(LDFLAGS) $(CMOCKA_LIBS) $(NNG_LIBS)

$(BUILD)/example_minimal: examples/c/minimal.c $(STATIC_LIB) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ examples/c/minimal.c $(STATIC_LIB) $(LDFLAGS)

example: $(BUILD)/example_minimal

test-wire: $(BUILD)/test_wire_frame
	$(BUILD)/test_wire_frame

$(BUILD)/test_wire_frame: tests/test_wire_frame.c $(WIRE_OBJS) $(STATIC_LIB) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(NNG_CFLAGS) -o $@ tests/test_wire_frame.c $(WIRE_OBJS) $(STATIC_LIB) $(LDFLAGS) $(NNG_LIBS)

test: $(BUILD)/grok-policyd $(BUILD)/supervisor_test test-wire
	@test -n "$(CMOCKA_LIBS)" || (echo "error: cmocka not found (pkg-config cmocka). Use pixi install --locked (provides cmocka), or install cmocka-dev / libcmocka-dev." && exit 1)
	$(BUILD)/supervisor_test

pc: $(BUILD)/grok-policyd.pc

check-version:
	bash scripts/check-version.sh


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
	sphinx-build -W -b html docs/source docs/build/html

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
