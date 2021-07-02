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

    vector_free(&vector);

    static char string[2048];
    memset(string, 'C', sizeof(string)-1);
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

    vector_free(&vector2);

    int i;

    Vector vector4;
    const int arr[10] = {10, 20, 30, 10, 70};
    if ((i = vector_init_arr(&vector4, arr, 5, sizeof(int))) == -1)  return EXIT_FAILURE;
    fprintf(stdout, "Vector4 (array initialization): %d %d %d %d \n", *(int*)vector_at(&vector4, 0), *(int*)vector_at(&vector4, 1), *(int*)vector_at(&vector4, 2), *(int*)vector_at(&vector4, 3));
    fprintf(stdout, "sizeof(arr): %ld \n", sizeof(arr));

    vector_free(&vector4);

    Vector vector3;
    const int a = 10;
    const int b = 19;
    const int c = 100;
    const int d = 400;
    
    if ((i = vector_init_list(&vector3, 10, sizeof(int), &a, &b, &c, &d)) == -1) return EXIT_FAILURE;
    fprintf(stdout, "Vector3 (list initialization): %d %d %d %d \n", *(int*)vector_at(&vector3, 0), *(int*)vector_at(&vector3, 1), *(int*)vector_at(&vector3, 2), *(int*)vector_at(&vector3, 3));

    return 0;
}