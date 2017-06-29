// This file is part of BOINC.
// http://boinc.berkeley.edu
// Copyright (C) 2008 University of California
//
// BOINC is free software; you can redistribute it and/or modify it
// under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation,
// either version 3 of the License, or (at your option) any later version.
//
// BOINC is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with BOINC.  If not, see <http://www.gnu.org/licenses/>.

// A sample validator that requires a majority of results to be
// bitwise identical.
// This is useful only if either
// 1) your application does no floating-point math, or
// 2) you use homogeneous redundancy

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>


#define TSLEEP 5      // seconds to sleep after failure to open file

#include "config.h"
#include "util.h"
#include "error_numbers.h"
#include "sched_util.h"
#include "sched_msgs.h"
#include "validate_util.h"
#include "md5_file.h"

using std::string;
using std::vector;

/* definitions for version 4503 onwards */
#define ROWSIZE 4096
#define NUMITEM 60
#define MTURPOS 0
#define STABPOS 1
#define VERSPOS 51
#define TUR1POS 21
#define TUR2POS 22
#define TUR3POS 23 // iza 26/05/2017
#define TOTTURN 58 // obsolete
#define DNMSPOS 58
#define CPUTIME 59
#define SIXCREDIT 2.0E-6
//#define SIXCREDIT 2.5E-6

struct FILE_CKSUM {
	FILE *fp;
};

struct FILE_CKSUM_LIST {
    vector<FILE *> fp;   // list of MD5s of files
    ~FILE_CKSUM_LIST(){}
};

