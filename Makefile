# Tools

CC = gcc

# Project

NAME = v4l2-hantro-h264-encoder

# Directories

BUILD = build
OUTPUT = .

# Sources

SOURCES = \
	v4l2-hantro-h264-encoder.c \
	v4l2-encoder.c \
	h264.c \
	h264-rate-control.c \
	media.c \
	v4l2.c \
	unit.c \
	bitstream.c \
	draw.c \
	csc.c
OBJECTS = $(SOURCES:.c=.o)
DEPS = $(SOURCES:.c=.d)

# Compiler

CFLAGS = -I. $(shell pkg-config --cflags cairo libudev) -Ofast
LDFLAGS = -lcairo -lm $(shell pkg-config --libs libudev)

# Produced files

BUILD_OBJECTS = $(addprefix $(BUILD)/,$(OBJECTS))
BUILD_DEPS = $(addprefix $(BUILD)/,$(DEPS))
BUILD_BINARY = $(BUILD)/$(NAME)
BUILD_DIRS = $(sort $(dir $(BUILD_BINARY) $(BUILD_OBJECTS)))

OUTPUT_BINARY = $(OUTPUT)/$(NAME)
OUTPUT_DIRS = $(sort $(dir $(OUTPUT_BINARY)))

all: $(OUTPUT_BINARY)

$(BUILD_DIRS):
	@mkdir -p $@

$(BUILD_OBJECTS): $(BUILD)/%.o: %.c | $(BUILD_DIRS)
	@echo " CC     $<"
	@$(CC) $(CFLAGS) -MMD -MF $(BUILD)/$*.d -c $< -o $@

$(BUILD_BINARY): $(BUILD_OBJECTS)
	@echo " LINK   $@"
	@$(CC) $(CFLAGS) -o $@ $(BUILD_OBJECTS) $(LDFLAGS)

$(OUTPUT_DIRS):
	@mkdir -p $@

$(OUTPUT_BINARY): $(BUILD_BINARY) | $(OUTPUT_DIRS)
	@echo " BINARY $@"
	@cp $< $@

.PHONY: clean
clean:
	@echo " CLEAN"
	@rm -rf $(foreach object,$(basename $(BUILD_OBJECTS)),$(object)*) $(basename $(BUILD_BINARY))*
	@rm -rf $(OUTPUT_BINARY)

.PHONY: distclean
distclean: clean
	@echo " DISTCLEAN"
	@rm -rf $(BUILD)

-include $(BUILD_DEPS)
