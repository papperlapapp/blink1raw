/*
 * blinkraw.c
 *
 * fgmr 2012-09-22
 *
 * playing with the hidraw interface to blink(1)
 *
 * Thank you Alan Ott for
 * http://lxr.free-electrons.com/source/samples/hidraw/hid-example.c
 */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/hidraw.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef HIDIOCSFEATURE
#define HIDIOCSFEATURE(len) _IOC(_IOC_WRITE|_IOC_READ, 'H', 0x06, len)
#define HIDIOCGFEATURE(len) _IOC(_IOC_WRITE|_IOC_READ, 'H', 0x07, len)
#endif /* HIDIOCSFEATURE */

#define BLINK1_MK2_SERIALSTART 0x20000000
#define BLINK1_VENDOR_ID       0x27B8 /* = 0x27B8 = 10168 = thingm */
#define BLINK1_DEVICE_ID       0x01ED /* = 0x01ED */
#define BLINK1_BUF_SIZE        9

#define STEPS 32

static void usage(const char* hunh)
{
    if (NULL != hunh)
    {
        fprintf(stderr, "Can't understand %s\n", hunh);
    }

    fprintf(stderr, "Usage: blinkraw {arg, ...}\n"
            "  /dev/whatever     -- open device\n"
            "  ./whatever        -- open device\n"
            "  =R,G,B,t[,n]      -- fade to color\n"
            "  :R,G,B            -- set color (now)\n"
            "  @step:R,G,B,t[,n] -- set step\n"
            "  +step[,p,c]       -- start playing at step\n"
            "  -[step[,p,c]]     -- stop playing at step (default zero)\n"
            "  %%                 -- clear all steps\n"
            "  _                 -- turn off\n"
            "  _t[,n]            -- fade off\n"
            "\n"
            "       step is on [0,31]\n"
            "       R, G, B are on [0, 255]\n"
            "       t is time in centiseconds\n"
            "       n defaults to zero, is the LED number\n"
            "       p is the stop index, c is the repeat count\n"
            "\n"
            "    Arguments are applied in order.  A new device, which is\n"
            "    a valid blink(1) device, will become the new target.\n"
            "\n"
            "    Example:\n"
            "    # blinkraw /dev/hidraw* %% =255,0,0,100\n");
    exit(1);
}

static void color(int fd, char action, uint8_t R, uint8_t G, uint8_t B, uint16_t T, uint8_t step, uint8_t led)
{
    char buf[BLINK1_BUF_SIZE];
    int rc;

    if (-1 == fd)
        return;

    memset(buf, 0, sizeof(buf));

    if (step > STEPS - 1)
    {
        step = STEPS - 1;
    }
    if (led > 2)
    {
        led = 0;
    }

    buf[0] = 1;
    buf[1] = action;
    buf[2] = R; /* R */
    buf[3] = G; /* G */
    buf[4] = B; /* B */
    buf[5] = (T >> 8); /* time/cs high */
    buf[6] = (T & 0xff); /* time/cs low */
    buf[7] = led ? led : step; // Commands without led must set it to zero.

    rc = ioctl(fd, HIDIOCSFEATURE(sizeof(buf)), buf);
    if (rc < 0)
        perror("HIDIOCSFEATURE");
}

static void setledn(int fd, uint8_t led)
{
    char buf[BLINK1_BUF_SIZE];
    int rc;

    if (-1 == fd)
    {
        return;
    }

    memset(buf, 0, sizeof(buf));

    buf[0] = 1;
    buf[1] = 'l';
    buf[2] = led;

    rc = ioctl(fd, HIDIOCSFEATURE(sizeof(buf)), buf);
    if (rc < 0)
    {
        perror("HIDIOCSFEATURE");
    }
}

static void play(int fd, char action, uint8_t play, uint8_t step, uint8_t stop, uint8_t count)
{
    char buf[BLINK1_BUF_SIZE];
    int rc;

    if (-1 == fd)
    {
        return;
    }

    memset(buf, 0, sizeof(buf));

    buf[0] = 1;
    buf[1] = action;
    buf[2] = play;
    buf[3] = step;
    buf[4] = stop;
    buf[5] = count;

    rc = ioctl(fd, HIDIOCSFEATURE(sizeof(buf)), buf);
    if (rc < 0)
    {
        perror("HIDIOCSFEATURE");
    }
}

