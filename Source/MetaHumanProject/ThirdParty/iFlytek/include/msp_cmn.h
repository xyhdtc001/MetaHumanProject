/**
 * @file    msp_cmn.h
 * @brief   Mobile Speech Platform Common Interface Header File
 * 
 *  This file contains the quick common programming interface (API) declarations 
 *  of MSP. Developer can include this file in your project to build applications.
 *  For more information, please read the developer guide.
 
 *  Use of this software is subject to certain restrictions and limitations set
 *  forth in a license agreement entered into between iFLYTEK, Co,LTD.
 *  and the licensee of this software.  Please refer to the license
 *  agreement for license use rights and restrictions.
 *
 *  Copyright (C)    1999 - 2012 by ANHUI USTC iFLYTEK, Co,LTD.
 *                   All rights reserved.
 * 
 * @author  Speech Dept. iFLYTEK.
 * @version 1.0
 * @date    2012/09/01
 * 
 * @see        
 * 
 * History:
 * index    version        date            author        notes
 * 0        1.0            2012/09/01      MSC40        Create this file
 */

#ifndef __MSP_CMN_H__
#define __MSP_CMN_H__

#include "msp_types.h"

#ifdef __cplusplus
extern "C" {
#endif /* C++ */

char *Wchar2Mbytes(const wchar_t* wcstr);
wchar_t *Mbytes2Wchar(const char *mbstr);

int MSPAPI MSPLogin(const char* usr, const char* pwd, const char* params);
typedef int (MSPAPI *Proc_MSPLogin)(const char* usr, const char* pwd, const char* params);

int MSPAPI MSPLoginW(const wchar_t* usr, const wchar_t* pwd, const wchar_t* params);
typedef int (MSPAPI *Proc_MSPLoginW)(const wchar_t* usr, const wchar_t* pwd, const wchar_t* params);

int MSPAPI MSPLogout();
typedef int (MSPAPI *Proc_MSPLogout)();

int MSPAPI MSPLogoutW();
typedef int (MSPAPI *Proc_MSPLogoutW)();

int MSPAPI MSPUpload( const char* dataName, const char* params, const char* dataID);
typedef int (MSPAPI* Proc_MSPUpload)( const char* dataName, const char* params, const char* dataID);

typedef int (*DownloadStatusCB)(int errorCode, long param1, const void *param2, void *userData);
typedef int (*DownloadResultCB)(const void *data, long dataLen, void *userData);
int MSPAPI MSPDownload(const char* dataName, const char* params, DownloadStatusCB statusCb, DownloadResultCB resultCb, void*userData);
typedef int (MSPAPI* Proc_MSPDownload)(const char* dataName, const char* params, DownloadStatusCB statusCb, DownloadResultCB resultCb, void*userData);

int MSPAPI MSPDownloadW(const wchar_t* wdataName, const wchar_t* wparams, DownloadStatusCB statusCb, DownloadResultCB resultCb, void*userData);
typedef int (MSPAPI* Proc_MSPDownloadW) (const wchar_t* wdataName, const wchar_t* wparams, DownloadStatusCB statusCb, DownloadResultCB resultCb, void*userData);

int MSPAPI MSPAppendData(void* data, unsigned int dataLen, unsigned int dataStatus);
typedef int (MSPAPI* Proc_MSPAppendData)(void* data, unsigned int dataLen, unsigned int dataStatus);

const char* MSPAPI MSPGetResult(unsigned int* rsltLen, int* rsltStatus, int *errorCode);
typedef const char * (MSPAPI *Proc_MSPGetResult)(unsigned int* rsltLen, int* rsltStatus, int *errorCode);

int MSPAPI MSPSetParam( const char* paramName, const char* paramValue );
typedef int (MSPAPI *Proc_MSPSetParam)(const char* paramName, const char* paramValue);

int MSPAPI MSPGetParam( const char *paramName, char *paramValue, unsigned int *valueLen );
typedef int (MSPAPI *Proc_MSPGetParam)( const char *paramName, char *paramValue, unsigned int *valueLen );

const char* MSPAPI MSPUploadData(const char* dataName, void* data, unsigned int dataLen, const char* params, int* errorCode);
typedef const char* (MSPAPI* Proc_MSPUploadData)(const char* dataName, void* data, unsigned int dataLen, const char* params, int* errorCode);

const void* MSPAPI MSPDownloadData(const char* params, unsigned int* dataLen, int* errorCode);
typedef const void* (MSPAPI* Proc_MSPDownloadData)(const char* params, unsigned int* dataLen, int* errorCode);

const void* MSPAPI MSPDownloadDataW(const wchar_t* params, unsigned int* dataLen, int* errorCode);
typedef const void* (MSPAPI* Proc_MSPDownloadDataW)(const wchar_t* params, unsigned int* dataLen, int* errorCode);

const char* MSPAPI MSPSearch(const char* params, const char* text, unsigned int* dataLen, int* errorCode);
typedef const char* (MSPAPI* Proc_MSPSearch)(const char* params, const char* text, unsigned int* dataLen, int* errorCode);

typedef int (*NLPSearchCB)(const char *sessionID, int errorCode, int status, const void* result, long rsltLen, void *userData);
const char* MSPAPI MSPNlpSearch(const char* params, const char* text, unsigned int textLen, int *errorCode, NLPSearchCB callback, void *userData);
typedef const char* (MSPAPI* Proc_MSPNlpSearch)(const char* params, const char* text, unsigned int textLen, int *errorCode, NLPSearchCB callback, void *userData);

int MSPAPI MSPNlpSchCancel(const char *sessionID, const char *hints);

typedef void ( *msp_status_ntf_handler)( int type, int status, int param1, const void *param2, void *userData );
int MSPAPI MSPRegisterNotify( msp_status_ntf_handler statusCb, void *userData );
typedef const char* (MSPAPI* Proc_MSPRegisterNotify)( msp_status_ntf_handler statusCb, void *userData );

const char* MSPAPI MSPGetVersion(const char *verName, int *errorCode);
typedef const char* (MSPAPI * Proc_MSPGetVersion)(const char *verName, int *errorCode);

#ifdef __cplusplus
} /* extern "C" */	
#endif /* C++ */

#endif /* __MSP_CMN_H__ */