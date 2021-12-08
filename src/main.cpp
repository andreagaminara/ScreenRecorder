#include <iostream>
#include "../include/ScreenRecorder.h"
using namespace std;

/* driver function to run the application */
int main()
{
    ScreenRecorder screen_record;

    screen_record.capture();

    return 0;
}