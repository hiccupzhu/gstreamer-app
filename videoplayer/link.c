/*
 * link.c
 *
 *  Created on: Jun 9, 2013
 *      Author: szhu
 */

#include "link.h"
#include "gdef.h"

void
link_init(Link *link, GstAppSink *sink, GstAppSrc * src)
{
    if(link == NULL) return;
    link->last_pts = 0;
    link->mutex = g_mutex_new();
    link->queue = g_queue_new();

    link->appsink = sink;
    link->appsrc = src;
    link->caps = NULL;
}

void
link_release(Link *link)
{
    link->last_pts = 0;
    link->appsink = NULL;
    link->appsrc = NULL;
    if(link->caps){
        gst_caps_unref(link->caps);
        link->caps = NULL;
    }

    SAFE_GMUTEX_FREE(link->mutex);
    SAFE_GQUEUE_FREE(link->queue);
}
