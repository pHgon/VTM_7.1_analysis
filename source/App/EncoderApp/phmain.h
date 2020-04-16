#ifndef PHMAIN_H
#define PHMAIN_H 1

// Minhas Diretivas
#define TRACE_PARTITIONING 1
#define TRACE_CU 1
#define TRACE_INTER 1

#include <iostream>
#include <time.h>
using namespace std;

class ph {
    
public:
    ph();
    static void printSummary();
    static string getEncTypeName(int index);
#if TRACE_CU || TRACE_INTER || TRACE_PARTITIONING
    static void printTrace(string step, int level);
#endif
    
    
private:
    static clock_t enc_time_start;
    static string enc_type_names[19];
#if TRACE_CU || TRACE_INTER || TRACE_PARTITIONING
    static int trace_depth;
#endif

};

#endif /* PHMAIN_H */

