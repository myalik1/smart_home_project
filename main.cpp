#include <iostream>
#include "include/SmartLamp.h"

int main() {
    SmartLamp myLamp("Lamp1", "123456", "Lamp", "home/lamp1");

    myLamp.onMessageReceived("{\"command\": \"ON\"}");

    return 0;
}
