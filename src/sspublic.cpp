#include "sspublic.h"
#include <sys/time.h>
//#include "AIXMLReader.h"

LerStatis *lers;

int Discard(char *line, int flds) {
    int i = 0;
    char dest[MAXLINE];
    char *word[20];
    int  len[20];
    int  num;


    strcpy(dest, line);
    num = Split(dest, ',', MAXLINE, word, len);


    if (num != flds) return 1;

    return 0;
}

int PutLine(char *line,  LineBuff *lb) {
    int plen;
    int i, len;

    len = strlen(line);

    if (lb->empty >= len) {
        plen = LBSIZE - lb->to;
        if (plen < len) {
            for (i = 0; i < plen; i++)
                lb->buf[lb->to+i] = line[i];

            for (i = 0; i < len - plen; i++)
                lb->buf[i] = line[plen+i];

            lb->to = len - plen;
        } else {
            /*bcopy(line, lb->buf+lb->to, len);*/
            memcpy(lb->buf + lb->to, line, len);
            lb->to = (lb->to + len) % LBSIZE;
        }
        lb->empty -= len;
        lb->lines++;

        return len;
    }
    return -1;
}

int GetLine(LineBuff *lb, char *line) {
    int i, j, start, num;
    if (lb->lines > 0) {
        start = lb->from;
        for (i = start; i < LBSIZE; i++) {
            if (lb->buf[i] == '\n' || lb->buf[i] == '\r') {
                num        = i - start + 1;
                lb->from   = (lb->from + num) % LBSIZE;
                lb->empty += num;
                lb->lines--;
                return num;
            }
            line[i - start] = lb->buf[i];
        }

        for (j = 0; j < start; j++) {
            if (lb->buf[j] == '\n' || lb->buf[i] == '\r') {
                num        = i - start + j + 1;
                lb->from   = j + 1;
                lb->empty += num;
                lb->lines--;
                return num;
            }
            line[i-start+j] = lb->buf[j];
        }
    }
    return -1;
}

int Split(char *line, char ch, int maxw, char *word[], int len[]) {
    int i, j, num;
    char lastch;
    int lasti;

    num = strlen(line);

    lastch = '\0';
    lasti = 0;
    for (i = 0, j = 0; i < num; i++) {
        if (line[i] == ch || line[i] == '\n') {
            line[i] = '\0';
            len[j] = i - lasti;
            lasti = i + 1;
        }

        if (lastch == '\0') {
            word[j] = (char *)(line + i);
            j++;
            if (j >= maxw)
                return j;
        }

        lastch = line[i];
    }


    len[j] = i - lasti;
    return j;
}

int Split_decode(char *line, char ch, int maxw, char *word[], int len[]) {
    int i, j, num;
    char lastch;
    int lasti;

    num = strlen(line);

    lastch = '\0';
    lasti = 0;
    for (i = 0, j = 0; i < num; i++) {
        if (line[i] == ch || line[i] == '\n') {
            line[i] = '\0';
            len[j] = i - lasti;
            lasti = i + 1;
        }

        if (lastch == '\0') {
            word[j] = (char *)(line + i);
            j++;
            if (j >= maxw)
                return j;
        }

        lastch = line[i];
    }


    len[j] = i - lasti;
    return j;
}

int ReadLine(const int fd, FileBuff *fb, char *line, int llen) {
    int i, scan, num, to = 0;

    for (i = 0; i < llen; i++) line[i] = '\0';
    while (1) {
        scan = min(fb->num, fb->from + llen - 1);
        for (i = fb->from; i < scan; i++) {
            line[to + i - fb->from] = fb->buf[i];
            if (fb->buf[i] == '\n') {
                num = ++i - fb->from;
                fb->from = i;
                return num;
            }
        }

        if (fb->num > fb->from + llen - 1) {
            line[llen - 1] = '\n';
            fb->from = (fb->from + llen - 1) ;
            return (llen);
        } else {
            to += (i - fb->from);
            fb->from = 0;
            /*bzero(fb->buf,FBUFFSIZE);*/
            memset(fb->buf, '\0', FBUFFSIZE);
            fb->num = read(fd, fb->buf, FBUFFSIZE);

            if (fb->num < 0) {
                switch (errno) {
                case EINTR:
                    printf("EINTR while reading, read will be continue.\n");
                    continue;
                default:
                    printf("errno:%d %s\n", errno, strerror(errno));
                    exit(-1);
                }
            } else if (fb->num == 0) {
                return 0;
            }
        }
    }
}

