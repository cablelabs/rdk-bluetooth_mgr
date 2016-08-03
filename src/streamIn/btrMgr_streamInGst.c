/*
 * ============================================================================
 * RDK MANAGEMENT, LLC CONFIDENTIAL AND PROPRIETARY
 * ============================================================================
 * This file (and its contents) are the intellectual property of RDK Management, LLC.
 * It may not be used, copied, distributed or otherwise  disclosed in whole or in
 * part without the express written permission of RDK Management, LLC.
 * ============================================================================
 * Copyright (c) 2016 RDK Management, LLC. All rights reserved.
 * ============================================================================
 */
/**
 * @file btrMgr_streamInGst.c
 *
 * @description This file implements bluetooth manager's GStreamer streaming interface to external BT devices
 *
 * Copyright (c) 2016  Comcast
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


/* System Headers */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


/* Ext lib Headers */
#include <gst/gst.h>
#include <gst/audio/audio.h>
#include <gst/app/gstappsrc.h>


/* Interface lib Headers */


/* Local Headers */
#include "btrMgr_streamInGst.h"


/* Local defines */
#define BTR_MGR_SLEEP_TIMEOUT_MS            1   // Suspend execution of thread. Keep as minimal as possible
#define BTR_MGR_WAIT_TIMEOUT_MS             2   // Use for blocking operations
#define BTR_MGR_MAX_INTERNAL_QUEUE_ELEMENTS 16  // Number of blocks in the internal queue

#define GST_ELEMENT_GET_STATE_RETRY_CNT_MAX 5



typedef struct _stBTRMgrSIGst {
    void*        pPipeline;
    void*        pSrc;
    void*        pSink;
    void*        pAudioDec;
    void*        pAudioParse;
    void*        pfdCapsFilter;
    void*        psbcdecCapsFilter;
    void*        pRtpAudioDePay;
    void*        pLoop;
    void*        pLoopThread;
    guint        busWId;
    GstClockTime gstClkTStamp;
    guint64      inBufOffset;
} stBTRMgrSIGst;


/* Local function declarations */
static gpointer btrMgr_SI_g_main_loop_run_context (gpointer user_data);
static gboolean btrMgr_SI_gstBusCall (GstBus* bus, GstMessage* msg, gpointer data);
static GstState btrMgr_SI_validateStateWithTimeout (GstElement* element, GstState stateToValidate, guint msTimeOut);


/* Local function definitions */
static gpointer
btrMgr_SI_g_main_loop_run_context (
    gpointer user_data
) {
  g_main_loop_run (user_data);
  return NULL;
}


static gboolean
btrMgr_SI_gstBusCall (
    GstBus*     bus,
    GstMessage* msg,
    gpointer    data
) {
    switch (GST_MESSAGE_TYPE (msg)) {
        case GST_MESSAGE_EOS: {
            g_print ("%s:%d:%s - End-of-stream\n", __FILE__, __LINE__, __FUNCTION__);
            break;
        }   
        case GST_MESSAGE_ERROR: {
            gchar*  debug;
            GError* err;

            gst_message_parse_error (msg, &err, &debug);
            g_printerr ("%s:%d:%s - Debugging info: %s\n", __FILE__, __LINE__, __FUNCTION__, (debug) ? debug : "none");
            g_free (debug);

            g_print ("%s:%d:%s - Error: %s\n", __FILE__, __LINE__, __FUNCTION__, err->message);
            g_error_free (err);
            break;
        }   
        default:
            break;
    }

    return TRUE;
}


