CC := g++
CXXFLAG := -O3 #-O2
#DEBUG := -g -DXDP_DEBUG_VERBOSE
LIB := -lpthread -lz 
SRC := $(wildcard *.cpp)
OBJ := $(SRC:%.cpp=%.o)
XDP_PACKET_LIB := ../../libxdppacket.a
XDP_PACKET_INCLUDE := -I../../

LIBBPF_LIB := -Wl,--whole-archive ../../libbpf-0.3/src/libbpf.a -Wl,--no-whole-archive -lelf
LIBBPF_INCLUDE := -I./ -I../../libbpf-0.3/src -I../../libbpf-0.3/include/uapi -I../../libbpf-0.3/src/build/usr/include
LIB += $(LIBBPF_LIB) $(XDP_PACKET_LIB) -lnuma
INCLUDE := $(LIBBPF_INCLUDE) $(XDP_PACKET_INCLUDE)
TARGET := synflood
all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -o $(TARGET) $(OBJ) $(LIB)

$(OBJ): %.o:%.cpp
	$(CC) -c $< -o $@ $(DEBUG) $(CXXFLAG) $(INCLUDE) 

debug:
	@echo $(SRC)
	@echo $(OBJ)

clean:
	rm -rf $(OBJ) $(TARGET)

.PHONY: all clean debug




