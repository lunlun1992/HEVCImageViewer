extern "C" {
#include <jni.h>
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"
#include "libavutil/avutil.h"
#include <stdlib.h>
#include <android/log.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <stdio.h>
#define TAG "NativeCodec"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

typedef enum CODEC_NAME
{
    CODEC_HEVC,
    CODEC_WEBP
}CODEC_NAME;
typedef struct CodecContext
{
    CODEC_NAME codec_name;
    AVCodec *pCodec = NULL;
    AVFrame *pFrame = NULL;
    AVCodecContext *pCodecCtx = NULL;
    AVCodecParserContext *pCodecParserCtx = NULL;
    uint8_t *buffer;
    size_t filesize;
    FILE *file;
    int IMG_WIDTH;
    int IMG_HEIGHT;
    uint8_t *yuv_buffer;
    jlong decode_times;
}CodecContext;



static CodecContext hevc, webp;

uint8_t yuv_webp[960 * 720 * 3 / 2];




static const char gVertexShader[] =
"attribute vec4 a_position;\n"
"attribute vec2 a_texCoord;\n"
"varying vec2 v_tc;\n"
"void main()\n"
"{\n"
"	gl_Position = a_position;\n"
"	v_tc = a_texCoord;\n"
"}\n";

static const char gFragmentShader[] =
"varying lowp vec2 v_tc;\n"
"uniform sampler2D u_texY;\n"
"uniform sampler2D u_texU;\n"
"uniform sampler2D u_texV;\n"
"void main(void)\n"
"{\n"
"mediump vec3 yuv;\n"
"lowp vec3 rgb;\n"
"yuv.x = texture2D(u_texY, v_tc).r;\n"
"yuv.y = texture2D(u_texU, v_tc).r - 0.5;\n"
"yuv.z = texture2D(u_texV, v_tc).r - 0.5;\n"
"rgb = mat3( 1,   1,   1,\n"
"0,       -0.39465,  2.03211,\n"
"1.13983,   -0.58060,  0) * yuv;\n"
"gl_FragColor = vec4(rgb, 1);\n"
"}\n";

static void printGLString(const char *name, GLenum s) {
    const char *v = (const char *) glGetString(s);
    LOGI("GL %s = %s\n", name, v);
}

static GLuint loadShader(GLenum shaderType, const char *pSource) {
    GLuint shader = glCreateShader(shaderType);
    if (shader) {
        glShaderSource(shader, 1, &pSource, NULL);
        glCompileShader(shader);
        GLint compiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            GLint infoLen = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
            if (infoLen) {
                char *buf = (char *) malloc(infoLen);
                if (buf) {
                    glGetShaderInfoLog(shader, infoLen, NULL, buf);
                    LOGE("Could not compile shader %d:\n%s\n", shaderType, buf);
                    free(buf);
                }
                glDeleteShader(shader);
                shader = 0;
            }
        }
    }
    return shader;
}

static GLuint createProgram(const char *pVertexSource,
                            const char *pFragmentSource) {
    GLuint vertexShader = loadShader(GL_VERTEX_SHADER, pVertexSource);
    if (!vertexShader) {
        return 0;
    }

    GLuint fragmentShader = loadShader(GL_FRAGMENT_SHADER, pFragmentSource);
    if (!fragmentShader) {
        return 0;
    }

    GLuint program = glCreateProgram();
    if (program) {
        glAttachShader(program, vertexShader);
        glAttachShader(program, fragmentShader);
        glLinkProgram(program);
        GLint linkStatus = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
        if (linkStatus != GL_TRUE) {
            GLint bufLength = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &bufLength);
            if (bufLength) {
                char *buf = (char *) malloc(bufLength);
                if (buf) {
                    glGetProgramInfoLog(program, bufLength, NULL, buf);
                    LOGE("Could not link program:\n%s\n", buf);
                    free(buf);
                }
            }
            glDeleteProgram(program);
            program = 0;
        }
    }
    return program;
}

static GLfloat vertexPositions[] = {
        -1.0, -1.0,
        1.0, -1.0,
        -1.0, 1.0,
        1.0, 1.0
};

static GLfloat textureCoords[] = {
        0.0, 1.0,
        1.0, 1.0,
        0.0, 0.0,
        1.0, 0.0
};

