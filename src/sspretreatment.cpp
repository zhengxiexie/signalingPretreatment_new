/**
* Copyright (c) 2010 AsiaInfo. All Rights Reserved.
* Creator: gaozhao
* Modified log：
*     1> chengxc    2010-03-15    modify
*     2> zhoulb        2010-03-22    modify
* Description:
*     对采集输出的信令文件进行预处理，
*     经过转换、校验、分发、过滤、合并，输出到不同的分拣目录；
* Input:
*     采集输出的信令源文件
* Output:
*     1> 输出到漫游、业务、位置三个处理上下文目录的信令文件；
*     2> 每个处理的信令文件对应的记录统计及明细记录；
* 错误代码范围：
*       R0001 -- R1000
*/
#include "sspublic.h"

#include <sys/types.h>
#include <regex.h>
#include <time.h>
#include <map>
#include <string>
#include <signal.h>
//#include "AIXMLReader.h"
#include "sspretreatment.h"
//#include "ssfilter.h"
#include "sscheck.h"
#include "sstrans.h"
#include "ssmsisdn.h"

using namespace std;

// 增加初始化互斥锁
pthread_mutex_t pthread_init_mutex;
pthread_mutex_t pthread_logfile_mutex;

// 输出字段分隔符
static const char output_split[2] = ",";

char prog_basename[256];

//                                     1    2    3    4    5    6    7    8    9   10   11   12
static const int day_of_year[] = {0,   0,  31,  59,  90, 120, 151, 181, 212, 243, 273, 304, 334};
static const int output_interval_value[] = {3600, 300, 600, 1800, 3600};
XMLReader *cfgXML;
int num_of_channel;
int num_of_picks;
int* pick_idx;
Analyze ana;
int    exitflag = 0;
int file_read_exitflag = 0;
//ListLogBuff llb;
LineBuff    *lb;
Check chk;
char dump_sig[] = "dump_now\n";
// 增加过滤互斥锁
pthread_mutex_t filtrate_mutex;
string access_numbers;

map<string, int> decode_map;
int num_of_inter_field = 0;

extern char **g_word;
extern ListLogBuff llb;

int pros_cnt = 0;
time_t pros_1 = 0, pros_2 = 0;

static char str_exit_file_name[200] = "";

static char this_program_name[200] = "";
int * output_per_pick = NULL;
int * output_offset   = NULL;

static volatile int msisdn_table_need_update = 0;
static int append_city_code(char * line, const char spliter);

// hash table
struct _hash_table {
    char value[32];
    struct _hash_table * next;
} ;
typedef struct _hash_table ht_t;

struct _general_filter {
    int num_hash;
    int field;
    int update_date;
    int update_min;
    int update_hour;
    int need_update;
    int last_update_time;
    int available;
    ht_t * hash;
    char filename[256];
};
typedef struct _general_filter gfilter_t;
static gfilter_t * general_filter;

// used in general_filter
static int read_general_filter(const char * file, gfilter_t * gfilter);
static int in_general_filter(const char * word, gfilter_t * gfilter);
static int filtered_general(const char * line, gfilter_t * gfilter, int num);
static inline unsigned int BKDRHash(const char *str);

static int hash_imsi(const char * imsi) {
    int ret = 0, p = 0;
    while (imsi[p]) ret = ret * 17 + imsi[p++];
    return ret;
}

void signal_handler( int signo ) {
    printf("signal %d catched.\n", signo);
    switch (signo) {
        case SIGALRM:
            // ignore these
            break;
        case SIGUSR1:
            // update msisdn_prefix_table
            msisdn_table_need_update = 1;
            break;
        default: break;
    }
    // Reinstall signal, for SVID-like system UNINSTALLs signal handler after EACH deliver!!!
    // AIX hornors SVID standards
    signal(signo, signal_handler);
    return;
}

void trans_format(char *line, char *word[], int *trans_order, int num, int all) {
    memset(line, '\0', sizeof(line));
    for (int i = 0; i < num; i++) {
        if (trans_order[i] > 0) {
            strcat(line, word[trans_order[i] - 1]);
        } else if (trans_order[i] < 0) {
           if (word[all + trans_order[i]]) strcat(line, word[all + trans_order[i]]);
        }
        strcat(line, output_split);
    }
}

