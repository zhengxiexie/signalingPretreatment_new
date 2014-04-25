#include<iostream>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<time.h>
#include <sys/times.h>

#include "sscheck.h"
#include "sspublic.h"

ListLogBuff llb;

char* isNumber(char *str, bool if_null, int min_value, int max_value) {
    int i   = 0;
    int len = strlen(str);

    if (len == 0 && !if_null) {
        lers->err_field_null_total++;
        return S_ID_FIELD_NULL_ERR;
    } else if (len == 0) {
        return NULL;
    }

    int str_int = atoi(str);
    
    //判断是否是整型
    int tmp_int = str_int;
    int nlen    = 1;

    while ((tmp_int = tmp_int / 10) != 0) nlen++;
    if (str_int < 0) nlen++;

    if (nlen != len || (str_int == 0 && str[0] != '0')) {
        //非整型字符；
        lers->err_int_invalid_total++;
        return S_ID_INT_INVALID;
    }
    //判断结束

    if (str_int < min_value || str_int > max_value) {
        //printf("int value range false\n ");
        lers->err_int_range_total++;
        return S_ID_INT_RANGE;
    }

    return NULL;
}

bool lengthCheck(char *str, int len) {
    if (strlen(str) == len)return true; else return false;
}

bool dateFormatCheck(char *str, char *format) {
    int str_len = strlen(str);
    if (str_len != strlen(format)) return false;
    for (int i = 0; i < strlen(str); i++) {
        if (format[i] != 'x' && format[i] != str[i]) return false;
        else if (format[i] == 'x' && (str[i] < '0' || str[i] > '9')) return false;
    }
    return true;
}

bool dataRangeCheck(char *date, char *lowerLimit, char *uperLimit) {
    return (strcmp(date, lowerLimit) >= 0 && strcmp(date, uperLimit) <= 0);
}

char* check_varchar(char* str, int len, bool can_null) {
    int str_len = strlen(str);
    if (str_len == 0 && !can_null) {
        lers->err_field_null_total++;
        return S_ID_FIELD_NULL_ERR;
    }

    if (len != 0 && str_len > len) {
        lers->err_str_long_total++;
        return S_ID_VACHAR_LEN_ERR;
    }

    return NULL;
}

char* check_char(char* str, int len, bool can_null, bool numeral) {
    int str_len = strlen(str);
    if (str_len == 0 && !can_null) {
        lers->err_field_null_total++;
        return S_ID_FIELD_NULL_ERR;
    }

    if (len != 0 && str_len != len) {
        lers->err_str_len_total++;
        return S_ID_CHAR_LEN_ERR;
    }

    if (numeral) {
        for (int i = 0; i < str_len; i++) {
            if (str[i] < '0' || str[i] > '9') {
                lers->err_int_invalid_total++;
                return S_ID_INT_INVALID;
            }
        }
    }

    return NULL;
}

char* check_date(char* time_str, time_t file_time, time_t min_time, time_t max_time,
                 time_t time_before, time_t time_after, char* time_fmt) {
    struct tm tmp_tm;
                   
    if (strptime(time_str, time_fmt, &tmp_tm) == NULL) {
        //printf("format false\n ");
        lers->err_date_invalid_total++;
        return S_ID_DATE_INVALID;
    } else {       
        time_t ler_tt = mktime(&tmp_tm);
        if (ler_tt != -1) {
            time_t diff = 0;
            diff = ler_tt - min_time;
            if (min_time != -1 && diff < 0) {
                //printf("date < min \n ");
                lers->err_data_less_total++;
                return S_ID_DATE_MIN_RANGE;
            }      
            diff = max_time - ler_tt;
            if (max_time != -1 && diff < 0) {
                //printf("date > max \n ");
                lers->err_data_more_total++;
                return S_ID_DATE_MAX_RANGE;
            }      
                   
            diff = ler_tt - file_time;
            if (time_after == 0 || (diff < time_after && diff > -time_before)) {
                return NULL;
            } else {
                lers->err_data_delay_total++;
            }
        } else {
            //printf("get ler date false \n ");
        }
    }
    return S_ID_DATE_FILE_RANGE;
}