static GstState 
btrMgr_SI_validateStateWithTimeout (
    GstElement* element,
    GstState    stateToValidate,
    guint       msTimeOut
) {
    GstState    gst_current;
    GstState    gst_pending;
    float       timeout = BTR_MGR_WAIT_TIMEOUT_MS;
    gint        gstGetStateCnt = GST_ELEMENT_GET_STATE_RETRY_CNT_MAX;

    do { 
        if ((GST_STATE_CHANGE_SUCCESS == gst_element_get_state(GST_ELEMENT(element), &gst_current, &gst_pending, timeout * GST_MSECOND)) && (gst_current == stateToValidate)) {
            g_print("%s:%d:%s - gst_element_get_state - SUCCESS : State = %d, Pending = %d\n", __FILE__, __LINE__, __FUNCTION__, gst_current, gst_pending);
            return gst_current;
        }
        usleep(msTimeOut * 1000); // Let element safely transition to required state 
    } while ((gst_current != stateToValidate) && (gstGetStateCnt-- != 0)) ;

    g_print("%s:%d:%s - gst_element_get_state - FAILURE : State = %d, Pending = %d\n", __FILE__, __LINE__, __FUNCTION__, gst_current, gst_pending);

    if (gst_pending == stateToValidate)
        return gst_pending;
    else
        return gst_current;
}


/* Interfaces */
eBTRMgrSIGstRet
BTRMgr_SI_GstInit (
    tBTRMgrSiGstHdl* phBTRMgrSiGstHdl
) {
    GstElement*     appsrc;
    GstElement*     auddec;
    GstElement*     rtpcapsfilter;
    GstElement*     rtpauddepay;
    GstElement*     audparse;
    GstElement*     fdsink;
    GstElement*     pipeline;
    stBTRMgrSIGst*  pstBtrMgrSiGst = NULL;

    GThread*      mainLoopThread;
    GMainLoop*    loop;
    GstBus*       bus;
    guint         busWatchId;


    if ((pstBtrMgrSiGst = (stBTRMgrSIGst*)malloc (sizeof(stBTRMgrSIGst))) == NULL) {
        g_print ("%s:%d:%s - Unable to allocate memory\n", __FILE__, __LINE__, __FUNCTION__);
        return eBTRMgrSIGstFailure;
    }

    memset((void*)pstBtrMgrSiGst, 0, sizeof(stBTRMgrSIGst));

    gst_init (NULL, NULL);

    /* Create elements */
    appsrc   = gst_element_factory_make ("fdsrc", "btmgr-si-fdsrc");

    /*TODO: Select the Audio Codec and RTP Audio Payloader based on input*/
    auddec      = gst_element_factory_make ("sbcdec", "btmgr-si-sbcdec");
    rtpcapsfilter = gst_element_factory_make ("capsfilter", "btmgr-si-rtpcapsfilter");
    rtpauddepay   = gst_element_factory_make ("rtpsbcdepay", "btmgr-si-rtpsbcdepay");
    audparse    = gst_element_factory_make ("sbcparse", "btmgr-si-rtpsbcparse");
// make fdsink a filesink, so you can  write pcm data to file or pipe, or alternatively, send it to the brcmpcmsink
    fdsink      = gst_element_factory_make ("brcmpcmsink", "btmgr-si-pcmsink");
    /* Create an event loop and feed gstreamer bus mesages to it */
    loop = g_main_loop_new (NULL, FALSE);

    /* Create a new pipeline to hold the elements */
    pipeline = gst_pipeline_new ("btmgr-si-pipeline");

        if (!appsrc || !auddec || !rtpcapsfilter || !rtpauddepay || !audparse || !fdsink || !loop || !pipeline) {
            g_print ("%s:%d:%s - Gstreamer plugin missing for streamIn\n", __FILE__, __LINE__, __FUNCTION__);
        return eBTRMgrSIGstFailure;
    }

    bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
    busWatchId = gst_bus_add_watch (bus, btrMgr_SI_gstBusCall, loop);
    g_object_unref (bus);

    /* setup */
    gst_bin_add_many (GST_BIN (pipeline), appsrc, rtpcapsfilter, rtpauddepay, audparse, auddec, fdsink, NULL);
    gst_element_link_many (appsrc, rtpcapsfilter, rtpauddepay, audparse, auddec, fdsink, NULL);

    mainLoopThread = g_thread_new("btrMgr_SI_g_main_loop_run_context", btrMgr_SI_g_main_loop_run_context, loop);

        
    gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_NULL);
    if (btrMgr_SI_validateStateWithTimeout(pipeline, GST_STATE_NULL, BTR_MGR_SLEEP_TIMEOUT_MS)!= GST_STATE_NULL) {
        g_print ("%s:%d:%s - Unable to perform Operation\n", __FILE__, __LINE__, __FUNCTION__);
        return eBTRMgrSIGstFailure;
    }


    pstBtrMgrSiGst->pPipeline       = (void*)pipeline;
    pstBtrMgrSiGst->pSrc            = (void*)appsrc;
    pstBtrMgrSiGst->pSink           = (void*)fdsink;
    pstBtrMgrSiGst->pAudioDec       = (void*)auddec;
    pstBtrMgrSiGst->pfdCapsFilter   = (void*)rtpcapsfilter;
    pstBtrMgrSiGst->pRtpAudioDePay    = (void*)rtpauddepay;
    pstBtrMgrSiGst->pAudioParse    = (void*)audparse;
    pstBtrMgrSiGst->pLoop           = (void*)loop;
    pstBtrMgrSiGst->pLoopThread     = (void*)mainLoopThread;
    pstBtrMgrSiGst->busWId          = busWatchId;
    pstBtrMgrSiGst->gstClkTStamp    = 0;
    pstBtrMgrSiGst->inBufOffset     = 0;

    *phBTRMgrSiGstHdl = (tBTRMgrSiGstHdl)pstBtrMgrSiGst;

    return eBTRMgrSIGstSuccess;
}


