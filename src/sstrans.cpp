/**
* Copyright (c) 2010 AsiaInfo. All Rights Reserved.
* Creator: gaozhao
* Modified log：
*     1> chengxc 2010-03-15 modify
*     2> zhoulb  2010-03-22    modify
*     3> chengxc  2011-03-23  modify
* Description:
*     预处理转换信令使用的函数；
* Input:
*
* Output:
*
* 错误代码范围：
*       R0001 -- R1000
*/

//#include "AIXMLReader.h"
#include "sspublic.h"
#include "sstrans.h"
#include "sscheck.h"


//int port_num = 0; //port个数；
//int PORT_MAX = 5;
map<string, int> port_map;

/* should all these be gloable? wtf...
char prefix[5][10]; //文件名前缀

int timestamp_begin[5]; //文件中时间戳的字符起始位置；
int timestamp_len[5]; //文件中时间戳长度，取到分钟；
int file_num_begin[5]; //文件中同一时间戳文件的分排序号；
int file_num_len[5]; //分排序号长度；

int trans_order[5][19];
int columns_num[5];
int origin_num[5];
int index_of_command[5];
int command_type_num[5];

int ler_column_num = 50 ; //原始信令字段数

char ler_file_type[10];  //当前信令文件名；
int port_type = 0;

char **tar;

char file_no[5];
char str_file_time[20];
time_t file_time;
*/

//XMLReader *cfgXML;
XMLCheck *xmlCheck;
//char **event_map[5];
char **g_word;

//map<string,string> command_map;

void line_add_filename(char * line, char *filename) {
    strcat(line, filename);
}

int split(char *str, char split_chr, char **res) {
    int i = 0;
    char *from, *to, *temp;
    int res_pos = 0;
    from = str;
    int str_len = strlen(str);
    for (;i <= str_len;i++) {
        if (str[i] == split_chr) {
            to = &str[i];
            strncpy(res[res_pos], from, to - from);
            res[res_pos++][to-from] = '\0';
            if (to - str < str_len)from = to + 1;
        }
    }
    to = &str[i];
    strncpy(res[res_pos], from, to - from);
    res[res_pos++][to-from] = '\0';

    return res_pos;
}