void * consume(void* pick_num) {
    int pick_number = *(int *)pick_num;
    int multi_part  = *(((int*)pick_num) + 1);
    int multi_start = *(((int*)pick_num) + 2);
    free(pick_num);
    int field_imsi = decode_map["imsi"];

    int i, ret;
    //char **m_event_type;
    int len[20];
    char *word[50];
    char imsi[16];
    char tmp[32];
    char line[MAXLINE], line_tmp[BIG_MAXLINE];
    char str_result[BIG_MAXLINE];

    //int user_event_type;
    char str_user_event_type[10];
    char lac_id[10], str_lac_id[10];
    char ci_id[10], str_ci_id[10];
    char lac_ci_id[20];
    char errinfo[200];
    //int idx = 0;
    time_t event_filetime = -1;

    char real_out_path[150];

    Remove_Prefix_Rule rprule;

    int pthread_id;
    pthread_id = pthread_self();
    char tmp_sfltr[500];

    // ============ 加载信令过滤规则 ===============
    Filter_Rule * sfltr;
    int num_sfltr = cfgXML->getNodeByName("picks", 0)->getNodeByName("pick", pick_idx[pick_number])->getNodeByName("filters", 0)->getNumOfChildren();
    sfltr = (Filter_Rule*) malloc(sizeof(Filter_Rule) * num_sfltr);
    for (int i = 0; i < num_sfltr; i++) {
        sfltr[i].available = (0 == strcmp("true", cfgXML->getNodeByName("picks", 0)->getNodeByName("pick", pick_idx[pick_number])->getNodeByName("filters", 0)->getNodeByName("filter", i)->property["available"].c_str()));
        memset(tmp_sfltr, '\0', 500);

        strcpy(tmp_sfltr, cfgXML->getNodeByName("picks", 0)->getNodeByName("pick", pick_idx[pick_number])->getNodeByName("filters", 0)->getNodeByName("filter", i)->property["value"].c_str());
        sfltr[i].reg = NULL;
        if (0 == strcmp("csv", tmp_sfltr)) {
            sfltr[i].num = get_num_field(cfgXML->getNodeByName("picks", 0)->getNodeByName("pick", pick_idx[pick_number])->getNodeByName("filters", 0)->getNodeByName("filter", i)->value, ',');
            sfltr[i].value = (char**) malloc(sizeof(char*) * sfltr[i].num);
            split_value(sfltr[i].value, cfgXML->getNodeByName("picks", 0)->getNodeByName("pick", pick_idx[pick_number])->getNodeByName("filters", 0)->getNodeByName("filter", i)->value, ',');
            sfltr[i].fv = fv_CSV;
        } else if (0 == strcmp("range", tmp_sfltr)) {
            sfltr[i].num = 2;
            sfltr[i].value = (char**) malloc(sizeof(char*) * 2);
            split_value(sfltr[i].value, cfgXML->getNodeByName("picks", 0)->getNodeByName("pick", pick_idx[pick_number])->getNodeByName("filters", 0)->getNodeByName("filter", i)->value, ',');
            sfltr[i].fv = fv_Range;
        } else if (0 == strcmp("regex", tmp_sfltr)) {
            strcpy(tmp_sfltr, cfgXML->getNodeByName("picks", 0)->getNodeByName("pick", pick_idx[pick_number])->getNodeByName("filters", 0)->getNodeByName("filter", i)->value);
            sfltr[i].num      = 0;
            sfltr[i].value    = NULL;
            sfltr[i].reg      = (regex_t*) malloc(sizeof(regex_t));
            int ret = 0;
            ret = regcomp(sfltr[i].reg, tmp_sfltr, REG_EXTENDED);
            if (ret != 0) {
                printf("Regular Expresion Error, disable this filter for good reason. regex: %s\n", tmp_sfltr);
                sfltr[i].available = false;
            }
            sfltr[i].fv = fv_Regex;
            //sfltr[i].fm = fm_Word;
        }

        memset(tmp_sfltr, '\0', 500);
        strcpy(tmp_sfltr, cfgXML->getNodeByName("picks", 0)->getNodeByName("pick", pick_idx[pick_number])->getNodeByName("filters", 0)->getNodeByName("filter", i)->property["match"].c_str());
        if (0 == strcmp("prefix", tmp_sfltr)) {
            sfltr[i].fm = fm_Prefix;
            qsort(sfltr[i].value, sfltr[i].num, sizeof(char*), strcomp);
        } else if (0 == strcmp("suffix", tmp_sfltr)) {
            sfltr[i].fm = fm_Suffix;
            qsort(sfltr[i].value, sfltr[i].num, sizeof(char*), strcomp);
        } else if (0 == strcmp("word", tmp_sfltr)) {
            sfltr[i].fm = fm_Word;
            qsort(sfltr[i].value, sfltr[i].num, sizeof(char*), strcomp);
        }

        memset(tmp_sfltr, '\0', 500);
        strcpy(tmp_sfltr, cfgXML->getNodeByName("picks", 0)->getNodeByName("pick", pick_idx[pick_number])->getNodeByName("filters", 0)->getNodeByName("filter", i)->property["field"].c_str());
        sfltr[i].field = decode_map[tmp_sfltr];
    }

    //得到输出格式序列
    char   buff[300];
    char **pick_format_list;
    int   *form_list;
    int num_of_format_item = atoi(cfgXML->getNodeByName("picks", 0)->getNodeByName("pick", pick_idx[pick_number])->getNodeByName("merge", 0)->getNodeByName("output_format", 0)->property["num"].c_str());

    // 读取local_flag, prov_code, city_code
    int num_append_item = 0;
    if (chk.local_msisdn_enabled) {
        memset(buff, '\0', sizeof(buff));
        strcpy(buff, cfgXML->getNodeByName("picks", 0)->getNodeByName("pick", pick_idx[pick_number])->getNodeByName("append", 0)->value);
        if (strlen(buff) != 0) {
            for (int pos = 0; buff[pos]; pos++) if (buff[pos] == ',') ++num_append_item;
            ++num_append_item;
        }
    }

    pick_format_list = (char **)Malloc((num_of_format_item + num_append_item) * sizeof(char*));
    form_list = (int *)Malloc((num_of_format_item + num_append_item) * sizeof(int));
    for (int q = 0; q < num_of_format_item + num_append_item; q++) {
        pick_format_list[q] = (char *)Malloc(16 * sizeof(char));
    }

    char tmp_str[200];
    // remove prefix rule
    if (0 == strcmp("recursive", cfgXML->getNodeByName("picks", 0)->getNodeByName("pick", pick_idx[pick_number])->getNodeByName("remove_prefix", 0)->property["type"].c_str())) {
        rprule.type = rp_Recursive;
    } else rprule.type = rp_Normal;
    rprule.available = (0 == strcmp("true", cfgXML->getNodeByName("picks", 0)->getNodeByName("pick", pick_idx[pick_number])->getNodeByName("remove_prefix", 0)->property["available"].c_str()));
    if ( rprule.available ) {
        memset(tmp_str, 0, sizeof(tmp_str));
        strcpy(tmp_str, cfgXML->getNodeByName("picks", 0)->getNodeByName("pick", pick_idx[pick_number])->getNodeByName("remove_prefix", 0)->property["field"].c_str());
        rprule.num_field = get_num_field(tmp_str, ',');
        rprule.field     = (int*) malloc(sizeof(int) * rprule.num_field);
        split_field_value(rprule.field, tmp_str, ',');
        printf("in pick[%d]:\n\tremove prefix in %s\n", pick_idx[pick_number], tmp_str);
        memset(tmp_str, 0, sizeof(tmp_str));
        strcpy(tmp_str, cfgXML->getNodeByName("picks", 0)->getNodeByName("pick", pick_idx[pick_number])->getNodeByName("remove_prefix", 0)->property["prefix"].c_str());
        rprule.num_prefix = get_num_field(tmp_str, ',');
        rprule.prefix     = (char**) malloc(sizeof(char*) * rprule.num_prefix);
        rprule.len_prefix = (int *)  malloc(sizeof(int)   * rprule.num_prefix);
        split_value(rprule.prefix, tmp_str, ',');
        qsort(rprule.prefix, rprule.num_prefix, sizeof(char*), strcomp);
        printf("\tprefix: ");
        for (int i = 0; i < rprule.num_prefix; i++) {
            rprule.len_prefix[i] = strlen(rprule.prefix[i]);
            printf("%s ", rprule.prefix[i]);
        }
        printf("\n\ttype: ");
        if (rprule.type == rp_Recursive)    printf("Recursive\n");
        else if (rprule.type == rp_Normal) printf("Normal\n");
    }

    //得到输出的顺序串
    split(cfgXML->getNodeByName("picks", 0)->getNodeByName("pick", pick_idx[pick_number])->getNodeByName("merge", 0)->getNodeByName("output_format", 0)->value, ',', pick_format_list);
    cout << "Out put format in pick[" << pick_idx[pick_number] << "]: ";
    for (int q = 0; q < num_of_format_item; q++) {
        form_list[q] = atoi(pick_format_list[q]);
        printf("%d, ", form_list[q]);
    }

    // prov_code, city_code, local_flag
    if (chk.local_msisdn_enabled) {
        for (int q = 0; q < num_append_item; q++) memset(pick_format_list[q], '0', 16);
        split(cfgXML->getNodeByName("picks", 0)->getNodeByName("pick", pick_idx[pick_number])->getNodeByName("append", 0)->value, ',', pick_format_list);
        for (int q = num_of_format_item; q < num_append_item + num_of_format_item; q++) {
            if (0 == strcmp("local_flag", pick_format_list[q - num_of_format_item])) form_list[q] = -3;
            else if (0 == strcmp("prov_code", pick_format_list[q - num_of_format_item])) form_list[q] = -2;
            else if (0 == strcmp("city_code", pick_format_list[q - num_of_format_item])) form_list[q] = -1;
            printf("%d, ", form_list[q]);
        }
    }
    printf("\n\tnum of append item: %d\n", num_append_item);

    printf("信令预处理消费处理线程[%d]启动..., processing pick[%d]\n", pthread_id, pick_idx[pick_number]);

    while (1) {
        pthread_mutex_lock(&lb[pick_number].mutex);
        while (lb[pick_number].lines <= 0 && exitflag == 0) {
            pthread_cond_wait(&lb[pick_number].more, &lb[pick_number].mutex);
        }

        if ((exitflag > 0) && (file_read_exitflag > 0)) {
            exitflag++;
            pthread_cond_broadcast(&lb[pick_number].less);
            pthread_mutex_unlock(&lb[pick_number].mutex);

            //缓冲写入文件
            for (int part = 0; part < multi_part; part++) {
                int pick_num_i = multi_start + part;
                ana.timeout[pick_num_i] = 1;
                WriteResult(pick_num_i, "");
            }

            //if(tar_temp != NULL) free(tar_temp);
            if (form_list != NULL) free(form_list);
            if (pick_format_list != NULL) free(pick_format_list);
            for (int i = 0; i < 2; i++) {
                if (sfltr[i].value != NULL) {
                    for (int j = 0; j < sfltr[i].num; j++) if (sfltr[i].value[j]) free(sfltr[i].value[j]);
                    free(sfltr[i].value);
                }
            }
            // free(sfltr);

            memset(errinfo, '\0', sizeof(errinfo));
            sprintf(errinfo, "信令预处理输出线程退出，请关注！");
            write_error_file(chk.err_file_path, MODULE_NAME, "65", 2, errinfo);
            printf("信令预处理分拣线程[%d]退出，请关注！\n", pick_number);
            //pthread_exit(0);
            break;
        }

        memset(line_tmp, '\0', sizeof(line_tmp));
        memset(line, '\0', sizeof(line));
        ret = GetLine(&lb[pick_number], line);
        pthread_cond_signal(&lb[pick_number].less);
        pthread_mutex_unlock(&lb[pick_number].mutex);
        //判断是否为dump_sig
        if (strncmp(line, dump_sig, 8) == 0) {
            for (int part = 0; part < multi_part; part++) {
                int pick_num_i = multi_start + part;
                ana.timeout[pick_num_i] = 1;
                printf("Idle Timeout, Dump Buffer to File in pick[%d:%d], thread[%d]\n", multi_start, part, pthread_id);
                WriteResult(pick_num_i, "");
                ana.event_time_begin[pick_num_i] = lb[pick_number].filetime;
                strcpy(ana.file_number[pick_num_i], lb[pick_number].file_number);
            }
        }

        //解析转换后的信令数据
        memset(line_tmp, '\0', sizeof(line_tmp));
        strcpy(line_tmp, line);
        ret = Split(line_tmp, ',', 50, word, len);
        //printf("%s, %s, %s, line: %s\n", word[num_of_inter_field + num_append_item- 3], word[num_of_inter_field + num_append_item- 2],word[num_of_inter_field + num_append_item- 1], line);
        //过滤
        ret = filter(word, sfltr, num_sfltr);
        if (ret != 0) {
            //printf("filter return: %d, line: %s\n", ret, line);
            continue;
        }

        // remove prefix
        if (rprule.available) remove_prefix(word, &rprule);

        //转换
        trans_format(line, word, form_list, num_of_format_item + num_append_item, num_of_inter_field + num_append_item);
        strcat(line, "\n");
        //idx ++;
        //printf("write file to buff: line = %s", line);
        //写入缓冲或临时文件
        int buffer_used = multi_start + hash_imsi(word[field_imsi]) % multi_part;
        WriteResult(buffer_used, line);
    } /* while */
    return NULL;
}

int strcmp1(const char* str1, const char* str2, size_t dummy) {
    return strcmp(str1, str2);
}

int strcomp(const void* str1, const void* str2) {
    return strcmp(*(char* const*) str1, *(char* const*) str2);
}

int strrcomp(const void* str1, const void* str2) {
    return strrcmp(*(char* const*) str1, *(char* const*) str2);
}

int strrcmp(const char* str1, const char* str2) {
    int pos1 = strlen(str1), pos2 = strlen(str2);
    int ret = 0;
    if (pos1 == 0 && pos2 == 0) return 0;
    if (pos2 == 0) return str1[pos1];
    if (pos1 == 0) return str2[pos2];
    while (pos1 >= 0 && pos2 >= 0) {
        ret = (int)(str1[pos1--]) - (int)(str2[pos2--]);
        if (ret != 0) return ret;
    }
    if (pos1 >= 0) return str1[pos1];
    if (pos2 >= 0) return str2[pos2];
    return 0;
}

int strrncmp(const char* str1, const char* str2, size_t n) {
    int pos1 = strlen(str1), pos2 = strlen(str2);
    int ret = 0;
    if (pos1 == 0 && pos2 == 0) return 0;
    if (pos1 == 0) return str1[pos1];
    if (pos2 == 0) return str2[pos2];
    for (int i = 0; i < n; i++) {
        ret = (int)(str1[pos1 - i]) - (int)(str2[pos2 - i]);
        if (ret != 0) return ret;
    }
    return 0;
}

int get_num_field(char* line, char splt) {
    int ret = 0;
    for (int i = 0; i < strlen(line); i++) if (line[i] == splt) ret++;
    return (ret + 1);
}

int remove_prefix(char** word, Remove_Prefix_Rule *rule) {
    int  flag_nomatch = 0;
    int  fld          = 0;
    char tmp_word[50];
    int  posm, posl, posr;
    int  comp         = 0;
    int  i            = 0;
    int  word_off     = 0;

    for (i = 0; i < rule->num_field; i++) {
        fld      = rule->field[i];
        word_off = 0;
        memset(tmp_word, 0, sizeof(tmp_word));
        strcpy(tmp_word, word[fld]);
        do {
            flag_nomatch = 0;
            posl         = 0;
            posr         = rule->num_prefix - 1;
            do {
                posm = (posl + posr) / 2;
                comp = strncmp(tmp_word + word_off, rule->prefix[posm], rule->len_prefix[posm]);
                if (comp == 0) {
                    flag_nomatch = 1;
                    word_off    += rule->len_prefix[posm];
                    break;
                } else if (comp > 0) posl = posm + 1;
                else posr = posm - 1;
            } while (posl <= posr);
        } while (rule->type == rp_Recursive && flag_nomatch != 0);
        strcpy(word[fld], tmp_word + word_off);
    }
    return 0;
}