bool files_match(RESULT &r1, FILE_CKSUM_LIST& f1, const RESULT &r2, FILE_CKSUM_LIST& f2) {

    int c1, c2;
    int ipos, jpos;
    unsigned int n = 0;

    char s1[ROWSIZE], s2[ROWSIZE];
    double mturn, tturn, turn1, turn2, turnx, credit, stab, vers1, vers2, cput1, cput2;
    int indx1[NUMITEM], indx2[NUMITEM];
    int ns1 = 0, ns2 = 0;
    int w1 = 0, w2 = 0;
    int p1, p2, k;
    int r1id = r1.id, u1id = r1.userid, h1id = r1.hostid; 
    double claimed_credit1 = r1.claimed_credit;
    int r2id = r2.id, u2id = r2.userid, h2id = r2.hostid; 
    double claimed_credit2 = r2.claimed_credit;
    int wrkunit = r1.workunitid;
    double fracturn = 1.0;
    int outlier = 0;
    char crl1, crl2;
    struct stat fileprop;
		

//    printf("start files_match: %d %d\n",f1.fp.size(),f2.fp.size());
    for (int i=0; i<f1.fp.size(); i++){
	fstat(fileno(f1.fp[i]), &fileprop);
	if( fileprop.st_size == 0 ) {
// IZA 20170610 add print of the WU and invalidate the file immediately
                log_messages.printf(MSG_DEBUG, "[RESULT#%d %d] WU#%d HOST[%d %d] EMPTY-1\n", r1id,r2id,wrkunit,h1id,h2id);
		return false;
	}
	rewind(f1.fp[i]);
	for(int j=0; j<f2.fp.size(); j++){
	   fstat(fileno(f2.fp[j]), &fileprop);
	   if( fileprop.st_size == 0 ) {
// IZA 20170610 add print of the WU and invalidate the file immediately
                log_messages.printf(MSG_DEBUG, "[RESULT#%d %d] WU#%d HOST[%d %d] EMPTY-2\n", r1id,r2id,wrkunit,h1id,h2id);
		return false;
 	   }
	   rewind(f2.fp[j]);
	   n = 0;
	   turnx = 0.0;
           do {
		ns1 = ns2 = 0;
		for(w1=0; w1<NUMITEM; w1++) { indx1[w1] = indx2[w1] = 0; }
		if(fgets(s1,ROWSIZE-1,f1.fp[i]) != NULL) ns1 = strlen(s1);
		if(fgets(s2,ROWSIZE-1,f2.fp[j]) != NULL) ns2 = strlen(s2);
		if((ns1 <= 0) && (ns2 <= 0)) break;
		turn1 = turn2 = 0.0;
		vers1 = vers2 = 0.0;

		if (ns1 > ROWSIZE-1 || ns2 > ROWSIZE-1){ //printf("IZA> %d %d %d %d %d %d   ns1=%d ns2=%d\n",r1id,r2id,u1id,u2id,h1id,h2id,ns1,ns2);
                   log_messages.printf(MSG_DEBUG, "[RESULT#%d %d] buffer too small: %d %d \n", r1id,r2id,ns1,ns2);
		   if(ns1 > ROWSIZE-1) ns1 = ROWSIZE-1;
		   if(ns2 > ROWSIZE-1) ns2 = ROWSIZE-1;
		}
		for(w1 = 0, p1 = 0; (w1 < NUMITEM) && (p1 < ns1); w1++) {
			while(isblank(s1[p1]) && (p1 < ns1)) p1++; indx1[w1] = p1; while(isgraph(s1[p1]) && (p1<ns1)) p1++; s1[p1++] = '\0';
		}
		for(w2 = 0, p2 = 0; (w2 < NUMITEM) && (p2 < ns2); w2++) {
			while(isblank(s2[p2]) && (p2 < ns2)) p2++; indx2[w2] = p2; while(isgraph(s2[p2]) && (p2<ns2)) p2++; s2[p2++] = '\0';
//			while((s2[p2] == ' ') && (p2 < ns2)) p2++; indx2[w2] = p2; while((s2[p2] != ' ') && (p2<ns2)) p2++; s2[p2++] = '\0';
		}
//for(w1=0; w1<NUMITEM; w1++) printf("w1=%d      %d %s       %d %s\n",w1, indx1[w1], &s1[indx1[w1]],indx2[w1], &s2[indx2[w1]]);
//printf(" w1=%d %d w2=%d %d %d %d %d %d %d %d\n",w1,p1,w2,p2,indx1[0],indx1[1],indx1[2],indx1[3],indx1[w1-1],p1);
//exit(1);			
// printf(" w1=%d w2=%d turn1=%s %s turn2\n",w1,w2,&(s1[indx1[22]]),&(s2[indx2[22]]));

		if(w1 != w2) {
		   if( w1 > 0 ) { // special case of \0 in the output
       		        turn1 = strtod(&s1[indx1[TUR1POS]], NULL);
       			vers1 = strtod(&s1[indx1[VERSPOS]], NULL);
			
		   }
		   if( w2 > 0 ) { // special case of \0 in the output
			turn2 = strtod(&s2[indx2[TUR2POS]], NULL);
       			vers2 = strtod(&s2[indx2[VERSPOS]], NULL);
		   }
//exit(1);
		   if( (turn1 == turn2) && (turn2 == 0) )  {
                        log_messages.printf(MSG_DEBUG,
		       "[RESULT#%d %d] case of zero file: %d %d %d %d turns: %.0f %.0f version: %.0f %.0f\n",
			r1id,r2id,ns1,ns2,w1,w2,turn1,turn2,vers1,vers2);

			return true;
		   }
                   log_messages.printf(MSG_DEBUG,
		       "[RESULT#%d %d] differ in #doubles: %d %d %d %d turns: %.0f %.0f version: %.0f %.0f\n",
			r1id,r2id,ns1,ns2,w1,w2,turn1,turn2,vers1,vers2);

		   return false;
                }
		for(k = 0; k < w1; k++) {
			if(k == VERSPOS) continue; // versions;
			if(k == DNMSPOS) continue; // dnms ;
			if(k == CPUTIME) continue; // cputime sec;

	        	ipos = indx1[k];
	        	jpos = indx2[k];
		        while(!isdigit(c1 = s1[ipos++]) && (c1 != 0)) ;
		        while(!isdigit(c2 = s2[jpos++]) && (c2 != 0)) ;
			if(c1 != c2) {
                   		log_messages.printf(MSG_DEBUG,
			               "[RESULT#%d %d] WU#%d HOST[%d %d] differ: %d %s %s p=%d V: %s %s\n",
					r1id,r2id,wrkunit,h1id,h2id,k+1,&s1[indx1[k]],&s2[indx2[k]],n,&s1[indx1[VERSPOS]],&s2[indx2[VERSPOS]]);
//exit(1);
		   		return false;
                	}
			//printf("values match  k = %d %s %s\n",k,&s1[indx1[k]],&s2[indx2[k]]);
		}

       		mturn = strtod(&s1[indx1[MTURPOS]], NULL);
//       		stab  = strtod(&s1[indx1[STABPOS]], NULL);
       		turnx += strtod(&s1[indx1[TUR1POS]], NULL) + strtod(&s2[indx2[TUR2POS]], NULL);
       		vers1 = strtod(&s1[indx1[VERSPOS]], NULL);
       		vers2 = strtod(&s2[indx2[VERSPOS]], NULL);
       		cput1 = strtod(&s1[indx1[CPUTIME]], NULL);
       		cput2 = strtod(&s2[indx2[CPUTIME]], NULL);
//       		tturn = strtod(&s1[indx1[TOTTURN]], NULL);
		tturn = 0;
		n++;
           } while ((ns1 > 0) || (ns2 > 0));
    	   credit = turnx * SIXCREDIT;
	   crl1 = crl2 = 'K';
	   if(claimed_credit1 > 10.0 || claimed_credit2 > 10.0) {
	   	if(claimed_credit1 > 1.3*credit) { r1.claimed_credit = credit; crl1='F'; } else crl1='K';
//	   	if(claimed_credit2 > 1.3*credit) { r2.claimed_credit = credit; crl2='F'; } else crl2='K';
	   }
	   fracturn = turnx/(mturn*n*2.0);
	   outlier = 0;
// Eric added OR if turnx is 0, also an outlier
	   if(fracturn <= 0.1 || turnx <= 0.1) { 
		r1.runtime_outlier = 1;
//              r2.runtime_outlier = 1;
		outlier = 1;
	   }
//    printf("files match  %.0f %d %.0f %.0f v: %s %s\n",mturn,n,turn1,turn2,&s1[indx1[VERSPOS]],&s2[indx2[VERSPOS]]);
           log_messages.printf(MSG_DEBUG,"RES[%-d %-d] WU=%-d HST[%-d %-d] match: m=%.0g %.0g p=%d turnx=%10.0f cred=%8.2f T1=%10.1f claim=%.2f %c T2=%10.1f %.2f %c %-1d v: %.0f %.0f\n",
		r1id,r2id,wrkunit,h1id,h2id,mturn,tturn,n,turnx,credit,cput1,claimed_credit1,crl1,cput2,claimed_credit2,crl2,outlier,vers1,vers2);
        }
    }

//exit(1);
    return true;
}

