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
 * @file btrMgr_streamIn.h
 *
 * @description This file defines bluetooth manager's data streaming interfaces to external BT devices
 *
 */

#ifndef __BTR_MGR_STREAMIN_H__
#define __BTR_MGR_STREAMIN_H__

typedef void* tBTRMgrSiHdl;

/* Interfaces */
eBTRMgrRet BTRMgr_SI_Init (tBTRMgrSiHdl* phBTRMgrSiHdl);
eBTRMgrRet BTRMgr_SI_DeInit (tBTRMgrSiHdl hBTRMgrSiHdl);
eBTRMgrRet BTRMgr_SI_GetDefaultSettings (tBTRMgrSiHdl hBTRMgrSiHdl);
eBTRMgrRet BTRMgr_SI_GetCurrentSettings (tBTRMgrSiHdl hBTRMgrSiHdl);
eBTRMgrRet BTRMgr_SI_GetStatus (tBTRMgrSiHdl hBTRMgrSiHdl, stBTRMgrMediaStatus* apstBtrMgrSiStatus);
eBTRMgrRet BTRMgr_SI_Start (tBTRMgrSiHdl hBTRMgrSiHdl, int aiInBufMaxSize, int aiBTDevFd, int aiBTDevMTU, unsigned int aiBTDevSFreq);
eBTRMgrRet BTRMgr_SI_Stop (tBTRMgrSiHdl hBTRMgrSiHdl);
eBTRMgrRet BTRMgr_SI_Pause (tBTRMgrSiHdl hBTRMgrSiHdl);
eBTRMgrRet BTRMgr_SI_Resume (tBTRMgrSiHdl hBTRMgrSiHdl);
eBTRMgrRet BTRMgr_SI_SendBuffer (tBTRMgrSiHdl hBTRMgrSiHdl, char* pcInBuf, int aiInBufSize);
eBTRMgrRet BTRMgr_SI_SendEOS (tBTRMgrSiHdl hBTRMgrSiHdl);

#endif /* __BTR_MGR_STREAMOUT_H__ */
