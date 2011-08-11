/*
Copyright (c) 2011, Code Aurora Forum. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of Code Aurora Forum, Inc. nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <pthread.h>
#include "mm_camera_dbg.h"
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include <camera.h>
#include "mm_camera_interface2.h"
#include "mm_camera.h"

static void mm_camera_read_raw_frame(mm_camera_obj_t * my_obj)
{
    int rc = 0;
    int idx;
    mm_camera_stream_t *stream;

    stream = &my_obj->ch[MM_CAMERA_CH_RAW].raw.stream;
    idx =  mm_camera_read_msm_frame(my_obj, stream);
    if (idx < 0) {
        return;
    }
    pthread_mutex_lock(&my_obj->ch[MM_CAMERA_CH_RAW].mutex);
    if(my_obj->ch[MM_CAMERA_CH_RAW].buf_cb.cb) {
        mm_camera_ch_data_buf_t data;
        data.type = MM_CAMERA_CH_RAW;
        data.def.idx = idx;
        data.def.frame = &my_obj->ch[MM_CAMERA_CH_RAW].raw.stream.frame.frame[idx].frame;
        my_obj->ch[MM_CAMERA_CH_RAW].raw.stream.frame.ref_count[idx]++;
        CDBG("%s:calling data notify cb 0x%x, 0x%x\n", __func__,
                 (uint32_t)my_obj->ch[MM_CAMERA_CH_RAW].buf_cb.cb,
                 (uint32_t)my_obj->ch[MM_CAMERA_CH_RAW].buf_cb.user_data);
        my_obj->ch[MM_CAMERA_CH_RAW].buf_cb.cb(&data,
                                my_obj->ch[MM_CAMERA_CH_RAW].buf_cb.user_data);
    }
    pthread_mutex_unlock(&my_obj->ch[MM_CAMERA_CH_RAW].mutex);
}

static void mm_camera_read_preview_frame(mm_camera_obj_t * my_obj)
{
    int rc = 0;
    int idx;
    mm_camera_stream_t *stream;

    stream = &my_obj->ch[MM_CAMERA_CH_PREVIEW].preview.stream;
    idx =  mm_camera_read_msm_frame(my_obj, stream);
    if (idx < 0) {
        return;
    }
    pthread_mutex_lock(&my_obj->ch[MM_CAMERA_CH_PREVIEW].mutex);
    if(my_obj->ch[MM_CAMERA_CH_PREVIEW].buf_cb.cb) {
        mm_camera_ch_data_buf_t data;
        data.type = MM_CAMERA_CH_PREVIEW;
        data.def.idx = idx;
        data.def.frame = &my_obj->ch[MM_CAMERA_CH_PREVIEW].preview.stream.frame.frame[idx].frame;
        my_obj->ch[MM_CAMERA_CH_PREVIEW].preview.stream.frame.ref_count[idx]++;
        CDBG("%s:calling data notify cb 0x%x, 0x%x\n", __func__,
                 (uint32_t)my_obj->ch[MM_CAMERA_CH_PREVIEW].buf_cb.cb,
                 (uint32_t)my_obj->ch[MM_CAMERA_CH_PREVIEW].buf_cb.user_data);
        my_obj->ch[MM_CAMERA_CH_PREVIEW].buf_cb.cb(&data,
                                my_obj->ch[MM_CAMERA_CH_PREVIEW].buf_cb.user_data);
    }
    pthread_mutex_unlock(&my_obj->ch[MM_CAMERA_CH_PREVIEW].mutex);
}
static void mm_camera_snapshot_send_snapshot_notify(mm_camera_obj_t * my_obj)
{
    int delivered = 0;
    mm_camera_frame_queue_t *s_q, *t_q;
    mm_camera_ch_data_buf_t data;
    mm_camera_frame_t *frame;
    s_q =   &my_obj->ch[MM_CAMERA_CH_SNAPSHOT].snapshot.main.frame.readyq;
    t_q =   &my_obj->ch[MM_CAMERA_CH_SNAPSHOT].snapshot.thumbnail.frame.readyq;
    pthread_mutex_lock(&my_obj->ch[MM_CAMERA_CH_SNAPSHOT].mutex);
    if(s_q->cnt && t_q->cnt && my_obj->ch[MM_CAMERA_CH_SNAPSHOT].buf_cb.cb) {
        data.type = MM_CAMERA_CH_SNAPSHOT;
        frame = mm_camera_stream_frame_deq(s_q);
        data.snapshot.main.frame = &frame->frame;
        data.snapshot.main.idx = frame->idx;
        frame = mm_camera_stream_frame_deq(t_q);
        data.snapshot.thumbnail.frame = &frame->frame;
        data.snapshot.thumbnail.idx = frame->idx;
        my_obj->ch[MM_CAMERA_CH_SNAPSHOT].snapshot.main.frame.ref_count[data.snapshot.main.idx]++;
        my_obj->ch[MM_CAMERA_CH_SNAPSHOT].snapshot.thumbnail.frame.ref_count[data.snapshot.thumbnail.idx]++;
        my_obj->ch[MM_CAMERA_CH_SNAPSHOT].buf_cb.cb(&data,
                                my_obj->ch[MM_CAMERA_CH_SNAPSHOT].buf_cb.user_data);
        delivered = 1;
    }
    pthread_mutex_unlock(&my_obj->ch[MM_CAMERA_CH_SNAPSHOT].mutex);
    if(delivered) {
      mm_camera_event_t data;
      data.event_type = MM_CAMERA_EVT_TYPE_CH;
      data.e.ch.evt = MM_CAMERA_CH_EVT_DATA_DELIVERY_DONE;
      data.e.ch.ch = MM_CAMERA_CH_SNAPSHOT;
      mm_camera_poll_send_ch_event(my_obj, &data);
    }
}

static void mm_camera_read_snapshot_main_frame(mm_camera_obj_t * my_obj)
{
    int rc = 0;
    int idx;
    mm_camera_stream_t *stream;
    mm_camera_frame_queue_t *q;

    q = &my_obj->ch[MM_CAMERA_CH_SNAPSHOT].snapshot.main.frame.readyq;
    stream = &my_obj->ch[MM_CAMERA_CH_SNAPSHOT].snapshot.main;
    idx =  mm_camera_read_msm_frame(my_obj,stream);
    if (idx < 0)
        return;
    /* send to HAL */
    mm_camera_stream_frame_enq(q, &stream->frame.frame[idx]);
    mm_camera_snapshot_send_snapshot_notify(my_obj);
}
static void mm_camera_read_snapshot_thumbnail_frame(mm_camera_obj_t * my_obj)
{
    int idx, rc = 0;
    mm_camera_stream_t *stream;
    mm_camera_frame_queue_t *q;

    q = &my_obj->ch[MM_CAMERA_CH_SNAPSHOT].snapshot.thumbnail.frame.readyq;
    stream = &my_obj->ch[MM_CAMERA_CH_SNAPSHOT].snapshot.thumbnail;
    idx =  mm_camera_read_msm_frame(my_obj,stream);
    if (idx < 0)
        return;
    mm_camera_stream_frame_enq(q, &stream->frame.frame[idx]);
    mm_camera_snapshot_send_snapshot_notify(my_obj);
}

