#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "ssmsisdn.h"

#define PRIME_1 13997
#define PRIME_2 127

#define MSISDN_MAX ((int)(500000))

#define MSISDN_HASH_1_MAX ((int)(MSISDN_MAX * 1.2))
#define MSISDN_HASH_2_MAX ((int)(MSISDN_MAX * 0.7))
#define MSISDN_HASH_C_MAX ((int)(MSISDN_MAX * 0.4))


struct _MSISDN_TABLE {
    unsigned int prefix;
    short city_code;
    short prov_code;
    short local_flag; 
};
int msisdn_records;
typedef struct _MSISDN_TABLE _MSISDN_TABLE;

struct _MSISDN_TABLE msisdn_table[MSISDN_MAX];
int msisdn_table_inited = 0;
int msisdn_lines = 0;

unsigned int msisdn_hash1[MSISDN_HASH_1_MAX];
unsigned int msisdn_hash2[MSISDN_HASH_2_MAX];
unsigned int msisdn_hashc[MSISDN_HASH_C_MAX];

static int _create_msisdn_index(void);
static unsigned int _msisdn_hash_func1(unsigned int msisdn);
static unsigned int _msisdn_hash_func2(unsigned int msisdn);

int read_msisdn_table_from_file(const char * const filename) {
    FILE * file = NULL;
    int line = 1;
    int ret  = 0;
    int func_ret = 0;
    unsigned int prefix = 0;
    short city_code = 0;
    short prov_code = 0;
    short type = 0;

    file = fopen(filename, "r");
    if (!file) return MSISDN_ERFILE;
    memset(msisdn_table, 0, sizeof(_MSISDN_TABLE)); 

    while (fscanf(file, "%u,%hd,%hd,%hd\n", &prefix, &type, &city_code, &prov_code) != EOF) {
        msisdn_table[line].prefix     = prefix;
        msisdn_table[line].city_code  = city_code;
        msisdn_table[line].prov_code  = prov_code;
        msisdn_table[line].local_flag = type;
        line++;
        //printf(" hash: %u,%u\n", _msisdn_hash_func1(prefix), _msisdn_hash_func2(prefix));
        if (line >= MSISDN_MAX) {
            func_ret = MSISDN_ELARGE;
            break;
        }
    }
    
    msisdn_lines = line;
    ret = _create_msisdn_index();
    if (ret) return MSISDN_EINDEX;
    
    return func_ret;
}

static int _create_msisdn_index() {
    unsigned int hash = 0;
    unsigned int i = 0;
    unsigned int conflicted = 0;
    int in_hash1 = 0, in_hash2 = 0;

    memset(msisdn_hash1, 0, sizeof(unsigned int) * MSISDN_HASH_1_MAX);
    memset(msisdn_hash2, 0, sizeof(unsigned int) * MSISDN_HASH_2_MAX);
    memset(msisdn_hashc, 0, sizeof(unsigned int) * MSISDN_HASH_C_MAX);

    for (i = 1; i < msisdn_lines + 1; i++) {
        hash = _msisdn_hash_func1(msisdn_table[i].prefix);
        if (msisdn_hash1[hash] == 0) { // in hash_table 1
            msisdn_hash1[hash] = i;
            ++in_hash1;
        } else {
            hash = _msisdn_hash_func2(msisdn_table[i].prefix);
            if (msisdn_hash2[hash] == 0) { // in hash_table 2
                msisdn_hash2[hash] = i;
                ++in_hash2;
            } else {
                if (conflicted >= MSISDN_HASH_C_MAX) printf("conflicted full, ignore\n");
                else msisdn_hashc[conflicted++] = i;
            }
        }
    }
    
    printf("%d, %d, %d, ", in_hash1, in_hash2, conflicted);
    msisdn_table_inited = 1;
    return 0;
}

int msisdn_prefix_find( unsigned int msisdn_int, 
                        short * city_code, short * prov_code, short * local_flag) {
    int hash = 0;
    int idx = 0;

    hash = _msisdn_hash_func1(msisdn_int);
    if (msisdn_int == msisdn_table[msisdn_hash1[hash]].prefix) {
        idx = msisdn_hash1[hash];
    } else { 
        hash = _msisdn_hash_func2(msisdn_int);
        if (msisdn_int == msisdn_table[msisdn_hash2[hash]].prefix) { 
            idx = msisdn_hash2[hash];
        } else {
            hash = 0;
            while (msisdn_table[msisdn_hashc[hash]].prefix != 0 
                && msisdn_table[msisdn_hashc[hash]].prefix != msisdn_int) ++hash; 
            idx = msisdn_hashc[hash];
        }
    }

    if (idx == 0) {
        return 1;
    } else {
        *city_code  = msisdn_table[idx].city_code;
        *prov_code  = msisdn_table[idx].prov_code;
        *local_flag = msisdn_table[idx].local_flag;
    }

    return 0;
}

static unsigned int _msisdn_hash_func1(unsigned int msisdn) {
    return (unsigned int)((msisdn % 100000) + (msisdn / 100000 - 10) / 4 * 100000) % MSISDN_HASH_1_MAX;
}

static unsigned int _msisdn_hash_func2(unsigned int msisdn) {
    return (unsigned int)(msisdn * PRIME_2) % MSISDN_HASH_2_MAX;
}

#ifdef DEBUG
int main (int argc, char * argv[]) {
    int ret = 0;
    unsigned int msisdn = (unsigned int) atoi(argv[1]);
    short city_code;
    short prov_code;
    short local_flag;

    ret = read_msisdn_table_from_file("sa_msisdn.dat");
    ret = msisdn_prefix_find(argv[1], &city_code, &prov_code, &local_flag);
    
    printf ("msisdn: %u, city: %hd, prov: %hd, local: %hd\n", msisdn, city_code, prov_code, local_flag);
    return ret;
}
#endif


