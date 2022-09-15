

/* @author Jiayi Wang
 *
 * @file simulate the behavior of cache, given
 * value for s, E, and b in the command line;
 *
 * @brief use array of  block_t (a self designed struct)
 * to represent each cache block,
 * the array is a one dimensional array,
 * block_t contains v bit, d bit, tag, and order,
 * increase order field in struct to implement LRU policy */

#include "cachelab.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SHIFT 63

/* define block struct to store v-bit, d-bit,tag, and LRU order */
typedef struct {
    int valid_bit; /* 0 = non-valid, 1 = valid */
    int dirty_bit; /* 0 = non-dirty, 1 = dirty */
    long tag;      /* leftmost bits of the address */
    int order;     /* min = most recently used */
} block_t;

/* update cache for load instruction
 * check hit
 * check miss
 * check eviction */
void eval_load(long tag, long set, long line, block_t *b_set,
               csim_stats_t *stat);

/* update cache for store instruction
 * check hit
 * check miss
 * check eviction */
void eval_store(long tag, long set, long line, block_t *b_bset,
                csim_stats_t *stat);

/* return: set_size-th power of 2 */
int set_power(long set_size);

/* main function
 * takes in a command line as arguement
 * parse the command line to initialize the cache
 * call different functions when parsing the trace file */
int main(int argc, char **argv) {

    /* initialize a pointer to the struct to store summary
     * and initialize every field as 0 */
    csim_stats_t *stat = malloc(sizeof(csim_stats_t));
    stat->hits = 0;
    stat->misses = 0;
    stat->evictions = 0;
    stat->dirty_bytes = 0;
    stat->dirty_evictions = 0;

    /* parse the command line and create an array of block struct
     * to represent cache blocks */
    int set_size = 0;
    int block_size = 0;
    int line_size = 0;
    char *p_file;
    int opt;
    while ((opt = getopt(argc, argv, "b:E:s:t:")) != -1) {
        switch (opt) {
        case 's':
            set_size = atoi(optarg);
            break;
        case 'E':
            line_size = atoi(optarg);
            break;
        case 'b':
            block_size = atoi(optarg);
            break;
        case 't':
            p_file = optarg;
            break;
        }
    }
    int set_number = set_power(set_size);
    int byte_number = set_power(block_size);

    /* create an array of block struct with the values
     * indicated by the command line
     * initialize all value to zero */
    unsigned long array_size =
        (unsigned long)((long)set_number * (long)line_size);

    block_t *b_set = calloc(array_size, sizeof(block_t));
    if (b_set == NULL)
        exit(1);

    for (int i = 0; i < set_number; i++) {
        for (int j = 0; j < line_size; j++) {
            int cur_idx = i * line_size + j;
            b_set[cur_idx].order = 0;
            b_set[cur_idx].valid_bit = 0;
            b_set[cur_idx].dirty_bit = 0;
        }
    }

    /* read file lines and update cache info */
    FILE *t_file;
    t_file = fopen(p_file, "r");

    char access_type;
    long address;
    int size;

    char load = 'L';
    while (fscanf(t_file, " %c %lx, %d", &access_type, &address, &size) > 0) {

        // get the set bit and the tag bit
        long set_at_right = address >> block_size;
        long set_mask = ~(((long)1 << SHIFT) >> (SHIFT - set_size));
        long set_num = set_mask & set_at_right;
        long tag_at_right = set_at_right >> set_size;
        long tag_mask = ~(((long)1 << SHIFT) >> (set_size + block_size));
        long tag_num = tag_mask & tag_at_right;

        if (access_type == load) {
            /* if operation is Load */
            eval_load(tag_num, set_num, line_size, b_set, stat);
        } else {
            /* if operation is store */
            eval_store(tag_num, set_num, line_size, b_set, stat);
        }
    }

    fclose(t_file);

    // change #of data into bytes of data
    stat->dirty_bytes *= (unsigned)(long)byte_number;
    stat->dirty_evictions *= (unsigned)(long)byte_number;
    printSummary(stat);
    free(b_set);
    free(stat);
    return 0;
}

/* search for hit and update stat;
 * search for empty block and update stat;
 * search for eviction by LRU */