// 过滤信令，根据xml中配置的规则，通过过滤返回0，否则返回未通过过滤序号
int filter(char* word[], Filter_Rule* sfltr, int num_sfltr) {
    int (*comp_func)(const char*, const char*, size_t) = NULL;
    for (int i = 0; i < num_sfltr; i++) {
        if (sfltr[i].available) {
            int  word_idx = sfltr[i].field;
            char *val;
            int  len;

            switch (sfltr[i].fm) {
            case fm_Prefix: {
                    len = strlen(sfltr[i].value[0]);
                    val = word[word_idx];
                    comp_func = strncmp;
                    break;
                }
            case fm_Suffix: {
                    len = strlen(sfltr[i].value[0]);
                    val = word[word_idx] + strlen(word[word_idx]) - len;
                    comp_func = strncmp;
                    break;
                }
            case fm_Word: {
                    len = 0;
                    val = word[word_idx];
                    comp_func = strcmp1;
                    break;
                }
            default: return -(i + 1);
            } /* switch (sfltr[i].fm) */

            switch (sfltr[i].fv) {
            case fv_Range: {
                    if (!(comp_func(val, sfltr[i].value[0], len) >= 0 && comp_func(sfltr[i].value[1], val, len) >= 0)) return i + 1;
                    break;
                }
            case fv_CSV: {
                    int filtered = 1;
                    int posl = 0;
                    int posr = sfltr[i].num - 1;
                    int posm = 0;
                    int comp;
                    do {
                        posm = (posl + posr) / 2;
                        comp = comp_func(val, sfltr[i].value[posm], len);
                        if (comp == 0) {
                            filtered = 0;
                            break;
                        } else if (comp > 0) posl = posm + 1;
                        else posr = posm - 1;
                    } while (posl <= posr);
                    if (filtered != 0) return i + 1;
                    break;
                }
            case fv_Regex: {
                    int ret = 0;
                    ret = regexec(sfltr[i].reg, word[word_idx], 0, NULL, 0);
                    if (ret != 0) return (i + 1);
                    break;
                }
            default: return -(i + 1);
            } /* switch (sfltr[i].fv) */
        } /* if */
    }
    return 0;
}

void *log_timer(void *arg) {
    int fd;
    time_t sys_time;
    char str_sys_time[20];
    char filename[200], rfilename[200];


    char errinfo[200];

    printf("日志定时线程启动...\n");
    while (1) {
        //cout<<"log interval:"<<chk.log_interval<<endl;
        sleep(chk.log_interval);

        // 取得系统当前时间
        memset(str_sys_time, '\0', sizeof(str_sys_time));
        sys_time = get_system_time_2(str_sys_time);
        //cout<<"sys_time:"<<sys_time<<endl;
        memset(filename, 0, sizeof(filename));
        memset(rfilename, 0, sizeof(rfilename));
        sprintf(filename, "%s%s.tmp", chk.del_file_path, MODULE_NAME);
        sprintf(rfilename, "%s%s_%d_%s.del", chk.del_file_path, MODULE_NAME, getpid(), str_sys_time);

        //printf("log-->filename:%s, rfilename:%s",filename, rfilename);

        fd = open(filename, O_RDONLY);
        if (fd > 0) {
            close(fd);
            pthread_mutex_lock(&pthread_logfile_mutex);
            rename(filename, rfilename);
            pthread_mutex_unlock(&pthread_logfile_mutex);
        }

        if ((exitflag > 0))     {
            memset(errinfo, '\0', sizeof(errinfo));
            sprintf(errinfo, "时段监控客户日志输出线程退出，请关注！");
            write_error_file(chk.err_file_path, MODULE_NAME, "65", 2, errinfo);
            //pthread_exit(0);
            break;
        }
    }
    return (NULL);
}

//明细日志定时输出
void *llblog_timer(void *arg) {
    int fd;
    time_t sys_time;
    char str_sys_time[20];
    char filename[200], rfilename[200];

    char filepath[200];  //明细日志路径；
    char file_write_time[20]; //明细日志的格式时间戳；

    char file_curr_time[20];
    memset(file_curr_time, '\0', sizeof(file_curr_time));
    get_system_time_3(file_curr_time);
    sprintf(llb.lfname, "%s_%s.avl", chk.avl_file_path, file_curr_time); //当天停掉程序时写入；

    char errinfo[200];

    memset(filepath, '\0', sizeof(filepath));
    strcpy(filepath, chk.avl_file_path);

    printf("明细日志定时输出线程启动...\n");
    while (1) {
        //cout<<"list log interval:"<<chk.lls_log_interval<<endl;
        sleep(chk.lls_log_interval);

        //得到当前的字符串；
        memset(file_write_time, '\0', sizeof(file_write_time));
        get_system_time_3(file_write_time);

        if (strcmp(file_write_time, file_curr_time) != 0) { //如果文件时间戳发生变化，写文件；
            memset(llb.lfname, '\0', sizeof(llb.lfname));
            sprintf(llb.lfname, "%s_%s.avl", chk.avl_file_path, file_curr_time);
            //printf("list log name:[%s]\n",llb.lfname);

            write_list_log_file();
            memset(file_curr_time, '\0', sizeof(file_curr_time));
            strcpy(file_curr_time, file_write_time);
        }


        if ((exitflag > 0))     {
            write_list_log_file();

            memset(errinfo, '\0', sizeof(errinfo));
            sprintf(errinfo, "明细日志定时输出线程退出，请关注！");
            write_error_file(chk.err_file_path, MODULE_NAME, "65", 2, errinfo);
            //pthread_exit(0);
            break;
        }
    }
    return (NULL);
}

