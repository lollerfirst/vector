#include "src/vector.h"
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

int main(void){
    Vector vector;
    if(vector_init(&vector, 128, sizeof(int)) == -1){
        fprintf(stderr, "%s", "vector_init failed\n");
        return EXIT_FAILURE;
    }

    int f = 10;
    int g = 20;
    vector_pushb(&vector, &f);
    vector_pushb(&vector, &g);

    vector_popb(&vector, &f);
    vector_popb(&vector, &g);
    printf("[pushb-popb]\nf: %d\ng: %d\n", f, g);

    vector_pushf(&vector, &f);
    vector_pushf(&vector, &g);

    vector_popf(&vector, &f);
    vector_popf(&vector, &g);

    printf("[pushf-popf]\nf: %d\ng: %d\n", f, g);

    static char string[2048];
    memset(string, 'C', sizeof(string)-1);  //stored in the data segment.
    string[2047] = '\0';
    
    Vector vector2;
    vector_init(&vector2, 4u, sizeof(string));
    vector_pushb(&vector2, string);
    vector_pushb(&vector2, string);
    vector_pushb(&vector2, string);
    vector_pushb(&vector2, string);
    vector_pushb(&vector2, string);
    vector_pushb(&vector2, string);

    fprintf(stdout, "%s\n[END]\n", (char*)vector_at(&vector2, 0));
    return 0;
}