bool trans_line(char * line, time_t file_time_t, Trans_cfg *tcfg) {
    int  num = 0 ;
    char res[MAXLINE] ;
    char *ret;
    int  line_sum = 0;
    char *column_index;
    bool tcfg_available = (tcfg != NULL);
    bool zero_flag = false;

    //trim EOL symbol
    int  tmp_len = strlen(line);
    for (int i = tmp_len - 1; i >= 0; i--) {
        if (line[i] == '\n' || line[i] == '\r') line[i] = '\0';
        else break;
    }

    //解析原始信令数据
    line_sum = split_value_noalloc(g_word, line, tcfg->field_spliter);
    if (line_sum < 0 || ( tcfg_available && tcfg->available && line_sum != tcfg->fmt_orig ))  {
        lers->err_filed_sum_total++;
        write_list_log_buff(lers->current_file_name, lers->total, S_ID_LINE_SUM_ERR, line);
        return false;
    }
    
    //event_type转换
    char *orig_evt = g_word[tcfg->evt_idx];
    if (orig_evt[0] == '\0') {
        write_list_log_buff(lers->current_file_name, lers->total, S_ID_FIELD_NULL_ERR, line);
        return false;
    } else if (tcfg_available && tcfg->available && tcfg->evt_map_available) {
        int posl = 0;
        int posr = tcfg->num_evt_map - 1;
        int posm = 0;
        int ret_comp;
        do {
            posm = (posl + posr) / 2;
            ret_comp = strcmp(orig_evt, tcfg->evt_map[posm][0]);
            if (ret_comp == 0) {
                strcpy(orig_evt, tcfg->evt_map[posm][1]);
                break;
            } else if (ret_comp < 0) posr = posm - 1;
            else posl  = posm + 1;
        } while (posl <= posr);
        if (ret_comp != 0) {
            write_list_log_buff(lers->current_file_name, lers->total, S_ID_NO_EVENT_IN_MAP, line);
            return false;
        }
    }
    
    // swap on event
    char *tmp_swap = NULL;
    if (strcmp(g_word[tcfg->evt_idx], tcfg->soe_event) == 0) {
        tmp_swap                       = g_word[tcfg->soe_field[0] - 1];
        g_word[tcfg->soe_field[0] - 1] = g_word[tcfg->soe_field[1] - 1];
        g_word[tcfg->soe_field[1] - 1] = tmp_swap;
    }
    
    //printf("line: %s\n", line);
    if (tcfg_available && tcfg->available && tcfg->fmt_available) num = tcfg->fmt_after;
    else num = tcfg->fmt_orig;
    //通过字段顺序重新排序
    memset(res, '\0', sizeof(res));
    for (int i = 0; i < num; i++) {
        zero_flag = false;
        if (tcfg->available && tcfg->fmt_available) {
            if (tcfg->fmt_order[i] == 0) zero_flag = true;
            else column_index = g_word[tcfg->fmt_order[i] - 1];
        } else column_index = g_word[i];

        if (xmlCheck[i].date_type == NULL) continue;
        else if (!zero_flag) {
            switch (xmlCheck[i].date_type[0]) {
            case 'c': { //字符定长类型校验
                    int len = xmlCheck[i].length;
                    ret = check_char(column_index, len, xmlCheck[i].can_be_null, xmlCheck[i].numeral);
                    if (NULL != ret) {
                        //cout<<tcfg->fmt_order[i] - 1<<" char false. "<< column_index<<endl;
                        write_list_log_buff(lers->current_file_name, lers->total, ret , line);
                        return false;
                    }
                    break;
                }
            case 'i': { //数字类型校验
                    //printf("int[%d]:[%d]\n",trans_order[port_type][i]-1,strlen(column_index));
                    int min_value = xmlCheck[i].min_value;
                    int max_value = xmlCheck[i].max_value;
                    ret = isNumber(column_index, xmlCheck[i].can_be_null, min_value, max_value);
                    if (NULL != ret) {
                        //cout<<tcfg->fmt_order[i] - 1<<" int false "<< column_index<<endl;
                        write_list_log_buff(lers->current_file_name, lers->total, ret, line);
                        return false;
                    }
                    break;
                }
            case 'v': { //字符类型校验
                    int len = xmlCheck[i].length;
                    ret = check_varchar(column_index, len, xmlCheck[i].can_be_null);
                    if (NULL != ret) {
                        //cout << tcfg->fmt_order[i] - 1<<" varchar false. "<< column_index << endl;
                        write_list_log_buff(lers->current_file_name, lers->total, ret, line);
                        return false;
                    }
                    break;
                }
            case 'd': { //时间型数据校验
                    time_t time_delay  = xmlCheck[i].time_delay;
                    time_t time_before = xmlCheck[i].time_before;
                    time_t max_time    = xmlCheck[i].max_time;
                    time_t min_time    = xmlCheck[i].min_time;
                    char  *time_format = xmlCheck[i].value;
                    ret = check_date(column_index, file_time_t, min_time, max_time, time_before, time_delay, time_format);
                    if (NULL != ret) {
                        write_list_log_buff(lers->current_file_name, lers->total, ret, line);
                        return false;
                    }
                    break;
                }
            default: return true;
            } /* switch */
        } /* if */
        if (!zero_flag) strcat(res, column_index);
        strcat(res, ",");
    }
    memset(line, '\0', sizeof(line));
    strcpy(line, res);
    return true;
}

int get_XML_config(char* filename) {
    cfgXML = new XMLReader();
    getXML(gatAllTextFromFile(filename), cfgXML);
    return 0;
}

