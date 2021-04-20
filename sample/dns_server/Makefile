CC := g++
DEBUG := -g
LIB := -lpthread 
SRC := $(wildcard *.cpp)
OBJ := $(SRC:%.cpp=%.o)
XDP_PACKET_LIB := ../../libxdppacket.a
XDP_PACKET_INCLUDE := -I../../

#LIBBPF_LIB := -Wl,--whole-archive ./libbpf/src/libbpf.a -Wl,--no-whole-archive -lelf
#LIBBPF_INCLUDE := -I./ -I./libbpf/src -I./libbpf/include/uapi -I./libbpf/src/build/usr/include
LIB += $(LIBBPF_LIB) $(XDP_PACKET_LIB)

TARGET := dns_server
all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -o $(TARGET) $(OBJ) $(LIB)

$(OBJ): %.o:%.cpp
	$(CC) -c $< -o $@ $(DEBUG) $(CFLAGS) $(XDP_PACKET_INCLUDE)

debug:
	@echo $(SRC)
	@echo $(OBJ)

clean:
	rm -rf $(OBJ) $(TARGET)

.PHONY: all clean debug




