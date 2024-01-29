#include <stdio.h>
#include <stdlib.h>
#include <acf.hpp>


char config[] =
"wait_timeout = 10000\n"
"backlog_size = 32\n"
"prune_timeout = 1000000\n"
"\n"
"unix_domain_socket = \"/tmp/webspider_unix_socket\"\n"
"\n"
"logger = {\n"
"    stream = 1\n"
"    file = 0\n"
"\n"
"    filename = \"/var/log/webspider.log\"\n"
"    max_size = 1000000\n"
"}\n"
;


void print_acf(acf t, int depth)
{
    char spaces[] = "                                                ";
    if (t.is_null())
    {
        printf("null");
    }
    else if (t.is_integer())
    {
        printf("%lld", t.get_integer());
    }
    else if (t.is_string())
    {
        string_view s = t.get_string();
        printf("\"%.*s\"", (int) s.size, s.data);
    }
    else if (t.is_object())
    {
        printf("{\n");

        for (auto p : t.pairs())
        {
            string_view key = p.key.get_string_view();
            printf("%.*s%.*s = ", (int) depth * 2, spaces, (int) key.size, key.data);
            print_acf(p.value, depth + 1);
            printf(";\n");
        }

        printf("%.*s}", (int) (depth - 1) * 2, spaces);
    }
}


int main()
{
    auto string_id_buffer = ALLOCATE_BUFFER_(mallocator(), MEGABYTES(1));
    auto string_id_arena = make_memory_arena(string_id_buffer);
    string_id::initialize(string_id_arena);

    auto buffer = ALLOCATE_BUFFER_(mallocator(), MEGABYTES(1));
    auto arena = make_memory_arena(buffer);

    auto source = make_memory_block(config, ARRAY_COUNT(config));

    acf result = acf::parse(arena, source);
    print_acf(result, 1);

    return 0;
}


#include <acf.cpp>
#include <memory_allocator.c>
#include <lexer.c>
#include <string_id.cpp>
