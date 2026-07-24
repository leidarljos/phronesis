# SPDX-License-Identifier: Apache-2.0

CC       ?= cc
CFLAGS   ?= -std=c11 -Wall -Wextra -Werror -O2
CPPFLAGS += -Iinclude -Isrc -Itests
LDFLAGS  ?=

BUILD    := build
LIB_SRCS := src/paths.c src/action_log.c src/cgroup.c src/policy.c src/supervisor.c
LIB_OBJS := $(addprefix $(BUILD)/,$(notdir $(LIB_SRCS:.c=.o)))

TEST_SRCS := tests/harness.c \
	tests/test_paths.c \
	tests/test_lifecycle.c \
	tests/test_kill_tree.c \
	tests/test_action_log.c \
	tests/test_persist.c \
	tests/test_policy.c \
	tests/test_main.c
TEST_OBJS := $(addprefix $(BUILD)/,$(notdir $(TEST_SRCS:.c=.o)))

.PHONY: all clean test

all: $(BUILD)/grok-policyd test

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(BUILD)/%.o: tests/%.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(BUILD)/grok-policyd: $(BUILD)/grok-policyd.o $(LIB_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD)/supervisor_test: $(TEST_OBJS) $(LIB_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test: $(BUILD)/grok-policyd $(BUILD)/supervisor_test
	$(BUILD)/supervisor_test

clean:
	rm -rf $(BUILD)

$(BUILD)/grok-policyd.o: include/grok-policyd/supervisor.h
$(BUILD)/supervisor.o: include/grok-policyd/supervisor.h src/internal.h
$(BUILD)/paths.o: src/internal.h include/grok-policyd/supervisor.h
$(BUILD)/action_log.o: src/internal.h include/grok-policyd/supervisor.h
$(BUILD)/cgroup.o: src/internal.h include/grok-policyd/supervisor.h
$(BUILD)/policy.o: src/internal.h include/grok-policyd/supervisor.h
$(TEST_OBJS): tests/harness.h include/grok-policyd/supervisor.h
