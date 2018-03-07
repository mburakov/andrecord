#define STRIMPL(op) #op
#define STR(op) STRIMPL(op)

#define LOG(level, ...)                                   \
    __android_log_print(ANDROID_LOG_##level, "andrecord", \
                        __FILE__ ":" STR(__LINE__) " " __VA_ARGS__)

#define LENGTH(op) (sizeof(op) / sizeof *(op))
#define FOR_EACH(it, container)                             \
    for (int keep = 1, count = 0, size = LENGTH(container); \
         keep && count != size; keep = !keep, ++count)      \
        for (it = (container) + count; keep; keep = !keep)

#define BEGIN_MAP(op) switch (op) {
#define MAP_STR(op) \
    case op:        \
        return #op;
#define END_MAP(op) \
    default:        \
        return op;  \
        }
