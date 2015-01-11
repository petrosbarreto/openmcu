
#include <ptlib.h>

#include "config.h"
#include "mcu.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(54, 0, 0)
BOOL MCU_AVEncodeFrame(CodecID codec_id, const void * src, int src_size, void * dst, int & dst_size, int src_width, int src_height)
#else
BOOL MCU_AVEncodeFrame(AVCodecID codec_id, const void * src, int src_size, void * dst, int & dst_size, int src_width, int src_height)
#endif
{
  AVCodecContext *context = NULL;
  AVCodec *codec = NULL;
  AVPacket pkt = { 0 };

  PixelFormat frame_pix_fmt;
  if(codec_id == AV_CODEC_ID_MJPEG)
    frame_pix_fmt = AV_PIX_FMT_YUVJ420P;
  else
    return FALSE;

  AVFrame frame;
  int frame_buffer_size = avpicture_get_size(frame_pix_fmt, src_width, src_height);
  MCUBuffer frame_buffer(frame_buffer_size);
  avpicture_fill((AVPicture *)&frame, frame_buffer.GetPointer(), frame_pix_fmt, src_width, src_height);

  int ret = 0;
  BOOL result = FALSE;

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(54, 0, 0)
  MCUBuffer pkt_buffer(dst_size);
#endif

  codec = avcodec_find_encoder(codec_id);
  if(codec == NULL)
  {
    MCUTRACE(1, "Could not find encoder for mjpeg");
    goto end;
  }

#if LIBAVCODEC_VERSION_INT > AV_VERSION_INT(53,8,0)
  context = avcodec_alloc_context3(codec);
#else
  context = avcodec_alloc_context();
#endif
  if(context == NULL)
  {
    MCUTRACE(1, "Failed to allocate context");
    goto end;
  }

  context->pix_fmt       = frame_pix_fmt;
  context->width         = src_width;
  context->height        = src_height;
  context->qmin          = 30;
  context->qmax          = 31;
  context->time_base.num = 1;
  context->time_base.den = 1;

  // open codec
  avcodecMutex.Wait();
#if LIBAVCODEC_VERSION_INT > AV_VERSION_INT(53,8,0)
  ret = avcodec_open2(context, codec, NULL);
#else
  ret = avcodec_open(context, codec);
#endif
  avcodecMutex.Signal();
  if(ret < 0)
  {
    MCUTRACE(1, "Could not open video codec: " << ret << " " << AVErrorToString(ret));
    goto end;
  }

  {
    struct SwsContext *sws_ctx = sws_getContext(src_width, src_height, AV_PIX_FMT_YUV420P,
                                                src_width, src_height, frame_pix_fmt,
                                                SWS_BILINEAR, NULL, NULL, NULL);
    if(sws_ctx == NULL)
    {
      MCUTRACE(1, "MCUVideoMixer\tImpossible to create scale context for the conversion "
                  << src_width << "x" << src_height << "->" << src_width << "x" << src_height);
      goto end;
    }

    // initialize linesize
    AVPicture src_picture;
    avpicture_fill(&src_picture, (uint8_t *)src, AV_PIX_FMT_YUV420P, src_width, src_height);

    sws_scale(sws_ctx, src_picture.data, src_picture.linesize, 0, src_height,
                       frame.data, frame.linesize);

    sws_freeContext(sws_ctx);
  }

  av_init_packet(&pkt);
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(54, 0, 0)
  ret = avcodec_encode_video(context, pkt_buffer.GetPointer(), pkt_buffer.GetSize(), &frame);
  if(ret >= 0)
  {
    pkt.size = ret;
    pkt.data = pkt_buffer.GetPointer();
  }
#else
  int got_packet;
  ret = avcodec_encode_video2(context, &pkt, &frame, &got_packet);
#endif

  // if size is zero, it means the image was buffered
  if(ret < 0)
  {
    MCUTRACE(1, "error encoding video frame: " << ret << " " << AVErrorToString(ret));
    goto end;
  }

  if(pkt.size > dst_size)
  {
    MCUTRACE(1, "Picture size " << pkt.size << " is larger than the buffer size " << dst_size);
    goto end;
  }

  dst_size = pkt.size;
  memcpy(dst, pkt.data, pkt.size);

  result = TRUE;

  end:
    if(context)
    {
      // close codec
      avcodecMutex.Wait();
      avcodec_close(context);
      avcodecMutex.Signal();
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

BOOL MCU_AVDecodeFrameFromFile(PString & filename, void *dst, int & dst_size, int & dst_width, int & dst_height)
{
  AVFormatContext *fmt_ctx = NULL;
  AVCodecContext *context = NULL;
  AVPacket pkt = { 0 };
  AVFrame frame;
  int ret = 0;
  BOOL result = FALSE;

  av_register_all();

  if((ret = avformat_open_input(&fmt_ctx, filename, 0, 0)) < 0)
  {
    MCUTRACE(1, "Could not open input file " << filename << " " << ret << " " << AVErrorToString(ret));
    goto end;
  }

  if((ret = avformat_find_stream_info(fmt_ctx, 0)) < 0)
  {
    MCUTRACE(1, "Failed to retrieve input stream information from file " << filename << " " << ret << " " << AVErrorToString(ret));
    goto end;
  }

  av_dump_format(fmt_ctx, 0, filename, 0);

  for(unsigned i = 0; i < fmt_ctx->nb_streams; ++i)
  {
    if(fmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
    {
      AVStream *stream = fmt_ctx->streams[i];
      context = stream->codec;
      break;
    }
  }

  if(context == NULL)
  {
    MCUTRACE(1, "Could not find video context in file " << filename);
    goto end;
  }

  av_init_packet(&pkt);
  ret = av_read_frame(fmt_ctx, &pkt);
  if(ret < 0)
  {
    MCUTRACE(1, "Failed read frame from file " << filename << " " << ret << " " << AVErrorToString(ret));
    goto end;
  }

  context->codec = avcodec_find_decoder(context->codec_id);
  if(context->codec == NULL)
  {
    MCUTRACE(1, "Could not find decoder " << context->codec_id);
    goto end;
  }

  // open codec
  avcodecMutex.Wait();
#if LIBAVCODEC_VERSION_INT > AV_VERSION_INT(53,8,0)
  ret = avcodec_open2(context, context->codec, NULL);
#else
  ret = avcodec_open(context, context->codec);
#endif
  avcodecMutex.Signal();
  if(ret < 0)
  {
    MCUTRACE(1, "Could not open video codec: " << ret << " " << AVErrorToString(ret));
    goto end;
  }

  int got_picture;
  ret = avcodec_decode_video2(context, &frame, &got_picture, &pkt);
  if(ret < 0 || got_picture == 0)
  {
    MCUTRACE(1, "Error decoding video frame: " << ret << " " << AVErrorToString(ret));
    goto end;
  }

  {
    int dst_picture_size = avpicture_get_size(AV_PIX_FMT_YUV420P, frame.width, frame.height);
    if(dst_picture_size > dst_size)
    {
      MCUTRACE(1, "Picture size " << dst_picture_size << " is larger than the buffer size " << dst_size);
      goto end;
    }

    struct SwsContext *sws_ctx = sws_getContext(frame.width, frame.height, (PixelFormat)frame.format,
                                                frame.width, frame.height, AV_PIX_FMT_YUV420P,
                                                SWS_BILINEAR, NULL, NULL, NULL);
    if(sws_ctx == NULL)
    {
      MCUTRACE(1, "MCUVideoMixer\tImpossible to create scale context for the conversion "
                  << frame.width << "x" << frame.height << "->" << frame.width << "x" << frame.height);
      goto end;
    }

    // initialize linesize
    MCUBuffer dst_buffer(dst_picture_size);
    AVPicture dst_picture;
    avpicture_fill(&dst_picture, (uint8_t *)dst_buffer.GetPointer(), AV_PIX_FMT_YUV420P, frame.width, frame.height);

    sws_scale(sws_ctx, frame.data, frame.linesize, 0, frame.height,
                       dst_picture.data, dst_picture.linesize);

    sws_freeContext(sws_ctx);

    dst_width = frame.width;
    dst_height = frame.height;
    dst_size = dst_picture_size;
    memcpy(dst, dst_buffer.GetPointer(), dst_size);

    result = TRUE;
  }

  end:
    if(fmt_ctx)
      avformat_close_input(&fmt_ctx);
    if(context)
    {
      avcodecMutex.Wait();
      avcodec_close(context);
      avcodecMutex.Signal();
    }

    return result;
}

///////////////////////////////////////////////////////////////////////////