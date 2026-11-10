#include "weights.h"
#include <stdio.h>
#include <stdlib.h>

weights mg_weights;
weights eg_weights;

void init_weights()
{
    parse_weights_from_file(WEIGHTS_FILE);
}

void parse_weights(char *raw)
{
    const int vals_len = sizeof(weights) / sizeof(I16);

    I16 vals_mg[vals_len];
    I16 vals_eg[vals_len];

    int index = 0;
    int count = 0;

    while (raw[index] && count < vals_len * 2)
    {
        // Skip whitespace
        if (isspace((unsigned char)raw[index]))
        {
            index++;
            continue;
        }

        // Skip comments
        if (raw[index] == '#')
        {
            while (raw[index] && raw[index] != '\n')
            {
                index++;
            }

            continue;
        }

        char *end;
        long val = strtol(raw + index, &end, 10);

        if (end == raw + index || val < INT16_MIN || val > INT16_MAX)
        {
            fprintf(stderr, "Invalid weight at position %d\n", index);
            return;
        }

        if (count < vals_len) vals_mg[count] = (I16) val;
        else vals_eg[count - vals_len] = (I16) val;

        count++;
        index = (int) (end - raw);
    }

    if (count != vals_len * 2)
    {
        fprintf(stderr, "Expected %d weights, got %d\n",
                vals_len * 2, count);
        return;
    }

    memcpy(&mg_weights, vals_mg, sizeof(weights));
    memcpy(&eg_weights, vals_eg, sizeof(weights));
}


void parse_weights_from_file(char *file)
{
    FILE *f = fopen(file, "rb");

    if (!f)
    {
        perror("Could not open weights file");
        return;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);

    if (fsize < 0)
    {
        fclose(f);
        return;
    }

    rewind(f);

    char *content = malloc((size_t)fsize + 1);

    if (!content)
    {
        fclose(f);
        return;
    }

    size_t bytes = fread(content, 1, (size_t)fsize, f);
    fclose(f);

    content[bytes] = '\0';

    parse_weights(content);

    free(content);
}
