/*
 * stb_image_impl.c
 * Compile stb_image once here so every other .c file can just #include "stb_image.h"
 * Download stb_image.h from: https://raw.githubusercontent.com/nothings/stb/master/stb_image.h
 */
#define STB_IMAGE_IMPLEMENTATION
#define STBI_WINDOWS_UTF8        /* enable UTF-8 path support on Windows */
#include "stb_image.h"
