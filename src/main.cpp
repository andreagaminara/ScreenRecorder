#include <iostream>
#include "../ScreenRecorder.h"
using namespace std;

/* driver function to run the application */
int main()
{
    ScreenRecorder screen_record;

    try {
        screen_record.capture();
    }
    catch (ScreenRecorderException e) {
        std::cout << e.what() << std::endl;
        exit(-1);
    }
    

    return 0;
}