static void mm_camera_read_video_frame(mm_camera_obj_t * my_obj)
{
    int idx, rc = 0;
    mm_camera_stream_t *stream;
    mm_camera_frame_queue_t *q;

    stream = &my_obj->ch[MM_CAMERA_CH_VIDEO].video.video;
    idx =  mm_camera_read_msm_frame(my_obj,stream);
    if (idx < 0)
        return;
    pthread_mutex_lock(&my_obj->ch[MM_CAMERA_CH_VIDEO].mutex);
    if(my_obj->ch[MM_CAMERA_CH_VIDEO].buf_cb.cb) {
        mm_camera_ch_data_buf_t data;
        data.type = MM_CAMERA_CH_VIDEO;
        data.video.main.frame = NULL;
        data.video.main.idx = -1;
        data.video.video.idx = idx;
        data.video.video.frame = &my_obj->ch[MM_CAMERA_CH_VIDEO].video.video.
            frame.frame[idx].frame;
        my_obj->ch[MM_CAMERA_CH_VIDEO].video.video.frame.ref_count[idx]++;
        my_obj->ch[MM_CAMERA_CH_VIDEO].buf_cb.cb(&data,
                                my_obj->ch[MM_CAMERA_CH_VIDEO].buf_cb.user_data);
    }
    pthread_mutex_unlock(&my_obj->ch[MM_CAMERA_CH_VIDEO].mutex);
}

static void mm_camera_read_video_main_frame(mm_camera_obj_t * my_obj)
{
    int rc = 0;
    return;rc;
}

static void mm_camera_read_zsl_main_frame(mm_camera_obj_t * my_obj)
{
    int idx, rc = 0;
    mm_camera_stream_t *stream;
    mm_camera_frame_queue_t *q;
    mm_camera_frame_t *frame;
    int cnt, watermark;

    q =   &my_obj->ch[MM_CAMERA_CH_SNAPSHOT].snapshot.main.frame.readyq;
    stream = &my_obj->ch[MM_CAMERA_CH_SNAPSHOT].snapshot.main;
    idx =  mm_camera_read_msm_frame(my_obj,stream);
    if (idx < 0)
        return;
    mm_camera_stream_frame_enq(q, &stream->frame.frame[idx]);
    cnt = mm_camera_stream_frame_get_q_cnt(q);
    watermark = my_obj->ch[MM_CAMERA_CH_SNAPSHOT].buffering_frame.water_mark;
    if(watermark < cnt) {
        /* water overflow, queue head back to kernel */
        frame = mm_camera_stream_frame_deq(q);
        if(frame) {
            rc = mm_camera_stream_qbuf(my_obj, stream, frame->idx);
            if(rc < 0) {
                CDBG("%s: mm_camera_stream_qbuf(idx=%d) err=%d\n",
                     __func__, frame->idx, rc);
                return;
            }
        }
    }
}

