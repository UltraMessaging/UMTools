/* 
 * TS_util.c 
 * Utility routines 
 *
 *  Created on: Jun 30, 2016
 *      Author: 
 *      	Ibu Akinyemi 
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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "TS_util.h"

#define STRINGS_MUTEX_UNLOCK_RETURN(ret_val) \
			  do { \
				pthread_mutex_unlock(&TS_strings_list_ptr->list_mutex); \
				return ret_val;\
			   } while(0)
#define STRINGS_SET_STATS(str_ptr)\
			  do { \
				if (ctr > TS_strings_list_ptr->stat_llsv) { \
					TS_strings_list_ptr->stat_llsv = ctr; \
				}\
				str_ptr->stat_hits++;\
			   } while(0)
#define MAX_WFILENAME 50
char TS_wfilename[MAX_WFILENAME];
uint32_t TS_current_log_level= PRINT_LBMR|PRINT_ERROR|PRINT_WARNING ;
uint32_t TS_max_fileline_count=0;
uint32_t TS_file_size_check=0;
uint64_t TS_lines_written=0;
uint32_t TS_files_created=0;
FILE *TS_output_file=NULL;

void TS_printf(uint32_t log_level, const char *format, ... ){
	va_list ptr;
        if( (log_level & TS_current_log_level) != 0){
#if DEBUG
                printf(" [Level %x] ",log_level);
#endif
                va_start(ptr, format);
                vprintf(format, ptr);
                va_end(ptr);

                if(TS_max_fileline_count!=0){
                        va_start(ptr, format);
                        TS_fprintf(log_level, format, ptr);
                        va_end(ptr);
                }
        }
}/* End TS_printf() */

void TS_fprintf(uint32_t log_level,const char *format, va_list ptr ){
        if(TS_output_file==NULL){
                return;
        }
        if (TS_file_size_check > TS_max_fileline_count) {
                /* Close the previous file pointer and create a new one */
                TS_file_size_check = 0;
                fclose(TS_output_file);
                TS_files_created++;
                snprintf(TS_wfilename,MAX_WFILENAME,"TR_LOGS/%c%i",'x',TS_files_created);
                TS_output_file = fopen(TS_wfilename, "w");
                if (TS_output_file == NULL){
                        TS_printf(PRINT_ERROR,"\n could not create output file %s", TS_wfilename);
                        fflush (stdout);
                        exit(EXIT_FAILURE);
                }
                printf("\n Writing to file: %s", TS_wfilename);
        }
        vfprintf(TS_output_file,format, ptr);
        TS_lines_written++;
        TS_file_size_check++;
}/* End TS_fprintf */


