#include <stdio.h>
#include <stdlib.h>
#include <lexer.hpp>
#include <acf.hpp>
#include <platform.hpp>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <util.hpp>


static char spaces[] = "                                                                ";


void output_object(FILE *h_file, FILE *c_file, acf obj, string_id *names, uint32 name_count)
{
    if (name_count == 1)
    {
        fprintf(h_file, "struct config\n{\n");
    }
    else
    {
        fprintf(h_file, "%.*sstruct\n%.*s{\n", (name_count - 1) * 4, spaces, (name_count - 1) * 4, spaces);
    }
    for (auto p : obj.pairs())
    {
        if (p.value.is_bool())
        {
            fprintf(h_file, "%.*sbool32 %s;\n", name_count * 4, spaces, p.key.get_cstring());
            fprintf(c_file, "    result");
            for (uint32 i = 1; i < name_count; i++)
                fprintf(c_file, ".%s", names[i].get_cstring());
            fprintf(c_file, ".%s", p.key.get_cstring());
            fprintf(c_file, " = cfg.");
            for (uint32 i = 1; i < name_count; i++)
                fprintf(c_file, "get_value(\"%s\").", names[i].get_cstring());
            fprintf(c_file, "get_value(\"%s\").to_bool();\n", p.key.get_cstring());
        }
        else if (p.value.is_integer())
        {
            fprintf(h_file, "%.*sint64 %s;\n", name_count * 4, spaces, p.key.get_cstring());
            fprintf(c_file, "    result");
            for (uint32 i = 1; i < name_count; i++)
                fprintf(c_file, ".%s", names[i].get_cstring());
            fprintf(c_file, ".%s", p.key.get_cstring());
            fprintf(c_file, " = cfg.");
            for (uint32 i = 1; i < name_count; i++)
                fprintf(c_file, "get_value(\"%s\").", names[i].get_cstring());
            fprintf(c_file, "get_value(\"%s\").to_integer();\n", p.key.get_cstring());
        }
        else if (p.value.is_floating())
        {
            fprintf(h_file, "%.*sfloat64 %s;\n", name_count * 4, spaces, p.key.get_cstring());
            fprintf(c_file, "    result");
            for (uint32 i = 1; i < name_count; i++)
                fprintf(c_file, ".%s", names[i].get_cstring());
            fprintf(c_file, ".%s", p.key.get_cstring());
            fprintf(c_file, " = cfg.");
            for (uint32 i = 1; i < name_count; i++)
                fprintf(c_file, "get_value(\"%s\").", names[i].get_cstring());
            fprintf(c_file, "get_value(\"%s\").to_floating();\n", p.key.get_cstring());
        }
        else if (p.value.is_string())
        {
            fprintf(h_file, "%.*sstring_view %s;\n", name_count * 4, spaces, p.key.get_cstring());
            fprintf(c_file, "    result");
            for (uint32 i = 1; i < name_count; i++)
                fprintf(c_file, ".%s", names[i].get_cstring());
            fprintf(c_file, ".%s", p.key.get_cstring());
            fprintf(c_file, " = cfg.");
            for (uint32 i = 1; i < name_count; i++)
                fprintf(c_file, "get_value(\"%s\").", names[i].get_cstring());
            fprintf(c_file, "get_value(\"%s\").to_string();\n", p.key.get_cstring());
        }
        else if (p.value.is_object())
        {
            names[name_count] = p.key;
            output_object(h_file, c_file, p.value, names, name_count + 1);
        }
    }
    if (name_count == 1)
    {
        fprintf(h_file, "    static config load(memory_allocator *, memory_buffer);\n};\n");
    }
    else
    {
        fprintf(h_file, "%.*s} %s;\n", (name_count - 1) * 4, spaces, names[name_count - 1].get_cstring());
    }
}

int main()
{
    {
        auto string_id_arena = mallocator()->allocate_arena(MEGABYTES(1));
        string_id::initialize(string_id_arena);
    }

    memory_allocator arena = mallocator()->allocate_arena(MEGABYTES(10));

    memory_buffer source = platform::load_file("../www/config.acf", &arena);

    acf cfg = acf::parse(&arena, source);

    FILE *h_file = fopen("gen/config.hpp", "w");
    FILE *c_file = fopen("gen/config.cpp", "w");

    fprintf(h_file, "#ifndef CONFIG_H\n"
                    "#define CONFIG_H\n"
                    "\n"
                    "#include <base.h>\n"
                    "\n\n");

    fprintf(c_file, "#include \"config.hpp\"\n"
                    "\n"
                    "config config::load(memory_allocator *a, memory_buffer content)\n"
                    "{\n"
                    "    config result;\n"
                    "    acf cfg = acf::parse(a, content);\n");

    if (cfg.is_object())
    {
        string_id names[16] = { string_id::from("cfg") };
        output_object(h_file, c_file, cfg, names, 1);

        fprintf(h_file, "\n"
                        "#endif // CONFIG_H\n");

        fprintf(c_file, "    return result;\n}\n");
    }

    return 0;
}


#include <memory/allocator.cpp>
#include <string_id.cpp>
#include <lexer.cpp>
#include <acf.cpp>
#include <util.cpp>
#include <memory_bucket.cpp>
#include <os/platform_posix.cpp>