int main(int argc, char *argv[]) {
    int i;
    float size;
    //三组线程
    pthread_t    tid_produce, tid_log, tid_list_log;
    pthread_t    *tid_consume;
    pthread_attr_t attr;

    pthread_t    tid_statistic;

    char time_buffer[128];
    time_t timestamp_start = time(NULL);
    strftime(time_buffer, 127, "%04Y-%02m-%02d %02H:%02M:%02S", localtime(&timestamp_start));
    printf("\x0C");
    printf("================== [%s] %s started ==================\n", time_buffer, argv[0]);

    // 进程名
    strncpy(this_program_name, argv[0], 200);

    //输出路径
    char real_out_path[200];
    Config *cfg;
    lers = (LerStatis *)Malloc(sizeof(LerStatis));

    // get exitfile name
    char program_name[200];
    memset(program_name, 0, sizeof(program_name));
    memset(prog_basename, 0, sizeof(prog_basename));
    strcpy(program_name, argv[0]);
    int last_slash = 0;
    for (int i = 0; i < strlen(program_name); i++) {
        if (program_name[i] == '/') last_slash = i;
    }
    if (last_slash != 0) {
        program_name[last_slash] = '\0';
        sprintf(str_exit_file_name, "%s/../../flag/%s.exitflag", program_name, program_name + last_slash + 1);
        strcpy(prog_basename, program_name + last_slash + 1);
    } else {
        sprintf(str_exit_file_name, "../../flag/%s.exitflag", program_name);
        strcpy(prog_basename, program_name);
    }
    printf("str_exit_file_name: %s\n", str_exit_file_name);
    int exit_file_fd = 0;
    exit_file_fd = open(str_exit_file_name, O_RDONLY);
    if (exit_file_fd > 0) {
        printf("exitflag found, remove...\n");
        close(exit_file_fd);
        remove(str_exit_file_name);
    }

    // 读取XML配置文件
    char cfg_name[256];
    sprintf(cfg_name, "../../etc/c/%s.xml", prog_basename);
    printf("Using Config File: %s\n", cfg_name);
    get_XML_config(cfg_name);
    g_word = (char**) malloc(sizeof(char*) * MAX_SIG_FIELD);
    for (int i = 0; i < MAX_SIG_FIELD; i++) {
        g_word[i] = (char*) malloc(MAX_SIG_FIELD_LEN * sizeof(char));
        memset(g_word[i], 0, MAX_SIG_FIELD_LEN);
    }

    // 计算分拣线程总数
    num_of_picks = 0;
    int tmp_num_pick = cfgXML->getNodeByName("picks", 0)->getNumOfChildren();
    printf("Num of all pick %d.\n", tmp_num_pick);
    for (int i = 0; i < tmp_num_pick; i++) {
        if (0 == strcmp("true", cfgXML->getNodeByName("picks", 0)->getNodeByName("pick", i)->property["available"].c_str())) {
            num_of_picks++;
        }
    }
    printf("Num of Picks available: %d.\n", num_of_picks);
    pick_idx        = (int*) malloc(sizeof(int) * num_of_picks);
    output_per_pick = (int*) malloc(sizeof(int) * num_of_picks);
    output_offset   = (int*) malloc(sizeof(int) * num_of_picks);
    int num_of_output = 0;
    int j = 0;
    for (int i = 0; i < tmp_num_pick; i++) {
        if (0 == strcmp("true", cfgXML->getNodeByName("picks", 0)->getNodeByName("pick", i)->property["available"].c_str())) {
            int nt = atoi(cfgXML->getNodeByName("picks", 0)->getNodeByName("pick", i)->property["multi"].c_str());
            if (nt == 0) nt = 1;
            output_offset[j] = num_of_output;
            output_per_pick[j]= nt;
            num_of_output += nt;
            printf("thread[%d] => Pick[%d], starting %d, output to %d parts\n", j, i, output_offset[j], output_per_pick[j]);
            pick_idx[j] = i;
            j++;
        }
    }
    //读取信令配置
    cfg = (Config *)Malloc(sizeof(Config));
    ReadConfig(cfgXML, cfg);

    // 输出ssanalyze.conf文件配置信息
    PrintCfg(cfg);
    memset(&chk, 0, sizeof(Check));
    strcpy(chk.input_file_path,  cfg->input_file_path);
    strcpy(chk.pros_file_path,   cfg->pros_file_path);
    strcpy(chk.del_file_path,    cfg->del_file_path);
    strcpy(chk.err_file_path,    cfg->err_file_path);
    strcpy(chk.avl_file_path,    cfg->avl_file_path);
    strcpy(chk.host_code,        cfg->host_code);
    strcpy(chk.entity_type_code, cfg->entity_type_code);
    strcpy(chk.entity_sub_code,  cfg->entity_sub_code);
    chk.log_interval     =       cfg->log_interval;
    chk.sleep_interval   =       cfg->sleep_interval;
    chk.lls_log_interval =       cfg->lls_log_interval;
    chk.local_msisdn_enabled =   cfg->local_msisdn_enabled;
    strcpy(chk.msisdn_script_name, cfg->msisdn_script_name);
    free(cfg);

    int filter_channel_number = num_of_picks;
    int *channelNum = NULL;
    channelNum = (int *)Malloc(sizeof(int) * filter_channel_number);
    lb = (LineBuff *)Malloc(sizeof(LineBuff) * filter_channel_number);

    // bind SIGAARM and SIGUSR1 and SIGHUP
    if  (signal(SIGALRM, signal_handler) == SIG_ERR) signal(SIGALRM, SIG_DFL);
    if  (signal(SIGUSR1, signal_handler) == SIG_ERR) printf("SIGUSR1 Failed\n");

    //初始化缓冲区
    for (i = 0; i < filter_channel_number; i++) {
        lb[i].empty    = LBSIZE;
        lb[i].to       = 0;
        lb[i].from     = 0;
        lb[i].lines    = 0;
        lb[i].filetime = -1;
        channelNum[i]  = i;

        memset(lb[i].buf, '\0', sizeof(lb[i].buf));
        pthread_cond_init(&lb[i].more,   NULL);
        pthread_mutex_init(&lb[i].mutex, NULL);
        pthread_cond_init(&lb[i].less,   NULL);
    }

    //初始化文件输出块
    ana.fbuf = (char *)Malloc(num_of_output * OUTBUFFSIZE);
    ana.event_time_begin = (time_t *)Malloc(num_of_output * sizeof(time_t));
    int c = 0;
    for (i = 0; i < filter_channel_number; i++) {
        for (int j = 0; j < output_per_pick[i]; j++, c++) {
            pthread_mutex_init(&ana.mutex[c], NULL);
            //得到文件输出的目录和前缀
            memset(real_out_path, '\0', sizeof(real_out_path));
            if (output_per_pick[i] <= 1) {
                sprintf(ana.fname[c], "%s/%s.tmp",
                         cfgXML->getNodeByName("picks", 0)->getNodeByName("pick", pick_idx[i])->getNodeByName("merge", 0)->property["output_path"].c_str(),
                         cfgXML->getNodeByName("picks", 0)->getNodeByName("pick", pick_idx[i])->getNodeByName("merge", 0)->property["output_prefix"].c_str());
            } else {
                sprintf(ana.fname[c], "%s/%d/%s.tmp",
                         cfgXML->getNodeByName("picks", 0)->getNodeByName("pick", pick_idx[i])->getNodeByName("merge", 0)->property["output_path"].c_str(),
                         j + 1,
                         cfgXML->getNodeByName("picks", 0)->getNodeByName("pick", pick_idx[i])->getNodeByName("merge", 0)->property["output_prefix"].c_str());
            }
            printf("pick[%d:%d], prefix file = [%s] \n", i, j, ana.fname[c]);
            ana.num[c]              = 0;
            ana.counts[c]           = 0;
            ana.fd[c]               = -1;
            ana.timeout[c]          = -1;
            ana.event_time_begin[c] = -1;
            memset(ana.file_number[c], '\0', sizeof(ana.file_number[c]));
        }
    }

    //初始化明细文件输出块
    memset(llb.ltmpfname, '\0', sizeof(llb.ltmpfname));
    sprintf(llb.ltmpfname, "%s.tmp", chk.avl_file_path);
    llb.lbuf = (char *)Malloc(LLOGBUFFSIZE);
    llb.lnum = 0;
    llb.fd = NULL;

    //初始化字段映射关系
    read_decode_info();

    //初始化
    pthread_attr_init(&attr);
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);

    //初始化互斥锁
    pthread_mutex_init(&pthread_init_mutex, NULL);
    pthread_mutex_init(&pthread_logfile_mutex, NULL);

    //程序主体启动完成,写心跳文件
    char moudleName[250] ;
    memset(moudleName, '\0', sizeof(moudleName));
    sprintf(moudleName, "%s%s.proc", chk.pros_file_path, MODULE_NAME);
    write_heart_file(moudleName);

    //启动文件读取线程
    PthreadCreate(&tid_produce, NULL, read_nonparticipant_data, NULL);

    //启动定时输出日志线程
    PthreadCreate(&tid_log, NULL, log_timer, NULL);

    //启动明细日志定时输出线程
    PthreadCreate(&tid_list_log, NULL, llblog_timer, NULL);

    //启动消费线程
    tid_consume = (pthread_t*)Malloc(filter_channel_number * sizeof(pthread_t));
    for (i = 0; i < num_of_picks; i++) {
        int * thread_var = (int *)malloc(sizeof(int) * 3);
        thread_var[0] = channelNum[i];
        thread_var[1] = output_per_pick[i];
        thread_var[2] = output_offset[i];
        PthreadCreate(tid_consume + i, &attr, consume, (void *)thread_var);
    }

    PthreadCreate(&tid_statistic, &attr, statistics_thread, NULL);

    //等待线程结束,然后再退出
    //pthread_join(tid_list_log, NULL);
    pthread_join(tid_produce, NULL);
    for (i = 0; i < filter_channel_number; i++) {
        pthread_join(*(tid_consume + i), NULL);
    }
    alarm(1);
    pthread_join(tid_log, NULL);

    //释放内存
    if (lb                   != NULL) free(lb);
    if (tid_consume          != NULL) free(tid_consume);
    if (lers                 != NULL)  free(lers);
    if (ana.fbuf             != NULL)  free(ana.fbuf);
    if (ana.event_time_begin != NULL)  free(ana.event_time_begin);
    if (channelNum           != NULL) free(channelNum);
    //if(ffrs                != NULL) free(ffrs);
    //if(lfrs                != NULL) free(lfrs);
    //if(event_map           != NULL) free(event_map);
    if (g_word               != NULL) free(g_word);
    if (llb.lbuf             != NULL) free(llb.lbuf);

    char errinfo[200];
    memset(errinfo, '\0', sizeof(errinfo));
    sprintf(errinfo, "信令预处理程序退出,立即检查！");
    printf("%s\n", errinfo);
    write_error_file(chk.err_file_path, MODULE_NAME, "61", 2, errinfo);

    //close_db();

    //exit(0);
    return 0;
}

// 从xml配置文件中读取decode map
void read_decode_info() {
    char tmp_str[500];
    char tmp_field[20];
    strcpy(tmp_str, cfgXML->getNodeByName("field_name", 0)->value);
    int i = 0;
    int idx = 0;
    int pos_start = 0;
    printf("decode_map:");
    num_of_inter_field = atoi(cfgXML->getNodeByName("field_name", 0)->property["num"].c_str());

    do {
        if (tmp_str[i] == ',' || tmp_str[i] == '\0') {
            memset(tmp_field, '\0', 20);
            strncpy(tmp_field, tmp_str + pos_start, i - pos_start);
            pos_start = i + 1;
            decode_map[tmp_field] = idx;
            printf(" [%s:%d]", tmp_field, idx);
            idx++;
        }
    }  while (tmp_str[i++] != '\0');
    printf("  total: %d\n", num_of_inter_field);
    return;
}

