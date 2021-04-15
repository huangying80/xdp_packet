/*
 * huangying, email: hy_gzr@163.com, huangying-c@360.cn
 */

#ifndef _XDP_KERN_PROG_H_
#define _XDP_KERN_PROG_H_
#define _textify(x) #x
#define textify(x) _textify(x)

#define XDP_ACTION_API  static __always_inline
#define XDP_ACTION_DEFINE(_type) \
    XDP_ACTION_API __u32 action_##_type(struct hdr_cursor *cur, void *data_end)
#define XDP_ACTION_CALL(_type, ...) action_##_type(__VA_ARGS__)

#define MAP_NAME(_type) map_action_##_type
#define MAP_REF(_type) &MAP_NAME(_type)
#define MAP_DEFINE(_type) struct bpf_map_def SEC("maps") MAP_NAME(_type)


#define MAP_INIT(_t, _ks, _vs, _me)   \
{                                     \
    .type = (_t),                     \
    .key_size = (_ks),                \
    .value_size = (_vs),              \
    .max_entries = (_me),             \
}

#define MAP_INIT_NO_PREALLOC(_t, _ks, _vs, _me) \
{                                     \
    .type = (_t),                     \
    .key_size = (_ks),                \
    .value_size = (_vs),              \
    .max_entries = (_me),             \
    .map_flags = BPF_F_NO_PREALLOC    \
}

#endif
