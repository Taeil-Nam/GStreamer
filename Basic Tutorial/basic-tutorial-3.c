#include <gst/gst.h>

/* Structure to contain all our information, so we can pass it to callbacks */
typedef struct _CustomData {
  GstElement *pipeline;
  GstElement *source;
  GstElement *a_convert;
  GstElement *a_resample;
  GstElement *a_sink;
  GstElement *v_convert;
  GstElement *v_sink;
} CustomData;

/* Handler for the pad-added signal */
static void pad_added_handler (GstElement *src, GstPad *pad, CustomData *data);

int main(int argc, char *argv[]) {
  CustomData data;
  GstBus *bus;
  GstMessage *msg;
  GstStateChangeReturn ret;
  gboolean terminate = FALSE;

  /* Initialize GStreamer */
  gst_init (&argc, &argv);

  /* Create the elements */
  data.source = gst_element_factory_make ("uridecodebin", "source");
  data.a_convert = gst_element_factory_make ("audioconvert", "a_convert");
  data.a_resample = gst_element_factory_make ("audioresample", "a_resample");
  data.a_sink = gst_element_factory_make ("autoaudiosink", "a_sink");
  data.v_convert = gst_element_factory_make ("videoconvert", "v_convert");
  data.v_sink = gst_element_factory_make ("autovideosink", "v_sink");

  /* Create the empty pipeline */
  data.pipeline = gst_pipeline_new ("test-pipeline");

  if (!data.pipeline || !data.source || !data.a_convert || !data.a_resample || !data.a_sink
      || !data.v_convert || !data.v_sink) {
    g_printerr ("Not all elements could be created.\n");
    return -1;
  }

  /* Build the pipeline. Note that we are NOT linking the source at this
   * point. We will do it later. */
  gst_bin_add_many (GST_BIN (data.pipeline), data.source, data.a_convert, data.a_resample, data.a_sink,
                    data.v_convert, data.v_sink, NULL);
  if (!gst_element_link_many (data.a_convert, data.a_resample, data.a_sink, NULL)
      || !gst_element_link_many (data.v_convert, data.v_sink, NULL)) {
    g_printerr ("Elements could not be linked.\n");
    gst_object_unref (data.pipeline);
    return -1;
  }

  /* Set the URI to play */
  g_object_set (data.source, "uri", "https://gstreamer.freedesktop.org/data/media/sintel_trailer-480p.webm", NULL);

  /* Connect to the pad-added signal */
  g_signal_connect (data.source, "pad-added", G_CALLBACK (pad_added_handler), &data);

  /* Start playing */
  ret = gst_element_set_state (data.pipeline, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    g_printerr ("Unable to set the pipeline to the playing state.\n");
    gst_object_unref (data.pipeline);
    return -1;
  }

  /* Listen to the bus */
  bus = gst_element_get_bus (data.pipeline);
  do {
    msg = gst_bus_timed_pop_filtered (bus, GST_CLOCK_TIME_NONE,
        GST_MESSAGE_STATE_CHANGED | GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

    /* Parse message */
    if (msg != NULL) {
      GError *err;
      gchar *debug_info;

      switch (GST_MESSAGE_TYPE (msg)) {
        case GST_MESSAGE_ERROR:
          gst_message_parse_error (msg, &err, &debug_info);
          g_printerr ("Error received from element %s: %s\n", GST_OBJECT_NAME (msg->src), err->message);
          g_printerr ("Debugging information: %s\n", debug_info ? debug_info : "none");
          g_clear_error (&err);
          g_free (debug_info);
          terminate = TRUE;
          break;
        case GST_MESSAGE_EOS:
          g_print ("End-Of-Stream reached.\n");
          terminate = TRUE;
          break;
        case GST_MESSAGE_STATE_CHANGED:
          /* We are only interested in state-changed messages from the pipeline */
          if (GST_MESSAGE_SRC (msg) == GST_OBJECT (data.pipeline)) {
            GstState old_state, new_state, pending_state;
            gst_message_parse_state_changed (msg, &old_state, &new_state, &pending_state);
            g_print ("Pipeline state changed from %s to %s:\n",
                gst_element_state_get_name (old_state), gst_element_state_get_name (new_state));
          }
          break;
        default:
          /* We should not reach here */
          g_printerr ("Unexpected message received.\n");
          break;
      }
      gst_message_unref (msg);
    }
  } while (!terminate);

  /* Free resources */
  gst_object_unref (bus);
  gst_element_set_state (data.pipeline, GST_STATE_NULL);
  gst_object_unref (data.pipeline);
  return 0;
}

/* This function will be called by the pad-added signal */
static void pad_added_handler (GstElement *src, GstPad *new_pad, CustomData *data) {
  GstPad *a_sink_pad = gst_element_get_static_pad (data->a_convert, "a_sink");
  GstPad *v_sink_pad = gst_element_get_static_pad (data->v_convert, "v_sink");
  GstPadLinkReturn ret;
  GstCaps *new_pad_caps = NULL;
  GstStructure *new_pad_struct = NULL;
  const gchar *new_pad_type = NULL;

  g_print ("Received new pad '%s' from '%s':\n", GST_PAD_NAME (new_pad), GST_ELEMENT_NAME (src));

  /* Check the new pad's type */
  new_pad_caps = gst_pad_get_current_caps (new_pad);
  new_pad_struct = gst_caps_get_structure (new_pad_caps, 0);
  new_pad_type = gst_structure_get_name (new_pad_struct);

  if (g_str_has_prefix(new_pad_type, "audio/x-raw")) {
    GstPad *sink_pad = gst_element_get_static_pad(data->a_convert, "sink");
    if (gst_pad_is_linked(sink_pad)) {
      g_print("Audio pad already linked. Ignoring.\n");
    } else if (gst_pad_link(new_pad, sink_pad) == GST_PAD_LINK_OK) {
      g_print("Audio pad linked successfully.\n");
    } else {
      g_print("Audio pad link failed.\n");
    }
    gst_object_unref(sink_pad);
  } else if (g_str_has_prefix(new_pad_type, "video/x-raw")) {
    GstPad *sink_pad = gst_element_get_static_pad(data->v_convert, "sink");
    if (gst_pad_is_linked(sink_pad)) {
      g_print("Video pad already linked. Ignoring.\n");
    } else if (gst_pad_link(new_pad, sink_pad) == GST_PAD_LINK_OK) {
      g_print("Video pad linked successfully.\n");
    } else {
      g_print("Video pad link failed.\n");
    }
    gst_object_unref(sink_pad);
  } else {
    g_print("Unknown pad type '%s'. Ignoring.\n", new_pad_type);
  }

  if (new_pad_caps != NULL)
    gst_caps_unref(new_pad_caps);
}