int get_trans_config(XMLReader *xml, Trans_cfg *tcfg) {
    char tmp_str[200];
    memset(tcfg, 0, sizeof(Trans_cfg));
    tcfg->available = (0 == strcmp("true", xml->property["available"].c_str()));
    strncpy(tcfg->filename_prefix, xml->property["prefix"].c_str(), 20);
    memset(tmp_str, '\0', sizeof(tmp_str));
    strncpy(tmp_str, xml->property["type"].c_str(), 20);
    if (0 == strncmp("cs", tmp_str, 2)) tcfg->pt = pt_CS;
    else if (0 == strncmp("ps", tmp_str, 2)) tcfg->pt = pt_PS;
    else tcfg->pt = pt_Undefined;
    tcfg->timestamp_begin  = atoi(xml->getNodeByName("file_name", 0)->property["timestamp_begin"].c_str());
    tcfg->timestamp_length = atoi(xml->getNodeByName("file_name", 0)->property["timestamp_len"]  .c_str());
    tcfg->filenum_length   = atoi(xml->getNodeByName("file_name", 0)->property["file_num_len"]   .c_str());
    tcfg->fmt_orig         = atoi(xml->getNodeByName("format", 0)   ->property["origin_num"]     .c_str());
    tcfg->fmt_after        = atoi(xml->getNodeByName("format", 0)   ->property["num"]            .c_str());
    tcfg->fmt_available    = (0 == strcmp("true", xml->getNodeByName("format", 0)->property["available"].c_str()));
    tcfg->fmt_order = (int*) malloc(sizeof(int) * tcfg->fmt_after);
    memset(tmp_str, '\0', sizeof(tmp_str));
    strcpy(tmp_str, xml->getNodeByName("format", 0)->value);
    split_value_to_int(tcfg->fmt_order, tmp_str, ',');
    tcfg->evt_map_available = (0 == strcmp("true", xml->getNodeByName("event", 0)->property["available"].c_str()));
    tcfg->evt_idx     = atoi(xml->getNodeByName("event", 0)->property["index"]   .c_str()) - 1;
    tcfg->num_evt_map = atoi(xml->getNodeByName("event", 0)->property["type_num"].c_str());
    tcfg->evt_map = (char***) malloc(sizeof(char**) * tcfg->num_evt_map);
    for (int i = 0; i < tcfg->num_evt_map; i++) {
        tcfg->evt_map[i] = (char**) malloc(sizeof(char*) * 2);
        memset(tmp_str, '\0', sizeof(tmp_str));
        strcpy(tmp_str, xml->getNodeByName("event", 0)->getNodeByName("event_type", i)->value);
        split_value(tcfg->evt_map[i], tmp_str, ',');
    }
    qsort(tcfg->evt_map, tcfg->num_evt_map, sizeof(char**), strcomp1);
    if (xml->getNodeByName("swap_on_event", 0) != NULL) {
        strcpy(tcfg->soe_event,             xml->getNodeByName("swap_on_event", 0)->property["event"].c_str());
        split_value_to_int(tcfg->soe_field, xml->getNodeByName("swap_on_event", 0)->property["col"]  .c_str(), ',');
    }
    tcfg->field_spliter = xml->getNodeByName("field_spliter", 0)->value[0];
    return 0;
}

int strcomp1(const void* str1, const void* str2) {
    return strcmp((*(char***)str1)[0], (*(char***)str2)[0]);
}

int split_value_to_int(int* dest, const char* line, char splt) {
    if (line == NULL) return 0;
    int ret = 0;
    int idx = 0;
    int cpstart = 0;
    int i = 0;
    char tmp[50];
    do {
        if (line[i] == splt || line[i] == '\0') {
            if (cpstart != i)  {
                memset(tmp, '\0', sizeof(tmp));
                strncpy(tmp, line + cpstart, i - cpstart);
                dest[idx] = atoi(tmp);
            } else {
                dest[idx] = 0;
            }
            cpstart = i + 1;
            idx++;
        }
    } while (line[i++] != '\0');
    return ret;
}

