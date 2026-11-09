CXX ?= g++
PYTHON ?= python3
HANDOFF ?= ../runs/handoff/RAGEBOT_HANDOFF_V10
CPPFLAGS := -Iinclude
CXXFLAGS := -std=c++17 -O2 -g -Wall -Wextra -Wpedantic -Werror -fno-fast-math -ffp-contract=off
BUILD ?= build
SOURCES := $(wildcard src/*.cpp)
HEADERS := $(wildcard include/rage/*.hpp)
TEST_NAMES := $(patsubst tests/%.cpp,%,$(wildcard tests/test_*.cpp))
NATIVE_TESTS := $(addprefix $(BUILD)/,$(TEST_NAMES))
SANITIZED_TESTS := $(addsuffix _sanitize,$(NATIVE_TESTS))

.PHONY: test sanitize evidence check clean
check: test sanitize evidence

test: $(NATIVE_TESTS)
	@set -e; for test in $(NATIVE_TESTS); do echo "RUN $$test"; "$$test"; done

sanitize: $(SANITIZED_TESTS)
	@set -e; for test in $(SANITIZED_TESTS); do echo "RUN $$test"; \
	  ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$$test"; done

evidence:
	$(PYTHON) tests/verify_evidence.py --handoff "$(HANDOFF)"

$(BUILD):
	mkdir -p $@

$(BUILD)/%_sanitize: tests/%.cpp $(SOURCES) $(HEADERS) Makefile | $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -O1 -fno-omit-frame-pointer -fsanitize=address,undefined $(SOURCES) $< -o $@

$(BUILD)/%: tests/%.cpp $(SOURCES) $(HEADERS) Makefile | $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(SOURCES) $< -o $@

clean:
	rm -rf $(BUILD)