static int isblink1(int fd)
{
    int rc;
    struct hidraw_devinfo info;
    memset(&info, 0, sizeof(info));

    rc = ioctl(fd, HIDIOCGRAWINFO, &info);
    if (rc < 0)
    {
        perror("HIDIOCGRAWINFO");
        return 0;
    }

    if ((info.vendor == BLINK1_VENDOR_ID) && (info.product == BLINK1_DEVICE_ID))
    {
        return 1;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    int fd = -1;

    if (argc < 2)
    {
        usage(NULL);
    }

    while (++argv, --argc)
    {
        int rc = -1;
        uint8_t led = 0;
        uint8_t step = 0;
        uint8_t stop = 0;
        uint8_t count = 0;
        uint8_t R = 0;
        uint8_t G = 0;
        uint8_t B = 0;
        uint16_t T = 0;

        switch (**argv)
        {
        case '/':
        case '.':
            rc = open(*argv, O_RDWR | O_NONBLOCK);
            if (rc < 0)
            {
                perror(*argv);
                continue;
            }

            if (isblink1(rc))
            {
                if (fd >= 0)
                    close(fd);
                fd = rc;
            }

            break;
        case '=':
            rc = sscanf(*argv, "=%hhu,%hhu,%hhu,%hu,%hhu", &R, &G, &B, &T, &led);
            if (rc == 5)
            {
                if (step > 2)
                {
                    step = 0;
                }
            }
            else if (rc != 4)
            {
                usage(*argv);
            }
            color(fd, 'c', R, G, B, T, step, led);
            break;
        case ':':
            rc = sscanf(*argv, ":%hhu,%hhu,%hhu", &R, &G, &B);
            if (rc != 3)
            {
                usage(*argv);
            }
            color(fd, 'n', R, G, B, 0, 0, 0);
            break;
        case '@':
            rc = sscanf(*argv, "@%hhu:%hhu,%hhu,%hhu,%hu,%hhu", &step, &R, &G, &B, &T, &led);
            if (rc == 6)
            {
                if (led > 2)
                {
                    led = 0;
                }
            }
            else if (rc != 5)
            {
                usage(*argv);
            }
            if (step > STEPS)
            {
                usage(*argv);
            }
            setledn(fd, led);
            color(fd, 'P', R, G, B, T, step, 0);
            break;
        case '_':
            rc = sscanf(*argv, "_%hu,%hhu", &T, &led);
            if (rc == 1 || rc == 2)
            {
                color(fd, 'c', 0, 0, 0, T, 0, led);
            }
            else if (rc == -1)
            {
                color(fd, 'n', 0, 0, 0, 0, 0, 0);
            }
            else
            {
                usage(*argv);
            }
            break;
        case '+':
            rc = sscanf(*argv, "+%hhu,%hhu,%hhu", &step, &stop, &count);
            if (rc == 3)
            {
                if (stop < 1 || stop > STEPS + 1)
                {
                    usage(*argv);
                }
            }
            else if (rc != 1)
            {
                usage(*argv);
            }
            if (step > STEPS - 1)
            {
                usage(*argv);
            }
            play(fd, 'p', 1, step, stop, count);
            break;
        case '-':
            rc = sscanf(*argv, "-%hhu", &step);
            if (rc == 3)
            {
                if (stop < 1 || stop > STEPS + 1)
                {
                    usage(*argv);
                }
            }
            else if (rc != 1)
            {
                usage(*argv);
            }
            if (step > STEPS - 1)
            {
                step = 0;
            }
            play(fd, 'p', 0, step, stop, count);
            break;
        case '%':
            for (step = 0; step < STEPS + 1; ++step)
            {
                color(fd, 'P', 0, 0, 0, 0, step, 0);
            }
            break;
        default:
            usage(*argv);
        }
    }

    close(fd);
    return 0;
}
