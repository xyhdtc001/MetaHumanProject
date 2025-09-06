/** 
 * @file	qtts.h
 * @brief   iFLY Speech Synthesizer Header File
 * 
 *  This file contains the quick application programming interface (API) declarations 
 *  of TTS. Developer can include this file in your project to build applications.
 *  For more information, please read the developer guide.
 
 *  Use of this software is subject to certain restrictions and limitations set
 *  forth in a license agreement entered into between iFLYTEK, Co,LTD.
 *  and the licensee of this software.  Please refer to the license
 *  agreement for license use rights and restrictions.
 *
 *  Copyright (C)    1999 - 2009 by ANHUI USTC iFLYTEK, Co,LTD.
 *                   All rights reserved.
 * 
 * @author	Speech Dept.
 * @version	1.0
 * @date	2009/11/26
 * 
 * @see		
 * 
 * <b>History:</b><br>
 * <table>
 *  <tr> <th>Version	<th>Date		<th>Author	<th>Notes</tr>
 *  <tr> <td>1.0		<td>2009/11/26	<td>Speech	<td>Create this file</tr>
 * </table>
 * 
 */
#ifndef __QTTS_H__
#define __QTTS_H__

#if !defined(MSPAPI)
#if defined(WIN32)
	#define MSPAPI __stdcall
#else
	#define MSPAPI
#endif /* WIN32 */
#endif /* MSPAPI */

#ifdef __cplusplus
extern "C" {
#endif /* C++ */

#include "msp_types.h"

const char* MSPAPI QTTSSessionBegin(const char* params, int* errorCode);
typedef const char* (MSPAPI *Proc_QTTSSessionBegin)(const char* params, int* errorCode);

int MSPAPI QTTSTextPut(const char* sessionID, const char* textString, unsigned int textLen, const char* params);
typedef int (MSPAPI *Proc_QTTSTextPut)(const char* sessionID, const char* textString, unsigned int textLen, const char* params);

const void* MSPAPI QTTSAudioGet(const char* sessionID, unsigned int* audioLen, int* synthStatus, int* errorCode);
typedef const void* (MSPAPI *Proc_QTTSAudioGet)(const char* sessionID, unsigned int* audioLen, int* synthStatus, int* errorCode);

const char* MSPAPI QTTSAudioInfo(const char* sessionID);
typedef const char* (MSPAPI *Proc_QTTSAudioInfo)(const char* sessionID);

int MSPAPI QTTSSessionEnd(const char* sessionID, const char* hints);
typedef int (MSPAPI *Proc_QTTSSessionEnd)(const char* sessionID, const char* hints);

int MSPAPI QTTSGetParam(const char* sessionID, const char* paramName, char* paramValue, unsigned int* valueLen);
typedef int (MSPAPI *Proc_QTTSGetParam)(const char* sessionID, const char* paramName, char* paramValue, unsigned int* valueLen);

int MSPAPI QTTSSetParam(const char *sessionID, const char *paramName, const char *paramValue);
typedef int (MSPAPI *Proc_QTTSSetParam)(const char* sessionID, const char* paramName, char* paramValue);

typedef void ( *tts_result_ntf_handler)( const char *sessionID, const char *audio, int audioLen, int synthStatus, int ced, const char *audioInfo, int audioInfoLen, void *userData ); 
typedef void ( *tts_status_ntf_handler)( const char *sessionID, int type, int status, int param1, const void *param2, void *userData);
typedef void ( *tts_error_ntf_handler)(const char *sessionID, int errorCode,	const char *detail, void *userData);
int MSPAPI QTTSRegisterNotify(const char *sessionID, tts_result_ntf_handler rsltCb, tts_status_ntf_handler statusCb, tts_error_ntf_handler errCb, void *userData);

#ifdef __cplusplus
} /* extern "C" */
#endif /* C++ */

#endif /* __QTTS_H__ */