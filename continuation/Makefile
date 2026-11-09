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
	  ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 "$$test"; done

evidence:
	$(PYTHON) tests/verify_evidence.py --handoff "$(HANDOFF)"
	$(PYTHON) evidence/gap01/verify_gap01.py --handoff "$(HANDOFF)"
	$(PYTHON) evidence/gap03/verify_charge_evidence.py --handoff "$(HANDOFF)"
	$(PYTHON) evidence/gap04/check_bindings.py --handoff "$(HANDOFF)"
	$(PYTHON) evidence/gap05/replay_position_lifecycle.py "$(HANDOFF)"
	$(PYTHON) evidence/gap06/verify_h01_replay.py --handoff "$(HANDOFF)"
	$(PYTHON) -B evidence/wave2/code01/review_baseline.py --handoff "$(HANDOFF)"
	$(PYTHON) -B evidence/wave2/gap01/verify_gap01_wave2.py --handoff "$(HANDOFF)"
	$(PYTHON) -B evidence/wave2/gap02/verify_inventory.py --handoff "$(HANDOFF)"
	$(PYTHON) -B evidence/wave2/gap03/verify_reset_evidence.py --handoff "$(HANDOFF)"
	$(PYTHON) -B evidence/wave2/gap04/verify_scalar_helper.py --handoff "$(HANDOFF)"
	$(PYTHON) -B evidence/wave2/gap05/verify_optional_output.py --handoff "$(HANDOFF)"
	$(PYTHON) -B evidence/wave2/gap06/verify_remaining_h01.py --handoff "$(HANDOFF)"
	$(PYTHON) -B -m unittest discover -s evidence/wave2/gap06 -p 'test_*.py' -v
	$(PYTHON) -B evidence/wave3/marker/verify_composition.py --handoff "$(abspath $(HANDOFF))"
	$(PYTHON) -B evidence/wave3/source/verify_source.py --handoff "$(abspath $(HANDOFF))"
	$(PYTHON) -B evidence/next/scalar-provider/verify_fallback_provider.py --handoff "$(abspath $(HANDOFF))"
	$(PYTHON) -B evidence/wave5/aggregate/verify_contract.py --handoff "$(abspath $(HANDOFF))"
	$(PYTHON) -B evidence/wave6/aggregate/verify_callback.py --handoff "$(abspath $(HANDOFF))"
	$(PYTHON) -B evidence/wave6/modes/verify_release.py --handoff "$(abspath $(HANDOFF))"
	$(PYTHON) -B evidence/wave6/message/verify_message_frontier.py --handoff "$(abspath $(HANDOFF))"
	$(PYTHON) -B evidence/wave6/reset/verify_reset_predecessor.py --handoff "$(abspath $(HANDOFF))"
	$(PYTHON) -B evidence/wave6/geometry/verify_geometry.py --handoff "$(abspath $(HANDOFF))"
	$(PYTHON) -B evidence/wave6/history/verify_source_bytes.py --handoff "$(abspath $(HANDOFF))"
	$(PYTHON) -B evidence/wave7/rank/verify_source_bytes.py --handoff "$(abspath $(HANDOFF))"
	$(PYTHON) -B evidence/wave7/abi/verify_source_bytes.py --handoff "$(abspath $(HANDOFF))"
	$(PYTHON) -B evidence/wave7/reset/verify_source_bytes.py --handoff "$(abspath $(HANDOFF))"
	$(PYTHON) -B evidence/wave7/history/verify_source_bytes.py --handoff "$(abspath $(HANDOFF))"
	$(PYTHON) -B evidence/wave7/movement/verify_source_bytes.py --handoff "$(abspath $(HANDOFF))"
	$(PYTHON) -B evidence/wave7/damage/verify_source_bytes.py --handoff "$(abspath $(HANDOFF))"
	$(PYTHON) -B evidence/wave8/aggregate/verify_source_bytes.py --handoff "$(abspath $(HANDOFF))"
	$(PYTHON) -B evidence/wave8/damage/verify_source_bytes.py --handoff "$(abspath $(HANDOFF))"
	$(PYTHON) -B evidence/wave8/movement/verify_source_bytes.py --handoff "$(abspath $(HANDOFF))"
	$(PYTHON) -B evidence/wave8/message/verify_source_bytes.py --handoff "$(abspath $(HANDOFF))"
	$(PYTHON) -B evidence/wave8/geometry/verify_source_bytes.py --handoff "$(abspath $(HANDOFF))"
	$(PYTHON) -B evidence/wave8/provider/verify_source_bytes.py --handoff "$(abspath $(HANDOFF))"
	$(PYTHON) -B evidence/wave8/reset/verify_source_bytes.py --handoff "$(abspath $(HANDOFF))"
	$(PYTHON) -B evidence/wave8/history/verify_source_bytes.py --handoff "$(abspath $(HANDOFF))"
	$(PYTHON) -B evidence/wave5/bindings/verify_resolution.py --handoff "$(abspath $(HANDOFF))"
	$(PYTHON) -B evidence/wave5/charge/verify_full_reset.py --handoff "$(abspath $(HANDOFF))"
	$(PYTHON) -B evidence/wave5/esi/verify_preservation.py --handoff "$(abspath $(HANDOFF))"
	$(PYTHON) -B evidence/wave5/fallback24/verify_initializer.py --handoff "$(abspath $(HANDOFF))"
	$(PYTHON) -B evidence/wave5/global9bc/verify_chain.py --handoff "$(abspath $(HANDOFF))"
	$(PYTHON) -B evidence/wave5/guards/verify_guards.py --handoff "$(abspath $(HANDOFF))"
	$(PYTHON) -B evidence/wave5/indirect/verify_indirect.py --handoff "$(abspath $(HANDOFF))"
	$(PYTHON) -B evidence/wave5/lifecycle/verify_seed_path.py --handoff "$(abspath $(HANDOFF))"
	$(PYTHON) -B evidence/wave5/messages/verify_message_fields.py --handoff "$(abspath $(HANDOFF))"
	$(PYTHON) -B evidence/wave5/point26/verify_point26.py --handoff "$(abspath $(HANDOFF))"
	$(PYTHON) -B evidence/wave5/tf4/verify_tf4.py --handoff "$(abspath $(HANDOFF))"
	$(PYTHON) -B evidence/wave5/threshold/verify_threshold_frontier.py --handoff "$(abspath $(HANDOFF))"
	$(PYTHON) -B -m unittest discover -s evidence/wave5/esi -p 'test_*.py' -v

$(BUILD):
	mkdir -p $@

$(BUILD)/%_sanitize: tests/%.cpp $(SOURCES) $(HEADERS) Makefile | $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -O1 -fno-omit-frame-pointer -fsanitize=address,undefined $(SOURCES) $< -o $@

$(BUILD)/%: tests/%.cpp $(SOURCES) $(HEADERS) Makefile | $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(SOURCES) $< -o $@

clean:
	rm -rf $(BUILD)