int SuperReadLine(const int fd, FileBuff *fb, char *line, int llen, int *flds) {
    int i, scan, num, to = 0;

    *flds = 0;
    for (i = 0; i < llen; i++) line[i] = '\0';
    while (1) {
        scan = min(fb->num, fb->from + llen - 1);
        for (i = fb->from; i < scan; i++) {
            line[to+i-fb->from] = fb->buf[i];

            if (fb->buf[i] == ',') {
                (*flds)++;
            } else if (fb->buf[i] == '\n') {
                num = ++i - fb->from;
                fb->from = i;
                return num;
            }
        }

        if (fb->num > fb->from + llen - 1) {
            line[llen-1] = '\n';
            fb->from = (fb->from + llen - 1) ;
            return (llen);
        } else {
            to += (i - fb->from);
            fb->from = 0;
            /*bzero(fb->buf,FBUFFSIZE);*/
            memset(&fb->buf, '\0', FBUFFSIZE);
            fb->num = read(fd, fb->buf, FBUFFSIZE);

            if (fb->num < 0) {
                switch (errno) {
                case EINTR:
                    printf("EINTR while reading, read will be continue.\n");
                    continue;
                default:
                    printf("errno:%d %s\n", errno, strerror(errno));
                    exit(-1);
                }
            } else if (fb->num == 0) {
                return 0;
            }
        }
    }
}

int WriteLine(const int fd, FileBuff *fb, const char *src, const int length) {
    size_t left, from, part;

    left = length;
    from = 0;

    while (left > 0) {
        if (left + fb->num <= FBUFFSIZE) {
            /*bcopy(src+from, fb->buf+fb->num, left);*/
            memcpy(fb->buf + fb->num, src + from, left);
            fb->num += left;
            left = 0;
        } else {
            part = FBUFFSIZE - fb->num;

            /*bcopy(src+from, fb->buf+fb->num, part);*/
            memcpy(fb->buf + fb->num, src + from, part);
            left -= part;
            from += part;
            WriteN(fd, fb->buf, FBUFFSIZE);
            /*bzero(fb->buf, FBUFFSIZE);*/
            memset(fb->buf, '\0', FBUFFSIZE);
            fb->num = 0;
        }
    }

    // 将缓存中未满的数据写到文件，以保证一条完整的记录如果需要向文件写入，是在本函数调用一次的过程中完成
    if (fb->num != 0) {
        WriteN(fd, fb->buf, fb->num);
        /*bzero(fb->buf, FBUFFSIZE);*/
        memset(fb->buf, '\0', FBUFFSIZE);
        fb->num = 0;
    }

    return 0;
}

int ReadN(const int fd, FileBuff *fb, char *block, int size) {
    size_t  left;
    int     i, to, has;

    to = 0;
    left = size;
    while (1) {
        has = fb->num - fb->from;
        if (has >= left) {
            /*bcopy(fb->buf+fb->from, block+to, left);*/
            memcpy(block + to, fb->buf + fb->from, left);
            fb->from += left;
            return size;
        } else if (has == 0) {
            //bzero(fb->buf,FBUFFSIZE);
            memset(fb->buf, '\0', FBUFFSIZE);
            fb->num = read(fd, fb->buf, FBUFFSIZE);
            if (fb->num < 0) {
                switch (errno) {
                case EINTR:
                    continue;
                default:
                    printf("errno:%d %s\n", errno, strerror(errno));
                    exit(-1);
                }
            } else if (fb->num == 0) {
                return 0;
            }
        } else {
            //bcopy(fb->buf+fb->from, block+to, has);
            memcpy(block + to, fb->buf + fb->from, has);
            to += has;
            left -= has;
            fb->from = 0;
        }
    }
}

