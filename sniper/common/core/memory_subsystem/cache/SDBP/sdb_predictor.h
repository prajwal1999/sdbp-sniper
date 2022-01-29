#ifndef SDB_PREDICTOR_H
#define SDB_PREDICTOR_H

#include <cstdlib>
#include <stdint.h>



struct sampler_entry {
    unsigned int lru_stack_position, tag, trace, prediction;
    bool valid;

    sampler_entry(void) {
        lru_stack_position = 0;
        valid = false;
        tag = 0;
        trace = 0;
        prediction = 0;
    };
};


// this stores the pointer to array of ways in the sampler set. ie. 12
struct sampler_set {
    sampler_entry *blocks;
    sampler_set(void);
};


struct predictor {
    int **tables; // tables of two bit counters. say, 3 tables with 2^16 entries
    predictor(void);
    unsigned int get_table_index(int tid, unsigned int trace, int t);
    void block_is_dead (int tid, unsigned int trace, bool d);
    bool get_prediction (int tid, unsigned int trace, int set);
};


struct sampler {
    sampler_set *sets;
    int nsampler_sets, sampler_modulus;
    predictor *pred;
    sampler (int nsets, int assoc);
    void access (int tid, int set, int tag, int PC);
};




// ######################################################################################################
unsigned int mix (unsigned int a, unsigned int b, unsigned int c);
unsigned int f1 (unsigned int x);
unsigned int f2 (unsigned int x);
unsigned int fi (unsigned int x, int i);
unsigned int make_trace (int tid, predictor *pred, int PC);




#endif