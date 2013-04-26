#include <gst/gst.h>
#include <glib.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <string.h>


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

    /* We can now link this pad with the vorbis-decoder sink pad */
    g_print ("Dynamic pad created, linking demuxer/decoder\n");

    sinkpad = gst_element_get_static_pad (decoder, "sink");

    gst_pad_link (pad, sinkpad);

    gst_object_unref (sinkpad);
}

static char cmd_source[1024];
static char cmd_others[1024];

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

        if(!strcmp(xmlnode->name, "source")){
            strcpy(cmd_source, content);
        }else if(!strcmp(xmlnode->name, "others")){
            strcpy(cmd_others, content);
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
    GstElement *pipeline, *srcbin, *sinkbin;
    GstBus *bus;
    guint bus_watch_id;

    if(config_from_xml("./sample.xml") < 0){
        return -1;
    }


    gst_init (&argc, &argv);
    loop = g_main_loop_new (NULL, FALSE);

    pipeline = gst_pipeline_new ("audio-player");

    srcbin  = gst_parse_bin_from_description(cmd_source, TRUE, &err);
    sinkbin = gst_parse_bin_from_description(cmd_others, TRUE, &err);




    if (!pipeline || !srcbin || !sinkbin) {
        g_printerr ("One element could not be created. Exiting.\n");
        return -1;
    }

    bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
    bus_watch_id = gst_bus_add_watch (bus, bus_call, loop);
    gst_object_unref (bus);

    gst_bin_add_many (GST_BIN (pipeline), srcbin, sinkbin, NULL);

    gst_element_link (srcbin, sinkbin);

    gst_element_set_state (pipeline, GST_STATE_PLAYING);

    /* Iterate */
    g_print ("Running...\n");
    g_main_loop_run (loop);


    /* Out of the main loop, clean up nicely */
    g_print ("Returned, stopping playback\n");
    gst_element_set_state (pipeline, GST_STATE_NULL);

    g_print ("Deleting pipeline\n");
    gst_object_unref (GST_OBJECT (pipeline));
    g_source_remove (bus_watch_id);
    g_main_loop_unref (loop);

    return 0;
}