void get_file_time(char* time_str, time_t tt) {
    struct tm temp_tm;
    if (strptime(time_str, "", &temp_tm) == NULL) {
        //printf(" ");
    } else {
        time_t  tt = mktime(&temp_tm);
        if (tt == -1) {
            //printf(" ");
        }
    }
}

char * get_file_timestamp(char* fileName) {
    char *delim = "_";
    char *p;
    char *other;
    char *date_str;

    strtok(fileName, delim);
    while ((p = strtok(NULL, delim))) {
        other = p;
    }

    date_str = strtok(other, ".");

    return strcat(date_str, "00");
}

char * get_file_timestamp2(char* fileName) {
    char *delim = "_";
    char *p;
    char *other;
    char *date_str;

    strtok(fileName, delim);
    while ((p = strtok(NULL, delim))) {
        other = p;
    }

    date_str = strtok(other, ".");

    return date_str;
}

//将明细记录写入明细缓存中，如果缓存满，将缓存写入tmp文件，继续写缓存；
int write_list_log_buff(char *data_file_name, int row_num, char *errcode, char *error_line) {
    int  ret = 1 ;
    char filename[250];
    char line[500];
    int  linelen = 0;
    int  ltmpfd;

    memset(line, '\0', sizeof(line));
    sprintf(line, "%s,%d,%s,%s\n", data_file_name, row_num, errcode, error_line);
    pthread_mutex_lock(&llb.lmutex);
    linelen = strlen(line);
    //printf("llb. linelen:[%d],lnum:[%d], fbuffersize:[%d]\n", linelen, llb.lnum, FBUFFSIZE);
    if ((linelen + llb.lnum) < LLOGBUFFSIZE) {
        strcpy(llb.lbuf + llb.lnum, line);
        llb.lnum += linelen;
        linelen = 0;
    } else {
        if (llb.fd <= 0) {
            //printf("llb. open file again.[%s]\n",llb.ltmpfname);
            llb.fd = open(llb.ltmpfname,  O_WRONLY | O_CREAT, 0644);
        }

        if (llb.fd > 0) {
            //printf("llb. write to tmp file.[%d]\n",llb.fd);
            WriteN(llb.fd, llb.lbuf, llb.lnum);
        } else {
            perror("Can't open result file for list log file\n");
            exit(-1);
        }

        memset(llb.lbuf, '\0', LLOGBUFFSIZE);
        llb.lnum = 0;
        strcpy(llb.lbuf, line);
        llb.lnum += linelen;
        linelen = 0;
    }

    pthread_mutex_unlock(&llb.lmutex);
    return ret;
}

//将缓存和临时文件tmp中的明细记录写入明细文件中；
int  write_list_log_file() {
    int ret = 0;
    int ltmpfd;
    int lfd;
    int num;

    char buff[300];

    pthread_mutex_lock(&llb.lmutex);

    if (llb.lnum > 0) {
        if (llb.fd <= 0) {
            //printf("llb. open file again.[%s]\n",llb.ltmpfname);
            llb.fd = open(llb.ltmpfname,  O_WRONLY | O_CREAT, 0644);
        }

        if (llb.fd > 0) {
            WriteN(llb.fd, llb.lbuf, llb.lnum);

            memset(llb.lbuf, '\0', LLOGBUFFSIZE);
            llb.lnum = 0;

            close(llb.fd);
            llb.fd = NULL;
        } else {
            perror("Can't open result file for list log file\n");
            exit(-1);
        }

        printf("写入正式明细文件。");

        memset(buff, 0, sizeof(buff));
        sprintf(buff, "mv %s %s", llb.ltmpfname, llb.lfname);
        printf("llb...buff = [%s]\n", buff);
        system(buff);
    }
    pthread_mutex_unlock(&llb.lmutex);

    return ret;
}


