#include "phmain.h"


clock_t ph::encTimeStart;

#if TRACE_CU || TRACE_INTER
int ph::trace_depth;
#endif


ph::ph(){
    encTimeStart = clock();
#if TRACE_CU || TRACE_INTER
    trace_depth = 0;
#endif
}

void ph::printSummary(){
    cout << endl << "---------------------------------------------------------------------" << endl;
    cout << "Encoding Time = " << (double) (clock()-encTimeStart) / CLOCKS_PER_SEC << endl;
    cout << "---------------------------------------------------------------------" << endl;
}

#if TRACE_CU || TRACE_INTER
void ph::printTrace(string step, int level){
    for (auto i=0; i<trace_depth+(level==-1 ? level : 0); i++) { cout << "|  ";} 
    cout << step << ((level<1) ? " (END)" : " (INI)") << endl;
    
    trace_depth += level;
}
#endif