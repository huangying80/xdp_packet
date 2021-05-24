CLANG := clang
CLANG_FLAGS := -D__BPF_TRACING__ -Wall -Werror -Wno-unused-function -O3 #-O2
LLC := llc
XDP_PROG_SRC := $(wildcard xdp_kern_prog/*.c)
XDP_PROG_OBJ := $(XDP_PROG_SRC:%.c=%.ll)
XDP_PROG_TARGET := $(XDP_PROG_OBJ:%.ll=%.o)
XDP_PROG_DEBUG := #-DKERN_DEBUG -g

CC := gcc
AR := ar
CFLAGS +=  -Wall -Werror -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -O3 #-O2 -O
#DEBUG := -DXDP_DEBUG_VERBOSE -DXDP_ERROR_VERBOSE -g
LIB := -lpthread

XDP_PACKET_SRC := $(wildcard *.c)
XDP_PACKET_OBJ := $(XDP_PACKET_SRC:%.c=%.o)
XDP_PACKET_TARGET := libxdppacket.a

LIBBPF_LIB := -Wl,--whole-archive ./libbpf-0.3/src/libbpf.a -Wl,--no-whole-archive -lelf -lz
LIBBPF_INCLUDE := -I./ -I./libbpf-0.3/src -I./libbpf-0.3/include/uapi -I./libbpf-0.3/src/build/usr/include

all: static prog

static: $(XDP_PACKET_TARGET)
	$(AR) rcs $(XDP_PACKET_TARGET) $(XDP_PACKET_OBJ) 

$(XDP_PACKET_TARGET): $(XDP_PACKET_OBJ)

vpath %.c .
$(XDP_PACKET_OBJ): %.o:%.c
	$(CC) -c $< -o $@ $(DEBUG) $(CFLAGS) $(LIBBPF_INCLUDE)

prog: $(XDP_PROG_TARGET)

$(XDP_PROG_TARGET): $(XDP_PROG_OBJ)
	$(LLC) -march=bpf -filetype=obj -o $@ $<
	@chmod a+x $@

vpath %.c xdp_kern_prog
$(XDP_PROG_OBJ): %.ll:%.c
	$(CLANG) -S -target bpf $(XDP_PROG_DEBUG) $(CLANG_FLAGS) $(LIBBPF_INCLUDE)  -emit-llvm -c -o $@ $<

clean:
	rm -rf $(XDP_PACKET_OBJ) $(XDP_PACKET_TARGET) $(XDP_PROG_TARGET) $(XDP_PROG_OBJ)

.PHONY: all clean static prog




