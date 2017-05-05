#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>

/* The flag used in the map is an unsigned 8 bit value:
 *
 *   x x x    x x x x x
 *   flags   --measure--
 *
 * Tht first three (high order) bits are:
 *
 *   v - stem contains a vowel
 *   d - stem ends in double consonant
 *   o - stem ends CVS
 *
 * The final five bits (low order) contain the measure of the word.  This
 * means that words longer than 31 characters may not be used.
 */

static inline uint8_t PORTER_setMeasure(uint8_t flags, uint8_t val)
{
    flags &= 0xE0;          /* clear any existing length */
    flags |= (val & 0x1F);  /* set to given length */
    return flags;
}

static inline uint8_t PORTER_setHasVowel(uint8_t flags, uint8_t val)
{
    /* 1000 0000 0x80 - OR Mask to set flag*/
    /* 0111 1111 0x7F - AND Mask to clear flag */

    if (val == 0)
        flags &= 0x7F;
    else
        flags |= 0x80;

    return flags;
}

static inline uint8_t PORTER_setCC(uint8_t flags, uint8_t val)
{
    /* 0100 0000 0x40 */
    /* 1011 1111 0xBF*/

    if (val == 0)
        flags &= 0xBF;
    else
        flags |= 0x40;

    return flags;
}

static inline uint8_t PORTER_setCVC(uint8_t flags, uint8_t val)
{
    /* 0010 0000 0x20 */
    /* 1101 1111 0xDF*/

    if (val == 0)
        flags &= 0xDF;
    else
        flags |= 0x20;

    return flags;
}

static inline uint8_t PORTER_getMeasure(uint8_t m)
{
    uint8_t rval;
    rval = m & 0x1F;
    return rval;
}

static inline uint8_t PORTER_hasVowel(uint8_t m)
{
    uint8_t rval;
    rval = ((m & 0x80) >> 7);
    return rval;
}

static inline uint8_t PORTER_endsCC(uint8_t m)
{
    uint8_t rval;
    rval = ((m & 0x40) >> 6);
    return rval;
}

static inline uint8_t PORTER_endsCVC(uint8_t m)
{
    uint8_t rval;
    rval = ((m & 0x20) >> 5);
    return rval;
}

static inline char PORTER_isCV(char *str, int pos)
{
    switch (str[pos])
    {
        case 'A': case 'E': case 'I':
        case 'O': case 'U':
            return 'V';

        case 'Y':
            if (pos == 0) return 'C';
            switch(str[pos - 1])
            {
                case 'A': case 'E': case 'I':
                case 'O': case 'U':
                    return 'C';
            }

            return 'V';
    }

    return 'C';
}

static inline int PORTER_ReMeasure(char *word, int off, uint8_t *map)
{
    int i;
    int m;
    char cur, prev;
    uint8_t flags;
    uint8_t hasVowel;

    if (off == 0)
    {
        m = 0;
        prev = 'C';
        hasVowel = 0;
    }
    else
    {
        m = PORTER_getMeasure(map[off]);
        prev = PORTER_isCV(word, off);
        hasVowel = PORTER_hasVowel(map[off]);
    }

    for (i = off; word[i] != '\0'; i++)
    {
        word[i] = toupper(word[i]);
        cur = PORTER_isCV(word, i);

        if (prev == 'V' && cur == 'C') m++;
        if (map != NULL)
        {
            flags = 0;

            flags = PORTER_setMeasure(flags, m);

            if (cur == 'V') hasVowel = 1;
            flags = PORTER_setHasVowel(flags, hasVowel);

            if (cur == 'C')
            {
                if (prev == 'C' && i > 0 && word[i] == word[i - 1])
                    flags = PORTER_setCC(flags, 1);
                else
                {
                    if (i >= 2 && prev == 'V' &&
                        word[i] != 'W' && word[i] != 'X' && word[i] != 'Y')
                    {
                        if (PORTER_isCV(word, i - 2) == 'C')
                            flags = PORTER_setCVC(flags, 1);
                    }
                }
            }

            map[i] = flags;
        }

        prev = cur;
    }

    return m;
}

/** Measure (by the Porter definition) a word.
 *
 *  @param word  the word to be measured.
 *  @param map   if not NULL, the accumulated measure of the word is recorded
 *               into each byte of the map (storage provided by the caller).
 *               This can be used to make subsequent applications of rules
 *               faster since the word does not need to be remeasured each
 *               time.
 */
