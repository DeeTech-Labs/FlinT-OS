# Host-side pure C++ tests (no ESP-IDF). Run from repo root:
#   make -C tests/host
#   make -C tests/host run

FLINT := ../../components/flint
BUILD := build
CXX ?= g++
# Keep asserts armed if any sneak in; prefer hostFail / hostRequire.
CXXFLAGS := -std=c++17 -Wall -Wextra -Werror -UNDEBUG -I$(FLINT) -I.

.PHONY: all run clean

all: $(BUILD)/test_day_mode $(BUILD)/test_date_fmt $(BUILD)/test_ui_dirty \
	$(BUILD)/test_settings_clamp

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/test_day_mode: test_day_mode.cpp $(FLINT)/day_mode/day_mode_hour.cpp \
		$(FLINT)/day_mode/day_mode_profile.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ test_day_mode.cpp $(FLINT)/day_mode/day_mode_hour.cpp \
		$(FLINT)/day_mode/day_mode_profile.cpp

$(BUILD)/test_date_fmt: test_date_fmt.cpp stubs/i18n_stub.cpp $(FLINT)/time/date_fmt.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ test_date_fmt.cpp stubs/i18n_stub.cpp $(FLINT)/time/date_fmt.cpp

$(BUILD)/test_ui_dirty: test_ui_dirty.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ test_ui_dirty.cpp

$(BUILD)/test_settings_clamp: test_settings_clamp.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ test_settings_clamp.cpp

run: all
	./$(BUILD)/test_day_mode
	./$(BUILD)/test_date_fmt
	./$(BUILD)/test_ui_dirty
	./$(BUILD)/test_settings_clamp

clean:
	rm -rf $(BUILD)