eBTRMgrSIGstRet
BTRMgr_SI_GstDeInit (
    tBTRMgrSiGstHdl hBTRMgrSiGstHdl
) {
    stBTRMgrSIGst* pstBtrMgrSiGst = (stBTRMgrSIGst*)hBTRMgrSiGstHdl;

    if (!pstBtrMgrSiGst) {
        g_print ("%s:%d:%s - Invalid input argument\n", __FILE__, __LINE__, __FUNCTION__);
        return eBTRMgrSIGstFailInArg;
    }

    GstElement* pipeline        = (GstElement*)pstBtrMgrSiGst->pPipeline;
    GstElement* appsrc          = (GstElement*)pstBtrMgrSiGst->pSrc;
    GMainLoop*  loop            = (GMainLoop*)pstBtrMgrSiGst->pLoop;
    GThread*    mainLoopThread  = (GThread*)pstBtrMgrSiGst->pLoopThread;
    guint       busWatchId      = pstBtrMgrSiGst->busWId;

    (void)pipeline;
    (void)appsrc;
    (void)loop;
    (void)busWatchId;

    /* cleanup */
    g_object_unref (GST_OBJECT(pipeline));
    g_source_remove (busWatchId);

    g_main_loop_quit (loop);
    g_thread_join (mainLoopThread);
    g_main_loop_unref (loop);

    memset((void*)pstBtrMgrSiGst, 0, sizeof(stBTRMgrSIGst));
    free((void*)pstBtrMgrSiGst);
    pstBtrMgrSiGst = NULL;

    return eBTRMgrSIGstSuccess;
}


