#ifndef CONFIG_H
#define CONFIG_H

#include <base.h>


struct config
{
    int64 wait_timeout;
    int64 backlog_size;
    int64 prune_timeout;
    string_view unix_domain_socket;
    struct
    {
        bool32 stream;
        bool32 file;
        string_view filename;
        int64 max_size;
        struct
        {
            int64 one_;
            int64 two_;
            int64 three;
        } something_else;
    } logger;
    static config load(memory_allocator, memory_block);
};

#endif // CONFIG_H
