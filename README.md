# porter
Porter Stemming Algorithm

This implementation is known not to match the canonical implementation,
but it can be argued that it matches the specification more correctly.
Specifically, the cases:

(this impl) | (porter)
          S | 
         CS | C
         ES | E
         EY | EI
         YS | Y

In the above cases, 'C' represents a consonant, 'E' represents a vowel,
and 'Y' represents the special letter 'Y'.

All other cases have been thoroughly tested.