void *read_nonparticipant_data(void *arg) {
    int fd;

    int  num = 1;
    char line[MAXLINE];
    FileBuff fb;

    char buff[300];

    fb.num = 0;
    fb.from = 0;

    DIR    *dp;
    struct dirent *entry;
    struct stat    buf;

    char input_file_path[200];
    char input_file_name[200];
    char curr_file_name[200];

    char output_file_name[200];

    time_t begin_time;
    char str_begin_time[20];
    char *file_timestamp;
    time_t file_time_t;
    char **ler_fields;

    char errinfo[200];
    char msgvalue[20];
    char str_cur_time[20];
    int ret;

    time_t debug_begin_t;
    time_t debug_end_t;
    time_t debug_begin_t2;
    time_t debug_end_t2;
    char debug_begin_time[20];
    //char tmp_str_file_time[200];
    char str_file_time[20];

    char file_no[5];
    time_t file_time;

    File_Info *input_file;
    int        num_input_file     = 0;
    int        maxsize_input_file = 2;

    //文件名过滤
    int  num_ffltr;
    int* num_ffltr_per_pick;
    bool ffltr_default_filtered;
    File_Filter** ffltr;
    char* ffltr_file_prefix;
    bool* ffltr_pass_pick;

    // 固定间隔将缓存导出
    time_t* last_write_time;
    time_t  dump_buffer_timeout;
    time_t  tmp_curr_time;
    bool*   pick_need_dump;
    dump_buffer_timeout = 60 * atoi(cfgXML->getNodeByName("dump_to_file_when_idle", 0)->getNodeByName("timeout", 0)->value);

    pick_need_dump = (bool*) malloc(sizeof(bool) * num_of_picks);
    last_write_time = (time_t*) malloc(num_of_picks * sizeof(time_t));
    tmp_curr_time = time(NULL);
    for (int i = 0; i < num_of_picks; i++)  {
        last_write_time[i] = tmp_curr_time;
        pick_need_dump[i]  = false;
    }

    strcpy(input_file_path, chk.input_file_path);
    printf("文件采集路径：input_file_path=[%s]\n", input_file_path);

    // 旧文件后到
    bool *file_timestamp_order_available = (bool*) malloc(num_of_picks + sizeof(bool));
    char last_file_time[15];
    char last_file_time_new[15];
    int  *file_time_max_delay = (int*) malloc(num_of_picks + sizeof(int));
    memset(last_file_time, '\0', 15);
    memset(last_file_time_new, '\0', 15);

    for (int i = 0; i < num_of_picks; i++) {
        if (0 == strcmp("true", cfgXML->getNodeByName("picks", 0)->getNodeByName("pick", pick_idx[i])->getNodeByName("file_timestamp_order_filter", 0)->property["available"].c_str())) {
            file_timestamp_order_available[i] = true;
        } else file_timestamp_order_available[i] = false;
        file_time_max_delay[i] = 60 * atoi(cfgXML->getNodeByName("picks", 0)->getNodeByName("pick", pick_idx[i])->getNodeByName("file_timestamp_order_filter", 0)->getNodeByName("max_delay", 0)->value);
    }
    char tmp_time[20];
    memset(tmp_time, '\0', 20);

    // 丢弃文件路径
    char path_deserted[200];
    memset(path_deserted, '\0', 200);
    strcpy(path_deserted, cfgXML->getNodeByName("path_deserted_file", 0)->value);

    // 从xml中读取到文件名过滤规则
    num_ffltr = num_of_picks;
    ffltr_pass_pick = (bool*) malloc(sizeof(bool) * num_ffltr);
    num_ffltr_per_pick = (int*) malloc(sizeof(int) * num_ffltr);
    //ffltr_available = (bool*) malloc(sizeof(bool) * num_ffltr);
    ffltr = (File_Filter**) malloc(sizeof(File_Filter*) * num_ffltr);
    for (int i = 0; i < num_ffltr; i++) {
        int idx = pick_idx[i];
        num_ffltr_per_pick[i] = cfgXML->getNodeByName("picks", 0)->getNodeByName("pick", idx)->getNodeByName("file_filter_rules", 0)->getNumOfChildren();
        ffltr[i] = (File_Filter*) malloc(num_ffltr_per_pick[i] * sizeof(File_Filter));
        for (int j = 0; j < num_ffltr_per_pick[i]; j++) {
            memset(ffltr[i][j].file_prefix, '\0', 17);
            memset(ffltr[i][j].time_start,  '\0', 15);
            memset(ffltr[i][j].time_stop,   '\0', 15);
            strcpy(ffltr[i][j].file_prefix, cfgXML->getNodeByName("picks", 0)->getNodeByName("pick", idx)->getNodeByName("file_filter_rules", 0)->getNodeByName("file_filter_rule", j)->getNodeByName("file_prefix", 0)->value);
            strcpy(ffltr[i][j].time_start,  cfgXML->getNodeByName("picks", 0)->getNodeByName("pick", idx)->getNodeByName("file_filter_rules", 0)->getNodeByName("file_filter_rule", j)->getNodeByName("time_start", 0) ->value);
            strcpy(ffltr[i][j].time_stop,   cfgXML->getNodeByName("picks", 0)->getNodeByName("pick", idx)->getNodeByName("file_filter_rules", 0)->getNodeByName("file_filter_rule", j)->getNodeByName("time_stop", 0)  ->value);
            ffltr[i][j].time_pos = atoi(    cfgXML->getNodeByName("picks", 0)->getNodeByName("pick", idx)->getNodeByName("file_filter_rules", 0)->getNodeByName("file_filter_rule", j)->getNodeByName("time_pos", 0)   ->value);
            printf("Rule %d-%d: File_Prefix: [%s] Time_Pos: [%d], Time_Start: [%s], Time_Stop: [%s]\n",
                   pick_idx[i], j, ffltr[i][j].file_prefix, ffltr[i][j].time_pos, ffltr[i][j].time_start, ffltr[i][j].time_stop);
        }
    }
    if (0 == strcmp("true", cfgXML->getNodeByName("picks", 0)->property["default_filtered"].c_str())) {
        ffltr_default_filtered = true;
    } else ffltr_default_filtered = false;

    //读取信令转换规则
    Trans_cfg *tcfg = NULL;
    bool tcfg_availiable = (0 == strcmp(cfgXML->getNodeByName("transform", 0)->property["available"].c_str(), "true"));
    int num_tcfg = cfgXML->getNodeByName("transform", 0)->getNumOfChildren();
    printf("Num of port: %d.\n", num_tcfg);
    tcfg = (Trans_cfg*) malloc(sizeof(Trans_cfg) * num_tcfg);
    if (tcfg_availiable) {
        for (int i = 0; i < num_tcfg; i++) {
            XMLReader *tmp_xml = cfgXML->getNodeByName("transform", 0)->getNodeByName("port", i);
            get_trans_config(tmp_xml, &tcfg[i]);
            printf("in Port[%d]:\n\tfield splited by \"%c\"\n\tevent map: ", i, tcfg[i].field_spliter);
            for (int j = 0; j < tcfg[i].num_evt_map; j++) printf("[%s, %s] ", tcfg[i].evt_map[j][0], tcfg[i].evt_map[j][1]);
            printf("\n\ttransform order: ");
            for (int j = 0; j < tcfg[i].fmt_after; j++) printf("%2d ", tcfg[i].fmt_order[j]);
            printf("\n\tfilename prefix: %s\n", tcfg[i].filename_prefix);
            if (tcfg[i].soe_event[0] != 0) printf("\tswap column %d and %d on event %s\n",
                                                   tcfg[i].soe_field[0],
                                                   tcfg[i].soe_field[1],
                                                   tcfg[i].soe_event
                                                  );
        }
    }

    // 读取校验规则
    get_check_config();

    // 判断是否有停止标志文件
    int fd_exit;

    bool reject_illegal_file = true;
    reject_illegal_file = (0 == strcmp("reject", cfgXML->getNodeByName("illegal_filename", 0)->property["action"].c_str()));

    // 读取 dim_msisdn_prefix
    time_t last_msisdn_table_update_time = 0;
    char msisdn_update_time_str[8];
    int msisdn_update_hour   = 0;
    int msisdn_update_minute = 0;
    char script_command[200];
    char datafile[200];
    strncpy(datafile, cfgXML->getNodeByName("local_msisdn", 0)->getNodeByName("datafile", 0)->value, 200);
    if (chk.local_msisdn_enabled) {
        char tmp_datafile[200];
        sprintf(tmp_datafile, "%s/%s", cfgXML->getNodeByName("local_msisdn", 0)->getNodeByName("path", 0)->value, datafile);
        strcpy(datafile, tmp_datafile);
        msisdn_table_need_update = 1;
        strncpy(msisdn_update_time_str, cfgXML->getNodeByName("local_msisdn", 0)->getNodeByName("update", 0)->value, 6);
        sscanf(msisdn_update_time_str, "%d:%d", &msisdn_update_hour, &msisdn_update_minute);
        printf("Update dim_msisdn_prefix on %02d:%02d or SIGUSR1 catched.\n", msisdn_update_hour, msisdn_update_minute);
    }

    // General_Filter_Rules
    int num_general_filter = cfgXML->getNodeByName("general_filter", 0)->getNumOfChildren();
    general_filter = (gfilter_t *) malloc(sizeof(gfilter_t) * num_general_filter);
    for (int i = 0; i < num_general_filter; i++) {
        char tmp_str[256];
        general_filter[i].hash        = NULL;
        general_filter[i].num_hash    = 0;
        general_filter[i].field       = decode_map[cfgXML->getNodeByName("general_filter", 0)->getNodeByName("filter", i)->property["field"].c_str()];
        general_filter[i].available   = (0 == strcmp("true", cfgXML->getNodeByName("general_filter", 0)->getNodeByName("filter", i)->property["available"].c_str()))? 1: 0;
        general_filter[i].update_date = atoi(cfgXML->getNodeByName("general_filter", 0)->getNodeByName("filter", i)->property["date"].c_str());
        general_filter[i].update_hour = atoi(cfgXML->getNodeByName("general_filter", 0)->getNodeByName("filter", i)->property["hour"].c_str());
        general_filter[i].update_min  = atoi(cfgXML->getNodeByName("general_filter", 0)->getNodeByName("filter", i)->property["min"].c_str());
        strcpy(general_filter[i].filename,  cfgXML->getNodeByName("general_filter", 0)->getNodeByName("filter", i)->value);

        strcpy(tmp_str, cfgXML->getNodeByName("general_filter", 0)->getNodeByName("filter", i)->property["update"].c_str());
        if (strcmp("monthly", tmp_str) == 0) {
            printf("General Filter on %d, using file %s, update on %02d:%02d, %d every month, %s.\n",
                    general_filter[i].field, general_filter[i].filename, general_filter[i].update_hour, general_filter[i].update_min,
                    general_filter[i].update_date, general_filter[i].available? "Enabled": "Disabled");
        } else if (strlen(tmp_str) == 0 || 0 == strcmp("never", tmp_str)) {
            general_filter[i].update_date = -1;
            printf("General Filter on %d, using file %s, %s.\n",
                    general_filter[i].field, general_filter[i].filename, general_filter[i].available? "Enabled": "Disabled");
        } else {
            general_filter[i].update_date = 0;
            printf("General Filter using on %d, file %s, update on %02d:%02d, every day, %s.\n",
                    general_filter[i].field, general_filter[i].filename, general_filter[i].update_hour, general_filter[i].update_min,
                     general_filter[i].available? "Enabled": "Disabled");
        }
        general_filter[i].need_update = 1;
        general_filter[i].last_update_time = 0;
    }

    // ignore "file not found"?
    int ignore_file_not_found = 0;
    ignore_file_not_found = (0 == strcmp("true", cfgXML->getNodeByName("ignore_file_not_found", 0)->value));
    printf("\nIgnore file not found error: %s\n", ignore_file_not_found? "Yes": "No");
    // readdir_flag
    char readdir_flagfile[200];
    sprintf(readdir_flagfile, "../../flag/%s.readdir", this_program_name);

    while (1) {
        //打开输入文件
        FILE * flag_readdir = fopen(readdir_flagfile, "w");
        fclose(flag_readdir);
        if ((dp = opendir(input_file_path)) == NULL) {
            printf("路径[%s]打开错误！\n", input_file_path);
            memset(errinfo, '\0', sizeof(errinfo));
            sprintf(errinfo, "路径[%s]打开错误！", input_file_name);
            write_error_file(chk.err_file_path, MODULE_NAME, "31", 2, errinfo);
            sleep(5*60);
            continue;
        } else {
            num_input_file = 0;
            input_file = (File_Info*) malloc(sizeof(File_Info) * maxsize_input_file);
            memset(input_file, 0, sizeof(File_Info) * maxsize_input_file);

            while ((entry = readdir(dp)) != NULL) {
                memset(input_file_name, 0, sizeof(input_file_name));
                strcpy(input_file_name, input_file_path);
                strcat(input_file_name, "/");
                strcat(input_file_name, entry->d_name);

                if ((strcmp(input_file_name, ".") == 0) || (strcmp(input_file_name, "..") == 0)) continue;
                if (lstat(input_file_name, &buf) < 0) continue;
                if (!S_ISREG(buf.st_mode)) continue;
                if (strstr(input_file_name, ".tmp") != NULL) continue;

                memset(input_file_name, 0, sizeof(input_file_name));
                strcpy(input_file_name, entry->d_name);

                //根据前缀查找对应行转换规则及事件映射规则
                int file_port = -1;
                if (tcfg_availiable) {
                    file_port = check_file_port(input_file_name, tcfg, num_tcfg);
                    if (file_port < 0) {
                        printf("%s, illegal filename, skipped\n", input_file_name);
                        char cmd_tmp[200];
                        sprintf(cmd_tmp, "mv %s/%s %s", input_file_path, input_file_name, path_deserted);
                        system(cmd_tmp);
                        continue;
                    }
                }

                if (num_input_file == maxsize_input_file) {
                    // need realloc
                    maxsize_input_file *= 2;
                    File_Info *tmp_ptr  = input_file;
                    input_file = (File_Info*) malloc(sizeof(File_Info) * maxsize_input_file);
                    memset(input_file, 0,       sizeof(File_Info) * maxsize_input_file);
                    memcpy(input_file, tmp_ptr, sizeof(File_Info) * num_input_file);
                    free(tmp_ptr);
                }

                strcpy(input_file[num_input_file].filename, input_file_name);
                input_file[num_input_file].port       = file_port;
                input_file[num_input_file].len_prefix = strlen(tcfg[file_port].filename_prefix);

                num_input_file++;
            }

            // remove readdir falg
            remove(readdir_flagfile);

            // sort input file
            qsort(input_file, num_input_file, sizeof(File_Info), comp_File_Info);

            // remove duplicate file
            if (num_input_file > 1) {
                int distinct_num_file = 0;
                for (int idx = 1; idx < num_input_file; idx++) {
                    if (0 == strcmp(input_file[distinct_num_file].filename, input_file[idx].filename)) {
                        memcpy(&input_file[distinct_num_file], &input_file[idx], sizeof(File_Info));
                    } else {
                        distinct_num_file++;
                    }
                }
                num_input_file = distinct_num_file + 1;
            }

            if (num_input_file > 0) {
                printf("The following %d files will be processed: \n", num_input_file);
                for (int idx = 0; idx < num_input_file; idx++) printf("\t%s\n", input_file[idx].filename);
            }

            closedir(dp);

            for (int idx = 0; idx < num_input_file; idx++) {
                char tmp_file_name[100];
                memset(tmp_file_name, 0, sizeof(tmp_file_name));
                strcpy(tmp_file_name, input_file[idx].filename);

                memset(input_file_name, '\0', sizeof(input_file_name));
                strcpy(input_file_name, input_file_path);
                strcat(input_file_name, "/");
                strcat(input_file_name, input_file[idx].filename);

                // 过滤文件名
                bool is_file_pass_filter = false;
                for (int i = 0; i < num_ffltr; i++) ffltr_pass_pick[i] = false;

                int ffltr_filtered = 1; // 1 不符合任何规则, 0 有对应规则
                for (int i = 0; i < num_ffltr; i++) {
                    for (int j = 0; j < num_ffltr_per_pick[i]; j++) {
                        if (0 == strncmp(input_file[idx].filename, ffltr[i][j].file_prefix, strlen(ffltr[i][j].file_prefix))) {
                            ffltr_filtered = 0;
                            char* filetime = (input_file[idx].filename + ffltr[i][j].time_pos + strlen(ffltr[i][j].file_prefix));
                            if (!isTimeBetween(filetime, ffltr[i][j].time_start, ffltr[i][j].time_stop)) continue;
                            //printf("%s pass filter rule: %d-%d\n", input_file[idx].filename, pick_idx[i], j);
                            ffltr_pass_pick[i] = true;
                            // 删除旧文件后到
                            if (file_timestamp_order_available[i] && strlen(last_file_time) != 0) {
                                memset(tmp_time, '\0', 20);
                                strncpy(tmp_time, input_file[idx].filename + strlen(ffltr[i][j].file_prefix), 14);
                                int tmp = yyyymmddhhmmss_to_sec(last_file_time) - yyyymmddhhmmss_to_sec(tmp_time);
                                if (tmp > file_time_max_delay[i]) {
                                    ffltr_pass_pick[i] = false;
                                    printf("%s skipped, which should have arrived %dmin ago.\n", input_file[idx].filename, ((tmp - file_time_max_delay[i]) / 60));
                                }
                            }
                            break;
                        }
                    }
                }

                if (ffltr_filtered == 1) for (int i = 0; i < num_ffltr; i++) ffltr_pass_pick[i] = !ffltr_default_filtered;
                for (int i = 0; i < num_ffltr; i++) is_file_pass_filter = is_file_pass_filter || ffltr_pass_pick[i];

                if (!is_file_pass_filter) {
                    char cmd_tmp[200];
                    // log?
                    sprintf(cmd_tmp, "mv %s/%s %s", input_file_path, input_file[idx].filename, path_deserted);
                    //printf("%s\n", cmd_tmp);
                    system(cmd_tmp);
                    printf("%s filtered.\n", input_file[idx].filename);
                    continue;
                }

                memset(lers->current_file_name, '\0', sizeof(lers->current_file_name));
                strcpy(lers->current_file_name, tmp_file_name);
                lers->total                  = 0;
                lers->err_total              = 0;
                lers->err_filed_sum_total    = 0;
                lers->err_field_null_total   = 0;
                lers->err_str_len_total      = 0;
                lers->err_str_long_total     = 0;
                lers->err_date_invalid_total = 0;
                lers->err_data_less_total    = 0;
                lers->err_data_more_total    = 0;
                lers->err_data_delay_total   = 0;
                lers->err_int_invalid_total  = 0;
                lers->err_int_range_total    = 0;

                memset(str_begin_time, '\0', sizeof(str_begin_time));
                begin_time = get_system_time(str_begin_time);

                memset(last_file_time_new, '\0', 15);
                strncpy(last_file_time_new, input_file[idx].filename + input_file[idx].len_prefix, 14);

                init_file_info(tmp_file_name, &tcfg[input_file[idx].port], file_no, str_file_time, file_time);
                if (file_time < 0) {
                    memset(errinfo, '\0', sizeof(errinfo));
                    sprintf(errinfo, "原始信令的文件[%s]的命名有问题，请立即检查！", input_file_name);
                    printf("Err:%s\n", errinfo);
                    write_error_file(chk.err_file_path, MODULE_NAME, "32", 2, errinfo);
                    if (reject_illegal_file) {
                        char cmd_buf[201];
                        memset(cmd_buf, 0, sizeof(cmd_buf));
                        sprintf(cmd_buf, "mv %s/%s %s", input_file_path, tmp_file_name, path_deserted);
                        system(cmd_buf);
                        continue;
                    }
                }

                //每次处理的文件日期
                for (int i = 0; i < num_of_picks; i++) {
                    // if (ffltr_pass_pick[i]) {
                        lb[i].filetime = file_time;
                        memset(lb[i].file_number, '\0', sizeof(lb[i].file_number));
                        strcpy(lb[i].file_number, file_no);
                    // }
                }

                for (int i = 0; i < num_of_picks; i++) {
                    if (ffltr_pass_pick[i]) {
                        if (lb[i].filetime != ana.event_time_begin[i]) {
                            for (int part = 0; part < output_per_pick[i]; part++) {
                                int p = output_offset[i] + part;
                                if (ana.event_time_begin[p] == -1) {
                                    ana.event_time_begin[p] = lb[i].filetime ;
                                    strcpy(ana.file_number[p], lb[i].file_number);
                                } else {
                                    ana.timeout[p] = 1;
                                    //printf("FileTime Changed in pick %d\n", pick_idx[i]);
                                    WriteResult(p, "");

                                    ana.event_time_begin[p]  = lb[i].filetime ;
                                    strcpy(ana.file_number[p], lb[i].file_number);
                                }
                            }
                        }
                    }
                }

                if (chk.local_msisdn_enabled) {
                    if (((file_time / 60 - msisdn_update_minute) / 60 + 8) % 24 == msisdn_update_hour) {
                        msisdn_table_need_update = 1;
                    }

                    // need update msisdn_table?
                    if (msisdn_table_need_update) {
                        if (last_msisdn_table_update_time == 0 || file_time - last_msisdn_table_update_time > 3600) {
                            printf("reloading dim_msisdn_prefix...");
                            int ret;
                            ret = read_msisdn_table_from_file(datafile);
                            if (ret) {
                                printf("Failed, ERROR: %s, dim_msisdn_prefix not loaded.\n");
                            }
                            last_msisdn_table_update_time = file_time;
                            printf("Done!\n");
                        } else {
                            printf("Updated less then an hour, ignpred.\n");
                        }
                        msisdn_table_inited = 1;
                        msisdn_table_need_update = 0;
                    }
                }

                // general_filter
                for (int ig = 0; ig < num_general_filter; ig++) {
                    if (general_filter[ig].available && general_filter[ig].update_date >= 0) {
                        char file_date[4] = {0, 0, 0, 0};
                        strncpy(file_date, str_file_time + 8, 2);
                        if (    (general_filter[ig].update_date == 0 && ((file_time / 60 - general_filter[ig].update_min) / 60 + 8) % 24 == general_filter[ig].update_hour)
                             || (general_filter[ig].update_date != 0 && ((file_time / 60 - general_filter[ig].update_min) / 60 + 8) % 24 == general_filter[ig].update_hour
                                                                     && general_filter[ig].update_date == atoi(file_date))) general_filter[ig].need_update = 1;
                        if (general_filter[ig].need_update) {
                            if (general_filter[ig].last_update_time == 0 || file_time - general_filter[ig].last_update_time > 3600) {
                                printf("(Re)loading %s...", general_filter[ig].filename);
                                int ret = read_general_filter(general_filter[ig].filename, &general_filter[ig]);
                                if (ret) {
                                    printf("FAILED!!!\n");
                                } else {
                                    printf("Done!\n");
                                    general_filter[ig].last_update_time = file_time;
                                }
                            } else {
                                printf("Update less than a hour, ignored.\n");
                            }
                            general_filter[ig].need_update = 0;
                        }
                    }
                }

                printf("当前处理文件 %s, begin_time: [%s], ", tmp_file_name, str_begin_time);
                printf("port: %d, pick: ", input_file[idx].port);
                for (int j = 0; j < num_ffltr - 1; j++) if (ffltr_pass_pick[j]) printf("%d, ", j);
                if (ffltr_pass_pick[num_ffltr - 1]) printf("%d\n", num_ffltr - 1);

                // 处理文件中的每条话单
                if ((fd = open(input_file_name, O_RDONLY)) == -1) {
                    printf("ERROR! cannot open: %s, %s\n", input_file_name,
                            ignore_file_not_found? "Ignored": "Exiting");
                    if (ignore_file_not_found) {
                        continue;
                    } else {
                        exit(1);
                    }
                }

                num = 1;
                while (num > 0) {
                    // 取得文件中的每条记录
                    memset(line, 0, sizeof(line));
                    num = ReadLine(fd, &fb, line, MAXLINE);    // 当num==0时，表示已到达文件尾或是无可读取的数据
                    if (num > 0) {
                        lers->total ++;

                        bool ret_trans_line;
                        if (tcfg_availiable) ret_trans_line = trans_line(line, file_time, &tcfg[input_file[idx].port]);
                        else ret_trans_line = trans_line(line, file_time, NULL);

                        if (!ret_trans_line) {
                            lers->err_total ++;
                            continue;
                        }

                        if (chk.local_msisdn_enabled) {
                            ret = append_city_code(line, ',');
                            if (ret) NULL;
                        }

                        // general_filter
                        if (filtered_general(line, general_filter, num_general_filter)) continue;
                        //int fffxxx_ret = filtered_general(line, general_filter, num_general_filter);
                        //printf("%d, %s\n", fffxxx_ret, line);

                        strcat(line, "\n");

                        for (int i = 0; i < num_of_picks; i++) {
                            if (ffltr_pass_pick[i]) {
                                pthread_mutex_lock(&lb[i].mutex);
                                while (lb[i].empty < MAXLINE) {
                                    //printf("wait\n");
                                    pthread_cond_wait(&lb[i].less, &lb[i].mutex);
                                }
                                num = PutLine(line, &lb[i]);
                                pros_cnt++;
                                pthread_cond_signal(&lb[i].more);
                                pthread_mutex_unlock(&lb[i].mutex);
                            }
                        }
                    } /* if */
                } /* while(num > 0) */
                // 更新最后写入时间
                tmp_curr_time = time(NULL);
                for (int i = 0; i < num_of_picks; i++) {
                    if (ffltr_pass_pick[i]) {
                        last_write_time[i] = tmp_curr_time;
                        pick_need_dump[i] = true;
                    }
                }

                printf("Finished %s\n", input_file_name);
                close(fd);

                //写文件处理日志
                get_system_time_2(str_cur_time);
                pthread_mutex_lock(&pthread_logfile_mutex);
                sprintf(msgvalue, "%d", lers->total);
                ret = write_log_file(chk.del_file_path, MODULE_NAME, S_ID_TOTAL, tmp_file_name, str_cur_time, msgvalue);
                sprintf(msgvalue, "%d", lers->err_total);
                ret = write_log_file(chk.del_file_path, MODULE_NAME, S_ID_ERR_TOTAL, tmp_file_name, str_cur_time, msgvalue);
                sprintf(msgvalue, "%d", lers->err_filed_sum_total);
                ret = write_log_file(chk.del_file_path, MODULE_NAME, S_ID_LINE_SUM_ERR, tmp_file_name, str_cur_time, msgvalue);
                sprintf(msgvalue, "%d", lers->err_field_null_total);
                ret = write_log_file(chk.del_file_path, MODULE_NAME, S_ID_FIELD_NULL_ERR, tmp_file_name, str_cur_time, msgvalue);
                sprintf(msgvalue, "%d", lers->err_str_len_total);
                ret = write_log_file(chk.del_file_path, MODULE_NAME, S_ID_CHAR_LEN_ERR, tmp_file_name, str_cur_time, msgvalue);
                sprintf(msgvalue, "%d", lers->err_str_long_total);
                ret = write_log_file(chk.del_file_path, MODULE_NAME, S_ID_VACHAR_LEN_ERR, tmp_file_name, str_cur_time, msgvalue);
                sprintf(msgvalue, "%d", lers->err_date_invalid_total);
                ret = write_log_file(chk.del_file_path, MODULE_NAME, S_ID_DATE_INVALID, tmp_file_name, str_cur_time, msgvalue);
                sprintf(msgvalue, "%d", lers->err_data_less_total);
                ret = write_log_file(chk.del_file_path, MODULE_NAME, S_ID_DATE_MIN_RANGE, tmp_file_name, str_cur_time, msgvalue);
                sprintf(msgvalue, "%d", lers->err_data_more_total);
                ret = write_log_file(chk.del_file_path, MODULE_NAME, S_ID_DATE_MAX_RANGE, tmp_file_name, str_cur_time, msgvalue);
                sprintf(msgvalue, "%d", lers->err_data_delay_total);
                ret = write_log_file(chk.del_file_path, MODULE_NAME, S_ID_DATE_FILE_RANGE, tmp_file_name, str_cur_time, msgvalue);
                sprintf(msgvalue, "%d", lers->err_int_invalid_total);
                ret = write_log_file(chk.del_file_path, MODULE_NAME, S_ID_INT_INVALID, tmp_file_name, str_cur_time, msgvalue);
                sprintf(msgvalue, "%d", lers->err_int_range_total);
                ret = write_log_file(chk.del_file_path, MODULE_NAME, S_ID_INT_RANGE, tmp_file_name, str_cur_time, msgvalue);

                pthread_mutex_unlock(&pthread_logfile_mutex);
                if (ret <= 0) {
                    write_deal_log_error();
                }

                remove(input_file_name);

                fd_exit = open(str_exit_file_name, O_RDONLY);
                if (fd_exit > 0) {
                    char time_buffer[128];
                    time_t timestamp_start = time(NULL);
                    strftime(time_buffer, 127, "%04Y-%02m-%02d %02H:%02M:%02S", localtime(&timestamp_start));
                    printf("[%s] 检测到停止文件存在，程序退出！\n", time_buffer);
                    exitflag = 1;
                    close(fd_exit);
                    remove(str_exit_file_name);
                    break;
                }
                close(fd_exit);
                // flush stdout and stderr to update output in nohup
                fflush(stdout);
                fflush(stderr);
            } //end for( ... each file in file_list ... )

            // 获取上次文件时间
            if ((strlen(last_file_time) == 0) || yyyymmddhhmmss_to_sec(last_file_time_new) > yyyymmddhhmmss_to_sec(last_file_time)) {
                memset(last_file_time, '\0', 15);
                strcpy(last_file_time, last_file_time_new);
                //printf("Last file timestamp: %s\n", last_file_time);
            }

            free(input_file);
            input_file = NULL;

            if (exitflag <= 0) {
                fd_exit = open(str_exit_file_name, O_RDONLY);
                if (fd_exit > 0) {
                    char time_buffer[128];
                    time_t timestamp_start = time(NULL);
                    strftime(time_buffer, 127, "%04Y-%02m-%02d %02H:%02M:%02S", localtime(&timestamp_start));
                    printf("[%s] 检测到停止文件存在，程序退出！\n", time_buffer);
                    exitflag = 1;
                    close(fd_exit);
                    remove(str_exit_file_name);
                }
            }

            if (exitflag > 0) {
                file_read_exitflag = 1;
                for (int i = 0;i < num_of_picks;i++) {
                    pthread_cond_signal(&lb[i].more);
                }

                memset(errinfo, '\0', sizeof(errinfo));
                sprintf(errinfo, "信令预处理文件读取线程退出,立即检查！");
                printf("%s\n", errinfo);
                write_error_file(chk.err_file_path, MODULE_NAME, "64", 2, errinfo);

                //if (ffltr_available != NULL) free(ffltr_available);
                if (pick_idx != NULL) free(pick_idx);
                if (ffltr != NULL) {
                    for (int i = 0; i < num_ffltr; i++) {
                        if (ffltr[i] != NULL) free(ffltr[i]);
                    }
                    free(ffltr);
                }
                if (input_file                     != NULL) free(input_file);
                if (num_ffltr_per_pick             != NULL) free(num_ffltr_per_pick);
                if (ffltr_pass_pick                != NULL) free(ffltr_pass_pick);
                if (file_time_max_delay            != NULL) free(file_time_max_delay);
                if (file_timestamp_order_available != NULL) free(file_timestamp_order_available);
                if (last_write_time                != NULL) free(last_write_time);

                break;
                // pthread_exit(0);
            }
        }

        // 一定时间没有写入
        tmp_curr_time = time(NULL);
        for (int i = 0; i < num_of_picks; i++) {
            if (pick_need_dump[i] && tmp_curr_time - last_write_time[i] > dump_buffer_timeout) { // 向lb中写入dump_sig
                last_write_time[i] = tmp_curr_time;
                pthread_mutex_lock(&lb[i].mutex);
                while (lb[i].empty < MAXLINE) pthread_cond_wait(&lb[i].less, &lb[i].mutex);
                // printf("write buffer timeout, write dump_sig.\n");
                PutLine(dump_sig, &lb[i]);
                pick_need_dump[i] = false;
                pthread_cond_signal(&lb[i].more);
                pthread_mutex_unlock(&lb[i].mutex);
            }
        }
        sleep(chk.sleep_interval);
    } /* while(1) */
    return NULL;
}

