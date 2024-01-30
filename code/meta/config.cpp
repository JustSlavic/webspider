#include <stdio.h>
#include <stdlib.h>
#include <acf.hpp>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>


// static char spaces[] = "                                      ";


memory_block load_file(memory_allocator allocator, char const *filename);
void output_object(FILE *h_file, FILE *c_file, acf obj, string_id *names, uint32 name_count)
{
    if (name_count == 1)
    {
        fprintf(h_file, "struct config\n{\n");
    }
    else
    {
        fprintf(h_file, "struct\n{\n");
    }
    for (auto p : obj.pairs())
    {
        if (p.value.is_bool())
        {
            fprintf(h_file, "    bool32 %s;\n", p.key.get_cstring());
            fprintf(c_file, "    result");
            for (int i = 1; i < name_count; i++)
                fprintf(c_file, ".%s", names[i].get_cstring());
            fprintf(c_file, ".%s", p.key.get_cstring());
            fprintf(c_file, " = cfg.");
            for (int i = 1; i < name_count; i++)
                fprintf(c_file, "get_value(\"%s\").", names[i].get_cstring());
            fprintf(c_file, "get_value(\"%s\").to_bool();\n", p.key.get_cstring());
        }
        else if (p.value.is_integer())
        {
            fprintf(h_file, "    int64 %s;\n", p.key.get_cstring());
            fprintf(c_file, "    result");
            for (int i = 1; i < name_count; i++)
                fprintf(c_file, ".%s", names[i].get_cstring());
            fprintf(c_file, ".%s", p.key.get_cstring());
            fprintf(c_file, " = cfg.");
            for (int i = 1; i < name_count; i++)
                fprintf(c_file, "get_value(\"%s\").", names[i].get_cstring());
            fprintf(c_file, "get_value(\"%s\").to_integer();\n", p.key.get_cstring());
        }
        else if (p.value.is_floating())
        {
            fprintf(h_file, "    float64 %s;\n", p.key.get_cstring());
            fprintf(c_file, "    result");
            for (int i = 1; i < name_count; i++)
                fprintf(c_file, ".%s", names[i].get_cstring());
            fprintf(c_file, ".%s", p.key.get_cstring());
            fprintf(c_file, " = cfg.");
            for (int i = 1; i < name_count; i++)
                fprintf(c_file, "get_value(\"%s\").", names[i].get_cstring());
            fprintf(c_file, "get_value(\"%s\").to_floating();\n", p.key.get_cstring());
        }
        else if (p.value.is_string())
        {
            fprintf(h_file, "    string_view %s;\n", p.key.get_cstring());
            fprintf(c_file, "    result");
            for (int i = 1; i < name_count; i++)
                fprintf(c_file, ".%s", names[i].get_cstring());
            fprintf(c_file, ".%s", p.key.get_cstring());
            fprintf(c_file, " = cfg.");
            for (int i = 1; i < name_count; i++)
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
        fprintf(h_file, "    static config load(memory_allocator, memory_block);\n};\n");
    }
    else
    {
        fprintf(h_file, "} %s;\n", names[name_count - 1].get_cstring());
    }
}

int main()
{
    auto string_id_buffer = ALLOCATE_BUFFER_(mallocator(), MEGABYTES(1));
    auto string_id_arena = make_memory_arena(string_id_buffer);
    string_id::initialize(string_id_arena);

    byte *memory = (byte *) malloc(MEGABYTES(1));
    memory_block buffer = make_memory_block(memory, MEGABYTES(10));
    memory_allocator arena = make_memory_arena(buffer);

    memory_block source = load_file(arena, "../www/config.acf");

    acf cfg = acf::parse(arena, source);

    FILE *h_file = fopen("config.hpp", "w");
    FILE *c_file = fopen("config.cpp", "w");

    fprintf(h_file, "#ifndef CONFIG_H\n"
                    "#define CONFIG_H\n"
                    "\n"
                    "#include <base.h>\n"
                    "\n\n");

    fprintf(c_file, "#include \"config.hpp\"\n"
                    "\n"
                    "config config::load(memory_allocator a, memory_block content)\n"
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

memory_block load_file(memory_allocator allocator, char const *filename)
{
    memory_block result = {};

    int fd = open(filename, O_RDONLY, 0);
    if (fd < 0)
    {
        return result;
    }

    struct stat st;
    int ec = fstat(fd, &st);
    if (ec < 0)
    {
        return result;
    }

    memory_block block = ALLOCATE_BUFFER(allocator, st.st_size);
    if (block.memory != NULL)
    {
        uint32 bytes_read = read(fd, block.memory, st.st_size);
        if (bytes_read < st.st_size)
        {
            DEALLOCATE(allocator, block);
        }
        else
        {
            result = block;
        }
    }

    return result;
}


#include <memory_allocator.c>
#include <string_id.cpp>
#include <lexer.c>
#include <acf.cpp>
