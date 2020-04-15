#ifndef PHMAIN_H
#define PHMAIN_H 1

// Minhas Diretivas
#define TRACE_CU 1
#define TRACE_INTER 0

#include <iostream>
#include <time.h>
using namespace std;

class ph {
public:
    ph();
    static void printSummary();
#if TRACE_CU || TRACE_INTER
    static void printTrace(string step, int level);
#endif
private:
    static clock_t encTimeStart;
#if TRACE_CU || TRACE_INTER
    static int trace_depth;
#endif

};

#endif /* PHMAIN_H */

