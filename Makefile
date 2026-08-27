# cpp_jq - SPDX-License-Identifier: MIT
CXX      ?= g++
CXXSTD   ?= -std=c++17
WARN     ?= -Wall -Wextra -Wpedantic
OPT      ?= -g -O0
INCLUDES  = -Iinclude -Ithird_party

SRCS = src/main.cc src/lexer.cc src/parser.cc src/evaluator.cc \
       src/builtin.cc src/printer.cc src/diag.cc
OBJS = $(SRCS:src/%.cc=build/%.o)
DEPS = $(OBJS:.o=.d)
BIN  = build/cpp_jq

.PHONY: all build release test clean format

all: build

build: $(BIN)

$(BIN): $(OBJS)
	@mkdir -p build
	$(CXX) $(CXXSTD) $(WARN) $(OPT) $^ -o $@

build/%.o: src/%.cc
	@mkdir -p build
	$(CXX) $(CXXSTD) $(WARN) $(OPT) $(INCLUDES) -MMD -MP -c $< -o $@

release:
	@$(MAKE) OPT='-O2 -DNDEBUG' build

test: build
	@bash tests/run_e2e.sh

clean:
	rm -rf build

format:
	@command -v clang-format >/dev/null && clang-format -i $(SRCS) include/cpp_jq/*.hpp || true

-include $(DEPS)