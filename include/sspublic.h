#ifndef _SSPUBLIC_H
#define _SSPUBLIC_H

/*
//using 64-bit time_t
#ifndef _TIME_T
#define _TIME_T
typedef long long time_t;
#endif
*/

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/times.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <malloc.h>
#include <time.h>
#include <dirent.h>
#include <pthread.h>

#include <iostream>
#include <string>
#include<map>
using namespace std;

#define _ORACLE_ 8
//#define		_DB2_	7

#define		FBUFFSIZE	32768
#define		OUTBUFFSIZE	1048576
#define		LLOGBUFFSIZE	1048576
#define		LBSIZE		4096
#define		MAXLINE		256//128

#define		BIG_MAXLINE	1024
#define		FILE_NAME_LEN	100
//设置从数据库中读取数据的最大缓冲行数
#define		MAX_EXTRACT_ROW	1000

//设置在BCDContext中对段号码的最大存储长度，当超过此长度时，对上下文的信息输出，
//最大不能超过100字节!!!!!!!!! 因为load中的表相关字段就设置了100个字节
//建议采用25个字节的配置
//保证内存的的最大使用量，对此值的调整，会影响初始化时分配的内存大小
//#define		MAX_OTHER_NUMBERS_LEN	50	// 没啥情况，不要轻易动
#define		MAX_OTHER_NUMBERS_LEN	5	// 没啥情况，不要轻易动

//设置在BCDContext中对段号码归属地的最大存储长度，当超过此长度时，清楚以前的存储信息
//最大不能超过30字节!!!!!!!!! 因为load中的表相关字段就设置了30个字节
//#define		MAX_AREA_LEN	15	// 没啥情况，不要轻易动
#define		MAX_AREA_LEN	5	// 没啥情况，不要轻易动

//#define		MAX_LAST_ACTION_TYPE_LEN	11	// 没啥情况，不要轻易动
#define		MAX_LAST_ACTION_TYPE_LEN	5	// 没啥情况，不要轻易动

// 当初始化时，设置需要加载的用户上下文信息
// 目前情况，每次初始化时，不加载以前用户信息
#define		RESTORE_FILE_NAME "../data/context.txt"

#define		LOAD_FILE_NUM 25
#define		BACKUP_FILE_PREFIX "../output/out_bak/backup_context"
//模块名
#define		MODULE_NAME "saPretreatment"

#define MAX_SIG_FIELD     30
#define MAX_SIG_FIELD_LEN 31


#define  max(A, B) ((A) > (B) ? (A) : (B))
#define  min(A, B) ((A) > (B) ? (B) : (A))


typedef struct {
    char input_file_path[201];
    char ler_field_split[2];
    unsigned int sleep_interval;
    unsigned int lls_log_interval;
    char pros_file_path[201];
    char del_file_path[201];
    char err_file_path[201];
    char avl_file_path[201];
    char host_code[21];
    char entity_type_code[21];
    char entity_sub_code[21];
    int  log_interval;
    int  local_msisdn_enabled;
    char msisdn_script_name[200];
} Config;

typedef struct ler_errs_sum
{
    char current_file_name[200]; //当前处理文件名；
    int total;  //信令总数
    int err_total; //出错总数
    int err_filed_sum_total; //列数不对总数；
    int err_field_null_total; //列值为空总数；
    int err_str_len_total; //字符串长度不对；
    int err_str_long_total; //字符串超长总数；
    int err_date_invalid_total; //时间格式出错总数；
    int err_data_less_total; //时间超出最小格式总数；
    int err_data_more_total; //时间超出最大格式总数；
    int err_data_delay_total; //时间超出延时总数；
    int err_int_invalid_total; //数据类型无效总数；
    int err_int_range_total; //数据类型超出范围总数；
} LerStatis;

extern LerStatis *lers;

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t  more;
    pthread_cond_t  less;
    int empty;
    int to;
    int from;
    int lines;
    time_t filetime;
    char file_number[6];
    char buf[LBSIZE];
}LineBuff;

typedef struct {
    int num;
    int from;
    char buf[FBUFFSIZE];
} FileBuff;


typedef struct {
    pthread_mutex_t	mutex[LOAD_FILE_NUM];
    char *fbuf;
    size_t num[LOAD_FILE_NUM];
    int	counts[LOAD_FILE_NUM];
    char fname[LOAD_FILE_NUM][500];
    char tabname[LOAD_FILE_NUM][200];
    int fd[LOAD_FILE_NUM];
    int timeout[LOAD_FILE_NUM];
    time_t *event_time_begin;
    char file_number[LOAD_FILE_NUM][6];
} Analyze;


typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
    char input_file_path[200];
    char ler_field_split[2];
    unsigned int sleep_interval;
    unsigned int lls_log_interval;
    char pros_file_path[200];
    char del_file_path[200];
    char err_file_path[200];
    char avl_file_path[200];
    char msisdn_script_name[200];
    char host_code[20];
    char entity_type_code[20];
    char entity_sub_code[20];
    int  log_interval;
    int  local_msisdn_enabled;
} Check;

class XMLReader;
extern XMLReader *cfgXML;
extern Check chk;
extern map<string, int> decode_map;
//增加printf锁
//pthread_mutex_t printf_mutex;