static inline int PORTER_Measure(char *word, uint8_t *map)
{
    return PORTER_ReMeasure(word, 0, map);
}

static inline int PORTER_endsWith(char *word, int wordlen,
                                  char *suffix, int suflen)
{
    int off;

    if (wordlen < suflen) return 0;

    off = wordlen - suflen;
    if (strncmp(&word[off], suffix, suflen) == 0) return 1;
    return 0;
}

#ifdef DEBUG
static void PORTER_DumpMap(char *word, uint8_t *map)
{
    int len;
    int j;
    char substr[128];

    len = strlen(word);
    for (j = 0; j < len; j++)
    {
        memset(substr, 0x00, sizeof(substr));
        strncpy(substr, word, j + 1);

        fprintf(stdout, "[% 2d] '%s'", j, substr);
        fprintf(stdout, ", measure = %d", PORTER_getMeasure(map[j]));
        if (PORTER_hasVowel(map[j])) fprintf(stdout, ", hasVowel");
        if (PORTER_endsCC(map[j])) fprintf(stdout, ", endsCC");
        if (PORTER_endsCVC(map[j])) fprintf(stdout, ", endsCVC");
        fprintf(stdout, "\n");
    }

    return;
}
#endif

static inline int PORTER_step1a(char *word, int len, uint8_t *map)
{
#ifdef DEBUG
    fprintf(stderr, "%s() -> '%s'\n", __func__, word);
#endif

    /* SSES -> SS */
    if (PORTER_endsWith(word, len, "SSES", 4))
    {
        len -= 2;
        word[len] = '\0';
        return len;
    }

    /* IES -> I */
    if (PORTER_endsWith(word, len, "IES", 3))
    {
        len -= 2;
        word[len] = '\0';
        return len;
    }

    /* SS -> SS */
    if (PORTER_endsWith(word, len, "SS", 2))
    {
        return len;
    }

    /* S ->  */
    if (PORTER_endsWith(word, len, "S", 1))
    {
        len--;
        word[len] = '\0';
        return len;
    }

    return len;
}

static inline int PORTER_step1b(char *word, int len, uint8_t *map)
{
    int tryMore;
#ifdef DEBUG
    fprintf(stderr, "%s() -> '%s'\n", __func__, word);
#endif

    /* (m>0) EED -> EE */
    if (PORTER_endsWith(word, len, "EED", 3))
    {
        if (len > 4 && PORTER_getMeasure(map[len - 4]) > 0)
        {
            len--;
            word[len] = '\0';
        }

        return len;
    }

    tryMore = 0;

    /* (*v*) ED ->  */
    if (PORTER_endsWith(word, len, "ED", 2))
    {
        if (PORTER_hasVowel(map[len - 3]) != 0)
        {
            len -= 2;
            word[len] = '\0';
            tryMore = 1;
        }
    }

    /* (*v*) ING ->  */
    else if (PORTER_endsWith(word, len, "ING", 3))
    {
        if (PORTER_hasVowel(map[len - 4]) != 0)
        {
            len -= 3;
            word[len] = '\0';  /* truncate last three letters */
            tryMore = 1;
        }
    }

    if (tryMore == 0) return len;

    /* AT -> ATE */
    if (PORTER_endsWith(word, len, "AT", 2))
    {
        word[len] = 'E';
        len++;
        word[len] = '\0';

        PORTER_ReMeasure(word, len - 2, map);  /* word "grew", so remap */
        return len;
    }

    /* BL -> BLE */
    if (PORTER_endsWith(word, len, "BL", 2))
    {
        word[len] = 'E';
        len++;
        word[len] = '\0';

        PORTER_ReMeasure(word, len - 2, map);  /* word "grew", so remap */
        return len;
    }

    /* IZ -> IZE */
    if (PORTER_endsWith(word, len, "IZ", 2))
    {
        word[len] = 'E';
        len++;
        word[len] = '\0';

        PORTER_ReMeasure(word, len - 2, map);  /* word "grew", so remap */
        return len;
    }

    /* *d and not (*L or *S or *Z)) -> single letter */
    if (len > 1 && PORTER_endsCC(map[len - 1]) != 0 &&
        word[len - 1] != 'L' && word[len - 1] != 'S' && word[len - 1] != 'Z')
    {
        len--;
        word[len] = '\0';
        return len;
    }

    /* (m=1 and *o)  -> E */
    if (PORTER_getMeasure(map[len - 1]) == 1 &&
        PORTER_endsCVC(map[len - 1]) != 0)
    {
        word[len] = 'E';
        len++;
        word[len] = '\0';

        PORTER_ReMeasure(word, len - 1, map);  /* word "grew", so remap */
    }

    return len;
}

