/**
 * @file    qisr.h
 * @brief   iFLY Speech Recognizer Header File
 * 
 *  This file contains the quick application programming interface (API) declarations 
 *  of ISR. Developer can include this file in your project to build applications.
 *  For more information, please read the developer guide.
 
 *  Use of this software is subject to certain restrictions and limitations set
 *  forth in a license agreement entered into between iFLYTEK, Co,LTD.
 *  and the licensee of this software.  Please refer to the license
 *  agreement for license use rights and restrictions.
 *
 *  Copyright (C)    1999 - 2007 by ANHUI USTC iFLYTEK, Co,LTD.
 *                   All rights reserved.
 * 
 * @author  Speech Dept. iFLYTEK.
 * @version 1.0
 * @date    2008/12/12
 * 
 * @see        
 * 
 * History:
 * index    version        date            author        notes
 * 0        1.0            2008/12/12      Speech        Create this file
 */

#ifndef __QISR_H__
#define __QISR_H__

#ifdef __cplusplus
extern "C" {
#endif /* C++ */

#include "msp_types.h"

const char* MSPAPI QISRSessionBegin(const char* grammarList, const char* params, int* errorCode);
typedef const char* (MSPAPI *Proc_QISRSessionBegin)(const char* grammarList, const char* params, int *result);

int MSPAPI QISRAudioWrite(const char* sessionID, const void* waveData, unsigned int waveLen, int audioStatus, int *epStatus, int *recogStatus);
typedef int (MSPAPI *Proc_QISRAudioWrite)(const char* sessionID, const void* waveData, unsigned int waveLen, int audioStatus, int *epStatus, int *recogStatus);

const char * MSPAPI QISRGetResult(const char* sessionID, int* rsltStatus, int waitTime, int *errorCode);
typedef const char * (MSPAPI *Proc_QISRGetResult)(const char* sessionID, int* rsltStatus, int waitTime, int *errorCode);

int MSPAPI QISRSessionEnd(const char* sessionID, const char* hints);
typedef int (MSPAPI *Proc_QISRSessionEnd)(const char* sessionID, const char* hints);

int MSPAPI QISRGetParam(const char* sessionID, const char* paramName, char* paramValue, unsigned int* valueLen);
typedef int (MSPAPI *Proc_QISRGetParam)(const char* sessionID, const char* paramName, char* paramValue, unsigned int* valueLen);

int MSPAPI QISRSetParam(const char* sessionID, const char* paramName, const char* paramValue);
typedef int (MSPAPI *Proc_QISRSetParam)(const char* sessionID, const char* paramName, const char* paramValue);

typedef void ( *recog_result_ntf_handler)( const char *sessionID, const char *result, int resultLen, int resultStatus, void *userData ); 
typedef void ( *recog_status_ntf_handler)( const char *sessionID, int type, int status, int param1, const void *param2, void *userData);
typedef void ( *recog_error_ntf_handler)(const char *sessionID, int errorCode,	const char *detail, void *userData);
int MSPAPI QISRRegisterNotify(const char *sessionID, recog_result_ntf_handler rsltCb, recog_status_ntf_handler statusCb, recog_error_ntf_handler errCb, void *userData);

#ifdef __cplusplus
} /* extern "C" */	
#endif /* C++ */

#endif /* __QISR_H__ */