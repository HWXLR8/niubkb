PICOTOOL_PATH := picotool
PICO_SDK_PATH := ./pico-sdk

.PHONY: all left right clean

all: left right

$(PICO_SDK_PATH):
	git clone --depth 1 --recurse-submodules https://github.com/raspberrypi/pico-sdk.git

left:  HAND := HAND_LEFT
right: HAND := HAND_RIGHT

left right: $(PICO_SDK_PATH)
	cmake -S . -B build/$@ -DPICO_SDK_PATH=$(PICO_SDK_PATH) -DHAND=$(HAND)
	cmake --build build/$@ -j

flash-%: %
	@bytes=$$(wc -c < build/$*/niufw.bin); \
	printf "\n\033[1;35m=== BIN SIZE: %s BYTES ===\033[0m\n\n" $$bytes 1>&2
	$(PICOTOOL_PATH) load -v -x build/$*/niufw.uf2

clean:
	rm -rf build