int comp_File_Info(const void* file1, const void* file2) {
    return ( strcmp( ((File_Info*)file1)->filename + ((File_Info*)file1)->len_prefix,
                      ((File_Info*)file2)->filename + ((File_Info*)file2)->len_prefix
                    ));
}

//判断time是否位于start和stop之间
//只要是相同格式的时间即可
bool isTimeBetween(const char* time, const char* start, const char* stop) {
    return (strncmp(time, start, strlen(start)) >= 0) && (strncmp(time, stop, strlen(stop)) < 0);
}

// 输入时间格式：yyyymmddhhmmss, 基准时间20000101000000
unsigned long int yyyymmddhhmmss_to_sec(char* time) {
    unsigned long int ret = 0;
    int tmp = 0;
    ret = 1000 * ((int)time[0] - (int)'0') + 100 * ((int)time[1] - (int)'0') + 10 * ((int)time[2] - (int)'0') + ((int)time[3] - (int)'0') - 2000;
    tmp = ret / 4 - ret / 400;
    ret = ret * 365 + tmp;
    tmp = 0;
    tmp = 10 * ((int)time[4]  - (int)'0') + (int)time[5]  - (int)'0';
    if (tmp <= 3) ret--;
    ret += day_of_year[tmp];
    tmp = 10 * ((int)time[6]  - (int)'0') + (int)time[7]  - (int)'0';
    ret = (ret + tmp) * 24;
    tmp = 10 * ((int)time[8]  - (int)'0') + (int)time[9]  - (int)'0';
    ret = (ret + tmp) * 60;
    tmp = 10 * ((int)time[10] - (int)'0') + (int)time[11] - (int)'0';
    ret = (ret + tmp) * 60;
    tmp = 10 * ((int)time[12] - (int)'0') + (int)time[13] - (int)'0';
    ret = (ret + tmp);
    return ret;
}

