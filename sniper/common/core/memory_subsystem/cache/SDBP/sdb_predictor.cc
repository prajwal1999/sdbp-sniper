#include <stdlib.h>
#include <math.h>
#include <cstring>
#include "sdb_predictor.h"
#include <assert.h>

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

    if(nsets == 4096) {
        dan_sampler_assoc = 13;
        dan_predictor_index_bits = 14;
    }
    int nbits_total = (nsets * assoc * 8 + 1024);
    int nbits_lru = assoc * nsets * (int) log2 (assoc);
    int nbits_predictor = dan_counter_width * dan_predictor_tables * dan_predictor_table_entries;
    int nbits_cache = 1 * nsets * assoc;
    int nbits_extra = 85;
    int nbits_left_over = nbits_total - (nbits_predictor + nbits_cache + nbits_lru + nbits_extra);
    int nbits_one_sampler_set = dan_sampler_assoc * (dan_sampler_tag_bits + 1 + 1 + 4 + dan_sampler_trace_bits);
    nsampler_sets = nbits_left_over / nbits_one_sampler_set;

    assert(nsampler_sets >= 0);


    sampler_modulus = nsets/nsampler_sets;
    pred = new predictor(); // make a new predictor
    sets = new sampler_set[nsampler_sets];
}

void sampler::access(int tid, int set, int tag, int PC) {
    sampler_entry *blocks = &sets[set].blocks[0]; // first sampler entry address in given set.
    unsigned int partial_tag = tag & ((1<<dan_sampler_tag_bits)-1); // get a partial tag to search for
    bool miss = false; // assume we do not miss
    int i; // this will be the way we will hit or replace
    // search for matching tag
    for(i=0; i<dan_sampler_assoc; i++) {
        if(blocks[i].valid && (blocks[i].tag == partial_tag)) {
            // we know this block is not dead; inform the predictor
            pred->block_is_dead(tid, blocks[i].trace, false);
        }
    }

    // did we find a match ?
    if(i == dan_sampler_assoc) {
        // no, so this is a miss in sampler
        miss = true;
        // look for invalid block to replace
        for (i=0; i<dan_sampler_assoc; i++) if(blocks[i].valid == false) break;
        // no invalid block ? look for deadblock
        if(i==dan_sampler_assoc) {
            // find lru dead block
            for(i=0; i<dan_sampler_assoc; i++) if(blocks[i].prediction) break;
        }
        // no invalid or dead block ? use lru block
        if(i == dan_sampler_assoc) {
            int j;
            for(j=0; j<dan_sampler_assoc; j++) {
                if(blocks[j].lru_stack_position == (unsigned int) (dan_sampler_assoc - 1)) break;
            }
            assert(j < dan_sampler_assoc);
            i = j;
        }

        // previous trace leads to block being dead; inform the predictor
        pred->block_is_dead(tid, blocks[i].trace, true);
        // fill the victim block
        blocks[i].tag = partial_tag;
        blocks[i].valid = true;
    }

    // record the trace
    blocks[i].trace = make_trace(tid, pred, PC);
    // get the next prediction for this entry
    blocks[i].prediction = pred->get_prediction(tid, blocks[i].trace, -1);
    // now the replaced entry should be moved to the MRU position
    unsigned int position = blocks[i].lru_stack_position;
    for(int way=0; way<dan_sampler_assoc; way++) {
        if(blocks[way].lru_stack_position < position) blocks[way].lru_stack_position++;
    }
    blocks[i].lru_stack_position = 0;

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