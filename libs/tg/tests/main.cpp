#include <stdlib.h>
#include <tl/basic.hpp>
#include <tl/str.hpp>
#include <tl/fmt.hpp>

//bool test_segmentIntersect();
bool test_cylinderMapToCubeMap();
bool test_cubemap();

struct TestInfo {
    CStr name;
    bool (*fn)();
};

static TestInfo tests[] = {
    {"cylinderMapToCubeMap", test_cylinderMapToCubeMap},
    {"cubemap", test_cubemap}
};

static char scratchStr[1024];

int main(int argc, char* argv[])
{
    const int numTests = tl::size(tests);
    CStr selectedTestStr;
    if(argc >= 2) {
        selectedTestStr = argv[1];
    }
    else {
        for(int i = 0; i < numTests; i++)
            tl::println(i, ") ", tests[i].name);
        fgets(scratchStr, tl::size(scratchStr), stdin);
        selectedTestStr = scratchStr;
    }

    int selectedTest = -1;
    if(selectedTestStr[0] >= '0' && selectedTestStr[0] <= '9')
        selectedTest = atoi(selectedTestStr);
    else {
        for(int i = 0; i < numTests; i++)
        if(tests[i].name == selectedTestStr) {
            selectedTest = i;
            break;
        }
    }

    if(selectedTest >= 0 && selectedTest < numTests)
        tests[selectedTest].fn();
    else
        tl::println("invalid test name or number");
}