//输出文件函数
int WriteResult(int ruleid, char *src) {
    char *dest;
    char tmp[500];
    size_t    left, from;
    int ret;

    //线程指向的缓冲的首地址
    dest = ana.fbuf + ruleid * OUTBUFFSIZE;
    pthread_mutex_lock(&ana.mutex[ruleid]);

    if (ana.timeout[ruleid] == 1) {
        ana.timeout[ruleid] = 0;
        if (ana.counts[ruleid] > 0) {
            if (ana.fd[ruleid] <= 0) {
                ana.fd[ruleid] = open(ana.fname[ruleid], O_WRONLY | O_CREAT, 0644);
            }
            write(ana.fd[ruleid], dest, ana.num[ruleid]);
            close(ana.fd[ruleid]);

            // 取得事件起始点时间戳
            tm *event_time_begin;
            char str_event_time_begin[20];
            memset(str_event_time_begin, '\0', sizeof(str_event_time_begin));
            event_time_begin = get_str_time(str_event_time_begin, ana.event_time_begin[ruleid]);

            memset(tmp, '\0', sizeof(tmp));
            char fname_tmp[200];
            memset(fname_tmp, '\0', sizeof(fname_tmp));

            strncpy(fname_tmp, ana.fname[ruleid], strlen(ana.fname[ruleid]) - 4);
            //由输出系统时间戳，改为输出事件起始点时间戳
            sprintf(tmp, "%s_%s_%s", fname_tmp, str_event_time_begin, ana.file_number[ruleid]);
            //printf("输出文件名为=[%s]\n", tmp);
            rename(ana.fname[ruleid], tmp);

            /*bzero(dest, FBUFFSIZE);*/
            memset(dest, '\0', OUTBUFFSIZE);
            ana.fd[ruleid] = Open(ana.fname[ruleid], O_WRONLY | O_CREAT);
            ana.num[ruleid] = 0;
            ana.counts[ruleid] = 0;
        }
    } else {
        left = strlen(src);
        from = 0;
        if (left != 0) ana.counts[ruleid] ++;
        while (left > 0) {
            if (left + ana.num[ruleid] <= OUTBUFFSIZE) {
                memcpy(dest + ana.num[ruleid], src + from, left);
                ana.num[ruleid] += left;
                left = 0;
            } else {
                memcpy(dest + ana.num[ruleid], src + from, OUTBUFFSIZE - ana.num[ruleid]);
                left -= (OUTBUFFSIZE - ana.num[ruleid]);
                from += (OUTBUFFSIZE - ana.num[ruleid]);

                if (ana.fd[ruleid] > 0) {
                    write(ana.fd[ruleid], dest, OUTBUFFSIZE);
                } else {
                    ana.fd[ruleid] = open(ana.fname[ruleid], O_WRONLY | O_CREAT, 0644);
                    if (ana.fd[ruleid] > 0) {
                        write(ana.fd[ruleid], dest, OUTBUFFSIZE);
                    } else {
                        perror("Can't open result file.");
                        exit(-1);
                    }
                }
                /*bzero(dest, FBUFFSIZE);*/
                memset(dest, '\0', OUTBUFFSIZE);
                ana.num[ruleid] = 0;
            }
        }
    }
    //释放锁
    pthread_mutex_unlock(&ana.mutex[ruleid]);

    return 0;
}

