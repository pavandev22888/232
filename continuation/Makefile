CXX ?= g++
PYTHON ?= python3
HANDOFF ?= ../runs/handoff/RAGEBOT_HANDOFF_V10
CPPFLAGS := -Iinclude
CXXFLAGS := -std=c++17 -O2 -g -Wall -Wextra -Wpedantic -Werror -fno-fast-math -ffp-contract=off
BUILD := build
SOURCES := src/position_records.cpp tests/test_position_records.cpp
HEADERS := include/rage/position_records.hpp

.PHONY: test sanitize evidence check clean
check: test sanitize evidence

test: $(BUILD)/test_position_records
	./$<

sanitize: $(BUILD)/test_position_records_sanitize
	ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./$<

evidence:
	$(PYTHON) tests/verify_evidence.py --handoff "$(HANDOFF)"

$(BUILD):
	mkdir -p $@

$(BUILD)/test_position_records: $(SOURCES) $(HEADERS) Makefile | $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(SOURCES) -o $@

$(BUILD)/test_position_records_sanitize: $(SOURCES) $(HEADERS) Makefile | $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -O1 -fno-omit-frame-pointer -fsanitize=address,undefined $(SOURCES) -o $@

clean:
	rm -rf $(BUILD)
