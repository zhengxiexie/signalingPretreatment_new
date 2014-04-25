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
//���ô����ݿ��ж�ȡ���ݵ���󻺳�����
#define		MAX_EXTRACT_ROW	1000

//������BCDContext�жԶκ�������洢���ȣ��������˳���ʱ���������ĵ���Ϣ�����
//����ܳ���100�ֽ�!!!!!!!!! ��Ϊload�еı�����ֶξ�������100���ֽ�
//�������25���ֽڵ�����
//��֤�ڴ�ĵ����ʹ�������Դ�ֵ�ĵ�������Ӱ���ʼ��ʱ������ڴ��С
//#define		MAX_OTHER_NUMBERS_LEN	50	// ûɶ�������Ҫ���׶�
#define		MAX_OTHER_NUMBERS_LEN	5	// ûɶ�������Ҫ���׶�

//������BCDContext�жԶκ�������ص����洢���ȣ��������˳���ʱ�������ǰ�Ĵ洢��Ϣ
//����ܳ���30�ֽ�!!!!!!!!! ��Ϊload�еı�����ֶξ�������30���ֽ�
//#define		MAX_AREA_LEN	15	// ûɶ�������Ҫ���׶�
#define		MAX_AREA_LEN	5	// ûɶ�������Ҫ���׶�

//#define		MAX_LAST_ACTION_TYPE_LEN	11	// ûɶ�������Ҫ���׶�
#define		MAX_LAST_ACTION_TYPE_LEN	5	// ûɶ�������Ҫ���׶�

// ����ʼ��ʱ��������Ҫ���ص��û���������Ϣ
// Ŀǰ�����ÿ�γ�ʼ��ʱ����������ǰ�û���Ϣ
#define		RESTORE_FILE_NAME "../data/context.txt"

#define		LOAD_FILE_NUM 25
#define		BACKUP_FILE_PREFIX "../output/out_bak/backup_context"
//ģ����
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
    char current_file_name[200]; //��ǰ�����ļ�����
    int total;  //��������
    int err_total; //��������
    int err_filed_sum_total; //��������������
    int err_field_null_total; //��ֵΪ��������
    int err_str_len_total; //�ַ������Ȳ��ԣ�
    int err_str_long_total; //�ַ�������������
    int err_date_invalid_total; //ʱ���ʽ����������
    int err_data_less_total; //ʱ�䳬����С��ʽ������
    int err_data_more_total; //ʱ�䳬������ʽ������
    int err_data_delay_total; //ʱ�䳬����ʱ������
    int err_int_invalid_total; //����������Ч������
    int err_int_range_total; //�������ͳ�����Χ������
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
//����printf��
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

// ȡ��ϵͳ��ǰʱ��
// str_sys_time ����"1970-01-01 08:00:00"��ʽ�ַ���
// ��������ֵΪ����
time_t get_system_time(char *str_curr_system_time);

// ȡ��ϵͳ��ǰʱ��
// str_sys_time ����"19710203040506"��ʽ�ַ���
// ��������ֵΪ����
time_t get_system_time_2(char *str_curr_system_time);

time_t get_system_time_3(char *str_curr_system_time);

// ��������ʱ�䣬ת��ΪYYYYMMDDHHMMSS��ʽ
// str_time ����"20100315100000"��ʽ�ַ���
// ��������ֵΪtm
tm * get_str_time(char *str_time,time_t t);

// ��������ʱ�䣬�Է��Ӻ����Ӳ�������
// ��ȡСʱ����ʱ���
// str_time ����"20100315100000"��ʽ�ַ���
// ��������ֵΪtime_t
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
    ���ڶ�ȡ�ͽ���xml�����ĵ���Ҫ��ֻ��һ�����ڵ�
                ���� Creat  2010-01-08
                ���� Update 2010-02-19
*/
class XMLReader{
private:
    FILE *dataF;
public:
    char* value;//�˽ڵ������
    char* name;//�˽ڵ��ǩ������
    map<string,string> property;//�˽ڵ�����ֵ��
    XMLReader *node[100];//�˽ڵ���ӽڵ㣬���Ϊ100��
    //��Ŀ���ַ�������xml����
    XMLReader(char* v){
        value=v;
        memset(node, 0, sizeof(node));
    }
    XMLReader(){
        memset(node, 0, sizeof(node));
    }
    //�����ƻ�ýڵ�
    XMLReader *getNodeByName(char *name,int no){
        int now_no=0;
        for(int i=0;i<100;i++){
            if(node[i]==NULL){
                //cout<<"û���ҵ��ýڵ�"<<name<<endl;
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
                //cout<<"û���ҵ��ýڵ�by data type,"<<name<<endl;
                return NULL;
            }
            //cout<<"�нڵ�:"<<name<<", node name:"<<node[i]->name<<endl;
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

    //��øýڵ���ӽڵ���Ŀ,�������ӽڵ���ӽڵ�
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

//xml��ʽУ��,������δ��ձ�ǩ���ǩ��ƥ���򷵻ط��㣬��ͨ��У�鷵��0��
int checkFormat(FILE *fp);
//��xml�ĵ����������ݶ����ڴ�
char * gatAllTextFromFile(char *fileName);
//�������Ŀ��xml�ַ�������ȡ��XMLReader����
void getXML(char *cont,XMLReader *node);
map<string,string> getProperty(char *from,char *to);
int split_field_value(int* dest, char* line, char splt);

#endif