int init_result(RESULT& result, void*& data) {
    int retval;
    FILE_CKSUM_LIST* fcl = new FILE_CKSUM_LIST;
    vector<OUTPUT_FILE_INFO> files;
    struct stat file_status;
    double nbytes;
	int ii;

    retval = get_output_file_infos(result, files);
    if (retval) {
        log_messages.printf(MSG_CRITICAL,
            "[RESULT#%d %s] check_set: can't get output filenames\n",
            result.id, result.name
        );
        return retval;
    }

    for (unsigned int i=0; i<files.size(); i++) {
        OUTPUT_FILE_INFO& fi = files[i];
	FILE * fpx;
        if (fi.no_validate) continue;
        //strcpy(buf,(const char *) fi.path.c_str());
        //printf("open name: %s %d %d\n",fi.path.c_str(),i,files.size());
	ii = 0;
//        fpx = fopen(fi.path.c_str(),"r");
//        while( ((fpx = fopen(fi.path.c_str(),"r")) == NULL) & (ii < 10)) { sleep(TSLEEP); ii++; }
//        if((fpx = fopen(fi.path.c_str(),"r")) == NULL) {
	retval = try_fopen(fi.path.c_str(), fpx, "r");
        if(retval) {

            log_messages.printf(MSG_CRITICAL,
		"[RESULT#%d %s] Couldn't open %s file %d out of %d errno=%d %s\n",
                    result.id, result.name, fi.path.c_str(),i,files.size(),errno,strerror(errno));

            return retval;
        }
	retval = stat(fi.path.c_str(), &file_status);
	if(retval) return retval;

	nbytes = file_status.st_size;

	if(nbytes <= 0) {

            log_messages.printf(MSG_DEBUG,
		"[RESULT#%d %s] Invalid %s file %d out of %d filesize=%d\n",
                    result.id, result.name, fi.path.c_str(),i,files.size(),nbytes);
	    return -1;   // invalid result if file zero length	
	}
	fcl->fp.push_back(fpx);

    }
    data = (void*) fcl;
    return 0;
}

int compare_results(
    RESULT & r1, void* data1,
    RESULT const& r2, void* data2,
    bool& match
) {
    FILE_CKSUM_LIST* f1 = (FILE_CKSUM_LIST*) data1;
    FILE_CKSUM_LIST* f2 = (FILE_CKSUM_LIST*) data2;

    match = files_match(r1, *f1, r2, *f2);
    return 0;
}

int cleanup_result(RESULT const& result, void* data) {
    FILE_CKSUM_LIST* fi = (FILE_CKSUM_LIST*) data;
    int i;
	if( fi == NULL) return(0);

//	log_messages.printf(MSG_DEBUG," cleanup_result start");

    for (int i=0; i<(*fi).fp.size(); i++){

	if((*fi).fp[i] != NULL) {
   	    if(fclose((*fi).fp[i]) > 0) {
               log_messages.printf(MSG_CRITICAL," error on closing, errno=%d %s",errno,strerror(errno));
/*		"[RESULT#%d %s] Couldn't close %s file %d out of %d errno=%d %s\n",
                    result.id, result.name, (*fi).fp[i].path.c_str(),i,(*fi).fp.size(),errno,strerror(errno));
*/          }
        }
    }
    delete (FILE_CKSUM_LIST*) data;
//	log_messages.printf(MSG_DEBUG," cleanup_result end\n");
    return 0;
}

//double compute_granted_credit(WORKUNIT& wu, vector<RESULT>& results) {
//    return median_mean_credit(wu, results);
//    return  stddev_credit(wu, results);
//}

const char *BOINC_RCSID_7ab2b7189c = "$Id: sample_bitwise_validator.cpp 17966 2009-05-01 18:25:17Z davea $";
