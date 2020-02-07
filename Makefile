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
T_LIBS = $(ARM_LIBS)/src
T_INCLUDE = $(ARM_LIBS)/include
T_DEFINES =
SRC_DIR = src
BUILD_DIR = build
BINARY_NAME = temp_daemon

INCLUDES = \
    -I"$(T_INCLUDE)"


#### Targets ####
all: prepare $(BINARY_NAME)

prepare:
	mkdir -p $(BUILD_DIR)/lib

OBJS = \
	$(BUILD_DIR)/main.o \
	$(BUILD_DIR)/temp_output_tsv.o \
	$(BUILD_DIR)/temp_output_json.o \
	$(BUILD_DIR)/mqtt_output.o \
	$(BUILD_DIR)/lib/dallas.o \
	$(BUILD_DIR)/lib/onewire.o \
	$(BUILD_DIR)/lib/ow_driver_linux_usart.o

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@echo "Building $@"
	$(CC) $(INCLUDES)  $(C_FLAGS) -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"

$(BUILD_DIR)/lib/%.o: $(T_LIBS)/%.c
	@echo "Building library $@"
	$(CC) $(INCLUDES) $(C_FLAGS) -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"

$(BINARY_NAME): $(OBJS)
	@echo "Linking final binary $(BINARY_NAME)"
	$(CC) -o $(BINARY_NAME) $(OBJS) $(SHARED_LIBS)
ifeq ($(BUILD), "RELEASE")
	strip $(BINARY_NAME)
endif


clean:
	rm -rf $(BUILD_DIR) $(BINARY_NAME)
