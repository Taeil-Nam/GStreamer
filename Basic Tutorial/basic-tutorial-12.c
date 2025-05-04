#include <gst/gst.h>         // GStreamer의 핵심 API를 사용하기 위한 헤더
#include <string.h>          // memset 같은 문자열 관련 함수 사용

// 사용자 정의 데이터 구조체: 메시지 콜백에서 사용할 데이터를 담음
typedef struct _CustomData {
  gboolean is_live;         // live stream 여부 판단용 (예: 네트워크 스트림)
  GstElement *pipeline;     // GStreamer 파이프라인 객체
  GMainLoop *loop;          // 메인 이벤트 루프 포인터
} CustomData;

// GStreamer 버스 메시지를 처리하는 콜백 함수
static void cb_message (GstBus *bus, GstMessage *msg, CustomData *data) {

  // 수신한 메시지의 타입에 따라 처리
  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_ERROR: {                    // 에러 메시지 처리
      GError *err;
      gchar *debug;

      // 에러 메시지 디코드
      gst_message_parse_error (msg, &err, &debug);
      g_print ("Error: %s\n", err->message);     // 에러 메시지 출력
      g_error_free (err);                        // 에러 객체 해제
      g_free (debug);                            // 디버그 문자열 해제

      // 파이프라인 정지 후 루프 종료
      gst_element_set_state (data->pipeline, GST_STATE_READY);
      g_main_loop_quit (data->loop);
      break;
    }
    case GST_MESSAGE_EOS:                        // 스트림 종료(End Of Stream) 메시지
      gst_element_set_state (data->pipeline, GST_STATE_READY);  // 파이프라인 정지
      g_main_loop_quit (data->loop);             // 메인 루프 종료
      break;
    case GST_MESSAGE_BUFFERING: {                // 버퍼링 관련 메시지
      gint percent = 0;

      if (data->is_live) break;                  // 라이브 스트림이면 버퍼링 무시

      gst_message_parse_buffering (msg, &percent);  // 버퍼링 퍼센트 읽기
      g_print ("Buffering (%3d%%)\r", percent);     // 퍼센트 출력

      if (percent < 100)                          // 버퍼링이 100% 안 됐으면
        gst_element_set_state (data->pipeline, GST_STATE_PAUSED);  // 일시정지
      else
        gst_element_set_state (data->pipeline, GST_STATE_PLAYING); // 재생 재개
      break;
    }
    case GST_MESSAGE_CLOCK_LOST:                 // 클럭 손실 시 메시지
      gst_element_set_state (data->pipeline, GST_STATE_PAUSED);   // 일시정지
      gst_element_set_state (data->pipeline, GST_STATE_PLAYING);  // 다시 재생
      break;
    default:
      // 처리하지 않는 메시지는 무시
      break;
  }
}

int main(int argc, char *argv[]) {
  GstElement *pipeline;               // 파이프라인 객체
  GstBus *bus;                        // 메시지를 전달받는 버스
  GstStateChangeReturn ret;          // 상태 변경 결과
  GMainLoop *main_loop;              // GLib 메인 루프
  CustomData data;                   // 사용자 정의 데이터

  gst_init (&argc, &argv);           // GStreamer 초기화

  memset (&data, 0, sizeof (data));  // 사용자 데이터 구조 초기화

  // playbin 요소를 사용해 파이프라인 생성 (스트리밍 영상)
  pipeline = gst_parse_launch (
    "playbin uri=https://gstreamer.freedesktop.org/data/media/sintel_trailer-480p.webm",
    NULL
  );

  bus = gst_element_get_bus (pipeline); // 버스를 가져옴

  // 파이프라인 재생 시작
  ret = gst_element_set_state (pipeline, GST_STATE_PLAYING);

  if (ret == GST_STATE_CHANGE_FAILURE) {   // 상태 변경 실패 시
    g_printerr ("Unable to set the pipeline to the playing state.\n");
    gst_object_unref (pipeline);           // 파이프라인 해제
    return -1;
  } else if (ret == GST_STATE_CHANGE_NO_PREROLL) {
    data.is_live = TRUE;                   // 라이브 스트림으로 인식
  }

  main_loop = g_main_loop_new (NULL, FALSE);  // 메인 루프 생성
  data.loop = main_loop;                      // 사용자 구조체에 저장
  data.pipeline = pipeline;

  gst_bus_add_signal_watch (bus);             // 버스를 감시하도록 설정
  g_signal_connect (bus, "message", G_CALLBACK (cb_message), &data);  // 메시지 콜백 등록

  g_main_loop_run (main_loop);                // 메인 루프 실행 (이벤트 처리 시작)

  // 루프 종료 후 자원 정리
  g_main_loop_unref (main_loop);              // 메인 루프 해제
  gst_object_unref (bus);                     // 버스 해제
  gst_element_set_state (pipeline, GST_STATE_NULL);  // 파이프라인 완전 정지
  gst_object_unref (pipeline);                // 파이프라인 해제
  return 0;
}
