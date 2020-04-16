#include "phmain.h"


clock_t ph::enc_time_start;
string ph::enc_type_names[19]= {"ETM_HASH_INTER", "ETM_MERGE_SKIP", "ETM_INTER_ME", "ETM_AFFINE", "ETM_MERGE_TRIANGLE",
            "ETM_INTRA", "ETM_PALETTE", "ETM_SPLIT_QT", "ETM_SPLIT_BT_H", "ETM_SPLIT_BT_V", "ETM_SPLIT_TT_H",
            "ETM_SPLIT_TT_V", "ETM_POST_DONT_SPLIT", "REUSE_CU_RESULTS", "ETM_RECO_CACHED", "ETM_TRIGGER_IMV_LIST",
            "ETM_IBC", "ETM_IBC_MERGE", "ETM_INVALID"};

#if TRACE_CU || TRACE_INTER || TRACE_PARTITIONING
int ph::trace_depth;
#endif

ph::ph() {
    enc_time_start = clock();
#if TRACE_CU || TRACE_INTER || TRACE_PARTITIONING
            trace_depth = 1; // Sempre inicia retornando a 0
#endif
}

void ph::printSummary() {
    cout << endl << "---------------------------------------------------------------------" << endl;
    cout << "Encoding Time = " << (double) (clock() - enc_time_start) / CLOCKS_PER_SEC << endl;
    cout << "---------------------------------------------------------------------" << endl;
}

string ph::getEncTypeName(int index){
    return enc_type_names[index];
}

#if TRACE_CU || TRACE_INTER || TRACE_PARTITIONING
void ph::printTrace(string step, int level) {
    switch(level){
        case 0:
            level = -1;
            cout << endl;
            break;
        case 1:
            for (auto i = 0; i < trace_depth; i++) { cout << "\t"; }
            cout << step << endl;
        case -1:
            break;
        default:       // NAO DEVE CHEGAR AQUI
            return;
    } 
    trace_depth += level;
}
#endif