int Discard(char *line,int flds);
int PutLine(char *line,  LineBuff *lb);
int GetLine(LineBuff *lb, char *line);
int Split(char *line, char ch,int maxw, char *word[], int len[]);
int ReadLine(const int fd, FileBuff *fb, char *line, int llen);
int SuperReadLine(const int fd, FileBuff *fb, char *line, int llen, int *flds);
int WriteLine(const int fd, FileBuff *fb, const char *src, const int length);
int ReadN(const int fd, FileBuff *fb, char *block, int size);
int WriteN(const int fd, const char *vptr, const int n);
int WriteN_wld(const int fd, const char *vptr, const int n);
void Char2BCD(char *src, char *dest, int cnum);
void BCD2Char(char *src, char *dest, int cnum);
// int isSame(char *src, char *dest, int len);
void *Malloc(unsigned capacity);
int Atoi(char *src);
int ReadConfig(XMLReader *xml, Config *cfg);
void PrintCfg(Config *cfg);
void ExecCmd(const char *cmd);
int System( char *cmd);
time_t ToTime_t(char *src);
time_t ToTime_t_2(char *src);
char *Now(char *now);
static void ShowTimes(void);
void ReadDir(int begin);
char *CurDir(char *dirname);
int Open(char *path, int oflag);
int Remove(char *path);
int Stat(char *path, struct stat *buf);
int PthreadCreate(pthread_t *tid, const pthread_attr_t *attr, void *(start_routine (void *)),void *arg);
pid_t Fork(void);
void SigChld(int sig);
int DaemonInit(void);
void PrintEnv(void);
int split_value(char** dest, char* line, char splt);

// 取得系统当前时间
// str_sys_time 返回"1970-01-01 08:00:00"格式字符串
// 函数返回值为秒数
time_t get_system_time(char *str_curr_system_time);

// 取得系统当前时间
// str_sys_time 返回"19710203040506"格式字符串
// 函数返回值为秒数
time_t get_system_time_2(char *str_curr_system_time);

time_t get_system_time_3(char *str_curr_system_time);

// 根据输入时间，转换为YYYYMMDDHHMMSS格式
// str_time 返回"20100315100000"格式字符串
// 函数返回值为tm
tm * get_str_time(char *str_time,time_t t);

// 根据输入时间，对分钟和秒钟部分清零
// 获取小时级的时间戳
// str_time 返回"20100315100000"格式字符串
// 函数返回值为time_t
time_t get_hour_time(time_t t);

int GetStr(char* , char* , int, char);

void LTrim(char* , char );

void RTrim(char* , char );

int split_value_noalloc(char **dest, char* line, char splt);

int Split_decode(char *line, char ch,int maxw, char *word[], int len[]);

int write_heart_file(char *filename);
int write_error_file(char *filepath,char *modulename,char *errcode,int level,char *errinfo);
int write_log_file(char *filepath,char *modulename,char *msgcode,char *file_name,char *end_time,char *msgvalue);

int write_error_recode_file(char *filepath,char *modulename,char *data_file_name,int row_num,char *errcode,char *error_line);

void write_deal_log_error();
void write_proc_log_error();



/*
    用于读取和解析xml配置文档，要求只有一个根节点
                高照 Creat  2010-01-08
                高照 Update 2010-02-19
*/
class XMLReader{
private:
    FILE *dataF;
public:
    char* value;//此节点的内容
    char* name;//此节点标签的名称
    map<string,string> property;//此节点属性值对
    XMLReader *node[100];//此节点的子节点，最多为100个
    //以目标字符串构造xml方法
    XMLReader(char* v){
        value=v;
        memset(node, 0, sizeof(node));
    }
    XMLReader(){
        memset(node, 0, sizeof(node));
    }
    //从名称或得节点
    XMLReader *getNodeByName(char *name,int no){
        int now_no=0;
        for(int i=0;i<100;i++){
            if(node[i]==NULL){
                //cout<<"没有找到该节点"<<name<<endl;
                //cout<< value << endl;
                return NULL;
            }
            if(node[i]&&strcmp(node[i]->name,name)==0){
                if(no==now_no){
                return node[i];
                }else{
                    now_no++;
                }
            }
        }
        return NULL;
    }

    XMLReader *getNodeByDateType(char *name, char* dateType) {
        for(int i = 0; i < 100; i++) {
            if(node[i]==NULL){
                //cout<<"没有找到该节点by data type,"<<name<<endl;
                return NULL;
            }
            //cout<<"有节点:"<<name<<", node name:"<<node[i]->name<<endl;
            if(node[i]&&strcmp(node[i]->name,name)==0){
                for(int j = 0; j < 100; j++) {
                    //cout<<"column name:"<<node[i]->getNodeByName("column",j)->property[dateType].c_str()<<endl;
                    if(strcmp(node[i]->getNodeByName("column",j)->property[dateType].c_str(), "date") == 0) {
                        return node[i]->getNodeByName("column",j);
                    }
                }
            }
        }

        return NULL;
    }

    //获得该节点的子节点数目,不包含子节点的子节点
    int getNumOfChildren(){
        int n=0;
        char *i=value;
        int brace_index=0;
        int node_num=0;
        for(;i<value+strlen(value)-1;i++){
            if(i[0]=='<'&&i[1]!='/'){
                brace_index++;
                if(brace_index==1){
                    n++;
                }

            }else if(i[0]=='<'&&i[1]=='/'){
                brace_index--;
            }

        }
        return n;
    }
};

//xml格式校验,若含有未封闭标签或标签不匹配则返回非零，若通过校验返回0；
int checkFormat(FILE *fp);
//将xml文档中所有内容读入内存
char * gatAllTextFromFile(char *fileName);
//深度搜索目标xml字符串，获取到XMLReader类中
void getXML(char *cont,XMLReader *node);
map<string,string> getProperty(char *from,char *to);
int split_field_value(int* dest, char* line, char splt);

#endif