static inline int PORTER_step1c(char *word, int len, uint8_t *map)
{
#ifdef DEBUG
    fprintf(stderr, "%s() -> '%s'\n", __func__, word);
#endif

    /* (*v*) Y -> I */
    if (word[len - 1] == 'Y' &&
        PORTER_hasVowel(map[len - 2]) != 0)
    {
        word[len - 1] = 'I';
        PORTER_ReMeasure(word, len - 2, map);
    }

    return len;
}

static inline int PORTER_step2(char *word, int len, uint8_t *map)
{
#ifdef DEBUG
    fprintf(stderr, "%s() -> '%s'\n", __func__, word);
#endif

    switch (word[len - 1])
    {
        case 'I':
            /* (m>0) ENCI    ->  ENCE */
            if (PORTER_endsWith(word, len, "ENCI", 4))
            {
                if (PORTER_getMeasure(map[len - 5]) > 0)
                {
                    word[len - 1] = 'E';
                }

                return len;
            }

            /* (m>0) ANCI    ->  ANCE */
            if (PORTER_endsWith(word, len, "ANCI", 4))
            {
                if (PORTER_getMeasure(map[len - 5]) > 0)
                {
                    word[len - 1] = 'E';
                }

                return len;
            }

            /* (m>0) ABLI    ->  ABLE */
            if (PORTER_endsWith(word, len, "ABLI", 4))
            {
                if (PORTER_getMeasure(map[len - 5]) > 0)
                {
                    word[len - 1] = 'E';
                }

                return len;
            }

            /* (m>0) ALLI    ->  AL   */
            if (PORTER_endsWith(word, len, "ALLI", 4))
            {
                if (PORTER_getMeasure(map[len - 5]) > 0)
                {
                    len -= 2;
                    word[len] = '\0';
                }

                return len;
            }

            /* (m>0) ENTLI   ->  ENT  */
            if (PORTER_endsWith(word, len, "ENTLI", 5))
            {
                if (PORTER_getMeasure(map[len - 6]) > 0)
                {
                    len -= 2;
                    word[len] = '\0';
                }

                return len;
            }

            /* (m>0) ELI     ->  E    */
            if (PORTER_endsWith(word, len, "ELI", 3))
            {
                if (PORTER_getMeasure(map[len - 4]) > 0)
                {
                    len -= 2;
                    word[len] = '\0';
                }

                return len;
            }

            /* (m>0) OUSLI   ->  OUS  */
            if (PORTER_endsWith(word, len, "OUSLI", 5))
            {
                if (PORTER_getMeasure(map[len - 6]) > 0)
                {
                    len -= 2;
                    word[len] = '\0';
                }

                return len;
            }

            /* (m>0) ALITI   ->  AL   */
            if (PORTER_endsWith(word, len, "ALITI", 5))
            {
                if (PORTER_getMeasure(map[len - 6]) > 0)
                {
                    len -= 3;
                    word[len] = '\0';
                }

                return len;
            }

            /* (m>0) IVITI   ->  IVE  */
            if (PORTER_endsWith(word, len, "IVITI", 5))
            {
                if (PORTER_getMeasure(map[len - 6]) > 0)
                {
                    len -= 2;
                    word[len - 1] = 'E';
                    word[len] = '\0';
                }

                return len;
            }

            /* (m>0) BILITI  ->  BLE  */
            if (PORTER_endsWith(word, len, "BILITI", 6))
            {
                if (PORTER_getMeasure(map[len - 7]) > 0)
                {
                    len -= 3;
                    word[len - 2] = 'L';
                    word[len - 1] = 'E';
                    word[len] = '\0';
                }

                return len;
            }

            break;

        case 'L':
            /* (m>0) ATIONAL ->  ATE  */
            if (PORTER_endsWith(word, len, "ATIONAL", 7))
            {
                if (PORTER_getMeasure(map[len - 8]) > 0)
                {
                    len -= 4;
                    word[len - 1] = 'E';
                    word[len] = '\0';
                }

                return len;
            }

            /* (m>0) TIONAL  ->  TION */
            if (PORTER_endsWith(word, len, "TIONAL", 6))
            {
                if (PORTER_getMeasure(map[len - 7]) > 0)
                {
                    len -= 2;
                    word[len] = '\0';
                }

                return len;
            }

            break;

        case 'M':
            /* (m>0) ALISM   ->  AL   */
            if (PORTER_endsWith(word, len, "ALISM", 5))
            {
                if (PORTER_getMeasure(map[len - 6]) > 0)
                {
                    len -= 3;
                    word[len] = '\0';
                }

                return len;
            }

            break;

        case 'N':
            if (len < 6) return 0;

            /* (m>0) IZATION ->  IZE  */
            if (PORTER_endsWith(word, len, "IZATION", 7))
            {
                if (PORTER_getMeasure(map[len - 8]) > 0)
                {
                    len -= 4;
                    word[len - 1] = 'E';
                    word[len] = '\0';
                }

                return len;
            }

            /* (m>0) ATION   ->  ATE  */
            if (PORTER_endsWith(word, len, "ATION", 5))
            {
                if (PORTER_getMeasure(map[len - 6]) > 0)
                {
                    len -= 2;
                    word[len - 1] = 'E';
                    word[len] = '\0';
                }

                return len;
            }

            break;

        case 'R':
            if (len <= 5) return len;
            if (PORTER_getMeasure(map[len - 5]) < 1) return len;

            /* (m>0) IZER    ->  IZE  */
            if (PORTER_endsWith(word, len, "IZER", 4))
            {
                len--;
                word[len] = '\0';
                return len;
            }

            /* (m>0) ATOR    ->  ATE  */
            if (PORTER_endsWith(word, len, "ATOR", 4))
            {
                len--;
                word[len - 1] = 'E';
                word[len] = '\0';
                return len;
            }

            break;

        case 'S':
            if (len <= 8) return len;
            if (PORTER_getMeasure(map[len - 8]) < 1) return len;

            /* (m>0) IVENESS ->  IVE  */
            if (PORTER_endsWith(word, len, "IVENESS", 7))
            {
                len -= 4;
                word[len] = '\0';
                return len;
            }

            /* (m>0) FULNESS ->  FUL  */
            if (PORTER_endsWith(word, len, "FULNESS", 7))
            {
                len -= 4;
                word[len] = '\0';
                return len;
            }

            /* (m>0) OUSNESS ->  OUS  */
            if (PORTER_endsWith(word, len, "OUSNESS", 7))
            {
                len -= 4;
                word[len] = '\0';
                return len;
            }

            break;
    }

    return len;
}