//初始化文件校验规则
int get_check_config() {
    int pos;
    int index = 0;
    int num_of_checks = cfgXML->getNodeByName("check", 0)->getNumOfChildren();
    xmlCheck = (XMLCheck *)Malloc(sizeof(XMLCheck) * num_of_checks);
    //初始化
    for (int i = 0 ; i < num_of_checks ; i++) {
        memset(xmlCheck[i].date_type, '\0', sizeof(xmlCheck[i].date_type));
        memset(xmlCheck[i].value, '\0', sizeof(xmlCheck[i].value));
        xmlCheck[i].can_be_null = false;
        xmlCheck[i].numeral     = false;
        xmlCheck[i].time_before = -1;
        xmlCheck[i].time_delay  = -1;
        xmlCheck[i].min_value   = -1;
        xmlCheck[i].max_value   = -1;
        xmlCheck[i].length      = -1;
    }

    struct tm tmp_time1, tmp_time2;
    //为存在的赋值
    for (int k = 0 ; k < num_of_checks ; k++) {
        //根据index做为下标
        index = atoi(cfgXML->getNodeByName("check", 0)->getNodeByName("column", k)->property["index"].c_str());
        pos   = index - 1;
        strcpy(xmlCheck[pos].date_type, cfgXML->getNodeByName("check", 0)->getNodeByName("column", k)->property["data_type"].c_str());
        xmlCheck[pos].length      = atoi(cfgXML->getNodeByName("check", 0)->getNodeByName("column", k)->property["length"].c_str());
        xmlCheck[pos].can_be_null = (strcmp("true", cfgXML->getNodeByName("check", 0)->getNodeByName("column", k)->property["can_be_null"].c_str()) == 0);
        xmlCheck[pos].numeral     = (strcmp("true", cfgXML->getNodeByName("check", 0)->getNodeByName("column", k)->property["numeral"].c_str()) == 0);
        strcpy(xmlCheck[pos].value, cfgXML->getNodeByName("check", 0)->getNodeByName("column", k)->value);
        strptime(cfgXML->getNodeByName("check", 0)->getNodeByName("column", k)->property["min_time"].c_str(), xmlCheck[pos].value, &tmp_time1);
        xmlCheck[pos].min_time    = mktime(&tmp_time1);
        strptime(cfgXML->getNodeByName("check", 0)->getNodeByName("column", k)->property["max_time"].c_str(), xmlCheck[pos].value, &tmp_time2);
        xmlCheck[pos].max_time    = mktime(&tmp_time2);
        xmlCheck[pos].time_before = atoi(cfgXML->getNodeByName("check", 0)->getNodeByName("column", k)->property["time_before"].c_str());
        xmlCheck[pos].time_delay  = atoi(cfgXML->getNodeByName("check", 0)->getNodeByName("column", k)->property["time_delay"] .c_str());
        xmlCheck[pos].min_value   = atoi(cfgXML->getNodeByName("check", 0)->getNodeByName("column", k)->property["min_value"]  .c_str());
        xmlCheck[pos].max_value   = atoi(cfgXML->getNodeByName("check", 0)->getNodeByName("column", k)->property["max_value"]  .c_str());
    }
    return 1;
}

int check_file_port(char* fileName, Trans_cfg *tcfg, int num_tcfg) {
    if (strlen(fileName) < 1) return -1;
    for (int i = 0; i < num_tcfg; i++) {
        int  prefix_len    = strlen(tcfg[i].filename_prefix);
        int  pos           = 0;
        bool not_this_port = false;
        if (strncmp(fileName, tcfg[i].filename_prefix, prefix_len) == 0) {
            pos = tcfg[i].timestamp_begin;
            for (int j = 0; j < tcfg[i].timestamp_length; j++) {
                if (fileName[pos + j] < '0' || fileName[pos + j] > '9') not_this_port = true;
            }
            pos += tcfg[i].filenum_length;
            for (int j = 0; j < tcfg[i].filenum_length; j++) {
                if (fileName[pos + j] < '0' || fileName[pos + j] > '9') not_this_port = true;
            }
            if (!not_this_port) return i;            
        }
    }
    return -1;
}

void init_file_info(char* fileName, Trans_cfg *tcfg, char file_no[], char str_file_time[], time_t &file_time) {
    memset(file_no, '\0', 5);
    memset(str_file_time, '\0', 20);
    strncpy(file_no, fileName + tcfg->timestamp_begin + tcfg->timestamp_length, tcfg->filenum_length);
    strncpy(str_file_time, fileName + tcfg->timestamp_begin, tcfg->timestamp_length);
    strcat(str_file_time, "00");
    str_file_time[19] = '\0';
    str_file_time[18] = '0';
    str_file_time[17] = '0';
    str_file_time[16] = ':';
    str_file_time[15] = str_file_time[11];
    str_file_time[14] = str_file_time[10];
    str_file_time[13] = ':';
    str_file_time[12] = str_file_time[9];
    str_file_time[11] = str_file_time[8];
    str_file_time[10] = ' ';
    str_file_time[9]  = str_file_time[7];
    str_file_time[8]  = str_file_time[6];
    str_file_time[7]  = '-';
    str_file_time[6]  = str_file_time[5];
    str_file_time[5]  = str_file_time[4];
    str_file_time[4]  = '-';
    file_time         = ToTime_t(str_file_time);
    //printf("--------------------------file_time:%d\n",file_time);
}
