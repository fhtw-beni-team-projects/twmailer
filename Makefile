CC = g++
ASAN_FLAGS = -fsanitize=address -fno-omit-frame-pointer -Wno-format-security
CFLAGS := -std=c++20 -Wall -lssl -lcrypto
LDFLAGS   += -lpthread

TARGET = client server
BUILD_DIR = build

SOURCES_CLIENT = client.cpp
OBJS_CLIENT = $(addprefix $(BUILD_DIR)/,$(SOURCES_CLIENT:.cpp=.o))

SOURCES_SERVER = server.cpp user.cpp user_handler.cpp
OBJS_SERVER = $(addprefix $(BUILD_DIR)/,$(SOURCES_SERVER:.cpp=.o))

all: $(BUILD_DIR) $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

client: $(OBJS_CLIENT)
	$(CC) $(CFLAGS) $(ASAN_FLAGS) -o twmailer-client $^

server: $(OBJS_SERVER)
	$(CC) $(CFLAGS) $(ASAN_FLAGS) -o twmailer-server $^

$(BUILD_DIR)/%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) twmailer-client twmailer-server