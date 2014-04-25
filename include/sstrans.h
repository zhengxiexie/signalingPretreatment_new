#ifndef _SSTRANS_H
#define _SSTRANS_H

/*
#ifndef _TIME_T
#define _TIME_T
typedef long long time_t;
#endif
*/

#include "sspublic.h"
#include <time.h>

#define NUM_TOTAL_EVENT  25
//#include <sys/times.h>
using namespace std;

typedef struct{
	char   date_type[10];
	int    length;
	bool   can_be_null;
	bool   numeral;
	//char min_time[21];
    time_t min_time; 
	//char max_time[21];
    time_t max_time;
	time_t time_before;
	time_t time_delay;
	int    min_value;
	int    max_value;
	char   value[50];
} XMLCheck;

enum Port_Type {
    pt_CS        = 0,
    pt_PS        = 1,
    pt_Undefined = 99,
};

struct Trans_cfg {
    bool      available;
    char      filename_prefix[20];
    char      field_spliter;
    Port_Type pt;
    int       timestamp_begin;
    int       timestamp_length;
    int       filenum_length; 
    int       fmt_orig;
    int       fmt_after;
    int       fmt_available;
    int       *fmt_order;
    bool      evt_map_available;
    int       evt_idx;
    int       num_evt_map;
    char      ***evt_map;
    char      soe_event[4];
    int       soe_field[2];
};
typedef struct Trans_cfg Trans_cfg;
void line_add_filename(char * line,char *filename);

int split(char *str,char split_chr,char **res);

bool trans_line(char * line, time_t file_time_t, Trans_cfg *tcfg);

int split_value_to_int(int* dest, const char* line, char splt);

int check_file_port(char* fileName, Trans_cfg *tcfg, int num_tcfg);

//void init_file_type(char* fileName);

void init_file_info(char* fileName, Trans_cfg *tcfg, char file_no[], char str_file_time[], time_t &filetime);

int get_XML_config(char* filename);
int strcomp1(const void* str1, const void* str2);
int get_trans_config(XMLReader *xml, Trans_cfg *tcfg);
int get_check_config();

//extern char **tar;
//extern char ler_file_type[10];
//extern char file_no[5];
//extern time_t file_time;	
extern int inter_column;
#endif

