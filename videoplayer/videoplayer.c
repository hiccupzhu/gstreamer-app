#include <gst/gst.h>
#include <glib.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <string.h>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <unistd.h>

#include "link.h"
#include "gdef.h"

#define MAX_PIPELINES    8

typedef struct _MPIPES{
    struct{
        GstElement *pipeline;
        guint bus_watch_id;
        char cmd[1024];
    }pipes[MAX_PIPELINES];

    int nb_pipelines;
}MPIPES;




static MPIPES mpipe;
static Link glink;

static void          appsrc_need_data_callback (GstAppSrc *src, guint length, gpointer user_data);
static GstFlowReturn appsink_new_buffer_callback(GstAppSink *sink, gpointer user_data);

static gboolean
bus_call (GstBus     *bus,
        GstMessage *msg,
        gpointer    data)
{
    GMainLoop *loop = (GMainLoop *) data;

    switch (GST_MESSAGE_TYPE (msg)) {

    case GST_MESSAGE_EOS:
        g_print ("End of stream\n");
        g_main_loop_quit (loop);
        break;

    case GST_MESSAGE_ERROR: {
        gchar  *debug;
        GError *error;

        gst_message_parse_error (msg, &error, &debug);
        g_free (debug);

        g_printerr ("Error: %s\n", error->message);
        g_error_free (error);

        g_main_loop_quit (loop);
        break;
    }
    default:
        break;
    }

    return TRUE;
}


static void
on_pad_added (GstElement *element,
        GstPad     *pad,
        gpointer    data)
{
    GstPad *sinkpad;
    GstElement *decoder = (GstElement *) data;

    /* We can now glink this pad with the vorbis-decoder sink pad */
    g_print ("Dynamic pad created, linking demuxer/decoder\n");

    sinkpad = gst_element_get_static_pad (decoder, "sink");

    gst_pad_link (pad, sinkpad);

    gst_object_unref (sinkpad);
}

static char cmd_pipe[MAX_PIPELINES][1024];

int
config_from_xml(const char* filename)
{
    xmlDocPtr  xmldoc;
    xmlNodePtr xmlnode;

    xmldoc =  xmlReadFile(filename,"UTF-8",XML_PARSE_NOBLANKS);
    if (xmldoc == NULL ) {
        fprintf(stderr, "Document not parsed successfully. \n");
        return -1;
    }

    xmlnode = xmlDocGetRootElement(xmldoc);

//    while(xmlnode){
//
//    }

    printf("%s\n", xmlnode->name);

    xmlnode = xmlnode->children;

    while(xmlnode){
        char* content = xmlNodeGetContent(xmlnode);

        printf("[%s]:%s\n", xmlnode->name, content);

        if(!strcmp(xmlnode->name, "pipe")){
            strcpy(mpipe.pipes[mpipe.nb_pipelines].cmd, content);
            mpipe.nb_pipelines ++;
        }else{
            g_print("parse xml failed!!!\n");
        }

        xmlnode= xmlnode->next;
    }

    xmlFreeDoc(xmldoc);

    return 0;
}


