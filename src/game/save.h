#pragma once
#include <Arduino.h>
#include "pet.h"

bool save_load(PetState& p, uint32_t now);   // 读不到/校验失败 -> 新个体
void save_store(PetState& p);
void save_wipe();

bool save_wifi_load(String& ssid, String& pass);
void save_wifi_store(const String& ssid, const String& pass);
void save_wifi_wipe();