int WriteN(const int fd, const char *vptr, const int n) {
    size_t left;
    size_t written;
    const char *ptr;

    ptr  = vptr;
    left = n;

    while (left > 0) {
        written = write(fd, ptr, left);
        if (written < 0) {
            switch (errno) {
            case EINTR:
                written = 0;
                break;
            default:
                printf("write failure  errno:%d %s\n", errno, strerror(errno));
                return(-1);
            }
        }
        left -= written;
        ptr  += written;
    }
    return n;
}

int WriteN_wld(const int fd, const char *vptr, const int n) {
    size_t left;
    ssize_t written;
    const char *ptr;

    ptr = vptr;
    left = n;
    printf("1111 n=[%d] left=[%d] vptr=[%s]\n", n, left, vptr);
    while (left > 0) {
        written = write(fd, ptr, left);
        printf("2222 written=[%d]\n", written);
        if (written < 0) {
            switch (errno) {
            case EINTR:
                written = 0;
                break;
            default:
                printf("write failure  errno:%d %s\n", errno, strerror(errno));
                return(-1);
            }
        }
        left -= written;
        ptr += written;
    }

    return n;
}

void Char2BCD(char *src, char *dest, int cnum) {
    int i;
    if (cnum % 2) {
        for (i = cnum / 2; i > 0; i--) {
            dest[i] = 0x0F & src[i*2-1];
            dest[i] |= src[i*2] << 4;
        }
        dest[0] = src[0] << 4;
    } else {
        for (i = cnum / 2 - 1; i >= 0; i--) {
            dest[i] = 0x0F & src[i*2];
            dest[i] |= src[i*2+1] << 4;
        }
    }
}

void BCD2Char(char *src, char *dest, int cnum) {
    int i;

    if (cnum % 2) {
        for (i = cnum / 2; i > 0; i--) {
            dest[i*2] = 0x0F & src[i] >> 4;
            dest[i*2-1] = 0x0F & src[i];
        }
        dest[0] = 0x0F & src[0] >> 4;
    } else {
        for (i = cnum / 2; i >= 0; i--) {
            dest[i*2+1] = 0x0F & src[i] >> 4;
            dest[i*2] = 0x0F & src[i];
        }
    }

    for (i = 0; i < cnum; i++) dest[i] += 48;
    dest[cnum] = '\0';
}

/* this function has same functionality with strncmp,
 * with totally opesite return value. removed to avoid confusing.
int isSame(char *src, char *dest, int len)
{
 int i;
 for(i=0; i<len; i++)
  if(src[i] != dest[i]) return 0;
 return 1;
}
*/

void *Malloc(unsigned capacity) {
    void *p;
    p = (void *)malloc(capacity);
    if (p == NULL) {
        printf("Can't Malloc:%s\n", strerror(errno));
        exit(-1);
    }
    /*else bzero(p, capacity);*/
    else memset(p, '\0', capacity);
    return p;
}

int Atoi(char *src) {
    //printf("Atoi :src=%s,strlen(src)=%d \n",src,strlen(src));
    int ret = atoi(src);
    if (ret <= 0) {
        perror("ssanalyze.conf configure error!");
        exit(-1);
    }
    return ret;
}

int ReadConfig(XMLReader *xml, Config *cfg) {
    cfg->sleep_interval   =   atoi(xml->getNodeByName("input", 0)->getNodeByName("sleep_interval",   0)->value     );
    strncpy(cfg->input_file_path,  xml->getNodeByName("input", 0)->getNodeByName("input_file_path",  0)->value, 200);
    strncpy(cfg->pros_file_path,   xml->getNodeByName("log",   0)->getNodeByName("pros_file_path",   0)->value, 200);
    strncpy(cfg->del_file_path,    xml->getNodeByName("log",   0)->getNodeByName("del_file_path",    0)->value, 200);
    strncpy(cfg->err_file_path,    xml->getNodeByName("log",   0)->getNodeByName("err_file_path",    0)->value, 200);
    strncpy(cfg->avl_file_path,    xml->getNodeByName("log",   0)->getNodeByName("avl_file_path",    0)->value, 200);
    cfg->log_interval     =   atoi(xml->getNodeByName("log",   0)->getNodeByName("log_interval",     0)->value     );
    cfg->lls_log_interval =   atoi(xml->getNodeByName("log",   0)->getNodeByName("lls_log_interval", 0)->value     );
    strncpy(cfg->host_code,        xml->getNodeByName("host",  0)->getNodeByName("host_code",        0)->value, 20 );
    strncpy(cfg->entity_type_code, xml->getNodeByName("host",  0)->getNodeByName("entity_type_code", 0)->value, 20 );
    strncpy(cfg->entity_sub_code,  xml->getNodeByName("host",  0)->getNodeByName("entity_sub_code",  0)->value, 20 );
    cfg->local_msisdn_enabled = (0 == strcmp("true", xml->getNodeByName("local_msisdn", 0)->property["available"].c_str()));
    strncpy(cfg->msisdn_script_name, xml->getNodeByName("local_msisdn", 0)->getNodeByName("script_file", 0)->value, 200);
    return 0;
}