eBTRMgrSIGstRet
BTRMgr_SI_GstStart (
    tBTRMgrSiGstHdl hBTRMgrSiGstHdl,
    int aiInBufMaxSize,
    int aiBTDevFd,
    int aiBTDevMTU
) {
    stBTRMgrSIGst* pstBtrMgrSiGst = (stBTRMgrSIGst*)hBTRMgrSiGstHdl;

    if (!pstBtrMgrSiGst) {
        g_print ("%s:%d:%s - Invalid input argument\n", __FILE__, __LINE__, __FUNCTION__);
        return eBTRMgrSIGstFailInArg;
    }

    GstElement* pipeline    = (GstElement*)pstBtrMgrSiGst->pPipeline;
    GstElement* appsrc      = (GstElement*)pstBtrMgrSiGst->pSrc;
    GstElement* auddec      = (GstElement*)pstBtrMgrSiGst->pAudioDec;
    GstElement* rtpcapsfilter = (GstElement*)pstBtrMgrSiGst->pfdCapsFilter;

    guint       busWatchId  = pstBtrMgrSiGst->busWId;

    GstCaps* appsrcSrcCaps  = NULL;

    (void)pipeline;
    (void)appsrc;
    (void)auddec;
    (void)busWatchId;

    /* Check if we are in correct state */
    if (btrMgr_SI_validateStateWithTimeout(pipeline, GST_STATE_NULL, BTR_MGR_SLEEP_TIMEOUT_MS) != GST_STATE_NULL) {
        g_print ("%s:%d:%s - Incorrect State to perform Operation\n", __FILE__, __LINE__, __FUNCTION__);
        return eBTRMgrSIGstFailure;
    }

    pstBtrMgrSiGst->gstClkTStamp = 0;
    pstBtrMgrSiGst->inBufOffset  = 0;

   appsrcSrcCaps = gst_caps_new_simple ("application/x-rtp",
                                         "media", G_TYPE_STRING, "audio",
                                         "encoding-name", G_TYPE_STRING, "SBC",
                                         "clock-rate", G_TYPE_INT, 48000,
                                         "payload", G_TYPE_INT, 96,
                                          NULL);


    g_object_set (rtpcapsfilter, "caps", appsrcSrcCaps, NULL);
    g_object_set (appsrc, "blocksize", aiBTDevMTU, NULL);
    g_object_set (appsrc, "fd", aiBTDevFd, NULL);

    gst_caps_unref(appsrcSrcCaps);

    /* start play back and listed to events */
    gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_PLAYING);
    if (btrMgr_SI_validateStateWithTimeout(pipeline, GST_STATE_PLAYING, BTR_MGR_SLEEP_TIMEOUT_MS) != GST_STATE_PLAYING) { 
        g_print ("%s:%d:%s - Unable to perform Operation\n", __FILE__, __LINE__, __FUNCTION__);
        return eBTRMgrSIGstFailure;
    }

    return eBTRMgrSIGstSuccess;
}


eBTRMgrSIGstRet
BTRMgr_SI_GstStop (
    tBTRMgrSiGstHdl hBTRMgrSiGstHdl
) {
    stBTRMgrSIGst* pstBtrMgrSiGst = (stBTRMgrSIGst*)hBTRMgrSiGstHdl;

    if (!pstBtrMgrSiGst) {
        g_print ("%s:%d:%s - Invalid input argument\n", __FILE__, __LINE__, __FUNCTION__);
        return eBTRMgrSIGstFailInArg;
    }

    GstElement* pipeline    = (GstElement*)pstBtrMgrSiGst->pPipeline;
    GstElement* appsrc      = (GstElement*)pstBtrMgrSiGst->pSrc;
    GMainLoop*  loop        = (GMainLoop*)pstBtrMgrSiGst->pLoop;
    guint       busWatchId  = pstBtrMgrSiGst->busWId;

    (void)pipeline;
    (void)appsrc;
    (void)loop;
    (void)busWatchId;

    /* stop play back */
    gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_NULL);
    if (btrMgr_SI_validateStateWithTimeout(pipeline, GST_STATE_NULL, BTR_MGR_SLEEP_TIMEOUT_MS) != GST_STATE_NULL) {
        g_print ("%s:%d:%s - - Unable to perform Operation\n", __FILE__, __LINE__, __FUNCTION__);
        return eBTRMgrSIGstFailure;
    }

    pstBtrMgrSiGst->gstClkTStamp = 0;
    pstBtrMgrSiGst->inBufOffset  = 0;

    return eBTRMgrSIGstSuccess;
}


