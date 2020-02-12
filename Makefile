-include Config

#### Compilation flags ####
FLAGS_RELEASE = -O1
FLAGS_DEBUG = -Og -ggdb

ifeq ($(BUILD), "RELEASE")
    C_FLAGS = $(FLAGS_RELEASE)
else ifeq ($(BUILD), "DEBUG")
    C_FLAGS = $(FLAGS_DEBUG)
else
    $(error "BUILD should be either RELEASE or DEBUG")
endif

SHARED_LIBS = -lpthread -ljansson -lpaho-mqtt3as
C_FLAGS += -std=c11 -Wall -c -fmessage-length=0 $(SHARED_LIBS)

OW_LIBS = DallasOneWire

SRC_DIR = src
BUILD_DIR = build
BINARY_NAME = temp_daemon

INCLUDES = \
    -I"$(OW_LIBS)/dallas" \
    -I"$(OW_LIBS)/drivers" \
    -I"$(OW_LIBS)/onewire"


#### Targets ####
all: prepare $(BINARY_NAME)

prepare:
	mkdir -p $(BUILD_DIR)

OBJS = \
	$(BUILD_DIR)/$(SRC_DIR)/main.o \
	$(BUILD_DIR)/$(SRC_DIR)/temp_output_tsv.o \
	$(BUILD_DIR)/$(SRC_DIR)/temp_output_json.o \
	$(BUILD_DIR)/$(SRC_DIR)/mqtt_output.o \
	$(BUILD_DIR)/$(OW_LIBS)/dallas/dallas.o \
	$(BUILD_DIR)/$(OW_LIBS)/onewire/onewire.o \
	$(BUILD_DIR)/$(OW_LIBS)/drivers/ow_driver_linux_usart.o


$(OBJS): $(BUILD_DIR)/%.o: %.c
	mkdir -p $(@D)
	$(CC) $(INCLUDES)  $(C_FLAGS) $(T_DEFINES) -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"

$(BINARY_NAME): $(OBJS)
	@echo "Linking final binary $(BINARY_NAME)"
	$(CC) -o $(BINARY_NAME) $(OBJS) $(SHARED_LIBS)
ifeq ($(BUILD), "RELEASE")
	strip $(BINARY_NAME)
endif


clean:
	rm -rf $(BUILD_DIR) $(BINARY_NAME)
