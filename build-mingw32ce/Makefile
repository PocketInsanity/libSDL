CC = arm-mingw32ce-gcc
AS = arm-mingw32ce-as
AR = arm-mingw32ce-ar cru
RANLIB = arm-mingw32ce-ranlib

VPATH = ../

CFLAGS = -I../include -I../src/audio -I../src/cdrom -I../src/timer -I../src/joystick -I../src/events -I../src/video -I../src/thread -I../src/thread/win32 -I../src -I../src/video/wincommon -I../src/video/windib -UWM_GETMINMAXINFO -UWS_MAXIMIZE
CFLAGS += -D_WIN32_WCE=300 -D_ARM_ -D__ARM__ -DUNICODE -D_UNICODE -DNO_SIGNAL_H -DENABLE_WINDIB -DENABLE_WINGAPI -D_FORCE_CE_SEMAPHORE -DWIN32 -DGCC_BUILD -D__stdcall__= -DNO_STDIO_REDIRECT
CFLAGS += -DWINCE_EXTRADEBUG
#CFLAGS += -DWMMSG_DEBUG
#CFLAGS += -DDEBUG -g
CFLAGS += -O3 -march=armv4 -mtune=xscale

OBJS = src/audio/SDL_audio.o src/audio/SDL_audiocvt.o src/audio/SDL_audiodev.o src/audio/windib/SDL_dibaudio.o src/audio/SDL_mixer.o src/audio/SDL_wave.o
OBJS += src/audio/windib/SDL_dibaudio.o 
OBJS += src/cdrom/SDL_cdrom.o src/cdrom/win32/SDL_syscdrom.o src/events/SDL_active.o src/events/SDL_events.o src/events/SDL_expose.o
OBJS += src/events/SDL_keyboard.o src/events/SDL_mouse.o src/events/SDL_quit.o src/events/SDL_resize.o src/file/SDL_rwops.o src/joystick/SDL_joystick.o
OBJS += src/joystick/win32/SDL_mmjoystick.o src/thread/win32/SDL_sysmutex.o src/thread/win32/SDL_syssem.o src/thread/win32/SDL_systhread.o
OBJS += src/thread/SDL_thread.o src/thread/win32/win_ce_semaphore.o src/timer/win32/SDL_systimer.o src/timer/SDL_timer.o src/video/windib/SDL_dibevents.o
OBJS += src/video/wincommon/SDL_sysmouse.o src/video/wincommon/SDL_syswm.o src/video/windib/SDL_dibvideo.o src/video/wincommon/SDL_sysevents.o
OBJS += src/video/wingapi/SDL_gapivideo.o src/video/wingapi/ARM_rot.o src/video/SDL_blit.o src/video/SDL_blit_0.o src/video/SDL_blit_1.o src/video/SDL_blit_A.o 
OBJS += src/video/ARM_blit1to2.o src/video/SDL_blit_N.o src/video/SDL_bmp.o src/video/SDL_cursor.o src/video/SDL_gamma.o src/video/SDL_pixels.o src/video/SDL_RLEaccel.o
OBJS += src/video/SDL_stretch.o src/video/SDL_surface.o src/video/SDL_video.o src/video/SDL_yuv.o src/video/SDL_yuv_mmx.o src/video/SDL_yuv_sw.o
OBJS += src/main/win32/SDL_win32_main.o src/SDL.o src/SDL_error.o src/SDL_fatal.o src/stdlib/SDL_getenv.o src/loadso/win32/SDL_sysloadso.o

create-output-directories :=					\
	$(shell for f in $(OBJS);				\
		do						\
			test -d $$f || mkdir -p `dirname $$f`;	\
		done)

all : libSDL.lib

#ARM_blit1to2.o :
#	arm-wince-pe-as $*.s -o $@

libSDL.lib : $(OBJS)
	$(AR) $@ $^
	$(RANLIB) $@

clean : 
	rm `find . -name *.o`

