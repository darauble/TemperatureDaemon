-include Config.in

all: prepare $(BINARY_NAME)

prepare:
	mkdir -p $(BUILD_DIR)/lib

OBJS = \
	$(BUILD_DIR)/main.o \
	$(BUILD_DIR)/lib/dallas.o \
	$(BUILD_DIR)/lib/onewire.o \
	$(BUILD_DIR)/lib/ow_driver_linux_usart.o

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@echo "Building $@"
	$(CC) -I"$(T_INCLUDE)" $(C_FLAGS) -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"

$(BUILD_DIR)/lib/%.o: $(T_LIBS)/%.c
	@echo "Building library $@"
	$(CC) -I"$(T_INCLUDE)" $(C_FLAGS) -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"

$(BINARY_NAME): $(OBJS)
	@echo "Building final binary $(BINARY_NAME)"
	$(CC) -pthread -g3 -o $(BINARY_NAME) $(OBJS)


clean:
	rm -rf $(BUILD_DIR) $(BINARY_NAME)