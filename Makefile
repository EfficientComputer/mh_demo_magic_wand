BUILD_DIR := build
CMAKE_GENERATOR := Ninja
CMAKE_FLAGS := -DEFF_STDIO_PORT=3

.DEFAULT_GOAL := build

.PHONY: build clean config help

config:
	@mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake -G $(CMAKE_GENERATOR) .. $(CMAKE_FLAGS)

build: config
	cd $(BUILD_DIR) && ninja

clean:
	@if [ -d "$(BUILD_DIR)" ]; then rm -rf $(BUILD_DIR); fi

help:
	@echo "Usage: make [target]"
	@echo ""
	@echo "Targets:"
	@echo "  build  - Build the project (default)"
	@echo "  config - Run CMake configuration"
	@echo "  clean  - Remove build directory"
	@echo "  help   - Show this help message"