int
main (int  argc, char *argv[])
{
    GMainLoop *loop;
    GError *err = NULL;
    GstBus *bus;

    if(config_from_xml("./conf.xml") < 0){
        return -1;
    }

    for(int i = 0; i < MAX_PIPELINES; i++){
        mpipe.pipes[i].pipeline = NULL;
        mpipe.pipes[i].bus_watch_id = 0;
    }


    gst_init (&argc, &argv);
    loop = g_main_loop_new (NULL, FALSE);


    for(int i = 0; i < mpipe.nb_pipelines; i++){
        char name[1024];

        g_print("## pipe:\"%s\" Creating!!\n", mpipe.pipes[i].cmd);
        err = NULL;
        mpipe.pipes[i].pipeline = gst_parse_launch(mpipe.pipes[i].cmd, &err);
        if(!mpipe.pipes[i].pipeline){
            g_print("pipelines[%d] create failed!!!\n", i);
            return -1;
        }

        sprintf(name, "pipeline-%d", i);
        gst_element_set_name(mpipe.pipes[i].pipeline, name);

        bus = gst_pipeline_get_bus (GST_PIPELINE (mpipe.pipes[i].pipeline));
        mpipe.pipes[i].bus_watch_id = gst_bus_add_watch (bus, bus_call, loop);
        gst_object_unref (bus);
    }

    //add code to process the appsink and appsrc
    GstAppSrcCallbacks appsrc_callbacks = {
            appsrc_need_data_callback,
            NULL,
            NULL
    };
    GstAppSinkCallbacks appsink_callbacks = {
            NULL,
            NULL,
            appsink_new_buffer_callback,
            NULL
    };
    GstElement *p, *appsrc, *appsink;

    appsink = gst_bin_get_by_name(mpipe.pipes[0].pipeline, "appsink0");
    if(appsink == NULL){
        printf("get appsink failed!\n");
        return -1;
    }
    appsrc  = gst_bin_get_by_name(mpipe.pipes[1].pipeline, "appsrc0");
    if(appsrc == NULL){
        printf("get appsrc failed!\n");
        return -1;
    }

    gst_app_sink_set_callbacks (GST_APP_SINK(appsink), &appsink_callbacks, &glink, NULL);
    gst_object_unref (appsink);

    gst_app_src_set_callbacks (GST_APP_SRC (appsrc), &appsrc_callbacks, &glink, NULL);
    gst_object_unref (appsrc);


    link_init(&glink, appsink, appsrc);



    for(int i = 0; i < mpipe.nb_pipelines; i ++){
        int ret = gst_element_set_state (mpipe.pipes[i].pipeline, GST_STATE_PLAYING);
        av_print("set-pipe[%d] playing ret=%d\n", i, ret);
        if (ret <= 0){
            av_print("set_state[%d] failed!\n", i);
            exit(1);
        }
    }





    g_print ("Running...\n");
    g_main_loop_run (loop);


    /* Out of the main loop, clean up nicely */
    g_print ("Returned, stopping playback\n");
    for(int i = 0; i < mpipe.nb_pipelines; i ++){
        gst_element_set_state (mpipe.pipes[i].pipeline, GST_STATE_NULL);
        g_print ("Deleting pipelines[%d]\n", i);
        gst_object_unref (GST_OBJECT (mpipe.pipes[i].pipeline));
        g_source_remove (mpipe.pipes[i].bus_watch_id);
    }

    g_main_loop_unref (loop);

    return 0;
}

static void
appsrc_need_data_callback (GstAppSrc *src, guint length, gpointer user_data)
{
    Link* link = (Link*)user_data;
    GstBuffer *buffer;

    while(g_queue_get_length(link->queue) <= 0){
        av_print("link queue drain, waiting the data coming.\n");
        usleep(GST_MSECOND);
    }
    av_print("---queue-length=%d\n", g_queue_get_length(link->queue));

    g_mutex_lock(link->mutex);
    buffer = g_queue_pop_head(link->queue);
    g_mutex_unlock(link->mutex);

    if (gst_app_src_push_buffer (src, buffer) != GST_FLOW_OK) {
        GST_ERROR ("gst_app_src_push_buffer audio failure.");
        return;
    }

}

static GstFlowReturn
appsink_new_buffer_callback(GstAppSink *sink, gpointer user_data)
{
    Link* link = (Link*)user_data;

    GstBuffer *buffer;

    av_print("+++queue-length=%d\n", g_queue_get_length(link->queue));
    buffer = gst_app_sink_pull_buffer (GST_APP_SINK (sink));

    if(link->caps == NULL && GST_BUFFER_CAPS (buffer)){
        link->caps = gst_buffer_get_caps(buffer);

        av_print("SetAppsrc caps:%s\n", gst_caps_to_string(link->caps));
        gst_app_src_set_caps (link->appsrc, link->caps);
    }

    g_mutex_lock(link->mutex);
    g_queue_push_tail(link->queue, buffer);
    g_mutex_unlock(link->mutex);

}
