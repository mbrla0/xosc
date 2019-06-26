#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xdbe.h>

#define DEFAULT_WIDTH  800
#define DEFAULT_HEIGHT 600

struct
{
	FILE *in;
	int   disable_db; /* Disables double buffering */
} param;

int psetup(int argc, char **argv);

struct
{
	Display *d; /* Display handle			*/
	Window   w; /* Window handle			*/
	int      s; /* Screen number			*/
	Window   r; /* Root window				*/
	GC       g; /* Graphics Context			*/
	int      o; /* Window open?     		*/
	Drawable b; /* Buffer to be drawed on	*/

	unsigned int width, height;
	  signed int x, y;

	int xdbe;
	int xdbe_major;
	int xdbe_minor;
	
	Atom wm_delete;
} xorg;

int xsetup(char *dname);
int xfree();
int xerror(Display *d, XErrorEvent *ev);

struct
pixel
{
	unsigned char r;
	unsigned char g;
	unsigned char b;
};

struct
{
	unsigned int swaps;
	struct pixel *planes[2];
	struct pixel clear;
	unsigned int width, height;

	/* data frames per scan */
	unsigned int scale;

	/* milliseconds per scan */
	unsigned int period;
} rend;

int frsetup();
int frfree();
int frclr();
int frview(unsigned int w, unsigned int h);
int frswap();

int
main(int argc, char **argv)
{
	psetup(argc, argv);

	if(xsetup(NULL) != 0)
	{
		fprintf(stderr, "main()\tCannot initialize X window\n");
		return 1;
	}
	if(!xorg.xdbe)
		fprintf(stderr, "main()\tXdbe is not supported\n");

	frsetup();
	frview(DEFAULT_WIDTH, DEFAULT_HEIGHT);
	while(xorg.o)
	{
		/* Process Xorg events */
		XEvent ev;
		while(XPending(xorg.d))
		{
			XNextEvent(xorg.d, &ev);
			switch(ev.type)
			{
			case Expose:
				xorg.x = ev.xexpose.x;
				xorg.y = ev.xexpose.y;
				xorg.width  = ev.xexpose.width;
				xorg.height = ev.xexpose.height;
				frview(xorg.width, xorg.height);
				break;
			case ConfigureNotify:
				break;
			case ClientMessage:
				if(ev.xclient.data.l[0] == xorg.wm_delete)
					xorg.o = 0;
				break;
			}
		}

		frswap();
		usleep(2000);
	}

	frfree();
	xfree();
}

int
psetup(int argc, char **argv)
{
	int i;

	param.disable_db = 0;
	param.in = stdin;

	for(i = 0; i < argc; ++i)
	{
		if(strcmp(argv[i], "-s") == 0)
			param.disable_db = 1;
		else if(strcmp(argv[i], "-f") == 0)
		{
			if(i + 1 >= argc)
			{
				fprintf(stderr, "psetup()\t-f requires a file name\n");
				return 1;
			}
			param.in = fopen(argv[++i], "r");
		}
	}
	if(param.in == NULL)
	{
		fprintf(stderr, "psetup()\tcannot open input: %s\n",
			strerror(errno));
		return 1;
	}

	return 0;
}

int
frsetup()
{
	rend.planes[0] = NULL;
	rend.planes[1] = NULL;
	rend.swaps     = 0;
	rend.width     = 0;
	rend.height    = 0;
	rend.clear.r   = 255;
	rend.clear.g   = 255;
	rend.clear.b   = 255;
	rend.scale     = 0;
	rend.period    = 1;
	return 0;
}

int
frfree()
{
	return 0;
}