static void codec_context_init(CodecContext *codec, const char *filename)
{
    codec->file = fopen(filename, "rb");
    if (!codec->file) {
        LOGE("Error open file");
        return;
    }
    fseek(codec->file, 0, SEEK_END);
    codec->filesize = (size_t) ftell(codec->file);
    fseek(codec->file, 0, SEEK_SET);
    LOGI("filesize: %d", codec->filesize);
    codec->buffer = (uint8_t *) malloc(sizeof(uint8_t) * codec->filesize);
    if (!codec->buffer) {
        LOGE("Error malloc file buf");
        return;
    }
    if (codec->filesize != fread(codec->buffer, 1, codec->filesize, codec->file)) {
        LOGE("Error read file");
        return;
    }
    fclose(codec->file);

    avcodec_register_all();

    switch(codec->codec_name)
    {
        case CODEC_WEBP:
            codec->pCodec = avcodec_find_decoder(AV_CODEC_ID_WEBP);
            break;
        case CODEC_HEVC:
            codec->pCodec = avcodec_find_decoder(AV_CODEC_ID_HEVC);
            break;
        default:
            LOGE("Codec not support");
            return;
    }

    if (!codec->pCodec) {
        LOGE("Codec not found\n");
        return;
    }
    codec->pCodecCtx = avcodec_alloc_context3(codec->pCodec);
    if (!codec->pCodecCtx) {
        LOGE("Could not allocate video codec context\n");
        return;
    }

    switch(codec->codec_name)
    {
        case CODEC_WEBP:
            //codec->pCodecParserCtx = av_parser_init(AV_CODEC_ID_WEBP);
            break;
        case CODEC_HEVC:
            codec->pCodecParserCtx = av_parser_init(AV_CODEC_ID_HEVC);
            if (!codec->pCodecParserCtx) {
                LOGE("Could not allocate video parser context\n");
                return;
            }
            break;
        default:
            LOGE("Codec not support");
            return;
    }

    if (avcodec_open2(codec->pCodecCtx, codec->pCodec, NULL) < 0) {
        LOGE("Could not open codec\n");
        return;
    }
    codec->yuv_buffer = (uint8_t *)malloc(sizeof(uint8_t) * codec->IMG_HEIGHT * codec->IMG_WIDTH * 3 / 2);
    codec->decode_times = 0;
}

