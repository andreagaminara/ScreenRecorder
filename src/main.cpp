#include <iostream>

#ifdef _WIN32
    #include "../ScreenRecorder.h"
#elif __linux__
    #include "../include/ScreenRecorder.h"
#endif

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
