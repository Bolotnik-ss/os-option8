#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <errno.h>
#include <jpeglib.h>

#define DEFAULT_PATH "/tmp/snapshots"
#define DEFAULT_INTERVAL 30

static int running = 1;
static int interval = DEFAULT_INTERVAL;
static char save_path[256] = DEFAULT_PATH;

void handle_signal(int sig) {
    running = 0;
}

void daemonize() {
    pid_t pid = fork();
    if (pid < 0) {
        perror("Не удалось выполнить fork");
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    if (setsid() < 0) {
        perror("Не удалось создать новую сессию (setsid)");
        exit(EXIT_FAILURE);
    }

    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);

    pid = fork();
    if (pid < 0) {
        perror("Не удалось выполнить fork");
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    umask(0);
    chdir("/");

    for (int x = sysconf(_SC_OPEN_MAX); x >= 0; x--) {
        close(x);
    }
}

void save_image_as_jpeg(const void *buffer, size_t width, size_t height) {
    char filename[512];
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    snprintf(filename, sizeof(filename), "%s/snapshot_%04d%02d%02d_%02d%02d%02d.jpg",
             save_path, t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);

    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("Не удалось сохранить изображение");
        return;
    }

    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPROW row_pointer[1];

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, file);

    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_start_compress(&cinfo, TRUE);

    unsigned char *rgb_buffer = (unsigned char *)malloc(width * height * 3);
    if (!rgb_buffer) {
        perror("Не удалось выделить память для буфера RGB");
        fclose(file);
        return;
    }

    for (size_t i = 0; i < height; i++) {
        for (size_t j = 0; j < width; j++) {
            size_t yuyv_index = (i * width + j) * 2;
            unsigned char Y = ((unsigned char *)buffer)[yuyv_index];
            unsigned char U = ((unsigned char *)buffer)[yuyv_index + 1];
            unsigned char V = ((unsigned char *)buffer)[yuyv_index + 2];

            int C = Y - 16;
            int D = U - 128;
            int E = V - 128;

            int R = (298 * C + 409 * E + 128) >> 8;
            int G = (298 * C - 100 * D - 208 * E + 128) >> 8;
            int B = (298 * C + 516 * D + 128) >> 8;

            rgb_buffer[(i * width + j) * 3] = (R < 0) ? 0 : (R > 255) ? 255 : R;
            rgb_buffer[(i * width + j) * 3 + 1] = (G < 0) ? 0 : (G > 255) ? 255 : G;
            rgb_buffer[(i * width + j) * 3 + 2] = (B < 0) ? 0 : (B > 255) ? 255 : B;
        }

        row_pointer[0] = &rgb_buffer[i * width * 3];
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    fclose(file);
    free(rgb_buffer);

    printf("Сохранен снимок: %s\n", filename);
}

int main(int argc, char *argv[]) {
    int fd;
    struct v4l2_capability cap;
    struct v4l2_format fmt;
    struct v4l2_requestbuffers req;
    struct v4l2_buffer buf;
    void *buffer;
    unsigned int buffer_length;

    if (argc > 1) {
        strncpy(save_path, argv[1], sizeof(save_path) - 1);
        save_path[sizeof(save_path) - 1] = '\0';
    }
    if (argc > 2) {
        interval = atoi(argv[2]);
        if (interval <= 0) interval = DEFAULT_INTERVAL;
    }

    daemonize();

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    fd = open("/dev/video0", O_RDWR);
    if (fd == -1) {
        perror("Не удалось открыть видеоустройство");
        exit(EXIT_FAILURE);
    }

    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == -1) {
        perror("Не удалось запросить возможности устройства");
        close(fd);
        exit(EXIT_FAILURE);
    }

    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = 640;
    fmt.fmt.pix.height = 480;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

    if (ioctl(fd, VIDIOC_S_FMT, &fmt) == -1) {
        perror("Не удалось установить формат видео");
        close(fd);
        exit(EXIT_FAILURE);
    }

    memset(&req, 0, sizeof(req));
    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(fd, VIDIOC_REQBUFS, &req) == -1) {
        perror("Не удалось запросить буферы");
        close(fd);
        exit(EXIT_FAILURE);
    }

    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;

    if (ioctl(fd, VIDIOC_QUERYBUF, &buf) == -1) {
        perror("Не удалось запросить буфер");
        close(fd);
        exit(EXIT_FAILURE);
    }

    buffer = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
    buffer_length = buf.length;

    if (buffer == MAP_FAILED) {
        perror("Не удалось отобразить буфер в память");
        close(fd);
        exit(EXIT_FAILURE);
    }

    if (mkdir(save_path, 0755) == -1 && errno != EEXIST) {
        perror("Не удалось создать каталог для сохранения");
        munmap(buffer, buffer_length);
        close(fd);
        exit(EXIT_FAILURE);
    }

    while (running) {
        if (ioctl(fd, VIDIOC_QBUF, &buf) == -1) {
            perror("Не удалось поставить буфер в очередь");
            break;
        }

        if (ioctl(fd, VIDIOC_STREAMON, &buf.type) == -1) {
            perror("Не удалось запустить захват");
            break;
        }

        if (ioctl(fd, VIDIOC_DQBUF, &buf) == -1) {
            perror("Не удалось извлечь буфер из очереди");
            break;
        }

        save_image_as_jpeg(buffer, fmt.fmt.pix.width, fmt.fmt.pix.height);

        sleep(interval);
    }

    munmap(buffer, buffer_length);
    close(fd);

    return 0;
}
