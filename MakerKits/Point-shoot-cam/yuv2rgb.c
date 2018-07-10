/* YUV->RGB conversion for Raspberry Pi camera, because fast video
   port doesn't currently support raw RGB capture, only YUV.
   python-dev required to build.

   Adafruit invests time and resources providing this open source code,
   please support Adafruit and open-source development by purchasing
   products from Adafruit, thanks!

   Written by Phil Burgess / Paint Your Dragon for Adafruit Industries.
   BSD license, all text above must be included in any redistribution. */

#include <python2.7/Python.h>

static PyObject *convert(PyObject *self, PyObject *args) {
	Py_buffer      inBuf, outBuf;
	short          row, col, r, g, b, w, h, rd, gd, bd;
	unsigned char *rgbPtr, *yPtr, y;
	signed char   *uPtr, *vPtr, u, v;

	if(!PyArg_ParseTuple(args, "s*s*hh", &inBuf, &outBuf, &w, &h))
		return NULL;

	if(w & 31) w += 32 - (w & 31); // Round up width to multiple of 32
	if(h & 15) h += 16 - (h & 15); // Round up height to multiple of 16

	rgbPtr = outBuf.buf;
	yPtr   = inBuf.buf;
	uPtr   = (signed char *)&yPtr[w * h];
	vPtr   = &uPtr[(w * h) >> 2];
	w    >>= 1; // 2 columns processed per iteration

	for(row=0; row<h; row++) {
		for(col=0; col<w; col++) {
			// U, V (and RGB deltas) updated on even columns
			u          = uPtr[col] - 128;
			v          = vPtr[col] - 128;
			rd         =  (359 * v)             >> 8;
			gd         = ((183 * v) + (88 * u)) >> 8;
			bd         =  (454 * u)             >> 8;
			// Even column
			y          = *yPtr++;
			r          = y + rd;
			g          = y - gd;
			b          = y + bd;
			*rgbPtr++  = (r > 255) ? 255 : (r < 0) ? 0 : r;
			*rgbPtr++  = (g > 255) ? 255 : (g < 0) ? 0 : g;
			*rgbPtr++  = (b > 255) ? 255 : (b < 0) ? 0 : b;
			// Odd column
			y          = *yPtr++;
			r          = y + rd;
			g          = y - gd;
			b          = y + bd;
			*rgbPtr++  = (r > 255) ? 255 : (r < 0) ? 0 : r;
			*rgbPtr++  = (g > 255) ? 255 : (g < 0) ? 0 : g;
			*rgbPtr++  = (b > 255) ? 255 : (b < 0) ? 0 : b;
		}
		if(row & 1) {
			uPtr += w;
			vPtr += w;
		}
	}

	PyBuffer_Release(&inBuf);
	PyBuffer_Release(&outBuf);

	Py_INCREF(Py_None);
	return Py_None;
}

static PyMethodDef yuv2rgb_methods[] = {
	{"convert", convert, METH_VARARGS},
	{NULL,NULL}
};

PyMODINIT_FUNC inityuv2rgb(void) {
	(void)Py_InitModule("yuv2rgb", yuv2rgb_methods);
}

