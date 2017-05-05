#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>

#include "porter.h"

int main(int argc, char **argv)
{
    int i;
    char str[128];

    if (argc > 1)
    {
        for (i = 1; i < argc; i++)  /* for each input word... */
        {
            strcpy(str, argv[i]);

            PORTER_Stem(str);
            fprintf(stdout, "%s -> %s\n", argv[i], str);
        }
    }
    else
    {
        while (fgets(str, sizeof(str), stdin))
        {
            int off = 0;
            while (str[off] != '\0' && str[off] != '\n') off++;
            str[off] = '\0';

            PORTER_Stem(str);
            fprintf(stdout, "%s\n", str);
        }
    }

    return 0;
}
