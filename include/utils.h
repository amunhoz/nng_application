#include <stdlib.h>
#include <string.h>
#include <random>

char * gen_random_string( const int len) {
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "!@#$%&*()_-=+{[}]/?:;<,>.|"
        ;
    char *s = (char*)malloc(len * sizeof(char));
    for (int i = 0; i < len; ++i) {
        s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
    }
    s[len] = 0;
    return s;
}

std::mt19937 urbg(std::random_device{}());
uint32_t gen_random_number()
{
    return  urbg();    
}

