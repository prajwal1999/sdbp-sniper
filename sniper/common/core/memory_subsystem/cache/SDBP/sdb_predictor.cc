#include <stdlib.h>
#include <math.h>
#include <cstring>
#include "sdb_predictor.h"

int dan_sampler_assoc = 12;
int dan_sampler_trace_bits = 16;
int dan_sampler_tag_bits = 16;
int dan_predictor_tables = 3;
int dan_predictor_index_bits = 12;
int dan_predictor_table_entries = 1 << dan_predictor_index_bits;
int dan_counter_width = 2;
int dan_counter_max = (1 << dan_counter_width) - 1;
int dan_threshold = 8;

sampler_set::sampler_set (void) {
    blocks = new sampler_entry[dan_sampler_assoc];
    // initialize LRU replacement algorithm
    for (int i=0; i<dan_sampler_assoc; i++) blocks[i].lru_stack_position = i; // 0 means latest used
}

sampler::sampler(int nsets, int assoc) {
    
    nsampler_sets = 1024;
    sampler_modulus = 6;
    pred = new predictor(); // make a new predictor
    sets = new sampler_set[nsampler_sets];
}

void sampler::access(int tid, int set, int tag, int PC) {
    // sampler_entry *blocks = &sets[set].blocks[0]; // first sampler entry address in given set.
    // unsigned int partial_tag = tag & ((1<<dan_sampler_tag_bits)-1);
}



// ################################################################################################################################

predictor::predictor(void) {
    tables = new int* [dan_predictor_tables]; // array of pointers to table, length 3
    for(int i=0; i<dan_predictor_tables; i++) {
        tables[i] = new int[dan_predictor_table_entries];
        memset(tables[i], 0, sizeof(int)*dan_predictor_table_entries);
    }
    // 000
    // 000
    // 000
    // 000
    // 000
    // .
    // .
    // .
}

unsigned int predictor::get_table_index(int tid, unsigned int trace, int t) { // hash thread id, trace and table number
    unsigned int x = fi (trace ^ (tid << 2), t);
    return x & ((1<<dan_predictor_index_bits)-1);
}

void predictor::block_is_dead (int tid, unsigned int trace, bool d) {
    for (int i=0; i<dan_predictor_tables; i++) {
        int *c = &tables[i][get_table_index(tid, trace, i)]; // address of corres. entry in table
        if(d) {
            if(*c < dan_counter_max) (*c)++;
        } else { // else, decrease the counter
            if(i&1) (*c) >>= 1; // odd numbered tables decrease exponentially. why ?
            else {
                if (*c > 0) (*c)--; // even numbered tables decrease by one.
            }
        }
    }
}

bool predictor::get_prediction (int tid, unsigned int trace, int set) {
    int confidence = 0;
    for (int i=0; i<dan_predictor_tables; i++) {
        int val = tables[i][get_table_index(tid, trace, i)];
        confidence += val;
    }
    return confidence >= dan_threshold;
}


// ######################################################################################################
unsigned int mix (unsigned int a, unsigned int b, unsigned int c) {
    a=a-b; a=a-c; a=a^(c >> 13);
    b=b-c; b=b-a; b=b^(a >> 8);
    c=b-a; c=c-b; c=c^(b >> 13);
    return c;
};

unsigned int f1 (unsigned int x) {
    return mix (0xfeedface, 0xdeadb10c, x);
};

unsigned int f2 (unsigned int x) {
    return mix (0xc001d00d, 0xfade2b1c, x);
};

unsigned int fi (unsigned int x, int i) {
    return f1(x) + (f2(x) >> i);
};

unsigned int make_trace (int tid, predictor *pred, int PC) {
    return PC & ((1<<dan_sampler_trace_bits)-1);
};