void *statistics_thread(void* dummy) {
    pros_1 = time(NULL) - 1;
    double dps = 0;
    while (1) {
        pros_2 = time(NULL);
        dps = pros_cnt / (double)(pros_2 - pros_1);
        if (dps > 0.01) printf("saPretreatment: %5.2f data/s\n", dps);
        pros_1 = pros_2;
        pros_cnt = 0;
        sleep(60);
        if (exitflag > 1) break;
    }
    return NULL;
}

// 添加city_code, prov_code, local_prov_flag
static int append_city_code(char * line, const char spliter) {
    // first 7-digit of msisdn
    char msisdn[8];
    unsigned int msisdn_int = 0;
    int msisdn_idx  = decode_map["msisdn"];
    int comma_count = 0;
    int pos = 0;
    int ret = 1;

    char append_str[64];
    short city_code  = 0;
    short prov_code  = 0;
    short local_flag = 0;

    // get first 7 digit from line
    while (comma_count != msisdn_idx) if (line[pos++] == spliter) comma_count++;
    if (0 == strncmp(line + pos, "86", 2)) pos += 2;
    else if (0 == strncmp(line + pos, "084", 3)) pos += 3;
    else if (0 == strncmp(line + pos, "0086", 4)) pos += 4;

    strncpy(msisdn, line + pos, 7);
    msisdn[7] = '\0';
    msisdn_int = (unsigned int)atoi(msisdn);
    if (msisdn_int / 1000000 != 1) { // don't even bother to find
        strcpy(append_str, ",,");
    } else {
        ret = msisdn_prefix_find(msisdn_int, &city_code, &prov_code, &local_flag);
        if (!ret) { // found
            snprintf(append_str, 63, "%hd,%hd,%hd", local_flag, prov_code, city_code);
        } else { // not found
            strcpy(append_str, ",,");
        }
    }

    strcat(line, append_str);
    return 0;
}

static int read_general_filter(const char * file, gfilter_t * gfilter) {
    FILE * filter_file;
    int    lines = 0;
    int    hash_lines = 0;
    char   line[256];
    ht_t * ht = NULL;
    ht_t * this_node  = NULL;
    int    in_hash = 0;
    int    i = 0;
    int    conflict   = 0;
    int    this_depth = 0;
    int    dup_line   = 0;
    unsigned int hash = 0;

    filter_file = fopen(file, "r");
    if (!filter_file) {
        return 1;
    }

    while (EOF != fscanf(filter_file, "%s\n", line, 255)) lines++;
    hash_lines = lines * 1.6;
    fseek(filter_file, 0, SEEK_SET);

    ht = (ht_t*) malloc(sizeof(ht_t) * hash_lines);
    if (!ht) return 2;
    memset(ht, 0, sizeof(ht_t) * hash_lines);

    // read every line in file
    while (EOF != fscanf(filter_file, "%s\n", line, 255)) {
        this_depth = 1;
        hash = BKDRHash(line) % hash_lines;
        if (!ht[hash].value[0]) { // hash node not used
            this_node = &ht[hash];
        } else { // hash_node used
            this_node = &ht[hash];
            in_hash   = 0;
            while (this_node->next) {
                if (strcmp(this_node->value, line) == 0) { // already in
                    in_hash = 1;
                    this_depth = 0;
                    dup_line++;
                    break;
                } else {
                    this_depth++;
                    this_node = this_node->next;
                }
            }
            if (in_hash) continue;
            this_node->next = (ht_t*)malloc(sizeof(ht_t));
            this_node = this_node->next;
            memset(this_node, 0, sizeof(ht_t));
        }
        // flag new hash_node
        strncpy(this_node->value, line, sizeof(this_node->value));
        conflict += this_depth;
    }
    fclose(filter_file);

    printf(" Depth: %.3f ", conflict * 1.0 / (lines - dup_line));

    // free old hash table
    if (gfilter->hash) {
        for (i = 0; i < gfilter->num_hash; i++) {
            ht_t * tmp_node = NULL;
            this_node = gfilter->hash[i].next;
            while (this_node) {
                tmp_node  = this_node;
                this_node = tmp_node->next;
                free(tmp_node);
            }
        }
        free(gfilter->hash);
    }

    gfilter->hash     = ht;
    gfilter->num_hash = hash_lines;

    return 0;
}

static int in_general_filter(const char * word, gfilter_t * gfilter) {
    ht_t * this_node = NULL;
    unsigned int hash = 0;

    hash = BKDRHash(word);
    this_node = &gfilter->hash[hash % gfilter->num_hash];

    while (this_node && this_node->value[0]) {
        if (0 == strcmp(word, this_node->value)) return 1;
        this_node = this_node->next;
    }

    return 0;
}

// BKDR Hash Function
static inline unsigned int BKDRHash(const char * str) {
	static unsigned int seed = 1313; // 31 131 1313 13131 131313 etc..
	unsigned int hash = 0;

	while (*str) hash = hash * seed + (*str++);
	return (hash & 0x7FFFFFFF);
}

static int filtered_general(const char * line, gfilter_t * gfilter, int num) {
    int   i = 0;
    char  word_temp[64];
    int   pos_l = 0;
    int   pos_w = 0;
    int   f = 0;
    int   needed = 0;

    for (i = 0; i < num; i++) {
        if (!gfilter[i].available || !gfilter[i].hash) continue;

        pos_l  = 0;
        pos_w  = 0;
        f      = 0;
        needed = 0;
        // get specificated field;
        while (line[pos_l]) {
            if (line[pos_l] == ',') f++;
            if (f > gfilter[i].field) {
                break;
            }
            if (f == gfilter[i].field) word_temp[pos_w++] = line[pos_l];
            pos_l++;
        }
        word_temp[pos_w] = (char)0;
        if (in_general_filter(word_temp, &general_filter[i])) return (i + 1);
    }
    return 0;
}

