# Host-side pure C++ tests (no ESP-IDF). Run from repo root:
#   make -C tests/host
#   make -C tests/host run

FLINT := ../../components/flint
BUILD := build
CXX ?= c++
CXXFLAGS := -std=c++17 -Wall -Wextra -Werror -I$(FLINT)

.PHONY: all run clean

all: $(BUILD)/test_day_mode $(BUILD)/test_date_fmt

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/test_day_mode: test_day_mode.cpp $(FLINT)/day_mode/day_mode_hour.cpp \
		$(FLINT)/day_mode/day_mode_profile.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ test_day_mode.cpp $(FLINT)/day_mode/day_mode_hour.cpp \
		$(FLINT)/day_mode/day_mode_profile.cpp

$(BUILD)/test_date_fmt: test_date_fmt.cpp stubs/i18n_stub.cpp $(FLINT)/time/date_fmt.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ test_date_fmt.cpp stubs/i18n_stub.cpp $(FLINT)/time/date_fmt.cpp

run: all
	./$(BUILD)/test_day_mode
	./$(BUILD)/test_date_fmt

clean:
	rm -rf $(BUILD)