/* Store unique strings in an ordered double linked list */
char strings_list_store (char * string,  char * opt_string2, struct strings_list_head * TS_strings_list_ptr){
        struct strings_list *iter;
        struct strings_list *prev_iter, *new;
        char search_forward=0;
	uint64_t i, ctr=0;
	int  strcmp_res ;

	pthread_mutex_lock(&TS_strings_list_ptr->list_mutex);

	new = malloc( sizeof (struct strings_list));
	if (new == NULL){
		TS_printf(PRINT_ERROR, "ERROR! strings_list_store() Malloc failed");
		STRINGS_MUTEX_UNLOCK_RETURN(0);
	}

	if(string == NULL) { 
		TS_printf(PRINT_ERROR, "ERROR! strings_list_store() (string == NULL)" );
		STRINGS_MUTEX_UNLOCK_RETURN(0);
	} else {
        	/* copy the string that will be used for index */
		for (i = 0 ; i < TS_MAX_STRING_LENGTH && string[i] != '\0' ; i++) {new->string[i] = string[i]; }
		new->string[i]='\0';
		new->stat_hits=0;
	}

	if(opt_string2 != NULL) { 
        	/* copy optional string */
		for (i = 0 ; i < TS_MAX_STRING_LENGTH && opt_string2[i] != '\0' ; i++) {new->opt_string2[i] = opt_string2[i]; }
		new->opt_string2[i]='\0';
	} else {
		new->opt_string2[0]='\0';
	}
		

        if(TS_strings_list_ptr->last ==NULL){ /* First element in list */
                new->next = NULL;
                new->prev = NULL;
                TS_strings_list_ptr->top = new;
                TS_strings_list_ptr->end = new;
                TS_strings_list_ptr->last = new;
                TS_strings_list_ptr->sz = 1;
		TS_strings_list_ptr->stat_llsv=1;
                STRINGS_MUTEX_UNLOCK_RETURN(1);
        }


        /* If this is the same as the previous strings; nothing to store, just return */
        /* Otherwise, determine which direction to search */
        /* <TO DO>: Try binary search */
        iter = TS_strings_list_ptr->last;
        strcmp_res = strcmp(iter->string, new->string);
        if( strcmp_res == 0 ){
                /* non-unique OTID */
		free(new);
		iter->stat_hits++;
                STRINGS_MUTEX_UNLOCK_RETURN(0);
         } else if (strcmp_res<0) {
                search_forward = 1;
                prev_iter = iter;
                iter = iter->next;
         }else {
                search_forward = 0;
                prev_iter = iter;
                iter = iter->prev;
        }

        if (search_forward == 1){
                while (iter) {
			ctr++; 
                        strcmp_res = strcmp(iter->string, new->string);
                        if(strcmp_res ==0){
                                /* Non-unique String */
				free(new);
				STRINGS_SET_STATS(iter);
                                STRINGS_MUTEX_UNLOCK_RETURN(0);
                        } else if(strcmp_res <0){
                                prev_iter = iter;
                                iter = iter->next;
                        }else {
                               	iter->prev->next=new;
                               	new->next = iter;
                                new->prev = iter->prev;
                                iter->prev = new;
                		TS_strings_list_ptr->last = new; 
                		TS_strings_list_ptr->sz++;
				STRINGS_SET_STATS(new);
                                STRINGS_MUTEX_UNLOCK_RETURN(1);
                        }
                }    
                prev_iter->next = new; /* put on end */
                new->next = NULL;
                new->prev = prev_iter;
                TS_strings_list_ptr->last = new; 
                TS_strings_list_ptr->end = new; 
               	TS_strings_list_ptr->sz++;
		STRINGS_SET_STATS(new);
                STRINGS_MUTEX_UNLOCK_RETURN(2);
        }/* End forward direction */

        else {  /* search_forward == 0 */
                while (iter) {
			ctr++; 
                        strcmp_res = strcmp(iter->string, new->string);
                        if (strcmp_res==0){ 
                                /* Non-unique String */
				free(new);
				STRINGS_SET_STATS(iter);
                                STRINGS_MUTEX_UNLOCK_RETURN(0);
                        } else if(strcmp_res > 0){  
                                prev_iter = iter;
                                iter = iter->prev;
                        }else {
                                prev_iter->prev=new;
                                new->next = prev_iter;
                                new->prev = iter;
                                iter->next = new; 
                		TS_strings_list_ptr->last = new; 
               			TS_strings_list_ptr->sz++;
				STRINGS_SET_STATS(new);
                                STRINGS_MUTEX_UNLOCK_RETURN(5);
                        }    
                }    
                prev_iter->prev = new; /* put at top */
                new->next = prev_iter;
                new->prev = NULL;
               	TS_strings_list_ptr->sz++;
                TS_strings_list_ptr->top = new; 
               	TS_strings_list_ptr->last = new; 
		STRINGS_SET_STATS(new);
                STRINGS_MUTEX_UNLOCK_RETURN(7);
        }/* End back direction */

}/* End strings_list_store () */