int Java_com_example_fengweilun_hevcimageviewer_MyRender_nativeInit(JNIEnv *env, jclass clazz, jint tag)
{
    LOGI("Will setup ... \n");
    GLuint gProgram;
    GLuint gTexIds[3];
    GLuint gAttribPosition;
    GLuint gAttribTexCoord;
    GLuint gUniformTexY;
    GLuint gUniformTexU;
    GLuint gUniformTexV;
    enum CODEC_NAME codec_name = (enum CODEC_NAME)tag;

    printGLString("Version", GL_VERSION);
    printGLString("Vendor", GL_VENDOR);
    printGLString("Renderer", GL_RENDERER);
    printGLString("Extensions", GL_EXTENSIONS);

    // create and use our program
    gProgram = createProgram(gVertexShader, gFragmentShader);
    if (!gProgram) {
        LOGE("Could not create program.");
        return -1;
    }
    glUseProgram(gProgram);

    // get the location of attributes in our shader
    gAttribPosition = glGetAttribLocation(gProgram, "a_position");
    gAttribTexCoord = glGetAttribLocation(gProgram, "a_texCoord");

    // get the location of uniforms in our shader
    gUniformTexY = glGetUniformLocation(gProgram, "u_texY");
    gUniformTexU = glGetUniformLocation(gProgram, "u_texU");
    gUniformTexV = glGetUniformLocation(gProgram, "u_texV");

    // can enable only once
    glEnableVertexAttribArray(gAttribPosition);
    glEnableVertexAttribArray(gAttribTexCoord);

    // set the value of uniforms (uniforms all have constant value)
    glUniform1i(gUniformTexY, 0);
    glUniform1i(gUniformTexU, 1);
    glUniform1i(gUniformTexV, 2);

    // generate and set parameters for the textures
    glEnable(GL_TEXTURE_2D);
    glGenTextures(3, gTexIds);
    for (int i = 0; i < 3; i++) {
        glActiveTexture((GLenum) (GL_TEXTURE0 + i));
        glBindTexture(GL_TEXTURE_2D, gTexIds[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    // set the value of attributes
    glVertexAttribPointer(gAttribPosition, 2, GL_FLOAT, 0, 0,
                          vertexPositions);
    glVertexAttribPointer(gAttribTexCoord, 2, GL_FLOAT, 0, 0,
                          textureCoords);

    if(codec_name == CODEC_HEVC)
    {
        hevc.IMG_HEIGHT = 960;
        hevc.IMG_WIDTH = 720;
        hevc.codec_name = CODEC_HEVC;
        codec_context_init(&hevc, "/storage/emulated/0/out.hevc");
        glViewport(0, 0, hevc.IMG_WIDTH, hevc.IMG_HEIGHT);
    }
    else if (codec_name == CODEC_WEBP)
    {
        webp.IMG_HEIGHT = 640;
        webp.IMG_WIDTH = 480;
        webp.codec_name = CODEC_WEBP;
        codec_context_init(&webp, "/storage/emulated/0/out.webp");
        glViewport(0, 0, 720, 960);
    }

    LOGI("setup finished\n");

    return 0;
}

static void decode_picture(CodecContext *codec)
{
    AVPacket packet;
    int ret, got_picture;
    int len;
    int in_len;
    int i;
    uint8_t *ptr_buf;
    uint8_t *in_data;
    codec->pFrame = av_frame_alloc();
    av_init_packet(&packet);
    in_len = codec->filesize;
    in_data = codec->buffer;
    SwsContext *scale;

    if(codec->codec_name == CODEC_HEVC)
    {
        scale = NULL;
        while (1)
        {
            len = av_parser_parse2(codec->pCodecParserCtx, codec->pCodecCtx, &packet.data,
                                   &packet.size,
                                   in_data,
                                   in_len, AV_NOPTS_VALUE, AV_NOPTS_VALUE, AV_NOPTS_VALUE);
            in_data += len;
            in_len -= len;
            if (packet.size == 0)
                continue;
            //Some Info from AVCodecParserContext
            LOGI("[Packet]Size:%6d\t", packet.size);
            LOGI("Number:%4d\n", codec->pCodecParserCtx->output_picture_number);

            ret = avcodec_decode_video2(codec->pCodecCtx, codec->pFrame, &got_picture, &packet);
            if (ret < 0) {
                LOGE("Decode Error.\n");
                return;
            }
            if (got_picture) {
                LOGI("\nCodec Full Name:%s\n", codec->pCodecCtx->codec->long_name);
                LOGI("width:%d\nheight:%d\n\n", codec->pCodecCtx->width, codec->pCodecCtx->height);

                LOGI("Succeed to decode 1 frame!\n");
                break;
            }
            if (in_len <= 0)
                break;
        }
    }
    else if(codec->codec_name == CODEC_WEBP)
    {
        scale = sws_getContext(codec->IMG_WIDTH, codec->IMG_HEIGHT, AV_PIX_FMT_YUV420P,
                               720, 960, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);
        packet.size = in_len;
        packet.data = (uint8_t *)malloc(in_len);
        memcpy(packet.data, in_data, in_len);
        ret = avcodec_decode_video2(codec->pCodecCtx, codec->pFrame, &got_picture, &packet);
        if (ret < 0) {
            LOGE("Decode Error.\n");
            return;
        }
    }
    if(!got_picture) {
        packet.data = NULL;
        packet.size = 0;
        while (1) {
            ret = avcodec_decode_video2(codec->pCodecCtx, codec->pFrame, &got_picture, &packet);
            if (ret < 0) {
                LOGE("Decode Error.\n");
                return;
            }
            if (!got_picture) {
                break;
            } else {

                LOGI("Flush Decoder: Succeed to decode 1 frame!\n");
                break;
            }
        }
    }

    ptr_buf = codec->yuv_buffer;
    for(i = 0; i < codec->pFrame->height; i++)
    {
        memcpy(ptr_buf, codec->pFrame->data[0] + i * codec->pFrame->linesize[0], codec->pFrame->width);
        ptr_buf += codec->pFrame->width;
    }
    for(i = 0; i < (codec->pFrame->height >> 1); i++)
    {
        memcpy(ptr_buf, codec->pFrame->data[1] + i * codec->pFrame->linesize[1], codec->pFrame->width >> 1);
        ptr_buf += (codec->pFrame->width >> 1);
    }
    for(i = 0; i < (codec->pFrame->height >> 1); i++)
    {
        memcpy(ptr_buf, codec->pFrame->data[2] + i * codec->pFrame->linesize[2], codec->pFrame->width >> 1);
        ptr_buf += (codec->pFrame->width >> 1);
    }
    if(codec->codec_name == CODEC_HEVC)
    {
        // upload textures
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glActiveTexture(GL_TEXTURE0 + 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, codec->pFrame->width, codec->IMG_HEIGHT, 0,
                     GL_LUMINANCE,
                     GL_UNSIGNED_BYTE, codec->yuv_buffer);
        glActiveTexture(GL_TEXTURE0 + 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, codec->pFrame->width >> 1,
                     codec->IMG_HEIGHT >> 1, 0, GL_LUMINANCE,
                     GL_UNSIGNED_BYTE, codec->yuv_buffer + codec->IMG_WIDTH * codec->IMG_HEIGHT);
        glActiveTexture(GL_TEXTURE0 + 2);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, codec->pFrame->width >> 1,
                     codec->IMG_HEIGHT >> 1, 0, GL_LUMINANCE,
                     GL_UNSIGNED_BYTE,
                     codec->yuv_buffer + codec->IMG_WIDTH * codec->IMG_HEIGHT * 5 / 4);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
    else if(codec->codec_name == CODEC_WEBP)
    {
        const uint8_t *const srcslice[3] = {codec->yuv_buffer, codec->yuv_buffer + codec->IMG_WIDTH * codec->IMG_HEIGHT, codec->yuv_buffer + codec->IMG_WIDTH * codec->IMG_HEIGHT * 5 / 4};
        uint8_t *const dstslice[3] = {yuv_webp, yuv_webp + 720 * 960, yuv_webp + 720 * 960 * 5 / 4};
        const int srcStride[3] = {codec->pFrame->width, codec->pFrame->width >> 1, codec->pFrame->width >> 1};
        int dstStride[3] = {720, 360, 360};
        sws_scale(scale, srcslice, srcStride, 0, codec->pFrame->height, dstslice, dstStride);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glActiveTexture(GL_TEXTURE0 + 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 720, 960, 0,
                     GL_LUMINANCE,
                     GL_UNSIGNED_BYTE, dstslice[0]);
        glActiveTexture(GL_TEXTURE0 + 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 360,
                     480, 0, GL_LUMINANCE,
                     GL_UNSIGNED_BYTE, dstslice[1]);
        glActiveTexture(GL_TEXTURE0 + 2);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 360,
                     480, 0, GL_LUMINANCE,
                     GL_UNSIGNED_BYTE,
                     dstslice[2]);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        LOGI("decode one webp frame");
    }
}


jlong
Java_com_example_fengweilun_hevcimageviewer_MyRender_nativeDrawFrame(JNIEnv *env, jclass clazz, jint tag)
{
    enum CODEC_NAME codec_name = (enum CODEC_NAME)tag;
    if(codec_name == CODEC_HEVC) {
        decode_picture(&hevc);
        return ++hevc.decode_times;
    }
    else if(codec_name == CODEC_WEBP) {
        decode_picture(&webp);
        return ++webp.decode_times;
    }
}


}






int Java_com_example_fengweilun_hevcimageviewer_MyRender_nativeSetup(jint w, jint h) {

    LOGI("setupGraphics(%d, %d)", w, h);

    return 0;
}

//            //Y, U, V
//            for (int i = 0; i < pFrame->height; i++)
//                memcpy(yuv_buf + i * IMG_WIDTH, pFrame->data[0] + pFrame->linesize[0] * i,
//                       IMG_WIDTH);
//
//            for (int i = 0; i < pFrame->height / 2; i++)
//                memcpy(yuv_buf + IMG_WIDTH * IMG_HEIGHT + i * IMG_WIDTH / 2,
//                       pFrame->data[1] + pFrame->linesize[1] * i, IMG_WIDTH / 2);
//
//            for (int i = 0; i < pFrame->height / 2; i++)
//                memcpy(yuv_buf + IMG_WIDTH * IMG_HEIGHT * 5 / 4 + i * IMG_WIDTH / 2,
//                       pFrame->data[2] + pFrame->linesize[2] * i, IMG_WIDTH / 2);
//void
//Java_com_example_fengweilun_hevcimageviewer_MyRender_nativeDrawFrame(JNIEnv *env, jclass clazz)
//{
//    size_t filesize;
//    FILE *file = fopen("/storage/emulated/0/out.yuv", "rb");
//    if (!file) {
//        LOGE("Error open file");
//        return;
//    }
//    fseek(file, 0, SEEK_END);
//    filesize = (size_t) ftell(file);
//    fseek(file, 0, SEEK_SET);
//    LOGI("filesize: %d", filesize);
//    uint8_t *yuf_buf = (uint8_t *) malloc(sizeof(uint8_t) * filesize);
//    memset(yuf_buf, 0, filesize);
//    if (!yuf_buf) {
//        LOGE("Error malloc file buf");
//        return;
//    }
//    if (filesize != fread(yuf_buf, 1, filesize, file)) {
//        LOGE("Error read file");
//        return;
//    }
//    fclose(file);
//
//    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
//    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
//
//    // upload textures
//    glActiveTexture(GL_TEXTURE0 + 0);
//    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, IMG_WIDTH, IMG_HEIGHT, 0, GL_LUMINANCE,
//                 GL_UNSIGNED_BYTE, yuf_buf);
//    glActiveTexture(GL_TEXTURE0 + 1);
//    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, IMG_WIDTH >> 1, IMG_HEIGHT >> 1, 0, GL_LUMINANCE,
//                 GL_UNSIGNED_BYTE, yuf_buf + IMG_WIDTH * IMG_HEIGHT);
//    glActiveTexture(GL_TEXTURE0 + 2);
//
//    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, IMG_WIDTH >> 1, IMG_HEIGHT >> 1, 0, GL_LUMINANCE,
//                 GL_UNSIGNED_BYTE, yuf_buf + IMG_WIDTH * IMG_HEIGHT * 5 / 4);
//    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
//    free(yuf_buf);
//}


//static GLfloat vertexPositions[] = {
//        -1.0, -1.0, 0.0,
//        1.0, -1.0, 0.0,
//        -1.0,  1.0, 0.0,
//        1.0,  1.0, 0.0
//};
//
//static GLfloat textureCoords[] = {
//        0.0, 1.0,
//        1.0, 1.0,
//        0.0, 0.0,
//        1.0, 0.0
//};
//
//static const char gVertexShader[] =
//"attribute vec4 a_position;\n"
//"attribute vec2 a_texCoord;\n"
//"varying vec2 v_tc;\n"
//"void main()\n"
//"{\n"
//"	gl_Position = a_position;\n"
//"	v_tc = a_texCoord;\n"
//"}\n";
//
//static const char *gFragmentShader =
//{
//        "varying lowp vec2 v_tc;"
//        "uniform sampler2D u_texY;"
//        "uniform sampler2D u_texU;"
//        "uniform sampler2D u_texV;"
//        "void main(void)"
//        "{"
//        "mediump vec3 yuv;"
//        "lowp vec3 rgb;"
//        "yuv.x = texture2D(u_texY, v_tc).r;"
//        "yuv.y = texture2D(u_texU, v_tc).r - 0.5;"
//        "yuv.z = texture2D(u_texV, v_tc).r - 0.5;"
//        "rgb = mat3( 1,   1,   1,"
//        "0,       -0.39465,  2.03211,"
//        "1.13983,   -0.58060,  0) * yuv;"
//        "gl_FragColor = vec4(rgb, 1);"
//        "}"
//};
//
//
//
//void Java_com_example_fengweilun_hevcimageviewer_MyRender_nativeDrawFrame(JNIEnv *env, jclass clazz)
//{
//    GLuint gTexIds[3];
//    GLuint gAttribPosition;
//    GLuint gAttribTexCoord;
//    GLuint gUniformTexY;
//    GLuint gUniformTexU;
//    GLuint gUniformTexV;
//    GLuint fragmentShader;
//    GLuint vertexShader;
//    GLuint program;
//    uint8_t *yuv[3];
//
//    yuv[0] = (uint8_t *)malloc(sizeof(uint8_t) * IMG_WIDTH * IMG_HEIGHT);
//    yuv[1] = (uint8_t *)malloc(sizeof(uint8_t) * (IMG_WIDTH >> 1) * (IMG_HEIGHT >> 1));
//    yuv[2] = (uint8_t *)malloc(sizeof(uint8_t) * (IMG_WIDTH >> 1) * (IMG_HEIGHT >> 1));
//
//    vertexShader = glCreateShader(GL_VERTEX_SHADER);
//    if (vertexShader)
//    {
//        glShaderSource(vertexShader, 1, (const GLchar **)(&gVertexShader), NULL);
//        glCompileShader(vertexShader);
//        GLint compiled = 0;
//        glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &compiled);
//        if (!compiled)
//        {
//            GLint infoLen = 0;
//            glGetShaderiv(vertexShader, GL_INFO_LOG_LENGTH, &infoLen);
//            if (infoLen)
//            {
//                char* buf = (char*) malloc(infoLen);
//                if (buf)
//                {
//                    glGetShaderInfoLog(vertexShader, infoLen, NULL, buf);
//                    LOGE("Could not compile shader %d:\n%s\n", GL_VERTEX_SHADER, buf);
//                    free(buf);
//                }
//                glDeleteShader(vertexShader);
//                vertexShader = 0;
//            }
//        }
//    }
//    else
//    {
//        LOGE("Error create Vertex shader");
//        return;
//    }
//
//    fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
//    if (fragmentShader)
//    {
//        glShaderSource(fragmentShader, 1, &gFragmentShader, NULL);
//        glCompileShader(fragmentShader);
//        GLint compiled = 0;
//        glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &compiled);
//        if (!compiled)
//        {
//            GLint infoLen = 0;
//            glGetShaderiv(fragmentShader, GL_INFO_LOG_LENGTH, &infoLen);
//            if (infoLen)
//            {
//                char* buf = (char*) malloc((size_t)infoLen);
//                if (buf)
//                {
//                    glGetShaderInfoLog(fragmentShader, infoLen, NULL, buf);
//                    LOGE("Could not compile shader %d:\n%s\n", GL_FRAGMENT_SHADER, buf);
//                    free(buf);
//                }
//                glDeleteShader(fragmentShader);
//                fragmentShader = 0;
//            }
//        }
//    }
//    else
//    {
//        LOGE("Error create Fragment shader");
//        return;
//    }
//
//    program = glCreateProgram();
//    if (program)
//    {
//        glAttachShader(program, vertexShader);
//        glAttachShader(program, fragmentShader);
//        glLinkProgram(program);
//        GLint linkStatus = GL_FALSE;
//        glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
//        if (linkStatus != GL_TRUE) {
//            GLint bufLength = 0;
//            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &bufLength);
//            if (bufLength) {
//                char* buf = (char*) malloc((size_t)bufLength);
//                if (buf) {
//                    glGetProgramInfoLog(program, bufLength, NULL, buf);
//                    LOGE("Could not link program:\n%s\n", buf);
//                    free(buf);
//                }
//            }
//            glDeleteProgram(program);
//            program = 0;
//        }
//    }
//    else
//    {
//        LOGE("Could not create program.");
//        return;
//    }
//
//    glUseProgram(program);
//    // get the location of attributes in our shader
//    gAttribPosition = (GLuint)glGetAttribLocation(program, "a_position");
//    gAttribTexCoord = (GLuint)glGetAttribLocation(program, "a_texCoord");
//
//    // get the location of uniforms in our shader
//    gUniformTexY = (GLuint)glGetUniformLocation(program, "u_texY");
//    gUniformTexU = (GLuint)glGetUniformLocation(program, "u_texU");
//    gUniformTexV = (GLuint)glGetUniformLocation(program, "u_texV");
//
//    // can enable only once
//    glEnableVertexAttribArray(gAttribPosition);
//    glEnableVertexAttribArray(gAttribTexCoord);
//
//    //set the value of uniforms (uniforms all have constant value)
//    glUniform1i(gUniformTexY, 0);
//    glUniform1i(gUniformTexU, 1);
//    glUniform1i(gUniformTexV, 2);
//
//    //generate and set parameters for the textures
//    glEnable (GL_TEXTURE_2D);
//    glGenTextures(3, gTexIds);
//    for (int i = 0; i < 3; i++) {
//        glActiveTexture((GLenum)(GL_TEXTURE0 + i));
//        glBindTexture(GL_TEXTURE_2D, gTexIds[i]);
//        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
//        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
//        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
//        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
//    }
//
//
//    LOGI("Will setup ... \n");
//
//
//    // set the value of attributes
//    glVertexAttribPointer(gAttribPosition, 3, GL_FLOAT, 0, 0,
//                          vertexPositions);
//    glVertexAttribPointer(gAttribTexCoord, 2, GL_FLOAT, 0, 0,
//                          textureCoords);
//
//    glViewport(0, 0, BACK_WIDTH, BACK_HEIGHT);
//
//    LOGI("setup finished\n");
//    glClearColor(1.0f, 0.0f, 1.0f, 1.0f);
//    glClear (GL_COLOR_BUFFER_BIT);
//
//    //LOGI("before upload: %u (%f)", getms(), pts);
//
//    memset(yuv[0], 0, IMG_WIDTH * IMG_HEIGHT);
//    memset(yuv[1], 0, IMG_WIDTH * IMG_HEIGHT >> 2);
//    memset(yuv[2], 0, IMG_WIDTH * IMG_HEIGHT >> 2);
//    // upload textures
//    glActiveTexture(GL_TEXTURE0 + 0);
//    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, BACK_WIDTH, BACK_HEIGHT, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, yuv[0]);
//    glActiveTexture(GL_TEXTURE0 + 1);
//    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, BACK_WIDTH >> 1, BACK_HEIGHT >> 1, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, yuv[1]);
//    glActiveTexture(GL_TEXTURE0 + 2);
//    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, BACK_WIDTH >> 1, BACK_HEIGHT >> 1, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, yuv[2]);
//    //pthread_mutex_unlock(&gVFMutex);
//
//    //LOGD("after upload: %u (%f)", getms(), pts);
//
//    //LOGD("before glDrawArrays: %u (%f)", getms(), pts);
//    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
//
//    //LOGD("after glDrawArrays: %u (%f)", getms(), pts);
//    free(yuv[0]);
//    free(yuv[1]);
//    free(yuv[2]);
//
//}

//}
//extern "C" {
//#include <jni.h>
//#include <GLES/gl.h>
//unsigned int vbo[2];
//float positions[12] = {1, -1, 0, 1, 1, 0, -1, -1, 0, -1, 1, 0};
//short indices[4] = {0, 1, 2, 3};
//JNIEXPORT void JNICALL
//Java_com_example_fengweilun_hevcimageviewer_MyRender_nativeCreate(JNIEnv *env, jobject obj) {
//    //生成两个缓存区对象
//    glGenBuffers(2, vbo);
//    //绑定第一个缓存对象
//    glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
//    //创建和初始化第一个缓存区对象的数据
//    glBufferData(GL_ARRAY_BUFFER, 4 * 12, positions, GL_STATIC_DRAW);
//    //绑定第二个缓存对象
//    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo[1]);
//    //创建和初始化第二个缓存区对象的数据
//    glBufferData(GL_ELEMENT_ARRAY_BUFFER, 2 * 4, indices, GL_STATIC_DRAW);
//}
//JNIEXPORT void JNICALL
//Java_com_example_fengweilun_hevcimageviewer_MyRender_nativeChange(JNIEnv *env, jobject obj,
//                                                                  jint width, jint height) {
//    //图形最终显示到屏幕的区域的位置、长和宽
//    glViewport(0, 0, width, height);
//    //指定矩阵
//    glMatrixMode(GL_PROJECTION);
//    //将当前的矩阵设置为glMatrixMode指定的矩阵
//    glLoadIdentity();
//    glOrthof(-2, 2, -2, 2, -2, 2);
//}
//
//JNIEXPORT void JNICALL
//Java_com_example_fengweilun_hevcimageviewer_MyRender_nativeDrawFrame(JNIEnv *env, jobject obj) {
//    //启用顶点设置功能，之后必须要关闭功能
//    glEnableClientState(GL_VERTEX_ARRAY);
//    //清屏
//    glClearColor(0, 0, 1, 1);
//    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
//    glMatrixMode(GL_MODELVIEW);
//    glLoadIdentity();
//    glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
//    //定义顶点坐标
//    glVertexPointer(3, GL_FLOAT, 0, 0);
//    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo[1]);
//    //按照参数给定的值绘制图形
//    glDrawElements(GL_TRIANGLE_STRIP, 4, GL_UNSIGNED_SHORT, 0);
//    //关闭顶点设置功能
//    glDisableClientState(GL_VERTEX_ARRAY);
//}
//}