int
frswap()
{
	char textbuf[256];
	XdbeSwapInfo si;
	XTextItem ti;
	XImage im = { };

	im.width  = rend.width;
	im.height = rend.height;
	im.format = ZPixmap;
	im.depth  = 24;
	im.data   = (char*) rend.planes[rend.swaps % 2];
	im.byte_order = LSBFirst;
	im.bitmap_bit_order = LSBFirst;
	im.bitmap_unit = 8;
	im.bitmap_pad  = 8;
	im.bits_per_pixel = 24;
	im.red_mask   = 0xff;
	im.green_mask = 0xff;
	im.blue_mask  = 0xff;
	assert(XInitImage(&im) != 0);
	
	snprintf(textbuf, 256, "Frame: %u", rend.swaps);
	ti.chars  = textbuf;
	ti.nchars = strlen(textbuf);
	ti.delta  = 0;
	ti.font   = None;

	XPutImage(
		xorg.d,
		xorg.b,
		xorg.g,
		&im,
		0, 0,
		0, 0,
		rend.width,
		rend.height);
	
	XSetForeground(xorg.d, xorg.g, 0);
	XFillRectangle(
		xorg.d,
		xorg.b,
		xorg.g,
		0, xorg.height - 30, 100, 30);
	XSetForeground(xorg.d, xorg.g, -1);
	XDrawText(
		xorg.d,
		xorg.b,
		xorg.g,
		10, xorg.height - 10,
		&ti, 1);
	
	if(xorg.xdbe)
	{
		si.swap_window = xorg.w;
		si.swap_action = XdbeUndefined;
		if(!XdbeSwapBuffers(xorg.d, &si, 1))
			fprintf(stderr, "frswap()\tSwapping failed\n");
	}

	XFlush(xorg.d);
	++rend.swaps;
	return 0;
}

int
frview(unsigned int w, unsigned int h)
{
	unsigned int i;
	struct pixel *frame0;
	struct pixel *frame1;
	struct pixel *frame2;
	frame0 = malloc(sizeof(struct pixel) * w * h);
	frame1 = malloc(sizeof(struct pixel) * w * h);

	for(i = 0; i < w * h; ++i)
	{
		frame0[i] = rend.clear;
		frame1[i] = rend.clear;
	}

	frame2 = rend.planes[0];
	rend.planes[0] = frame0;
	free(frame2);

	frame2 = rend.planes[1];
	rend.planes[1] = frame1;
	free(frame2);

	rend.width  = w;
	rend.height = h;

	return 0;
}

int
frclr()
{
	struct pixel* plane;
	unsigned int i;

	plane = rend.planes[rend.swaps % 2];
	for(i = 0; i < rend.width * rend.height; ++i)
		plane[i] = rend.clear;

	return 0;
}

int
xsetup(char *dname)
{
	XSetWindowAttributes attrs;
	XGCValues cgv;

	XSetErrorHandler(xerror);
	xorg.d = XOpenDisplay(dname);
	if(xorg.d == NULL)
		return 1;
	
	xorg.xdbe = XdbeQueryExtension(xorg.d, &xorg.xdbe_major, &xorg.xdbe_minor);
	xorg.s = XDefaultScreen(xorg.d);
	xorg.r = XRootWindow(xorg.d, xorg.s);
	xorg.x = 0;
	xorg.y = 0;
	xorg.width  = DEFAULT_WIDTH;
	xorg.height = DEFAULT_HEIGHT;

	xorg.w = XCreateSimpleWindow(
		xorg.d,
		xorg.r,
		xorg.x, xorg.y,
		xorg.width, xorg.height,
		0,
		XBlackPixel(xorg.d, xorg.s),
		XBlackPixel(xorg.d, xorg.s));
	
	xorg.wm_delete = XInternAtom(xorg.d, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(xorg.d, xorg.w, &xorg.wm_delete, 1);

	attrs.event_mask = 
		  ExposureMask 
		| FocusChangeMask 
		| StructureNotifyMask;
	XChangeWindowAttributes(xorg.d, xorg.w, CWEventMask, &attrs);
	XMapWindow(xorg.d, xorg.w);

	cgv.foreground = 0xffffffffffff;
	cgv.background = 0x0000;
	xorg.g = XCreateGC(xorg.d, xorg.w, GCBackground | GCForeground, &cgv);

	if(param.disable_db)
		xorg.xdbe = 0;
	
	if(xorg.xdbe)
		xorg.b = XdbeAllocateBackBufferName(xorg.d, xorg.w, XdbeBackground);
	else
		xorg.b = xorg.w;

	xorg.o = 1;
	return 0;
}

int
xfree()
{
	return 0;
}

int
xerror(Display *d, XErrorEvent *ev)
{
	char errname[512];
	XGetErrorText(d, ev->error_code, errname, 512);

	fprintf(stderr, "xerror()\t%s\n", errname);
	fflush(stderr);
	return 0;
}

