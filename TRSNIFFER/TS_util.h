/* 
 * TS_util.h
 *
 * Created on: Jun 30, 2016
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INFORMATICA DISCLAIMS ALL WARRANTIES
 * EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION, ANY IMPLIED WARRANTIES OF
 * NON-INFRINGEMENT, MERCHANTABILITY OR FITNESS FOR A PARTICULAR
 * PURPOSE.  INFORMATICA DOES NOT WARRANT THAT USE OF THE SOFTWARE WILL BE
 * UNINTERRUPTED OR ERROR-FREE.  INFORMATICA SHALL NOT, UNDER ANY CIRCUMSTANCES, BE
 * LIABLE TO LICENSEE FOR LOST PROFITS, CONSEQUENTIAL, INCIDENTAL, SPECIAL OR
 * INDIRECT DAMAGES ARISING OUT OF OR RELATED TO THIS AGREEMENT OR THE
 * TRANSACTIONS CONTEMPLATED HEREUNDER, EVEN IF INFORMATICA HAS BEEN APPRISED OF
 * THE LIKELIHOOD OF SUCH DAMAGES.
 *
 */
#include <pthread.h>
#include "lbmrpckt.h"

#define TS_MAX_STRING_LENGTH 1024
struct strings_list {
        char string[TS_MAX_STRING_LENGTH];
        char opt_string2[TS_MAX_STRING_LENGTH];
        struct strings_list *next;
        struct strings_list *prev;
	uint32_t stat_hits; 
};
typedef struct strings_list_head  {
        struct strings_list *top;
        struct strings_list *end;
        struct strings_list *last;
	pthread_mutex_t list_mutex;
        uint64_t  sz;
	uint32_t stat_llsv; // longest linear search value
}TS_strings_listhead;

void print_string_list(TS_strings_listhead *TS_strings_list_ptr);
char strings_list_store (char * string, char * opt_string2, TS_strings_listhead * TS_strings_list_ptr);
void clear_string_list(struct strings_list_head *TS_strings_list_ptr);


#define PRINT_STATS     1<<0
#define PRINT_ERROR     1<<1
#define PRINT_WARNING   1<<2
#define PRINT_VERBOSE   1<<3
#define PRINT_UMP       1<<4
#define PRINT_ULB       1<<5
#define PRINT_DRO       1<<6
#define PRINT_TMR       1<<7
#define PRINT_IP_FRAG   1<<8  // 0x100
#define PRINT_OTIDS     1<<9
#define PRINT_DEBUG     1<<10 // 0x400
#define PRINT_PCAP      1<<11
#define PRINT_DETAILS   1<<12
#define PRINT_LBMR      1<<13
#define PRINT_SOCKET    1<<14
#define PRINT_LBMRD     1<<15
#define PRINT_ALL       0xFFFFFFF
#define PRINT_M         ((PRINT_ALL)-(PRINT_DEBUG|PRINT_IP_FRAG|PRINT_PCAP|PRINT_DETAILS))

#define STRINGS_MUTEX_UNLOCK_RETURN(ret_val) \
                          do { \
                                pthread_mutex_unlock(&TS_strings_list_ptr->list_mutex); \
                                return ret_val;\
                           } while(0)

void TS_printf(uint32_t log_level, const char *format, ...);
void TS_fprintf(uint32_t log_level, const char *format, va_list ptr);

#define TS_DATESTR_SZ 64
void TS_tv2str(struct timeval *tv, char *buf);