static inline int PORTER_step3(char *word, int len, uint8_t *map)
{
#ifdef DEBUG
    fprintf(stderr, "%s() -> '%s'\n", __func__, word);
#endif

    switch (word[len - 1])
    {
        case 'E':
            /* All of the *E cases are five letters, so we can do the length
             * check and the measure up front. */

            if (len < 7) return len;
            if (PORTER_getMeasure(map[len - 6]) < 1) return len;

            /* (m>0) ICATE ->  IC */
            if (PORTER_endsWith(word, len, "ICATE", 5))
            {
                len -= 3;
                word[len] = '\0';
                return len;
            }

            /* (m>0) ATIVE ->     */
            if (PORTER_endsWith(word, len, "ATIVE", 5))
            {
                len -= 5;
                word[len] = '\0';
                return len;
            }

            /* (m>0) ALIZE ->  AL */
            if (PORTER_endsWith(word, len, "ICATE", 5))
            {
                len -= 3;
                word[len] = '\0';
                return len;
            }

            break;

        case 'I':
            /* (m>0) ICITI ->  IC */
            if (PORTER_endsWith(word, len, "ICITI", 5))
            {
                if (len > 5 && PORTER_getMeasure(map[len - 6]) > 0)
                {
                    len -= 3;
                    word[len] = '\0';
                }

                return len;
            }

            break;

        case 'L':
            /* (m>0) ICAL  ->  IC */
            if (PORTER_endsWith(word, len, "ICAL", 4))
            {
                if (len > 5 && PORTER_getMeasure(map[len - 5]) > 0)
                {
                    len -= 2;
                    word[len] = '\0';
                }

                return len;
            }

            /* (m>0) FUL   ->     */
            if (PORTER_endsWith(word, len, "FUL", 3))
            {
                if (len > 4 && PORTER_getMeasure(map[len - 4]) > 0)
                {
                    len -= 3;
                    word[len] = '\0';
                }

                return len;
            }

            break;

        case 'S':
            /* (m>0) NESS  ->     */
            if (PORTER_endsWith(word, len, "NESS", 4))
            {
                if (len > 5 && PORTER_getMeasure(map[len - 5]) > 0)
                {
                    len -= 4;
                    word[len] = '\0';
                }

                return len;
            }

            break;
    }

    return len;
}

