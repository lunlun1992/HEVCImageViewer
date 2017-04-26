//
// Created by fengweilun on 20/04/2017.
//

#ifndef HEVCIMAGEVIEWER_IMAGECODEC_H
#define HEVCIMAGEVIEWER_IMAGECODEC_H
extern "C"
{
#include <stdint.h>
#include <pthread.h>
#include "libavcodec/avcodec.h"

#include "threadpool.h"
}


class ImageCodec {
private:
    //Singleton
    static ImageCodec *minstance;
    ImageCodec(int threads, int queue_size);
    ImageCodec(ImageCodec const &);
    void operator=(ImageCodec const &);

    //threads pool
    threadpool_t *mthreadpool;

public:
    //Singletion functions
    static int8_t initImageCodec(int threads, int queue_size)
    {
        if(minstance)
            return 0;
        minstance = new ImageCodec(threads, queue_size);
        return 0;
    }
    static int8_t uninitImageCodec()
    {
        threadpool_destroy(minstance->mthreadpool, threadpool_graceful);
        delete minstance;
        return 0;
    }
    static ImageCodec* GetInstance()
    {
        return minstance;
    }

    ~ImageCodec();

    //Queue functions
    int8_t decode_one_picture(CodecContext *context);

};


#endif //HEVCIMAGEVIEWER_IMAGECODEC_H
