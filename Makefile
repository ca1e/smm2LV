TARGET_EXEC ?= toost

BUILD_DIR ?= ./bin
SRC_DIRS ?= ./src

# Web builds target wasm32-wasip1 with wasi-sdk (https://github.com/WebAssembly/wasi-sdk).
# WASI_SDK_PATH selects the install prefix (the official image mounts it at /opt/wasi-sdk).
ifeq ($(PLATFORM),web)
	WASI_SDK_PATH ?= /opt/wasi-sdk
	CC := $(WASI_SDK_PATH)/bin/clang
	CXX := $(WASI_SDK_PATH)/bin/clang++
	TARGET_TRIPLE ?= wasm32-wasip1
else
	CC := gcc
	CXX := c++
endif

CFLAGS := -std=gnu11
CXXFLAGS := -std=gnu++2a
CPPFLAGS := -Wall -Wextra -Wno-missing-field-initializers -Wno-deprecated-enum-enum-conversion -Wno-narrowing \
	-I./src -I./third_party/fmt/include -I./third_party/rapidjson/include -I./third_party/zlib

ifeq ($(PLATFORM),web)
	# -fwasm-exceptions + legacy-eh=false match the prebuilt wasi-libc/libc++abi (see
	# wasi-sdk CppExceptions.md); -lunwind at link time provides __cpp_exception.
	# -msimd128: ~22% faster rasterization (148ms -> 116ms, dual-world render).
	WEB_FLAGS := --target=$(TARGET_TRIPLE) -fwasm-exceptions -mllvm -wasm-use-legacy-eh=false \
		-msimd128 -Wno-c++11-narrowing-const-reference
	WEB_LDFLAGS := $(WEB_FLAGS) -mexec-model=reactor -Wl,--no-entry -lunwind \
		-Wl,--export=web_load_level -Wl,--export=web_render_level -Wl,--export=web_get_result \
		-Wl,--export=web_get_info_html -Wl,--export=web_get_json -Wl,--export=web_set_options \
		-Wl,--export=web_set_image_scale -Wl,--export=web_tilesheet_path \
		-Wl,--export=web_set_asset -Wl,--export=web_set_deaths -Wl,--export=web_set_comments \
		-Wl,--export=web_render_overlay -Wl,--export=web_get_comment_marks \
		-Wl,--export=malloc -Wl,--export=free
endif

ifeq ($(BUILD),release)
	# "Release" build - optimization, and no debug symbols
	ifeq ($(PLATFORM),web)
	CPPFLAGS += -O3 $(WEB_FLAGS)
	else
	CPPFLAGS += -O3 -s
	endif
else
	# "Debug" build - no optimization, and debugging symbols
	ifeq ($(PLATFORM),web)
	CPPFLAGS += -O0 -g -DIS_DEBUG=1 $(WEB_FLAGS)
	else
	CPPFLAGS += -Og -g -ggdb -DIS_DEBUG=1
	endif
endif

SRCS := ./src/webapi.cpp ./src/LevelParser.cpp ./src/LevelDrawer.cpp ./src/Canvas.cpp ./src/Helpers.cpp \
	./src/kaitai/kaitaistream.cpp ./src/MM2/level.cpp ./src/SMM2CourseDecryptor/aes.cpp ./src/SMM2CourseDecryptor/decrypt.cpp \
	./third_party/fmt/src/format.cc ./third_party/fmt/src/os.cc

# vendored zlib: memory inflate/deflate only (gz* file API needs POSIX fds)
ifeq ($(PLATFORM),web)
SRCS += ./third_party/zlib/adler32.c ./third_party/zlib/crc32.c ./third_party/zlib/infback.c \
	./third_party/zlib/inffast.c ./third_party/zlib/inflate.c ./third_party/zlib/inftrees.c \
	./third_party/zlib/uncompr.c ./third_party/zlib/zutil.c ./third_party/zlib/deflate.c \
	./third_party/zlib/compress.c
endif

ifeq ($(OS)$(PLATFORM),Windows_NT)
SRCS += src/info.rc src/icon.rc
endif

OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

INC_DIRS := $(shell find $(SRC_DIRS) -type d)
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

CPPFLAGS += $(INC_FLAGS) -MMD -MP

ifeq ($(PLATFORM),web)
$(BUILD_DIR)/toost.wasm: $(OBJS)
	$(CXX) $(OBJS) -o $@ $(WEB_LDFLAGS)
else
$(BUILD_DIR)/$(TARGET_EXEC): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)
endif

# assembly
$(BUILD_DIR)/%.s.o: %.s
	$(MKDIR_P) $(dir $@)
	$(AS) $(ASFLAGS) -c $< -o $@

# c source
$(BUILD_DIR)/%.c.o: %.c
	$(MKDIR_P) $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# c++ source
$(BUILD_DIR)/%.cpp.o: %.cpp
	$(MKDIR_P) $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/%.cc.o: %.cc
	$(MKDIR_P) $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

ifeq ($(OS),Windows_NT)
# Windows RES file
$(BUILD_DIR)/%.rc.o: %.rc
	$(MKDIR_P) $(dir $@)
	windres $< $@
endif

.PHONY: clean

clean:
	$(RM) -r $(BUILD_DIR)

-include $(DEPS)

MKDIR_P ?= mkdir -p