static inline int PORTER_step4(char *word, int len, uint8_t *map)
{
#ifdef DEBUG
    fprintf(stderr, "%s() -> '%s'\n", __func__, word);
#endif

    switch (word[len - 1])
    {
        case 'C':
            /* (m>1) IC    ->               */
            if (PORTER_endsWith(word, len, "IC", 2))
            {
                if (len > 4 && PORTER_getMeasure(map[len - 3]) > 1)
                {
                    len -= 2;
                    word[len] = '\0';
                }

                return len;
            }

            break;

        case 'E':
            /* (m>1) ANCE  ->               */
            if (PORTER_endsWith(word, len, "ANCE", 4))
            {
                if (len > 6 && PORTER_getMeasure(map[len - 5]) > 1)
                {
                    len -= 4;
                    word[len] = '\0';
                }

                return len;
            }

            /* (m>1) ENCE  ->               */
            if (PORTER_endsWith(word, len, "ENCE", 4))
            {
                if (len > 6 && PORTER_getMeasure(map[len - 5]) > 1)
                {
                    len -= 4;
                    word[len] = '\0';
                }

                return len;
            }

            /* (m>1) ABLE  ->               */
            if (PORTER_endsWith(word, len, "ABLE", 4))
            {
                if (len > 6 && PORTER_getMeasure(map[len - 5]) > 1)
                {
                    len -= 4;
                    word[len] = '\0';
                }

                return len;
            }

            /* (m>1) IBLE  ->               */
            if (PORTER_endsWith(word, len, "IBLE", 4))
            {
                if (len > 6 && PORTER_getMeasure(map[len - 5]) > 1)
                {
                    len -= 4;
                    word[len] = '\0';
                }

                return len;
            }

            /* (m>1) ATE   ->               */
            if (PORTER_endsWith(word, len, "ATE", 3))
            {
                if (len > 5 && PORTER_getMeasure(map[len - 4]) > 1)
                {
                    len -= 3;
                    word[len] = '\0';
                }

                return len;
            }

            /* (m>1) IVE   ->               */
            if (PORTER_endsWith(word, len, "IVE", 3))
            {
                if (len > 5 && PORTER_getMeasure(map[len - 4]) > 1)
                {
                    len -= 3;
                    word[len] = '\0';
                }

                return len;
            }

            /* (m>1) IZE   ->               */
            if (PORTER_endsWith(word, len, "IZE", 3))
            {
                if (len > 5 && PORTER_getMeasure(map[len - 4]) > 1)
                {
                    len -= 3;
                    word[len] = '\0';
                }

                return len;
            }

            break;

        case 'I':
            /* (m>1) ITI   ->               */
            if (PORTER_endsWith(word, len, "ITI", 3))
            {
                if (len > 5 && PORTER_getMeasure(map[len - 4]) > 1)
                {
                    len -= 3;
                    word[len] = '\0';
                }

                return len;
            }

            break;

        case 'L':
            /* (m>1) AL    ->               */
            if (PORTER_endsWith(word, len, "AL", 2))
            {
                if (len > 4 && PORTER_getMeasure(map[len - 3]) > 1)
                {
                    len -= 2;
                    word[len] = '\0';
                }

                return len;
            }

            break;

        case 'M':
            /* (m>1) ISM   ->               */
            if (PORTER_endsWith(word, len, "ISM", 3))
            {
                if (len > 5 && PORTER_getMeasure(map[len - 4]) > 1)
                {
                    len -= 3;
                    word[len] = '\0';
                }

                return len;
            }

            break;

        case 'R':
            /* (m>1) ER    ->               */
            if (PORTER_endsWith(word, len, "ER", 2))
            {
                if (len > 4 && PORTER_getMeasure(map[len - 3]) > 1)
                {
                    len -= 2;
                    word[len] = '\0';
                }

                return len;
            }

            break;

        case 'T':
            /* VCVC+"ANT" (7 char) is minimum for m>1 + "ANT" */
            if (len <= 7) return len;

            /* (m>1) ANT   ->               */
            if (PORTER_endsWith(word, len, "ANT", 3))
            {
                if (PORTER_getMeasure(map[len - 4]) > 1)
                {
                    len -= 3;
                    word[len] = '\0';
                }

                return len;
            }

            /* (m>1) EMENT ->               */
            if (PORTER_endsWith(word, len, "EMENT", 5))
            {
                if (PORTER_getMeasure(map[len - 6]) > 1)
                {
                    len -= 5;
                    word[len] = '\0';
                }

                return len;
            }

            /* (m>1) MENT  ->               */
            if (PORTER_endsWith(word, len, "MENT", 4))
            {
                if (PORTER_getMeasure(map[len - 5]) > 1)
                {
                    len -= 4;
                    word[len] = '\0';
                }

                return len;
            }

            /* (m>1) ENT   ->               */
            if (PORTER_endsWith(word, len, "ENT", 3))
            {
                if (len > 5 && PORTER_getMeasure(map[len - 4]) > 1)
                {
                    len -= 3;
                    word[len] = '\0';
                }

                return len;
            }

            break;

        case 'N':
            /* (m>1 and (*S or *T)) ION ->  */
            if (PORTER_endsWith(word, len, "ION", 3))
            {
                if (len > 6 && PORTER_getMeasure(map[len - 4]) > 1 &&
                     (word[len - 4] == 'S' || word[len - 4] == 'T'))
                {
                    len -= 3;
                    word[len] = '\0';
                }

                return len;
            }

            break;

        case 'S':
            /* (m>1) OUS   ->               */
            if (PORTER_endsWith(word, len, "OUS", 3))
            {
                if (len > 5 && PORTER_getMeasure(map[len - 4]) > 1)
                {
                    len -= 3;
                    word[len] = '\0';
                }

                return len;
            }

            break;

        case 'U':
            /* (m>1) OU    ->               */
            if (PORTER_endsWith(word, len, "OU", 2))
            {
                if (len > 4 && PORTER_getMeasure(map[len - 3]) > 1)
                {
                    len -= 2;
                    word[len] = '\0';
                }

                return len;
            }

            break;
    }

    return len;
}

