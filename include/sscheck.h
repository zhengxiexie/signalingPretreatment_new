#ifndef _SSCHECK_H
#define _SSCHECK_H

/*
#ifndef _TIME_T
#define _TIME_T
typedef long long time_t;
#endif
*/

//using namespace std;

#define S_ID_TOTAL           "71"  //总记录数代码
#define S_ID_ERR_TOTAL       "72"  //错误总记录数代码
#define S_ID_LINE_SUM_ERR    "73"  //列数不对
#define S_ID_FIELD_NULL_ERR  "74"  //非空列值为空
#define S_ID_CHAR_LEN_ERR    "75"  //定长字符串长度不对
#define S_ID_VACHAR_LEN_ERR  "76"  //字符串长度超长
#define S_ID_DATE_INVALID    "77"  //日期类型格式不对
#define S_ID_DATE_MIN_RANGE  "78"  //日期超出最小范围
#define S_ID_DATE_MAX_RANGE  "79"  //日期超出最大范围
#define S_ID_DATE_FILE_RANGE "80"  //日期超出文件名时间允许范围
#define S_ID_INT_INVALID     "81"  //数据类型格式不对
#define S_ID_INT_RANGE       "82"  //数据超出范围
#define S_ID_NO_EVENT_IN_MAP "99"  //事件类型不在映射表内

typedef struct {
	pthread_mutex_t lmutex;
	char *lbuf;
	size_t lnum;
	char lfname[500];
	char ltmpfname[500];	
	int fd;
} ListLogBuff;

extern ListLogBuff llb;

char* isNumber(char *str, bool if_null, int min_value, int max_value);
bool lengthCheck(char *str,int len);
bool dateFormatCheck(char *str,char *format);
bool dataRangeCheck(char *date,char *lowerLimit,char *uperLimit);
// bool check_file(char* fileName, char* prefix);
char* check_varchar(char* str, int len, bool can_null);
char* check_char(char* str, int len, bool can_null, bool numeral);
char* check_date(char* time_str, time_t file_time, time_t min_time, time_t max_time, time_t time_before, time_t time_after, char* time_fmt);
void get_file_time(char* time_str, time_t tt);
char * get_file_timestamp(char* fileName);
char * get_file_timestamp2(char* fileName);
int write_list_log_buff(char *data_file_name,int row_num,char *errcode,char *error_line);
int  write_list_log_file();
#endif
