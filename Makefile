-include Config.in

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
	
	#paho.mqtt.c/build/output/libpaho-mqtt3as.so
	#/usr/local/lib/libjansson.a 

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@echo "Building $@"
	$(CC) $(INCLUDES)  $(C_FLAGS) -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"

$(BUILD_DIR)/lib/%.o: $(T_LIBS)/%.c
	@echo "Building library $@"
	$(CC) $(INCLUDES) $(C_FLAGS) -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"

$(BINARY_NAME): $(OBJS)
	@echo "Linking final binary $(BINARY_NAME)"
	$(CC) $(L_FLAGS) -o $(BINARY_NAME) $(OBJS) $(SHARED_LIBS)


clean:
	rm -rf $(BUILD_DIR) $(BINARY_NAME)