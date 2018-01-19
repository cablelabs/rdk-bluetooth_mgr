/*
 * If not stated otherwise in this file or this component's Licenses.txt file the
 * following copyright and licenses apply:
 *
 * Copyright 2016 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/
/**
 * @file btrMgr_audioCap.h
 *
 * @description This file defines bluetooth manager's audio capture interfaces to receiver
 * data from audio capture modules
 *
 */

#ifndef __BTR_MGR_AUDIOCAP_H__
#define __BTR_MGR_AUDIOCAP_H__

#include "btrMgr_Types.h"

typedef void* tBTRMgrAcHdl;

/* Fptr Callbacks types */
typedef eBTRMgrRet (*fPtr_BTRMgr_AC_DataReadyCb) (void* apvAcDataBuf, unsigned int aui32AcDataLen, void *apvUserData);

/* Interfaces */
eBTRMgrRet BTRMgr_AC_Init (tBTRMgrAcHdl* phBTRMgrAcHdl);
eBTRMgrRet BTRMgr_AC_DeInit (tBTRMgrAcHdl hBTRMgrAcHdl);
eBTRMgrRet BTRMgr_AC_GetDefaultSettings (tBTRMgrAcHdl hBTRMgrAcHdl, stBTRMgrOutASettings* apstBtrMgrAcOutASettings);
eBTRMgrRet BTRMgr_AC_GetCurrentSettings (tBTRMgrAcHdl hBTRMgrAcHdl, stBTRMgrOutASettings* apstBtrMgrAcOutASettings);
eBTRMgrRet BTRMgr_AC_GetStatus (tBTRMgrAcHdl hBTRMgrAcHdl, stBTRMgrMediaStatus* apstBtrMgrAcStatus);
eBTRMgrRet BTRMgr_AC_Start (tBTRMgrAcHdl hBTRMgrAcHdl, stBTRMgrOutASettings* apstBtrMgrAcOutASettings, fPtr_BTRMgr_AC_DataReadyCb afpcBBtrMgrAcDataReady, void* apvUserData);
eBTRMgrRet BTRMgr_AC_Stop (tBTRMgrAcHdl hBTRMgrAcHdl);
eBTRMgrRet BTRMgr_AC_Pause (tBTRMgrAcHdl hBTRMgrAcHdl);
eBTRMgrRet BTRMgr_AC_Resume (tBTRMgrAcHdl hBTRMgrAcHdl);

#endif /* __BTR_MGR_AUDIOCAP_H__ */