void PrintCfg(Config *cfg) {
    printf("input_file_path: [%s]\n",  cfg->input_file_path);
    printf("sleep_interval:  %dsec\n", cfg->sleep_interval);
}

time_t ToTime_t(char *src) {
    struct tm t;
    time_t totime;


    strptime(src, "%Y-%m-%d %H:%M:%S", &t);
    totime = mktime(&t);
    return totime;
}

// 输入格式为：20090602070001
time_t ToTime_t_2(char *src) {
    struct tm t;
    time_t totime;

    char str_y[10];
    char str_m[10];
    char str_d[10];
    char str_h[10];
    char str_M[10];
    char str_s[10];

    memset(str_y, '\0', sizeof(str_y));
    memcpy(str_y, src, 4);
    memset(str_m, '\0', sizeof(str_m));
    memcpy(str_m, src + 4, 2);
    memset(str_d, '\0', sizeof(str_d));
    memcpy(str_d, src + 6, 2);
    memset(str_h, '\0', sizeof(str_h));
    memcpy(str_h, src + 8, 2);
    memset(str_M, '\0', sizeof(str_M));
    memcpy(str_M, src + 10, 2);
    memset(str_s, '\0', sizeof(str_s));
    memcpy(str_s, src + 12, 2);

    char src_out[30];
    memset(src_out, '\0', sizeof(src_out));
    sprintf(src_out, "%4s-%2s-%2s %2s:%2s:%2s", str_y, str_m, str_d, str_h, str_M, str_s);
    //printf("src_out=[%s]\n", src_out);

    strptime(src_out, "%Y-%m-%d %H:%M:%S", &t);
    totime = mktime(&t);
    return totime;
}

