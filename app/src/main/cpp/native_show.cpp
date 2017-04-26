#include "ImageCodec.h"
#include "threadpool.h"

extern "C"
{
#include <jni.h>
#include <GLES2/gl2.h>
#include <stdlib.h>
#include "ImageCodec.h"
#include "utils.h"
#define TAG "NativeCodec"

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

static void printGLString(const char *name, GLenum s)
{
    const char *v = (const char *) glGetString(s);
    LOGI("GL %s = %s\n", name, v);
}

static GLuint loadShader(GLenum shaderType, const char *pSource)
{
    GLuint shader = glCreateShader(shaderType);
    if (shader)
    {
        glShaderSource(shader, 1, &pSource, NULL);
        glCompileShader(shader);
        GLint compiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled)
        {
            GLint infoLen = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
            if (infoLen)
            {
                char *buf = (char *) malloc((size_t)infoLen);
                if (buf)
                {
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

static GLuint createProgram(const char *pVertexSource, const char *pFragmentSource)
{
    GLuint vertexShader = loadShader(GL_VERTEX_SHADER, pVertexSource);
    if (!vertexShader) {
        return 0;
    }

    GLuint fragmentShader = loadShader(GL_FRAGMENT_SHADER, pFragmentSource);
    if (!fragmentShader) {
        return 0;
    }

    GLuint program = glCreateProgram();
    if (program)
    {
        glAttachShader(program, vertexShader);
        glAttachShader(program, fragmentShader);
        glLinkProgram(program);
        GLint linkStatus = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
        if (linkStatus != GL_TRUE)
        {
            GLint bufLength = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &bufLength);
            if (bufLength)
            {
                char *buf = (char *) malloc((size_t)bufLength);
                if (buf)
                {
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
        -1.0F, -1.0F,
        1.0, -1.0F,
        -1.0F, 1.0,
        1.0, 1.0
};

static GLfloat textureCoords[] = {
        0.0, 1.0,
        1.0, 1.0,
        0.0, 0.0,
        1.0, 0.0
};


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
    gAttribPosition = (GLuint)glGetAttribLocation(gProgram, "a_position");
    gAttribTexCoord = (GLuint)glGetAttribLocation(gProgram, "a_texCoord");

    // get the location of uniforms in our shader
    gUniformTexY = (GLuint)glGetUniformLocation(gProgram, "u_texY");
    gUniformTexU = (GLuint)glGetUniformLocation(gProgram, "u_texU");
    gUniformTexV = (GLuint)glGetUniformLocation(gProgram, "u_texV");

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

    LOGI("setup finished\n");
    ImageCodec::initImageCodec(1, 100);
    return 0;
}

static int times = 0;
jlong Java_com_example_fengweilun_hevcimageviewer_MyRender_nativeDrawFrame(JNIEnv * env, jclass clazz, jint tag)
{
    FILE *file;
    long filesize;
    AVPacket packet;
    int i;
    uint8_t *yuv_buffer, *ptr_buf;
    CodecContext c;

    pthread_mutex_init(&c.mutex, NULL);
    pthread_cond_init(&c.output_cond, NULL);

    av_init_packet(&packet);
    file = fopen("/storage/emulated/0/out.hevc", "rb");
    if (!file) {
        LOGE("Error open file");
        return 0;
    }
    fseek(file, 0, SEEK_END);
    filesize = ftell(file);
    fseek(file, 0, SEEK_SET);
    //LOGI("filesize: %ld", filesize);
    packet.data = (uint8_t *) malloc(sizeof(uint8_t) * filesize);
    packet.size = (int)filesize;
    if (!packet.data) {
        LOGE("Error malloc file buf");
        return 0;
    }
    if (filesize != fread(packet.data, 1, filesize, file)) {
        LOGE("Error read file");
        return 0;
    }
    fclose(file);


    c.packet = &packet;
    c.pFrame = av_frame_alloc();

    LOGE("Ready to decode one picture: %d", times++);
    ImageCodec *ins = ImageCodec::GetInstance();
    ins->decode_one_picture(&c);

    yuv_buffer = (uint8_t *)malloc(sizeof(uint8_t) * c.pFrame->height * c.pFrame->width * 3 / 2);
    ptr_buf = yuv_buffer;
    for(i = 0; i < c.pFrame->height; i++)
    {
        memcpy(ptr_buf, c.pFrame->data[0] + i * c.pFrame->linesize[0], (size_t)c.pFrame->width);
        ptr_buf += c.pFrame->width;
    }
    for(i = 0; i < (c.pFrame->height >> 1); i++)
    {
        memcpy(ptr_buf, c.pFrame->data[1] + i * c.pFrame->linesize[1], (size_t)c.pFrame->width >> 1);
        ptr_buf += (c.pFrame->width >> 1);
    }
    for(i = 0; i < (c.pFrame->height >> 1); i++)
    {
        memcpy(ptr_buf, c.pFrame->data[2] + i * c.pFrame->linesize[2], (size_t)c.pFrame->width >> 1);
        ptr_buf += (c.pFrame->width >> 1);
    }

    // upload textures
    LOGI("ready to Render Frames. width: %d, height: %d", c.pFrame->width, c.pFrame->height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glActiveTexture(GL_TEXTURE0 + 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, c.pFrame->width, c.pFrame->height, 0,
                 GL_LUMINANCE,
                 GL_UNSIGNED_BYTE, yuv_buffer);
    glActiveTexture(GL_TEXTURE0 + 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, c.pFrame->width >> 1,
                 c.pFrame->height >> 1, 0, GL_LUMINANCE,
                 GL_UNSIGNED_BYTE, yuv_buffer + c.pFrame->height * c.pFrame->width);
    glActiveTexture(GL_TEXTURE0 + 2);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, c.pFrame->width >> 1,
                 c.pFrame->height >> 1, 0, GL_LUMINANCE,
                 GL_UNSIGNED_BYTE,
                 yuv_buffer + c.pFrame->height * c.pFrame->width * 5 / 4);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    free(yuv_buffer);
    free(packet.data);
    av_packet_unref(&packet);
    av_frame_unref(c.pFrame);

    pthread_mutex_destroy(&c.mutex);
    pthread_cond_destroy(&c.output_cond);
    return 0;
}
}