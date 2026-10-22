#pragma once
#include <Arduino.h>
#include "game/pet.h"

void net_begin(PetState* pet);
void net_loop();
bool net_ap_mode();
String net_ip();
