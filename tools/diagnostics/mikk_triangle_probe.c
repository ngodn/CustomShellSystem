/* C17 diagnostic linked against the installed UE MikkTSpace library. */
#include <stdio.h>
#include <stdlib.h>
#include "mikktspace.h"

static float corners[3][8];
static int count(const SMikkTSpaceContext *context) { return 1; }
static int vertices(const SMikkTSpaceContext *context, int face) { return 3; }
static void position(const SMikkTSpaceContext *context, float *out, int face, int vertex)
{
    for (int i = 0; i < 3; ++i) out[i] = corners[vertex][i];
}
static void normal(const SMikkTSpaceContext *context, float *out, int face, int vertex)
{
    for (int i = 0; i < 3; ++i) out[i] = corners[vertex][i + 3];
}
static void uv(const SMikkTSpaceContext *context, float *out, int face, int vertex)
{
    for (int i = 0; i < 2; ++i) out[i] = corners[vertex][i + 6];
}
static void tangent(const SMikkTSpaceContext *context, const float *value,
                    float sign, int face, int vertex)
{
    printf("%d %.9g %.9g %.9g %.9g\n", vertex, value[0], value[1], value[2], sign);
}
int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    for (int v = 0; v < 3; ++v)
        for (int i = 0; i < 8; ++i)
            if (scanf("%f", &corners[v][i]) != 1) return 3;
    SMikkTSpaceInterface api = {count, vertices, position, normal, uv, tangent, NULL};
    SMikkTSpaceContext context = {&api, NULL, atoi(argv[1])};
    return genTangSpaceDefault(&context) ? 0 : 4;
}