eBTRMgrSIGstRet
BTRMgr_SI_GstPause (
    tBTRMgrSiGstHdl hBTRMgrSiGstHdl
) {
    stBTRMgrSIGst* pstBtrMgrSiGst = (stBTRMgrSIGst*)hBTRMgrSiGstHdl;

    if (!pstBtrMgrSiGst) {
        g_print ("%s:%d:%s - Invalid input argument\n", __FILE__, __LINE__, __FUNCTION__);
        return eBTRMgrSIGstFailInArg;
    }

    GstElement* pipeline    = (GstElement*)pstBtrMgrSiGst->pPipeline;
    GstElement* appsrc      = (GstElement*)pstBtrMgrSiGst->pSrc;
    GMainLoop*  loop        = (GMainLoop*)pstBtrMgrSiGst->pLoop;
    guint       busWatchId  = pstBtrMgrSiGst->busWId;

    (void)pipeline;
    (void)appsrc;
    (void)loop;
    (void)busWatchId;

    /* Check if we are in correct state */
    if (btrMgr_SI_validateStateWithTimeout(pipeline, GST_STATE_PLAYING, BTR_MGR_SLEEP_TIMEOUT_MS) != GST_STATE_PLAYING) {
        g_print ("%s:%d:%s - Incorrect State to perform Operation\n", __FILE__, __LINE__, __FUNCTION__);
        return eBTRMgrSIGstFailure;
    }

    /* pause playback and listed to events */
    gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_PAUSED);
    if (btrMgr_SI_validateStateWithTimeout(pipeline, GST_STATE_PAUSED, BTR_MGR_SLEEP_TIMEOUT_MS) != GST_STATE_PAUSED) {
        g_print ("%s:%d:%s - Unable to perform Operation\n", __FILE__, __LINE__, __FUNCTION__);
        return eBTRMgrSIGstFailure;
    } 

    return eBTRMgrSIGstSuccess;
}


eBTRMgrSIGstRet
BTRMgr_SI_GstResume (
    tBTRMgrSiGstHdl hBTRMgrSiGstHdl
) {
    stBTRMgrSIGst* pstBtrMgrSiGst = (stBTRMgrSIGst*)hBTRMgrSiGstHdl;

    if (!pstBtrMgrSiGst) {
        g_print ("%s:%d:%s - Invalid input argument\n", __FILE__, __LINE__, __FUNCTION__);
        return eBTRMgrSIGstFailInArg;
    }

    GstElement* pipeline    = (GstElement*)pstBtrMgrSiGst->pPipeline;
    GstElement* appsrc      = (GstElement*)pstBtrMgrSiGst->pSrc;
    GMainLoop*  loop        = (GMainLoop*)pstBtrMgrSiGst->pLoop;
    guint       busWatchId  = pstBtrMgrSiGst->busWId;

    (void)pipeline;
    (void)appsrc;
    (void)loop;
    (void)busWatchId;

    /* Check if we are in correct state */
    if (btrMgr_SI_validateStateWithTimeout(pipeline, GST_STATE_PAUSED, BTR_MGR_SLEEP_TIMEOUT_MS) != GST_STATE_PAUSED) {
        g_print ("%s:%d:%s - Incorrect State to perform Operation\n", __FILE__, __LINE__, __FUNCTION__);
        return eBTRMgrSIGstFailure;
    }

    /* Resume playback and listed to events */
    gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_PLAYING);
    if (btrMgr_SI_validateStateWithTimeout(pipeline, GST_STATE_PLAYING, BTR_MGR_SLEEP_TIMEOUT_MS) != GST_STATE_PLAYING) {
        g_print ("%s:%d:%s - Unable to perform Operation\n", __FILE__, __LINE__, __FUNCTION__);
        return eBTRMgrSIGstFailure;
    }

    return eBTRMgrSIGstSuccess;
}


