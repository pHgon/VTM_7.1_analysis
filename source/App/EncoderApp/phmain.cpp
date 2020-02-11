#include "phmain.h"


clock_t ph::encTimeStart;


ph::ph(){
    encTimeStart = clock();
}

void ph::printSummary(){
    cout << endl << "---------------------------------------------------------------------" << endl;
    cout << "Encoding Time = " << (double) (clock()-encTimeStart) / CLOCKS_PER_SEC << endl;
    cout << "---------------------------------------------------------------------" << endl;
}