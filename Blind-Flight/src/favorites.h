#pragma once

#include <Arduino.h>

#define FAV_MAX_COUNT 30

void favoritesInit();
uint8_t favoritesGetCount();
const char* favoritesGetName(int index);
bool favoritesAdd(const char* name);
bool favoritesRemove(int index);
bool favoritesContains(const char* name);