void eval_load(long tag_num, long set, long line, block_t *b_set,
               csim_stats_t *stat) {

    /* loop over all LRU to find the min */
    int min_order = b_set[set * line].order;
    int max_order = b_set[set * line].order;
    for (int i = 0; i < line; i++) {
        long cur_index = set * line + i;
        if (b_set[cur_index].order < min_order) {
            min_order = b_set[cur_index].order;
        }
        if (b_set[cur_index].order > max_order) {
            max_order = b_set[cur_index].order;
        }
    }
    /* loop over the current line
     * if there is a valid with same tag, hit and return */

    for (int i = 0; i < line; i++) {
        long cur_index = set * line + i;
        if (b_set[cur_index].tag == tag_num &&
            b_set[cur_index].valid_bit == 1) {
            stat->hits = stat->hits + 1;
            b_set[cur_index].order = max_order + 1;
            return;
        }
    }
    // if there is no hit, there should be a miss
    stat->misses = stat->misses + 1;

    // find a place to load
    for (int i = 0; i < line; i++) {
        long cur_index = set * line + i;
        if (b_set[cur_index].valid_bit == 0) {
            b_set[cur_index].tag = tag_num;
            b_set[cur_index].valid_bit = 1;
            b_set[cur_index].order = max_order + 1;
            return;
        }
    }

    // find one eviction, the row is full
    stat->evictions += 1;
    for (int i = 0; i < line; i++) {
        long cur_index = set * line + i;
        if (b_set[cur_index].order == min_order) {
            b_set[cur_index].tag = tag_num;
            b_set[cur_index].order = max_order + 1;
            if (b_set[cur_index].dirty_bit == 1) {
                stat->dirty_evictions += 1;
                stat->dirty_bytes -= 1;
                b_set[cur_index].dirty_bit = 0;
            }
            return;
        }
    }
}

/* search for hit and update stat;
 * search for empty block and update stat;
 * search for eviction by LRU */
void eval_store(long tag_num, long set, long line, block_t *b_set,
                csim_stats_t *stat) {

    int min_order = b_set[set * line].order;
    int max_order = b_set[set * line].order;
    for (int i = 0; i < line; i++) {
        long cur_index = set * line + i;
        if (b_set[cur_index].order < min_order) {
            min_order = b_set[cur_index].order;
        }
        if (b_set[cur_index].order > max_order) {
            max_order = b_set[cur_index].order;
        }
    }

    // loop over to look for not valid bits
    for (int i = 0; i < line; i++) {
        long cur_index = set * line + i;
        if ((b_set[cur_index].valid_bit == 1) &&
            (b_set[cur_index].tag == tag_num)) {
            stat->hits = stat->hits + 1;
            b_set[cur_index].order = max_order + 1;
            if (b_set[cur_index].dirty_bit == 0) {
                stat->dirty_bytes += 1;
                b_set[cur_index].dirty_bit = 1;
            }
            return;
        }
    }
    // if no hit, there is a miss
    stat->misses = stat->misses + 1;

    // find a place to store
    for (int i = 0; i < line; i++) {
        long cur_index = set * line + i;
        if (b_set[cur_index].valid_bit == 0) {
            b_set[cur_index].valid_bit = 1;
            b_set[cur_index].tag = tag_num;
            b_set[cur_index].dirty_bit = 1;
            b_set[cur_index].order = max_order + 1;
            stat->dirty_bytes += 1;
            return;
        }
    }

    // if line is full, find the eviction
    stat->evictions += 1;

    for (int i = 0; i < line; i++) {
        long cur_index = set * line + i;
        if (b_set[cur_index].order == min_order) {
            b_set[cur_index].tag = tag_num;
            b_set[cur_index].order = max_order + 1;
            stat->dirty_bytes += 1;
            if (b_set[cur_index].dirty_bit == 1) {
                stat->dirty_evictions += 1;
                stat->dirty_bytes -= 1;
            } else {
                b_set[cur_index].dirty_bit = 1;
            }
            return;
        }
    }
    return;
}

/* calculate number of sets given a set size */
int set_power(long set_size) {
    int counter = 1;
    while (0 < set_size) {
        counter = counter * 2;
        set_size--;
    }
    return counter;
}
