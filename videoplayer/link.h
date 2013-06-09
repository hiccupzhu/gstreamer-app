/*
 * link.h
 *
 *  Created on: Jun 9, 2013
 *      Author: szhu
 */

#ifndef LINK_H_
#define LINK_H_

#include <glib.h>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>

typedef struct _Link{
    GMutex *mutex;
    GQueue *queue;
    GstCaps*caps;

    GstAppSrc  *appsrc;
    GstAppSink *appsink;

    guint64 last_pts;
}Link;

void    link_init(Link *link, GstAppSink *sink, GstAppSrc *src);
void    link_release(Link *link);

#endif /* LINK_H_ */
