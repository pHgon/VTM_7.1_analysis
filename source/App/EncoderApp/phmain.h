#ifndef PHMAIN_H
#define PHMAIN_H

#include <iostream>
#include <time.h>
using namespace std;

class ph{
public:
    ph();
    static void printSummary();
private:
    static clock_t encTimeStart;
};

#endif /* PHMAIN_H */

