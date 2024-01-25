#include <stdio.h>
#include <stdlib.h>
#include <acf.h>


char config[] =
"hello = 42 blah = 2"
"";


int main()
{
    void *memory = malloc(MEGABYTES(1));
    memory_block buffer = { .memory = memory, .size = MEGABYTES(1) };
    memory_allocator arena = make_memory_arena(buffer);
    memory_block source = { .memory = (byte *) config, .size = ARRAY_COUNT(config) };

    acf *result = acf__parse(arena, source);
    if (acf__is_null(result))
    {
        printf("null\n");
    }
    else if (acf__is_integer(result))
    {
        printf("%lld\n", acf__get_integer(result));
    }
    else if (acf__is_object(result))
    {
        printf("Object:\n");

        string_view k;
        struct acf *v;
        acf__object_iterator it = acf__get_pairs(result);

        while (acf__get_key(&it, &k))
        {
            acf__get_val(&it, &v);
            printf("  %.*s = <something>\n", (int) k.size, k.data);

            acf__next_pair(&it);
        }
    }

    return 0;
}


#include <acf.c>
#include <memory_allocator.c>
#include <lexer.c>
