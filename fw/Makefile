PICOTOOL_PATH := picotool
PICO_SDK_PATH := ./pico-sdk
UF2 := build/niufw.uf2
BIN := build/niufw.bin

all: build

$(PICO_SDK_PATH):
	git clone --depth 1 --recurse-submodules https://github.com/raspberrypi/pico-sdk.git

config: $(PICO_SDK_PATH)
	cmake -S . -B build -DPICO_SDK_PATH=$(PICO_SDK_PATH)

build: config
	cmake --build build -j

flash: build
	@bytes=$$(wc -c < $(BIN)); \
	printf "\n\033[1;35m=== BIN SIZE: %s BYTES ===\033[0m\n\n" $$bytes 1>&2
	$(PICOTOOL_PATH) load -v -x $(UF2)

clean:
	rm -rf build
