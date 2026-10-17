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
	$(BUILD)/test_settings_clamp $(BUILD)/test_fi_validate $(BUILD)/test_cfg_fs $(BUILD)/test_clock_schedule

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/test_day_mode: test_day_mode.cpp $(FLINT)/day_mode/day_mode_hour.cpp \
		$(FLINT)/day_mode/day_mode_profile.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ test_day_mode.cpp $(FLINT)/day_mode/day_mode_hour.cpp \
		$(FLINT)/day_mode/day_mode_profile.cpp

$(BUILD)/test_date_fmt: test_date_fmt.cpp $(FLINT)/i18n/i18n_catalog.inc $(FLINT)/i18n/i18n.cpp $(FLINT)/time/date_fmt.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ test_date_fmt.cpp $(FLINT)/i18n/i18n.cpp $(FLINT)/time/date_fmt.cpp

$(BUILD)/test_ui_dirty: test_ui_dirty.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ test_ui_dirty.cpp

$(BUILD)/test_settings_clamp: test_settings_clamp.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ test_settings_clamp.cpp

$(BUILD)/test_fi_validate: test_fi_validate.cpp $(FLINT)/assets/flint_fi.h | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ test_fi_validate.cpp

$(BUILD)/test_cfg_fs: test_cfg_fs.cpp $(FLINT)/settings/cfg_fs.cpp $(wildcard stubs/cfg/*.h stubs/cfg/config/*.h) | $(BUILD)
	$(CXX) -Istubs/cfg $(CXXFLAGS) -Wl,--wrap=fflush,--wrap=fsync,--wrap=fclose,--wrap=fread,--wrap=ferror -o $@ test_cfg_fs.cpp $(FLINT)/settings/cfg_fs.cpp

$(BUILD)/test_clock_schedule: test_clock_schedule.cpp $(FLINT)/time/clock_schedule.h | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ test_clock_schedule.cpp

run: all
	./$(BUILD)/test_day_mode
	./$(BUILD)/test_date_fmt
	./$(BUILD)/test_ui_dirty
	./$(BUILD)/test_clock_schedule
	./$(BUILD)/test_cfg_fs
	./$(BUILD)/test_fi_validate
	./$(BUILD)/test_settings_clamp

clean:
	rm -rf $(BUILD)
