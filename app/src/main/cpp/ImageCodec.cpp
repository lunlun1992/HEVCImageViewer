//
// Created by fengweilun on 20/04/2017.
//
#include "ImageCodec.h"
#include "threadpool.h"

ImageCodec *ImageCodec::minstance = NULL;
ImageCodec::ImageCodec(int threads, int queue_size)
{
    mthreadpool = threadpool_create(threads, queue_size, 0);
}

ImageCodec::~ImageCodec()
{

}


/*
 * APIs for multi-thread query
 */
//TODO: How to tell the caller that no need for wait
int8_t ImageCodec::decode_one_picture(CodecContext *context)
{
    context->bhasdone = 0;
    if(threadpool_add(this->mthreadpool, (void *)context, 0) < 0)
    {
        LOGE("add task error");
        pthread_mutex_destroy(&context->mutex);
        pthread_cond_destroy(&context->output_cond);
        return -1;
    }
    pthread_mutex_lock(&context->mutex);
    while(!context->bhasdone)
        pthread_cond_wait(&context->output_cond, &context->mutex);
    pthread_mutex_unlock(&context->mutex);

    return 0;
}



//void *thread_routine(void *arg)
//{
////    PerThreadContext *p = (PerThreadContext *)arg;
////    FrameThreadContext *fctx = p->parent;
////
////    while (1)
////    {
////        pthread_mutex_lock(&p->mutex);
////
////        //tell every one I'm Available
////        pthread_mutex_lock(&fctx->list_mutex);
////        p->next = NULL;
////        fctx->thread_end->next = p;
////        fctx->thread_end = p;
////        fctx->navaithreads++;
////        pthread_cond_broadcast(&fctx->avai_cond);
////        pthread_mutex_unlock(&fctx->list_mutex);
////
////        while (p->state == STATE_IDLE && !fctx->die)
////            pthread_cond_wait(&p->input_cond, &p->mutex);
////
////        pthread_mutex_unlock(&p->mutex);
////        if (fctx->die) break;
////
////        //Busy now
////        pthread_mutex_lock(&p->mutex);
////        p->got_frame = 0;
////        p->result = decode_picture(p->context, &p->got_frame);
////
////        pthread_mutex_lock(&p->context->mutex);
////        pthread_cond_signal(&p->context->output_cond);
////        p->state = STATE_DONE;
////        pthread_mutex_unlock(&p->context->mutex);
////
////        pthread_mutex_lock(&p->progress_mutex);
////        pthread_cond_signal(&p->progress_cond);
////        p->state = STATE_IDLE;
////        pthread_mutex_unlock(&p->progress_mutex);
////
////        pthread_mutex_unlock(&p->mutex);
////    }
////
////    return NULL;
//}



//ImageCodec::ImageCodec(int threads)
//{
//    threadpool = threadpool_create(threads, 100, 0);
//
////    this->mthreads = threads;
////    pthread_mutex_init(&this->mthreadContext.list_mutex, NULL);
////    pthread_cond_init(&this->mthreadContext.avai_cond, NULL);
////
////    this->mthreadContext.threads = (PerThreadContext *)malloc(sizeof(PerThreadContext) * threads);
////    this->mthread_routine = thread_routine;
////    this->mthreadContext.die = 0;
////    this->mthreadContext.owner = this;
////    this->mthreadContext.navaithreads = 0;
////    this->mthreadContext.thread_head.next = NULL;
////    this->mthreadContext.thread_end = &this->mthreadContext.thread_head;
////    PerThreadContext *prev = &this->mthreadContext.thread_head;
////
////    for(int i = 0; i < threads; i++)
////    {
////        PerThreadContext *p = &(this->mthreadContext.threads[i]);
////        p->parent = &this->mthreadContext;
////        prev->next = p;
////        prev = p;
////
////        pthread_mutex_init(&p->mutex, NULL);
////        pthread_cond_init(&p->input_cond, NULL);
////        pthread_mutex_init(&p->progress_mutex, NULL);
////        pthread_cond_init(&p->progress_cond, NULL);
////
////        avcodec_register_all();
////        p->pCodec = avcodec_find_decoder(AV_CODEC_ID_HEVC);
////        if (!p->pCodec) {
////            LOGE("Codec not found\n");
////            return;
////        }
////
////        p->pCodecCtx = avcodec_alloc_context3(p->pCodec);
////        if (!p->pCodecCtx) {
////            LOGE("Could not allocate video codec context\n");
////            return;
////        }
////
////        p->pCodecParserCtx = av_parser_init(AV_CODEC_ID_HEVC);
////        if (!p->pCodecParserCtx) {
////            LOGE("Could not allocate video parser context\n");
////            return;
////        }
////
////        if (avcodec_open2(p->pCodecCtx, p->pCodec, NULL) < 0) {
////            LOGE("Could not open codec\n");
////            return;
////        }
////
////        pthread_create(&(p->thread), NULL, this->mthread_routine, p);
////    }
////    prev->next = NULL;
//}

//int8_t ImageCodec::decode_one_picture(CodecContext *context)
//{
//    pthread_mutex_lock(&this->mthreadContext.list_mutex);
//    while(!this->mthreadContext.navaithreads)
//        pthread_cond_wait(&this->mthreadContext.avai_cond, &this->mthreadContext.list_mutex);
//    PerThreadContext *p = this->mthreadContext.thread_head.next;
//    this->mthreadContext.thread_head.next = p->next;
//    this->mthreadContext.navaithreads--;
//    if(this->mthreadContext.navaithreads == 0)
//        this->mthreadContext.thread_end = &this->mthreadContext.thread_head;
//    pthread_mutex_unlock(&this->mthreadContext.list_mutex);
//
//    pthread_mutex_lock(&p->mutex);
//    p->context = context;
//    pthread_cond_signal(&p->input_cond);
//    p->state = STATE_READY;
//    pthread_mutex_unlock(&p->mutex);
//
//    pthread_mutex_lock(&context->mutex);
//    while(p->state == STATE_READY)
//        pthread_cond_wait(&context->output_cond, &context->mutex);
//    pthread_mutex_unlock(&context->mutex);
//}