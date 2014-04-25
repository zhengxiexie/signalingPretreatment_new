#ifndef SSMSISDN_H
#define SSMSISDN_H

#define MSISDN_ERFILE (1)
#define MSISDN_EINDEX (2)
#define MSISDN_ELARGE (3)

int read_msisdn_table_from_file(const char * const filename);

int msisdn_prefix_find( unsigned int msisdn_int, 
                        short * city_code, short * prov_code, short * local_flag);
extern int msisdn_table_inited;
#endif /* SSMSISDN_H */

