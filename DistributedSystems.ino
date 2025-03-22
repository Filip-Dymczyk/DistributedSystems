#include "Arduino.h"
#include "include/tasks_manager.h"

TasksManager tasks_manager {};

void
setup()
{
    Serial.begin(9600);
    tasks_manager.run();
}

void
loop()
{
}