static void mm_camera_read_zsl_postview_frame(mm_camera_obj_t * my_obj)
{
    int idx, rc = 0;
    mm_camera_stream_t *stream;
    mm_camera_frame_queue_t *q;
    mm_camera_frame_t *frame;
    int cnt, watermark;
    q = &my_obj->ch[MM_CAMERA_CH_SNAPSHOT].snapshot.thumbnail.frame.readyq;
    stream = &my_obj->ch[MM_CAMERA_CH_SNAPSHOT].snapshot.thumbnail;
    idx =  mm_camera_read_msm_frame(my_obj,stream);
    if (idx < 0)
        return;
    mm_camera_stream_frame_enq(q, &stream->frame.frame[idx]);
    watermark = my_obj->ch[MM_CAMERA_CH_SNAPSHOT].buffering_frame.water_mark;
    cnt = mm_camera_stream_frame_get_q_cnt(q);
    if(watermark < cnt) {
        /* water overflow, queue head back to kernel */
        frame = mm_camera_stream_frame_deq(q);
        if(frame) {
            rc = mm_camera_stream_qbuf(my_obj, stream, frame->idx);
            if(rc < 0) {
                CDBG("%s: mm_camera_stream_qbuf(idx=%d) err=%d\n",
                     __func__, frame->idx, rc);
                return;
            }
        }
    }
}

void mm_camera_msm_data_notify(mm_camera_obj_t * my_obj, int fd,
                               mm_camera_stream_type_t stream_type)
{
    switch(stream_type) {
    case MM_CAMERA_STREAM_RAW:
        mm_camera_read_raw_frame(my_obj);
        break;
    case MM_CAMERA_STREAM_PREVIEW:
        mm_camera_read_preview_frame(my_obj);
        break;
    case MM_CAMERA_STREAM_SNAPSHOT:
      if(my_obj->op_mode == MM_CAMERA_OP_MODE_ZSL)
        mm_camera_read_zsl_main_frame(my_obj);
      else
        mm_camera_read_snapshot_main_frame(my_obj);
        break;
    case MM_CAMERA_STREAM_THUMBNAIL:
      if(my_obj->op_mode == MM_CAMERA_OP_MODE_ZSL)
        mm_camera_read_zsl_postview_frame(my_obj);
      else
        mm_camera_read_snapshot_thumbnail_frame(my_obj);
        break;
    case MM_CAMERA_STREAM_VIDEO:
        mm_camera_read_video_frame(my_obj);
        break;
    case MM_CAMERA_STREAM_VIDEO_MAIN:
        mm_camera_read_video_main_frame(my_obj);
        break;
    default:
        break;
    }
}

static mm_camera_channel_type_t mm_camera_image_mode_to_ch(int image_mode)
{
    switch(image_mode) {
    case MSM_V4L2_EXT_CAPTURE_MODE_PREVIEW:
        return MM_CAMERA_CH_PREVIEW;
    case MSM_V4L2_EXT_CAPTURE_MODE_MAIN:
    case MSM_V4L2_EXT_CAPTURE_MODE_THUMBNAIL:
        return MM_CAMERA_CH_SNAPSHOT;
    case MSM_V4L2_EXT_CAPTURE_MODE_VIDEO:
        return MM_CAMERA_CH_VIDEO;
    case MSM_V4L2_EXT_CAPTURE_MODE_RAW:
        return MM_CAMERA_CH_RAW;
    default:
        return MM_CAMERA_CH_MAX;
    }
}

void mm_camera_dispatch_app_event(mm_camera_obj_t *my_obj, mm_camera_event_t *event)
{
    int i;
    mm_camera_evt_obj_t evtcb;

    pthread_mutex_lock(&my_obj->mutex);
    memcpy(&evtcb,
         &my_obj->evt[event->event_type],
         sizeof(mm_camera_evt_obj_t));
    pthread_mutex_unlock(&my_obj->mutex);
    for(i = 0; i < MM_CAMERA_EVT_ENTRY_MAX; i++) {
        if(evtcb.evt[i].evt_cb) {
            evtcb.evt[i].evt_cb(event, evtcb.evt[i].user_data);
        }
    }
}

void mm_camera_msm_evt_notify(mm_camera_obj_t * my_obj, int fd)
{
    struct v4l2_event ev;
    int rc;
    mm_camera_event_t *evt = NULL;

    CDBG("%s:E",__func__);
    memset(&ev, 0, sizeof(ev));
    rc = ioctl(fd, VIDIOC_DQEVENT, &ev);
    evt = (mm_camera_event_t *)ev.u.data;
    CDBG("%s:ioctl :rc :%d",__func__,rc);

    if (rc >= 0) {
        CDBG("%s: VIDIOC_DQEVENT type = 0x%x\n",
        __func__, ev.type);
        mm_camera_dispatch_app_event(my_obj, evt);
    }
}