void print_string_list(struct strings_list_head *TS_strings_list_ptr){
        struct strings_list *iter;
	int ctr=0;

	TS_printf(PRINT_STATS, "\n ********** Start print_string_list[%i] ********* ", TS_strings_list_ptr->sz);
#if DEBUG
        TS_printf(PRINT_DEBUG, "\n [TS_strings_list_ptr->last->string:%s ]", TS_strings_list_ptr->last->string);  
        TS_printf(PRINT_DEBUG, "\n TS_strings_list_ptr->top->string:%s ", TS_strings_list_ptr->top->string);  
        TS_printf(PRINT_DEBUG, "\n TS_strings_list_ptr->end->string:%s ", TS_strings_list_ptr->end->string);  
        TS_printf(PRINT_DEBUG, "\n TS_strings_list_ptr->sz:%i ", TS_strings_list_ptr->sz);  
        TS_printf(PRINT_DEBUG, "\n TS_strings_list_ptr->stat_llsv:%i ", TS_strings_list_ptr->stat_llsv);  
#endif

	pthread_mutex_lock(&TS_strings_list_ptr->list_mutex);

	if(TS_strings_list_ptr->sz==0 || TS_strings_list_ptr->top==0 || TS_strings_list_ptr->end==0 || TS_strings_list_ptr->last==0){
		pthread_mutex_unlock(&TS_strings_list_ptr->list_mutex);
		return;
	}
	
        for (iter=TS_strings_list_ptr->top; iter!=NULL && iter!=TS_strings_list_ptr->end && ctr<TS_strings_list_ptr->sz; iter=iter->next) {
		ctr++;
		if(iter->opt_string2 != NULL) {
			TS_printf(PRINT_STATS, "\n [%i,%i], %s%s ", ctr, iter->stat_hits, iter->string, iter->opt_string2);
		} else {
			TS_printf(PRINT_STATS,"\n [%i,%i], %s ", ctr, iter->stat_hits, iter->string);
		}
	}
	if(iter == TS_strings_list_ptr->end){
		if(iter->opt_string2 != NULL) {
			TS_printf(PRINT_STATS, "\n [%i,%i], %s%s ", ctr+1, iter->stat_hits, iter->string, iter->opt_string2);
		} else {
			TS_printf(PRINT_STATS, "\n [%i,%i], %s ", ctr+1, iter->stat_hits, iter->string);
		}
	}

	/* AUDITS START */
	/*********************/
	if(iter==NULL){
		TS_printf(PRINT_ERROR, "\n ERROR!: Iter was NULL!");
	}
	if(iter!=TS_strings_list_ptr->end && iter!=NULL ){
		/* End excluded */
		TS_printf(PRINT_ERROR, "\n ERROR! Did not get to iter!=TS_strings_list_ptr->end but ctr==TS_strings_list_ptr->sz \n");
	}
	/* AUDITS END */
	/*********************/

	pthread_mutex_unlock(&TS_strings_list_ptr->list_mutex);

	TS_printf(PRINT_STATS,"\n *********** End print_string_list[%i]  ******** ", TS_strings_list_ptr->sz);
}/* End print_string_list(void)*/


void clear_string_list(struct strings_list_head *TS_strings_list_ptr){
        struct strings_list *iter;
        struct strings_list *next_iter;
	int ctr=0;
	TS_printf(PRINT_VERBOSE, "\n Clear string_list[%i] ********* ", TS_strings_list_ptr->sz);
	pthread_mutex_lock(&TS_strings_list_ptr->list_mutex);

	if(TS_strings_list_ptr->sz==0 || TS_strings_list_ptr->top==0 || TS_strings_list_ptr->end==0 || TS_strings_list_ptr->last==0){
		pthread_mutex_unlock(&TS_strings_list_ptr->list_mutex);
		return;
	}
        for (iter=TS_strings_list_ptr->top; iter!=NULL && iter!=TS_strings_list_ptr->end && ctr<TS_strings_list_ptr->sz; ctr++) {
		next_iter = iter->next;
		free(iter);
		iter=next_iter;
	} 
	free(TS_strings_list_ptr->end);
        TS_strings_list_ptr->top = NULL;
        TS_strings_list_ptr->end = NULL;
        TS_strings_list_ptr->last = NULL;
        TS_strings_list_ptr->sz = 0;
	/* TS_strings_list_ptr stats are not cleared */

	pthread_mutex_unlock(&TS_strings_list_ptr->list_mutex);

} /* End clear_string_list(struct strings_list_head *TS_strings_list_ptr) */


void TS_tv2str(struct timeval *tv, char *buf){
	/* Make buf is large enough to accomodate the string i.e buf[TS_DATESTR_SZ] */
        time_t nowtime;
        struct tm *nowtm;
        char tmbuf[TS_DATESTR_SZ];

	nowtime = tv->tv_sec;
        nowtm = localtime(&nowtime);
        strftime(tmbuf, TS_DATESTR_SZ, "%Y-%m-%d/%H.%M/%S", nowtm);
        snprintf(buf, TS_DATESTR_SZ, "%s.%06d", tmbuf, (int)tv->tv_usec);
}