eBTRMgrSIGstRet
BTRMgr_SI_GstSendBuffer (
    tBTRMgrSiGstHdl hBTRMgrSiGstHdl,
    char*   pcInBuf,
    int     aiInBufSize
) {
    stBTRMgrSIGst* pstBtrMgrSiGst = (stBTRMgrSIGst*)hBTRMgrSiGstHdl;

    if (!pstBtrMgrSiGst) {
        g_print ("%s:%d:%s - Invalid input argument\n", __FILE__, __LINE__, __FUNCTION__);
        return eBTRMgrSIGstFailInArg;
    }

    GstElement* pipeline    = (GstElement*)pstBtrMgrSiGst->pPipeline;
    GstElement* appsrc      = (GstElement*)pstBtrMgrSiGst->pSrc;
    GMainLoop*  loop        = (GMainLoop*)pstBtrMgrSiGst->pLoop;
    guint       busWatchId  = pstBtrMgrSiGst->busWId;

    (void)pipeline;
    (void)appsrc;
    (void)loop;
    (void)busWatchId;

    /* push Buffers */
    {
        GstBuffer *gstBuf;
        GstMapInfo gstBufMap;

        gstBuf = gst_buffer_new_and_alloc (aiInBufSize);
        gst_buffer_map (gstBuf, &gstBufMap, GST_MAP_WRITE);

        //TODO: Repalce memcpy and new_alloc if possible
        memcpy (gstBufMap.data, pcInBuf, aiInBufSize);

        //TODO: Arrive at this vale based on Sampling rate, bits per sample, num of Channels and the 
		// size of the incoming buffer (which represents the num of samples received at this instant)
        GST_BUFFER_PTS (gstBuf)         = pstBtrMgrSiGst->gstClkTStamp;
        GST_BUFFER_DURATION (gstBuf)    = GST_USECOND * (aiInBufSize * 1000)/(48 * (16/8) * 2);
        pstBtrMgrSiGst->gstClkTStamp   += GST_BUFFER_DURATION (gstBuf);

        GST_BUFFER_OFFSET (gstBuf)      = pstBtrMgrSiGst->inBufOffset;
        pstBtrMgrSiGst->inBufOffset    += aiInBufSize;
        GST_BUFFER_OFFSET_END (gstBuf)  = pstBtrMgrSiGst->inBufOffset - 1;

        gst_app_src_push_buffer (GST_APP_SRC (appsrc), gstBuf);

        gst_buffer_unmap (gstBuf, &gstBufMap);
    }

    return eBTRMgrSIGstSuccess;
}


eBTRMgrSIGstRet
BTRMgr_SI_GstSendEOS (
    tBTRMgrSiGstHdl hBTRMgrSiGstHdl
) {
    stBTRMgrSIGst* pstBtrMgrSiGst = (stBTRMgrSIGst*)hBTRMgrSiGstHdl;

    if (!pstBtrMgrSiGst) {
        g_print ("%s:%d:%s - Invalid input argument\n", __FILE__, __LINE__, __FUNCTION__);
        return eBTRMgrSIGstFailInArg;
    }

    GstElement* pipeline    = (GstElement*)pstBtrMgrSiGst->pPipeline;
    GstElement* appsrc      = (GstElement*)pstBtrMgrSiGst->pSrc;
    GMainLoop*  loop        = (GMainLoop*)pstBtrMgrSiGst->pLoop;
    guint       busWatchId  = pstBtrMgrSiGst->busWId;

    (void)pipeline;
    (void)appsrc;
    (void)loop;
    (void)busWatchId;

    /* push EOS */
    gst_app_src_end_of_stream (GST_APP_SRC (appsrc));

    return eBTRMgrSIGstSuccess;
}