static inline int PORTER_step5a(char *word, int len, uint8_t *map)
{
#ifdef DEBUG
    fprintf(stderr, "%s() -> '%s'\n", __func__, word);
#endif

    if (word[len - 1] != 'E') return len;
    if (len < 3) return len;

    /* (m>1) E     ->          */
    if (PORTER_getMeasure(map[len - 2]) > 1)
    {
        len -= 1;
        word[len] = '\0';
        return len;
    }

    /* (m=1 and not *o) E ->   */
    if (PORTER_endsCVC(map[len - 2]) == 0)
    {
        if (PORTER_getMeasure(map[len - 2]) == 1)
        {
            len -= 1;
            word[len] = '\0';
            return len;
        }
    }

    return len;
}

static inline int PORTER_step5b(char *word, int len, uint8_t *map)
{
#ifdef DEBUG
    fprintf(stderr, "%s() -> '%s'\n", __func__, word);
#endif

    /* (m > 1 and *d and *L) -> single letter  */
    if (word[len - 1] == 'L' && word[len - 2] == 'L')
    {
        if (PORTER_getMeasure(map[len - 1]) > 1)
        {
            len -= 1;
            word[len] = '\0';
        }
    }

    return len;
}

int PORTER_Stem(char *word)
{
    int len;
    uint8_t *map;

    len = strlen(word);
    map = alloca(len);
    memset(map, 0x00, len);

    /* check for valid length */
    if (len < 1 || len > 31) return -1;

    PORTER_Measure(word, map);
#ifdef DEBUG
    PORTER_DumpMap(word, map);
#endif

    len = PORTER_step1a(word, len, map);
    len = PORTER_step1b(word, len, map);
    len = PORTER_step1c(word, len, map);
    len = PORTER_step2(word, len, map);
    len = PORTER_step3(word, len, map);
    len = PORTER_step4(word, len, map);
    len = PORTER_step5a(word, len, map);
    PORTER_step5b(word, len, map);          /* don't keep final length... */

    return 0;
}