char *Now(char *now) {
    struct tm *t;
    struct timeval tval;
    gettimeofday(&tval, NULL);
    t = localtime(&tval.tv_sec);
    sprintf(now, "%4d-%02d-%02d %02d:%02d:%02d",
            t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
    return now;
}

static void ShowTimes(void) {
    struct tms times_info;
    double ticks;

    if ((ticks = (double)sysconf(_SC_CLK_TCK)) < 0)
        perror("Can't determine clock ticks per second");
    else if (times(&times_info) < 0)
        perror("Can't get times information");
    else {
        fprintf(stderr, "User time:%8.3f seconds\n", times_info.tms_utime / ticks);
        fprintf(stderr, "System time:%8.3f seconds\n", times_info.tms_stime / ticks);
        fprintf(stderr, "Children's user time:%8.3f seconds\n", times_info.tms_cutime / ticks);
        fprintf(stderr, "Children's system time:%8.3f seconds\n", times_info.tms_cstime / ticks);
    }
}

void ReadDir(int begin) {
    char dirname[20];
    DIR *dirp;
    struct dirent *direntp;


    strcpy(dirname, ".");
    if ((dirp = opendir(dirname)) == NULL) {
        perror("Can't opendir!");
        exit(1);
    }
    while ((direntp = readdir(dirp)) != NULL)
        printf("%s\n", direntp->d_name);

    closedir(dirp);
}

char *CurDir(char *dirname) {
    char *cwd;
    if ((cwd = getcwd(NULL, 64)) == NULL) {
        perror("getcwd failure");
        return cwd;
    }
    strcpy(dirname, cwd);
    return (dirname);
}

int Open(char *path, int oflag) {
    int fd;

    fd = open(path, oflag, 0644);
    if (fd <= 0) {
        printf("Can not open file %s\n", path);
        exit(1);
    }
    return (fd);
}

int PthreadCreate(pthread_t *tid, const pthread_attr_t *attr, void *(start_routine(void *)), void *arg) {
    int ret;
    ret = pthread_create(tid, attr, start_routine, arg);
    if (ret != 0) {
        printf("Can't create thread!\n");
        exit(1);
    }
    return 0;
}

// 取得系统当前时间
// str_sys_time 返回"1970-01-01 08:00:00"格式字符串
// 函数返回值为秒数
time_t get_system_time(char *str_curr_system_time) {
    int i;
    time_t curr_system_time;

    struct tm *tp;

    // 取得系统当前时间
    memset(str_curr_system_time, '\0', sizeof(str_curr_system_time));
    time(&curr_system_time);
    tp = localtime(&curr_system_time);
    sprintf(str_curr_system_time, "%4d-%02d-%02d %02d:%02d:%02d",
            tp->tm_year + 1900, tp->tm_mon + 1, tp->tm_mday, tp->tm_hour, tp->tm_min, tp->tm_sec);
    //printf("str_curr_system_time=[%s]\n", str_curr_system_time);

    return curr_system_time;
}

// 取得系统当前时间
// str_sys_time 返回"19710203040506"格式字符串
// 函数返回值为秒数
time_t get_system_time_2(char *str_curr_system_time) {
    int i;
    time_t curr_system_time;

    struct tm *tp;

    // 取得系统当前时间
    memset(str_curr_system_time, '\0', sizeof(str_curr_system_time));
    time(&curr_system_time);
    tp = localtime(&curr_system_time);
    sprintf(str_curr_system_time, "%4d%02d%02d%02d%02d%02d",
            tp->tm_year + 1900, tp->tm_mon + 1, tp->tm_mday, tp->tm_hour, tp->tm_min, tp->tm_sec);
    //printf("str_curr_system_time=[%s]\n", str_curr_system_time);

    return curr_system_time;
}

// 根据输入时间，转换为YYYYMMDDHHMMSS格式
// str_time 返回"20100315100000"格式字符串
// 函数返回值为tm
tm * get_str_time(char *str_time, time_t t) {
    struct tm *tp;

    // 取得tm
    memset(str_time, '\0', sizeof(str_time));
    tp = localtime(&t);

    sprintf(str_time, "%4d%02d%02d%02d%02d%02d",
            tp->tm_year + 1900, tp->tm_mon + 1, tp->tm_mday, tp->tm_hour, tp->tm_min, tp->tm_sec);
    //printf("str_time=[%s]\n", str_time);

    return tp;
}

// 根据输入时间，对分钟和秒钟部分清零
// 获取小时级的时间戳
// str_time 返回"20100315100000"格式字符串
// 函数返回值为time_t
time_t get_hour_time(time_t t) {
    /*
       struct tm *tp;
    time_t t_hour;

    //保留小时，分钟和秒清零
    tp = localtime(&t);
    tp->tm_min=0;
    tp->tm_sec=0;
       t_hour = mktime(tp);
       */
    return t / 3600 * 3600;
}

void LTrim(char* str, char ch) {
    int len, i;
    char* tmp;

    len = strlen(str);
    tmp = str;

    for (i = 0; i < len; i++) {
        if (str[i] != ch) {
            memcpy(str, tmp + i, len - i);
            str[len - i] = 0;
            return;
        }
    }
}

void RTrim(char* str, char ch) {
    int len, i;

    len = strlen(str);

    for (i = len - 1; i >= 0; i--) {
        if (str[i] != ch) {
            str[i + 1] = 0;
            return;
        }
    }

    str[0] = 0;
}

int GetStr(char*    origin, char*    out, int num, char    ch) {
    int i;
    char*   str1;
    char*   str2;
    str1 = origin;

    for (i = 0;i < num;i++) {
        str1 = strchr(str1, ch);
        str1 = str1 + 1;
    }
    str2 = strchr(str1, ch);
    memcpy(out, str1, str2 - str1);

    return (str2 -str1);
}

/**
*  程序启动时写心跳文件
**/
int write_heart_file(char *filename) {
    int ret = 1 ;
    char spid[20];
    FILE *fp;
    char file_path[250];

    memset(file_path, '\0', sizeof(file_path));
    strcpy(file_path, filename);
    printf("proc file path:%s\n", file_path);
    if ((fp = fopen(file_path, "w")) == NULL) {
        printf("Cannot write file!");
        ret = 0 ;
    }
    sprintf(spid, "%d", getpid());
    if (fputs(spid, fp) == NULL) {
        printf("Cannot fputs data to file!");
        ret = 0 ;
    }
    fclose(fp);

    if (ret == 0) { //写进程文件失败；
        write_proc_log_error();
    }

    return ret;
}

/**
*  错误告警日志文件
**/
int write_error_file(char *filepath, char *modulename, char *errcode, int level, char *errinfo) {
    int ret = 1 ;
    FILE *fp;
    char filename[250];
    char line[500];
    char spid[20], slevel[3];
    time_t begin_time;
    char str_begin_time[20];

    sprintf(spid, "%d", getpid());
    sprintf(slevel, "%d", level);
    memset(str_begin_time, '\0', sizeof(str_begin_time));
    begin_time = get_system_time_2(str_begin_time);

    memset(filename, '\0', sizeof(filename));
    sprintf(filename, "%s%s_%d_%s.err", filepath, modulename, getpid(), str_begin_time);
    if ((fp = fopen(filename, "a+")) == NULL) {
        printf("Cannot write file!");
        ret = 0 ;
    }

    memset(line, '\0', sizeof(line));
    sprintf(line, "%s\t%s\t%s\t%d\t%s\t%s\t%s\t%d\t%s\n", chk.host_code, chk.entity_type_code, chk.entity_sub_code, getpid(), modulename, str_begin_time, errcode, level, errinfo);
    if (fputs(line, fp) == NULL) {
        printf("Cannot fputs data to file!");
        ret = 0 ;
    }
    fclose(fp);

    return ret;
}

int write_log_file(char *filepath, char *modulename, char *msgcode, char *file_name, char *end_time, char *msgvalue) {
    int ret = 1 ;
    FILE *ftp_tmp;
    char filename[200], rfilename[200];
    char line[1000];

    memset(filename, '\0', sizeof(filename));
    sprintf(filename, "%s%s.tmp", filepath, modulename);
    if ((ftp_tmp = fopen(filename, "a+")) == NULL) {
        printf("Cannot write file!");
        ret = 0 ;
    }

    memset(line, '\0', sizeof(line));
    sprintf(line, "%s\t%s\t%s\t%d\t%s\t%s\t%s\t%s\t\t\t\t\t\t\t\t\t\t\t%s\t\n", chk.host_code, chk.entity_type_code, chk.entity_sub_code, getpid(), modulename, end_time, msgcode, file_name, msgvalue);
    if (fputs(line, ftp_tmp) == NULL) {
        printf("Cannot fputs data to file!");
        ret = 0 ;
    }
    fclose(ftp_tmp);
    return ret;
}

int write_error_recode_file(char *filepath, char *modulename, char *data_file_name, int row_num, char *errcode, char *error_line) {
    int ret = 1 ;
    FILE *fp;
    char filename[250];
    char line[500];
    time_t begin_time;
    char str_begin_time[20];

    memset(str_begin_time, '\0', sizeof(str_begin_time));
    begin_time = get_system_time_3(str_begin_time);

    memset(filename, '\0', sizeof(filename));
    sprintf(filename, "%s%s_%s.avl", filepath, modulename, str_begin_time);
    if ((fp = fopen(filename, "a+")) == NULL) {
        printf("Cannot write file!");
        ret = 0 ;
    }

    memset(line, '\0', sizeof(line));
    sprintf(line, "%s,%d,%s,%s", data_file_name, row_num, errcode, error_line);
    if (fputs(line, fp) == NULL) {
        printf("Cannot fputs data to file!");
        ret = 0 ;
    }
    fclose(fp);

    return ret;
}

// 取得系统当前时间
// 时间精度为天
// str_sys_time 返回"19710203000000"格式字符串
// 函数返回值为秒数
time_t get_system_time_3(char *str_curr_system_time) {
    int i;
    time_t curr_system_time;

    struct tm *tp;

    // 取得系统当前时间
    memset(str_curr_system_time, '\0', sizeof(str_curr_system_time));
    time(&curr_system_time);
    tp = localtime(&curr_system_time);
    tp->tm_hour = 0;
    tp->tm_min = 0;
    tp->tm_sec = 0;
    sprintf(str_curr_system_time, "%4d%02d%02d%02d%02d%02d",
            tp->tm_year + 1900, tp->tm_mon + 1, tp->tm_mday, tp->tm_hour, tp->tm_min, tp->tm_sec);
    //printf("str_curr_system_time=[%s]\n", str_curr_system_time);

    return curr_system_time;
}

void write_deal_log_error() {
    char errinfo[200];
    memset(errinfo, '\0', sizeof(errinfo));
    sprintf(errinfo, "写文件处理日志失败");
    printf("%s\n", errinfo);
    write_error_file(chk.err_file_path, MODULE_NAME, "35", 2, errinfo);
}

void write_alert_log_error() {
    char errinfo[200];
    memset(errinfo, '\0', sizeof(errinfo));
    sprintf(errinfo, "写告警日志失败");
    printf("%s\n", errinfo);
    write_error_file(chk.err_file_path, MODULE_NAME, "34", 2, errinfo);
}

void write_proc_log_error() {
    char errinfo[200];
    memset(errinfo, '\0', sizeof(errinfo));
    sprintf(errinfo, "写进程通知日志失败:");
    printf("%s\n", errinfo);
    write_error_file(chk.err_file_path, MODULE_NAME, "33", 2, errinfo);
}

//xml格式校验,若含有未封闭标签或标签不匹配则返回非零，若通过校验返回0；
int checkFormat(FILE *fp) {
    int braceNum = 0;
    int root_num = 0;
    char thisChar;
    while (fscanf(fp, "%c", &thisChar) != EOF) {
        //printf("%c",thisChar);
        if (thisChar == '<') {
            braceNum++;
        } else if (thisChar == '>') {
            braceNum--;
            if (braceNum == 0)root_num++;
        }
        if (braceNum < 0) {
            cout << "xml文件格式错误" << endl;
            return 1;
        }
    }
    if (braceNum != 0) {
        cout << "xml文件格式错误" << endl;
        return 2;
    }
    cout << "括号匹配数" << root_num << endl;
    return 0;
}
//将xml文档中所有内容读入内存
char * gatAllTextFromFile(char *fileName) {
    FILE *data;
    data = fopen(fileName, "r");
    char *a = (char *)malloc(30000 * sizeof(char));
    char *b = a;
    char res[30000];
    bool iscat = true;
    int pos = 0;
    while (fscanf(data, "%c", a++) != EOF);


    *a = '\0';
    fseek(data, 0, 0);
    int data_len = strlen(b);
    int i;
    for (i = 0;i < data_len - 1;i++) {
        if (iscat == true && b[i] == '<' && (b[i+1] == '!' || b[i+1] == '?')) {
            iscat = false;
        }


        if (iscat) {
            res[pos++] = b[i];
        }

        if (iscat == false && b[i] == '>') {
            iscat = true;
        }


    }
    res[pos] = b[i];
    res[++pos] = '\0';
    if (checkFormat(data) != 0)return "";
    //cout<<"获取到的文件:\n"<<res<<endl;
    fclose(data);
    strcpy(b, res);
    return b;
}

map<string, string> getProperty(char *from, char *to) {
    char *key, *value;
    char *keyfrom, *keyto, *valuefrom, *valueto;
    char temp[100];
    map<string, string> res;
    for (char *i = from;i < to - 1;i++) {
        if (i[0] == ' ' && i[1] != ' ') {
            keyfrom = i + 1;
            keyto = strchr(i + 1, '=');
            if (keyto == NULL) {continue;}
            key = (char *)malloc(keyto - keyfrom + 2);
            strncpy(key, keyfrom, keyto - keyfrom);
            key[keyto-keyfrom] = '\0';

            valuefrom = strchr(i, '\"') + 1;
            valueto = strchr(valuefrom + 1, '\"');
            if (valuefrom == NULL || valueto == NULL) {
                continue;
            }
            value = (char *)malloc(valueto - valuefrom + 2);
            strncpy(value, valuefrom, valueto - valuefrom);
            value[valueto-valuefrom] = '\0';
            res.insert(pair<string, string>(key, value));
        }
    }
    return res;
}
//深度搜索目标xml字符串，获取到XMLReader类中
void getXML(char *cont, XMLReader *node) {
    if (strchr(cont, '<') == NULL) {return;}

    char *h, *pos, *from, *to, *end, *tmp;

    char name[100];
    char value[30000];
    char temp[30000], temp2[30000], endOfTag[100];
    char key[100], mapvalue[100];
    h = cont;
    from = strchr(h, '<');
    if (from[1] == '?' || from[1] == '-') {
        from = strchr(from + 1, '<');
    }
    to = strchr(from, '>');
    if (from == NULL || to == NULL)return;
    node->property = getProperty(from, to);
    tmp = strchr(from, ' ');
    if (tmp < to && tmp != NULL)to = tmp;
    pos = h;
    from++;
    strncpy(temp, from, to - from);
    temp[to-from] = '\0';

    node->name = (char *)malloc(sizeof(temp));
    strcpy(node->name, temp);
    //cout<<"name: "<<node->name<<endl;
    to = strchr(to, '>');

    endOfTag[0] = '\0';
    strcat(endOfTag, "</");
    strcat(endOfTag, temp);
    strcat(endOfTag, ">");
    end = strstr(to, endOfTag);
    if (end == NULL) {
        cout << "xml文档格式错误:标签没有封闭:" << endOfTag << endl;
        return;
    }
    to++;

    strcpy(temp2, to);

    temp2[end-to] = '\0';

    node->value = (char *)malloc(sizeof(temp2));
    strcpy(node->value, temp2);
    //cout<<"value: "<<node->value<<endl;
    char *i = node->value;
    int brace_index = 0;
    int node_num = 0;
    for (;i < node->value + strlen(node->value) - 1;i++) {
        if (i[0] == '<' && i[1] != '/') {
            brace_index++;
            if (brace_index == 1) {
                //cout<<"找到:"<<i<<endl;
                //cout<<"维度是:"<<node_num<<endl;
                node->node[node_num] = new XMLReader();
                getXML(i, node->node[node_num++]);
            }

        } else if (i[0] == '<' && i[1] == '/') {
            brace_index--;
        }
    }
}

int split_value(char** dest, char* line, char splt) {
    if (line == NULL) return 0;
    int idx = 0;
    int cpstart = 0;
    int i = 0;
    do {
        if (line[i] == splt || line[i] == '\0') {
            if (cpstart != i)  {
                dest[idx] = (char*) malloc(sizeof(char) * (i - cpstart + 1));
                memset(dest[idx], '\0', (i - cpstart + 1));
                strncpy(dest[idx], line + cpstart, i - cpstart);
            } else {
                dest[idx] = (char*) malloc(sizeof(char));
                dest[idx][0] = '\0';
            }
            cpstart = i + 1;
            idx++;
        }
    } while (line[i++] != '\0');
    return idx;
}

int split_field_value(int* dest, char* line, char splt) {
    if (line == NULL) return 0;
    int  idx     = 0;
    int  cpstart = 0;
    int  i       = 0;
    char tmp_word[20];

    do {
        if (line[i] == splt || line[i] == '\0') {
            if (cpstart != i)  {
                memset(tmp_word, 0, sizeof(tmp_word));
                strncpy(tmp_word, line + cpstart, i - cpstart);
                dest[idx] = decode_map[tmp_word];
            } else {
                dest[idx] = 0;
            }
            cpstart = i + 1;
            idx++;
        }
    } while (line[i++] != '\0');
    return idx;
}

int split_value_noalloc(char** dest, char* line, char splt) {
    if (line == NULL) return 0;
    int idx = 0;
    int cpstart = 0;
    int i = 0;
    do {
        if (line[i] == splt || line[i] == '\0') {
            memset(dest[idx], 0, MAX_SIG_FIELD_LEN);
            if (cpstart != i) strncpy(dest[idx], line + cpstart, i - cpstart);
            else dest[idx][0] = '\0';
            cpstart = i + 1;
            idx++;
            if (idx >= MAX_SIG_FIELD) return -1;
        }
    } while (line[i++] != '\0');
    return idx;
}

