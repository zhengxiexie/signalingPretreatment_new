#ifndef _SSANALYZE_H
#define _SSANALYZE_H

/*
#ifndef _TIME_T
#define _TIME_T
typedef long long time_t;
#endif
*/

#include "sspublic.h"
#include <sys/types.h>
#include <regex.h>
#include <map>
#include <set>
#include <string>

using namespace std;

typedef struct {
    char    file_prefix[17];
    int     time_pos;
    char    time_start[15];
    char    time_stop[15];
} File_Filter;

enum Filter_Match {
    fm_Prefix = 0,
    fm_Suffix = 1, 
    fm_Word   = 2
};

enum Filter_Value {
    fv_CSV    = 0, 
    fv_Range  = 1,
    fv_Regex  = 2
};

typedef struct {
    Filter_Match fm;
    Filter_Value fv;
    int     num;
    char    **value;
    regex_t *reg;
    int     field;
    bool    available;
    // action 表示过滤器行为，true 表示value内的值可通过
    //                       flase 表示value内值不可通过
    bool    action;
} Filter_Rule;

enum rpr_Type {
    rp_Normal    = 0,
    rp_Recursive = 1
};

struct Remove_Prefix_Rule {
    bool       available;
    int       *field;
    int        num_field;
    char     **prefix;
    int       *len_prefix;
    int        num_prefix;
    rpr_Type   type;
};

struct File_Info {
    char  filename[200];
    int   port;
    int   len_prefix;
};

extern int num_of_inter_field;

void *consume( void *arg);
void read_decode_info();
void *read_nonparticipant_data(void *arg);
int WriteResult(int ruleid, char *src);
int filter(char* word[], Filter_Rule* sfltr, int num_sfltr);
unsigned long int yyyymmddhhmmss_to_sec(char* time);
int get_num_field(char* line, char splt);
int split_value(char** fltr, char* line, char splt);
int strcomp(const void* str1, const void* str2);
int strrcomp(const void* str1, const void* str2);
int strrncmp(const char* str1, const char* str2, unsigned long n);
int strrcmp(const char* str1, const char* str2);
bool isTimeBetween(const char* time, const char* start, const char* stop);
int load_XML_config(char* file, XMLReader* cfgXML);
int strcmp1(const char* str1, const char* str2, unsigned long dummy);
void *statistics_thread(void* dummy);
int comp_File_Info(const void* file1, const void* file2);
int remove_prefix(char* word[], Remove_Prefix_Rule *rule);
#endif

