/* ResidualVM - A 3D game interpreter
 *
 * ResidualVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "common/scummsys.h"

#if defined(SDL_BACKEND)

#include "backends/graphics/surfacesdl/surfacesdl-graphics.h"
#include "backends/events/sdl/sdl-events.h"
#include "backends/platform/sdl/sdl.h"
#include "common/config-manager.h"
#include "common/mutex.h"
#include "common/textconsole.h"
#include "common/translation.h"
#include "common/util.h"
#ifdef USE_RGB_COLOR
#include "common/list.h"
#endif
#include "graphics/font.h"
#include "graphics/fontman.h"
#include "graphics/scaler.h"
#include "graphics/surface.h"
#include "graphics/pixelbuffer.h"
#include "gui/EventRecorder.h"

static const OSystem::GraphicsMode s_supportedGraphicsModes[] = {
	{0, 0, 0}
};

SurfaceSdlGraphicsManager::SurfaceSdlGraphicsManager(SdlEventSource *sdlEventSource)
	:
	SdlGraphicsManager(sdlEventSource),
	_screen(0),
	_targetScreen(NULL),
	_overlayVisible(false),
	_overlayscreen(0),
	_overlayWidth(0), _overlayHeight(0),
	_overlayDirty(true),
	_screenChangeCount(0)
#ifdef USE_OPENGL
	, _opengl(false), _overlayNumTex(0), _overlayTexIds(0)
#endif
#ifdef USE_OPENGL_SHADERS
	, _boxShader(nullptr), _boxVerticesVBO(0)
#endif
	{
}

SurfaceSdlGraphicsManager::~SurfaceSdlGraphicsManager() {
	closeOverlay();

	if (!_opengl && _screen) {
		SDL_FreeSurface(_screen);
		_screen = NULL;
	}
}

void SurfaceSdlGraphicsManager::activateManager() {
	SdlGraphicsManager::activateManager();

	// Register the graphics manager as a event observer
	g_system->getEventManager()->getEventDispatcher()->registerObserver(this, 10, false);
}

void SurfaceSdlGraphicsManager::deactivateManager() {
	// Unregister the event observer
	if (g_system->getEventManager()->getEventDispatcher()) {
		g_system->getEventManager()->getEventDispatcher()->unregisterObserver(this);
	}

	SdlGraphicsManager::deactivateManager();
}

void SurfaceSdlGraphicsManager::resetGraphicsScale() {
	setGraphicsMode(0);
}

bool SurfaceSdlGraphicsManager::hasFeature(OSystem::Feature f) {
	return
		(f == OSystem::kFeatureFullscreenMode) ||
#ifdef USE_OPENGL
		(f == OSystem::kFeatureOpenGL);
#else
	false;
#endif
}

void SurfaceSdlGraphicsManager::setFeatureState(OSystem::Feature f, bool enable) {
	switch (f) {
	case OSystem::kFeatureFullscreenMode:
		_fullscreen = enable;
		break;
	default:
		break;
	}
}

bool SurfaceSdlGraphicsManager::getFeatureState(OSystem::Feature f) {
	switch (f) {
		case OSystem::kFeatureFullscreenMode:
			return _fullscreen;
		default:
			return false;
	}
}

const OSystem::GraphicsMode *SurfaceSdlGraphicsManager::getSupportedGraphicsModes() const {
	return s_supportedGraphicsModes;
}

int SurfaceSdlGraphicsManager::getDefaultGraphicsMode() const {
	return 0;// ResidualVM: not use it
}

void SurfaceSdlGraphicsManager::beginGFXTransaction() {
	// ResidualVM: not use it
}

OSystem::TransactionError SurfaceSdlGraphicsManager::endGFXTransaction() {
	// ResidualVM: not use it
	return OSystem::kTransactionSuccess;
}

#ifdef USE_RGB_COLOR
Common::List<Graphics::PixelFormat> SurfaceSdlGraphicsManager::getSupportedFormats() const {
	// ResidualVM: not use it
	return _supportedFormats;
}
#endif

bool SurfaceSdlGraphicsManager::setGraphicsMode(int mode) {
	// ResidualVM: not use it
	return true;
}

int SurfaceSdlGraphicsManager::getGraphicsMode() const {
	// ResidualVM: not use it
	return 0;
}

void SurfaceSdlGraphicsManager::initSize(uint w, uint h, const Graphics::PixelFormat *format) {
	// ResidualVM: not use it
}

void SurfaceSdlGraphicsManager::launcherInitSize(uint w, uint h) {
	closeOverlay();
	setupScreen(w, h, false, false);
}

// Switch bilinear instead nearest filtering
#define ENABLE_BILINEAR

Graphics::PixelBuffer SurfaceSdlGraphicsManager::setupScreen(uint screenW, uint screenH, bool fullscreen, bool accel3d) {
	uint32 sdlflags = 0;
	int bpp;

	closeOverlay();

#ifdef USE_OPENGL
	_opengl = accel3d;
	_antialiasing = 0;
#endif
	_fullscreen = fullscreen;
	if (_fullscreen)
		sdlflags |= SDL_FULLSCREEN;

#ifdef USE_OPENGL
	if (_opengl) {
		if (ConfMan.hasKey("antialiasing"))
			_antialiasing = ConfMan.getInt("antialiasing");

		SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
		setAntialiasing(true);

		sdlflags |= SDL_OPENGL;
		bpp = 24;
		_screen = SDL_SetVideoMode(screenW, screenH, bpp, sdlflags);
	} else
#endif
	{
#if defined(ENABLE_BILINEAR)
		bpp = 32;
#else
		bpp = 16;
#endif
		sdlflags |= SDL_SWSURFACE;
		const SDL_VideoInfo *vi = SDL_GetVideoInfo();
		_targetScreen = SDL_SetVideoMode(vi->current_w, vi->current_h, bpp, sdlflags);
		SDL_PixelFormat *f = _targetScreen->format;
		_targetScreenFormat = Graphics::PixelFormat(f->BytesPerPixel, 8 - f->Rloss, 8 - f->Gloss, 8 - f->Bloss, 0,
										f->Rshift, f->Gshift, f->Bshift, f->Ashift);

		if (_screen)
			SDL_FreeSurface(_screen);

		uint32 rmask, gmask, bmask, amask;
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
		rmask = 0x00001f00;
		gmask = 0x000007e0;
		bmask = 0x000000f8;
		amask = 0x00000000;
#else
		rmask = 0x0000f800;
		gmask = 0x000007e0;
		bmask = 0x0000001f;
		amask = 0x00000000;
#endif
		_screen = SDL_CreateRGBSurface(SDL_SWSURFACE, screenW, screenH, 16,
						rmask, gmask, bmask, amask);
	}

#ifdef USE_OPENGL
	// If 32-bit with antialiasing failed, try 32-bit without antialiasing
	if (!_screen && _opengl && _antialiasing) {
		warning("Couldn't create 32-bit visual with AA, trying 32-bit without AA");
		setAntialiasing(false);
		_screen = SDL_SetVideoMode(screenW, screenH, bpp, sdlflags);
	}

	// If 32-bit failed, try 16-bit
	if (!_screen && _opengl) {
		warning("Couldn't create 32-bit visual, trying 16-bit");
		SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
		SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 5);
		SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
		SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 1);
		setAntialiasing(true);
		_screen = SDL_SetVideoMode(screenW, screenH, 0, sdlflags);
	}

	// If 16-bit with antialiasing failed, try 16-bit without antialiasing
	if (!_screen && _opengl && _antialiasing) {
		warning("Couldn't create 16-bit visual with AA, trying 16-bit without AA");
		setAntialiasing(false);
		_screen = SDL_SetVideoMode(screenW, screenH, 0, sdlflags);
	}

	// If 16-bit with alpha failed, try 16-bit without alpha
	if (!_screen && _opengl) {
		warning("Couldn't create 16-bit visual with alpha, trying without alpha");
		SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
		SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 6);
		SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
		SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);
		setAntialiasing(true);
		_screen = SDL_SetVideoMode(screenW, screenH, 0, sdlflags);
	}

	// If 16-bit without alpha and with antialiasing didn't work, try without antialiasing
	if (!_screen && _opengl && _antialiasing) {
		warning("Couldn't create 16-bit visual with AA, trying 16-bit without AA");
		setAntialiasing(false);
		_screen = SDL_SetVideoMode(screenW, screenH, 0, sdlflags);
	}
#endif

	if (!_screen) {
		warning("Error: %s", SDL_GetError());
		g_system->quit();
	}

#ifdef USE_OPENGL
	if (_opengl) {
		int glflag;
		const GLubyte *str;

		// apply atribute again for sure based on SDL docs
		SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

		str = glGetString(GL_VENDOR);
		debug("INFO: OpenGL Vendor: %s", str);
		str = glGetString(GL_RENDERER);
		debug("INFO: OpenGL Renderer: %s", str);
		str = glGetString(GL_VERSION);
		debug("INFO: OpenGL Version: %s", str);
		SDL_GL_GetAttribute(SDL_GL_RED_SIZE, &glflag);
		debug("INFO: OpenGL Red bits: %d", glflag);
		SDL_GL_GetAttribute(SDL_GL_GREEN_SIZE, &glflag);
		debug("INFO: OpenGL Green bits: %d", glflag);
		SDL_GL_GetAttribute(SDL_GL_BLUE_SIZE, &glflag);
		debug("INFO: OpenGL Blue bits: %d", glflag);
		SDL_GL_GetAttribute(SDL_GL_ALPHA_SIZE, &glflag);
		debug("INFO: OpenGL Alpha bits: %d", glflag);
		SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, &glflag);
		debug("INFO: OpenGL Z buffer depth bits: %d", glflag);
		SDL_GL_GetAttribute(SDL_GL_DOUBLEBUFFER, &glflag);
		debug("INFO: OpenGL Double Buffer: %d", glflag);
		SDL_GL_GetAttribute(SDL_GL_STENCIL_SIZE, &glflag);
		debug("INFO: OpenGL Stencil buffer bits: %d", glflag);

#ifdef USE_OPENGL_SHADERS
		debug("INFO: GLSL version: %s", glGetString(GL_SHADING_LANGUAGE_VERSION));

		debug("INFO: GLEW Version: %s", glewGetString(GLEW_VERSION));

		// GLEW needs to be initialized to use shaders
		GLenum err = glewInit();
		if (err != GLEW_OK) {
			warning("Error: %s", glewGetErrorString(err));
			g_system->quit();
		}

		const GLfloat vertices[] = {
			0.0, 0.0,
			1.0, 0.0,
			0.0, 1.0,
			1.0, 1.0,
		};

		// Setup the box shader used to render the overlay
		const char* attributes[] = { "position", "texcoord", NULL };
		_boxShader = Graphics::Shader::fromStrings("box", Graphics::BuiltinShaders::boxVertex, Graphics::BuiltinShaders::boxFragment, attributes);
		_boxVerticesVBO = Graphics::Shader::createBuffer(GL_ARRAY_BUFFER, sizeof(vertices), vertices);
		_boxShader->enableVertexAttribute("position", _boxVerticesVBO, 2, GL_FLOAT, GL_TRUE, 2 * sizeof(float), 0);
		_boxShader->enableVertexAttribute("texcoord", _boxVerticesVBO, 2, GL_FLOAT, GL_TRUE, 2 * sizeof(float), 0);
#endif

	}
#endif

	_overlayWidth = screenW;
	_overlayHeight = screenH;

#ifdef USE_OPENGL
	if (_opengl) {
		uint32 rmask, gmask, bmask, amask;
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
		rmask = 0x00001f00;
		gmask = 0x000007e0;
		bmask = 0x000000f8;
		amask = 0x00000000;
#else
		rmask = 0x0000f800;
		gmask = 0x000007e0;
		bmask = 0x0000001f;
		amask = 0x00000000;
#endif
		_overlayscreen = SDL_CreateRGBSurface(SDL_SWSURFACE, _overlayWidth, _overlayHeight, 16,
						rmask, gmask, bmask, amask);
		_overlayScreenGLFormat = GL_UNSIGNED_SHORT_5_6_5;
	} else
#endif
	{
		_overlayscreen = SDL_CreateRGBSurface(SDL_SWSURFACE, _overlayWidth, _overlayHeight, 16,
					_screen->format->Rmask, _screen->format->Gmask, _screen->format->Bmask, _screen->format->Amask);
	}

	if (!_overlayscreen) {
		warning("Error: %s", SDL_GetError());
		g_system->quit();
	}

	/*_overlayFormat.bytesPerPixel = _overlayscreen->format->BytesPerPixel;

// 	For some reason the values below aren't right, at least on my system
	_overlayFormat.rLoss = _overlayscreen->format->Rloss;
	_overlayFormat.gLoss = _overlayscreen->format->Gloss;
	_overlayFormat.bLoss = _overlayscreen->format->Bloss;
	_overlayFormat.aLoss = _overlayscreen->format->Aloss;

	_overlayFormat.rShift = _overlayscreen->format->Rshift;
	_overlayFormat.gShift = _overlayscreen->format->Gshift;
	_overlayFormat.bShift = _overlayscreen->format->Bshift;
	_overlayFormat.aShift = _overlayscreen->format->Ashift;*/

	_overlayFormat = Graphics::PixelFormat(2, 5, 6, 5, 0, 11, 5, 0, 0);

	_screenChangeCount++;

	SDL_PixelFormat *f = _screen->format;
	_screenFormat = Graphics::PixelFormat(f->BytesPerPixel, 8 - f->Rloss, 8 - f->Gloss, 8 - f->Bloss, 0,
										f->Rshift, f->Gshift, f->Bshift, f->Ashift);

	return Graphics::PixelBuffer(_screenFormat, (byte *)_screen->pixels);
}

#ifdef USE_OPENGL

#define BITMAP_TEXTURE_SIZE 256

void SurfaceSdlGraphicsManager::updateOverlayTextures() {
	if (!_overlayscreen)
		return;

	// remove if already exist
	if (_overlayNumTex > 0) {
		glDeleteTextures(_overlayNumTex, _overlayTexIds);
		delete[] _overlayTexIds;
		_overlayNumTex = 0;
	}

	_overlayNumTex = ((_overlayWidth + (BITMAP_TEXTURE_SIZE - 1)) / BITMAP_TEXTURE_SIZE) *
					((_overlayHeight + (BITMAP_TEXTURE_SIZE - 1)) / BITMAP_TEXTURE_SIZE);
	_overlayTexIds = new GLuint[_overlayNumTex];
	glGenTextures(_overlayNumTex, _overlayTexIds);
	for (int i = 0; i < _overlayNumTex; i++) {
		glBindTexture(GL_TEXTURE_2D, _overlayTexIds[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, BITMAP_TEXTURE_SIZE, BITMAP_TEXTURE_SIZE, 0, GL_RGB, _overlayScreenGLFormat, NULL);
	}

	int bpp = _overlayscreen->format->BytesPerPixel;

	glPixelStorei(GL_UNPACK_ALIGNMENT, bpp);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, _overlayWidth);

	int curTexIdx = 0;
	for (int y = 0; y < _overlayHeight; y += BITMAP_TEXTURE_SIZE) {
		for (int x = 0; x < _overlayWidth; x += BITMAP_TEXTURE_SIZE) {
			int t_width = (x + BITMAP_TEXTURE_SIZE >= _overlayWidth) ? (_overlayWidth - x) : BITMAP_TEXTURE_SIZE;
			int t_height = (y + BITMAP_TEXTURE_SIZE >= _overlayHeight) ? (_overlayHeight - y) : BITMAP_TEXTURE_SIZE;
			glBindTexture(GL_TEXTURE_2D, _overlayTexIds[curTexIdx]);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, t_width, t_height, GL_RGB, _overlayScreenGLFormat,
				(byte *)_overlayscreen->pixels + (y * _overlayscreen->pitch) + (bpp * x));
			curTexIdx++;
		}
	}
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
}

void SurfaceSdlGraphicsManager::drawOverlayOpenGL() {
	if (!_overlayscreen)
		return;

	// Save current state
	glPushAttrib(GL_TRANSFORM_BIT | GL_ENABLE_BIT | GL_DEPTH_BUFFER_BIT | GL_SCISSOR_BIT);

	// prepare view
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(0, _overlayWidth, _overlayHeight, 0, 0, 1);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	glMatrixMode(GL_TEXTURE);
	glPushMatrix();
	glLoadIdentity();

	glDisable(GL_LIGHTING);
	glEnable(GL_TEXTURE_2D);
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glEnable(GL_SCISSOR_TEST);

	glScissor(0, 0, _overlayWidth, _overlayHeight);

	int curTexIdx = 0;
	for (int y = 0; y < _overlayHeight; y += BITMAP_TEXTURE_SIZE) {
		for (int x = 0; x < _overlayWidth; x += BITMAP_TEXTURE_SIZE) {
			glBindTexture(GL_TEXTURE_2D, _overlayTexIds[curTexIdx]);
			glBegin(GL_QUADS);
			glTexCoord2f(0, 0);
			glVertex2i(x, y);
			glTexCoord2f(1.0f, 0.0f);
			glVertex2i(x + BITMAP_TEXTURE_SIZE, y);
			glTexCoord2f(1.0f, 1.0f);
			glVertex2i(x + BITMAP_TEXTURE_SIZE, y + BITMAP_TEXTURE_SIZE);
			glTexCoord2f(0.0f, 1.0f);
			glVertex2i(x, y + BITMAP_TEXTURE_SIZE);
			glEnd();
			curTexIdx++;
		}
	}

	// Restore previous state
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();

	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	glMatrixMode(GL_TEXTURE);
	glPopMatrix();

	glPopAttrib();
}

#ifdef USE_OPENGL_SHADERS
void SurfaceSdlGraphicsManager::drawOverlayOpenGLShaders() {
	if (!_overlayscreen)
		return;

	glDisable(GL_LIGHTING);
	glEnable(GL_TEXTURE_2D);
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glEnable(GL_SCISSOR_TEST);

	glScissor(0, 0, _overlayWidth, _overlayHeight);

	_boxShader->use();
	_boxShader->setUniform("sizeWH", Math::Vector2d(BITMAP_TEXTURE_SIZE / (float)_overlayWidth, BITMAP_TEXTURE_SIZE / (float)_overlayHeight));
	_boxShader->setUniform("flipY", true);
	_boxShader->setUniform("texcrop", Math::Vector2d(1.0, 1.0));

	int curTexIdx = 0;
	for (int y = 0; y < _overlayHeight; y += BITMAP_TEXTURE_SIZE) {
		for (int x = 0; x < _overlayWidth; x += BITMAP_TEXTURE_SIZE) {
			_boxShader->setUniform("offsetXY", Math::Vector2d(x / (float)_overlayWidth, y / (float)_overlayHeight));

			glBindTexture(GL_TEXTURE_2D, _overlayTexIds[curTexIdx]);
			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
			curTexIdx++;
		}
	}
}
#endif
#endif

void SurfaceSdlGraphicsManager::drawOverlay() {
	if (!_overlayscreen)
		return;

	SDL_LockSurface(_screen);
	SDL_LockSurface(_overlayscreen);
	Graphics::PixelBuffer srcBuf(_overlayFormat, (byte *)_overlayscreen->pixels);
	Graphics::PixelBuffer dstBuf(_screenFormat, (byte *)_screen->pixels);
	int h = _overlayHeight;

	do {
		dstBuf.copyBuffer(0, _overlayWidth, srcBuf);

		srcBuf.shiftBy(_overlayWidth);
		dstBuf.shiftBy(_overlayWidth);
	} while (--h);
	SDL_UnlockSurface(_screen);
	SDL_UnlockSurface(_overlayscreen);
}

// Based on:
// http://fastcpp.blogspot.com/2011/06/bilinear-pixel-interpolation-using-sse.html
// http://tech-algorithm.com/articles/bilinear-image-scaling/

void BlitBilinearScalerFloat(uint32 *dstPtr, int dstW, int dstH, Graphics::PixelFormat dstFmt, uint16 *srcPtr, int srcW, int srcH) {
	int a, b, c, d, x, y, index;
	int offset = 0;
	float x_step, y_step;
	float x_ratio = ((float)(srcW - 1)) / dstW;
	float y_ratio = ((float)(srcH - 1)) / dstH;

	y_step = 0;
	for (int i = 0; i < dstH; i++) {
		x_step = 0;
		y = (int)(y_step);
		float fy = y_step - y;
		float fy1 = 1.0f - fy;
		int indexBase = y * srcW;
		for (int j = 0;j < dstW; j++) {
			x = (int)(x_step);
			float fx = x_step - x;
			float fx1 = 1.0f - fx;

			index = indexBase + x;
			a = (srcPtr[index] & 0xF800) << 8 |
			    (srcPtr[index] & 0x07E0) << 5 |
			    (srcPtr[index] & 0x001F) << 3;
			b = (srcPtr[index + 1] & 0xF800) << 8 |
			    (srcPtr[index + 1] & 0x07E0) << 5 |
			    (srcPtr[index + 1] & 0x001F) << 3;
			c = (srcPtr[index + srcW] & 0xF800) << 8 |
			    (srcPtr[index + srcW] & 0x07E0) << 5 |
			    (srcPtr[index + srcW] & 0x001F) << 3;
			d = (srcPtr[index + srcW + 1] & 0xF800) << 8 |
			    (srcPtr[index + srcW + 1] & 0x07E0) << 5 |
			    (srcPtr[index + srcW + 1] & 0x001f) << 3;

			int w1 = fx1 * fy1 * 256.0f;
			int w2 = fx  * fy1 * 256.0f;
			int w3 = fx1 * fy  * 256.0f;
			int w4 = fx  * fy  * 256.0f;

// 565
//			int red   = (((a >> 16) & 0xff) * w1 + ((b >> 16) & 0xff) * w2 + ((c >> 16) & 0xff) * w3 + ((d >> 16) & 0xff) * w4) >> 8;
//			int green = (((a >> 8 ) & 0xff) * w1 + ((b >> 8 ) & 0xff) * w2 + ((c >> 8 ) & 0xff) * w3 + ((d >> 8 ) & 0xff) * w4) >> 8;
//			int blue  = ( (a & 0xff)        * w1 +  (b & 0xff)        * w2 +  (c & 0xff)        * w3 +  (d & 0xff)        * w4) >> 8;
//			dstPtr[offset++] = ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3);

			int red   = (((a >> 16) & 0xff) * w1 + ((b >> 16) & 0xff) * w2 + ((c >> 16) & 0xff) * w3 + ((d >> 16) & 0xff) * w4) >> 8;
			int green = (((a >> 8 ) & 0xff) * w1 + ((b >> 8 ) & 0xff) * w2 + ((c >> 8 ) & 0xff) * w3 + ((d >> 8 ) & 0xff) * w4) >> 8;
			int blue  = ( (a & 0xff)        * w1 +  (b & 0xff)        * w2 +  (c & 0xff)        * w3 +  (d & 0xff)        * w4) >> 8;
			dstPtr[offset++] = dstFmt.RGBToColor(red, green, blue);

			x_step += x_ratio;
		}
		y_step += y_ratio;
	}
}

// Based on:
// http://fastcpp.blogspot.com/2011/06/bilinear-pixel-interpolation-using-sse.html
// http://tech-algorithm.com/articles/bilinear-image-scaling/

void BlitBilinearScalerInteger(uint32 *dstPtr, int dstW, int dstH, Graphics::PixelFormat dstFmt, uint16 *srcPtr, int srcW, int srcH) {
	int a, b, c, d, x, y, index;
	int offset = 0;
	int x_step, y_step;
	int x_ratio = ((srcW - 1) * 256) / dstW;
	int y_ratio = ((srcH - 1) * 256) / dstH;

	y_step = 0;
	for (int i = 0; i < dstH; i++) {
		x_step = 0;
		y = y_step / 256;
		int fy = y_step - (y * 256);
		int fy1 = 256 - fy;
		int indexBase = y * srcW;
		for (int j = 0;j < dstW; j++) {
			x = x_step / 256;
			int fx = x_step - (x * 256);
			int fx1 = 256 - fx;

			index = indexBase + x;
			a = (srcPtr[index] & 0xF800) << 8 |
			    (srcPtr[index] & 0x07E0) << 5 |
			    (srcPtr[index] & 0x001F) << 3;
			b = (srcPtr[index + 1] & 0xF800) << 8 |
			    (srcPtr[index + 1] & 0x07E0) << 5 |
			    (srcPtr[index + 1] & 0x001F) << 3;
			c = (srcPtr[index + srcW] & 0xF800) << 8 |
			    (srcPtr[index + srcW] & 0x07E0) << 5 |
			    (srcPtr[index + srcW] & 0x001F) << 3;
			d = (srcPtr[index + srcW + 1] & 0xF800) << 8 |
			    (srcPtr[index + srcW + 1] & 0x07E0) << 5 |
			    (srcPtr[index + srcW + 1] & 0x001f) << 3;

			int w1 = fx1 * fy1;
			int w2 = fx  * fy1;
			int w3 = fx1 * fy;
			int w4 = fx  * fy;

// 565
//			int red   = (((a >> 16) & 0xff) * w1 + ((b >> 16) & 0xff) * w2 + ((c >> 16) & 0xff) * w3 + ((d >> 16) & 0xff) * w4);
//			int green = (((a >> 8 ) & 0xff) * w1 + ((b >> 8 ) & 0xff) * w2 + ((c >> 8 ) & 0xff) * w3 + ((d >> 8 ) & 0xff) * w4);
//			int blue  = ( (a & 0xff)        * w1 +  (b & 0xff)        * w2 +  (c & 0xff)        * w3 +  (d & 0xff)        * w4);
//			dstPtr[offset++] = ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3);

			int red   = (((a >> 16) & 0xff) * w1 + ((b >> 16) & 0xff) * w2 + ((c >> 16) & 0xff) * w3 + ((d >> 16) & 0xff) * w4) >> 16;
			int green = (((a >> 8 ) & 0xff) * w1 + ((b >> 8 ) & 0xff) * w2 + ((c >> 8 ) & 0xff) * w3 + ((d >> 8 ) & 0xff) * w4) >> 16;
			int blue  = ( (a & 0xff)        * w1 +  (b & 0xff)        * w2 +  (c & 0xff)        * w3 +  (d & 0xff)        * w4) >> 16;
			dstPtr[offset++] = dstFmt.RGBToColor(red, green, blue);

			x_step += x_ratio;
		}
		y_step += y_ratio;
	}
}

#include <xmmintrin.h>
#if defined(__SSE4_1__)
#include <smmintrin.h>
#endif

// Based on:
// http://fastcpp.blogspot.com/2011/06/bilinear-pixel-interpolation-using-sse.html
// http://tech-algorithm.com/articles/bilinear-image-scaling/

static const __m128 CONST_1111 = _mm_set1_ps(1);
static const __m128 CONST_256 = _mm_set1_ps(256);

static FORCEINLINE __m128 CalcWeights(float x, float y) {
	__m128 ssx = _mm_set_ss(x);
	__m128 ssy = _mm_set_ss(y);
	__m128 psXY = _mm_unpacklo_ps(ssx, ssy);      // 0 0 y x

#if defined(__SSE4_1__)
// compile with -msse4.1
	__m128 psXYfloor = _mm_floor_ps(psXY); // use this line for if you have SSE4
#else
	_MM_SET_ROUNDING_MODE(_MM_ROUND_TOWARD_ZERO);
	__m128 psXYfloor = _mm_cvtepi32_ps(_mm_cvtps_epi32(psXY));
#endif
	__m128 psXYfrac = _mm_sub_ps(psXY, psXYfloor); // = frac(psXY)

	__m128 psXYfrac1 = _mm_sub_ps(CONST_1111, psXYfrac); // ? ? (1-y) (1-x)
	__m128 w_x = _mm_unpacklo_ps(psXYfrac1, psXYfrac);   // ? ?     x (1-x)
	w_x = _mm_movelh_ps(w_x, w_x);      // x (1-x) x (1-x)
	__m128 w_y = _mm_shuffle_ps(psXYfrac1, psXYfrac, _MM_SHUFFLE(1, 1, 1, 1)); // y y (1-y) (1-y)

	// complete weight vector
	return _mm_mul_ps(w_x, w_y);
}

// SSE2 565 to 8888 conversion code based on: pixman-sse2.c code
static __m128i MaskRed;
static __m128i MaskGreen;
static __m128i MaskBlue;
static __m128i Mask565FixRB;
static __m128i Mask565FixG;

static FORCEINLINE __m128i unpack565to8888(__m128i lo) {
	__m128i r, g, b, rb, t;

	r = _mm_and_si128(_mm_slli_epi32(lo, 8), MaskRed);
	g = _mm_and_si128(_mm_slli_epi32(lo, 5), MaskGreen);
	b = _mm_and_si128(_mm_slli_epi32(lo, 3), MaskBlue);

	rb = _mm_or_si128(r, b);
	t  = _mm_and_si128(rb, Mask565FixRB);
	t  = _mm_srli_epi32(t, 5);
	rb = _mm_or_si128(rb, t);

	t  = _mm_and_si128(g, Mask565FixG);
	t  = _mm_srli_epi32(t, 6);
	g  = _mm_or_si128(g, t);

	return _mm_or_si128(rb, g);
}

static FORCEINLINE __m128i createMask_2x32_128(uint32_t mask0, uint32_t mask1) {
    return _mm_set_epi32(mask0, mask1, mask0, mask1);
}

static void BlitBilinearScalerSSE(uint32 *dstPtr, int dstW, int dstH, Graphics::PixelFormat dstFmt, uint16 *srcPtr, int srcW, int srcH) {
	float x_step, y_step;
	float x_ratio = ((float)(srcW - 1)) / dstW;
	float y_ratio = ((float)(srcH - 1)) / dstH;
	int index, offset = 0;

	MaskRed   = createMask_2x32_128(0x00f80000, 0x00f80000);
	MaskGreen = createMask_2x32_128(0x0000fc00, 0x0000fc00);
	MaskBlue  = createMask_2x32_128(0x000000f8, 0x000000f8);
	Mask565FixRB = createMask_2x32_128(0x00e000e0, 0x00e000e0);
	Mask565FixG  = createMask_2x32_128(0x0000c000, 0x0000c000);

	y_step = 0;
	for (int i = 0; i < dstH; i++) {
		x_step = 0;
		int indexBase = (int)y_step * srcW;
		for (int j = 0;j < dstW; j++) {
			index = indexBase + (int)x_step;

			__m128i p12 = _mm_loadl_epi64((const __m128i *)&srcPtr[index]);
			__m128i p34 = _mm_loadl_epi64((const __m128i *)&srcPtr[index + srcW]);

			p12 = unpack565to8888(_mm_unpacklo_epi16(p12, _mm_setzero_si128()));
			p34 = unpack565to8888(_mm_unpacklo_epi16(p34, _mm_setzero_si128()));

			__m128 weight = CalcWeights(x_step, y_step);
#if defined(__SSE4_1__)
// compile with -msse4.1

			// convert RGBA RGBA RGBA RGAB to RRRR GGGG BBBB AAAA (AoS to SoA)
			__m128i p1234 = _mm_unpacklo_epi8(p12, p34);
			__m128i p34xx = _mm_unpackhi_epi64(p1234, _mm_setzero_si128());
			__m128i p1234_8bit = _mm_unpacklo_epi8(p1234, p34xx);

			// extend to 16bit
			__m128i pRG = _mm_unpacklo_epi8(p1234_8bit, _mm_setzero_si128());
			__m128i pBA = _mm_unpackhi_epi8(p1234_8bit, _mm_setzero_si128());

			// convert weights to integer
			weight = _mm_mul_ps(weight, CONST_256);
			__m128i weighti = _mm_cvtps_epi32(weight); // w4 w3 w2 w1
			weighti = _mm_packs_epi32(weighti, weighti); // 32->2x16bit

			//outRG = [w1*R1 + w2*R2 | w3*R3 + w4*R4 | w1*G1 + w2*G2 | w3*G3 + w4*G4]
			__m128i outRG = _mm_madd_epi16(pRG, weighti);
			//outBA = [w1*B1 + w2*B2 | w3*B3 + w4*B4 | w1*A1 + w2*A2 | w3*A3 + w4*A4]
			__m128i outBA = _mm_madd_epi16(pBA, weighti);

			// horizontal add that will produce the output values (in 32bit)
			__m128i out = _mm_hadd_epi32(outRG, outBA);
			out = _mm_srli_epi32(out, 8); // divide by 256

			// convert 32bit->8bit
			out = _mm_packus_epi32(out, _mm_setzero_si128());
			out = _mm_packus_epi16(out, _mm_setzero_si128());

			int pixel = _mm_cvtsi128_si32(out);
#else
			// extend to 16bit
			p12 = _mm_unpacklo_epi8(p12, _mm_setzero_si128());
			p34 = _mm_unpacklo_epi8(p34, _mm_setzero_si128());

			// convert floating point weights to 16bit integer
			weight = _mm_mul_ps(weight, CONST_256);
			__m128i weighti = _mm_cvtps_epi32(weight); // w4 w3 w2 w1
			weighti = _mm_packs_epi32(weighti, _mm_setzero_si128()); // 32->16bit

			// prepare the weights
			__m128i w12 = _mm_shufflelo_epi16(weighti, _MM_SHUFFLE(1, 1, 0, 0));
			__m128i w34 = _mm_shufflelo_epi16(weighti, _MM_SHUFFLE(3, 3, 2, 2));
			w12 = _mm_unpacklo_epi16(w12, w12); // w2 w2 w2 w2 w1 w1 w1 w1
			w34 = _mm_unpacklo_epi16(w34, w34); // w4 w4 w4 w4 w3 w3 w3 w3

			// multiply each pixel with its weight (2 pixel per SSE mul)
			__m128i L12 = _mm_mullo_epi16(p12, w12);
			__m128i L34 = _mm_mullo_epi16(p34, w34);

			// sum the results
			__m128i L1234 = _mm_add_epi16(L12, L34);
			__m128i Lhi = _mm_shuffle_epi32(L1234, _MM_SHUFFLE(3, 2, 3, 2));
			__m128i L = _mm_add_epi16(L1234, Lhi);

			// convert back to 8bit
			__m128i L8 = _mm_srli_epi16(L, 8); // divide by 256
			L8 = _mm_packus_epi16(L8, _mm_setzero_si128());

			int pixel = _mm_cvtsi128_si32(L8);
#endif

// 565
//			int red   = (pixel >> 16) & 0xFF;
//			int green = (pixel >> 8)  & 0xFF;
//			int blue  = (pixel >> 0)  & 0xFF;
//			dstPtr[offset++] = ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3);

#if defined(MACOSX)
			dstPtr[offset++] = SWAP_CONSTANT_32(pixel);
#else
			int red   = (pixel >> 16) & 0xFF;
			int green = (pixel >> 8)  & 0xFF;
			int blue  = (pixel >> 0)  & 0xFF;
			dstPtr[offset++] = dstFmt.RGBToColor(red, green, blue);
#endif

			x_step += x_ratio;
		}
		y_step += y_ratio;
	}
}

// Based on http://tech-algorithm.com/articles/nearest-neighbor-image-scaling/
static void BlitNearestScaler(uint16 *dstPtr, int dstW, int dstH, uint16 *srcPtr, int srcW, int srcH) {
	int x_ratio = (int)((srcW << 16) / dstW) + 1;
	int y_ratio = (int)((srcH << 16) / dstH) + 1;
	int x2, y2;

	for (int i = 0;i < dstH;i++) {
		for (int j = 0;j < dstW;j++) {
			x2 = ((j * x_ratio) >> 16);
			y2 = ((i * y_ratio) >> 16);
			dstPtr[(i * dstW) + j] = srcPtr[(y2 * srcW) + x2];
		}
	}
}

void SurfaceSdlGraphicsManager::updateScreen() {
#ifdef USE_OPENGL
	if (_opengl) {
		if (_overlayVisible) {
			if (_overlayDirty) {
				updateOverlayTextures();
			}

#ifndef USE_OPENGL_SHADERS
			drawOverlayOpenGL();
#else
			drawOverlayOpenGLShaders();
#endif
		}
		SDL_GL_SwapBuffers();
	} else
#endif
	{
		if (_overlayVisible) {
			drawOverlay();
		}
		SDL_LockSurface(_screen);
		SDL_LockSurface(_targetScreen);
#if defined(ENABLE_BILINEAR)
//		BlitBilinearScalerFloat((uint32 *)_targetScreen->pixels, _targetScreen->w, _targetScreen->h, _targetScreenFormat,
//				(uint16 *)_screen->pixels, _screen->w, _screen->h);
//		BlitBilinearScalerInteger((uint32 *)_targetScreen->pixels, _targetScreen->w, _targetScreen->h, _targetScreenFormat,
//				(uint16 *)_screen->pixels, _screen->w, _screen->h);
		BlitBilinearScalerSSE((uint32 *)_targetScreen->pixels, _targetScreen->w, _targetScreen->h, _targetScreenFormat,
				(uint16 *)_screen->pixels, _screen->w, _screen->h);
#else
		BlitNearestScaler((uint16 *)_targetScreen->pixels, _targetScreen->w, _targetScreen->h,
				(uint16 *)_screen->pixels, _screen->w, _screen->h);
#endif
		SDL_UnlockSurface(_targetScreen);
		SDL_UnlockSurface(_screen);
		SDL_Flip(_targetScreen);
	}
}

void SurfaceSdlGraphicsManager::copyRectToScreen(const void *src, int pitch, int x, int y, int w, int h) {
	// ResidualVM: not use it
}

Graphics::Surface *SurfaceSdlGraphicsManager::lockScreen() {
	return NULL; // ResidualVM: not use it
}

void SurfaceSdlGraphicsManager::unlockScreen() {
	// ResidualVM: not use it
}

void SurfaceSdlGraphicsManager::fillScreen(uint32 col) {
	// ResidualVM: not use it
}

int16 SurfaceSdlGraphicsManager::getHeight() {
	// ResidualVM specific
	return _screen->h;
}

int16 SurfaceSdlGraphicsManager::getWidth() {
	// ResidualVM specific
	return _screen->w;
}

void SurfaceSdlGraphicsManager::setPalette(const byte *colors, uint start, uint num) {
	// ResidualVM: not use it
}

void SurfaceSdlGraphicsManager::grabPalette(byte *colors, uint start, uint num) {
	// ResidualVM: not use it
}

void SurfaceSdlGraphicsManager::setCursorPalette(const byte *colors, uint start, uint num) {
	// ResidualVM: not use it
}

void SurfaceSdlGraphicsManager::setShakePos(int shake_pos) {
	// ResidualVM: not use it
}

void SurfaceSdlGraphicsManager::setFocusRectangle(const Common::Rect &rect) {
	// ResidualVM: not use it
}

void SurfaceSdlGraphicsManager::clearFocusRectangle() {
	// ResidualVM: not use it
}

#pragma mark -
#pragma mark --- Overlays ---
#pragma mark -

void SurfaceSdlGraphicsManager::showOverlay() {
	if (_overlayVisible)
		return;

	_overlayVisible = true;

	clearOverlay();
}

void SurfaceSdlGraphicsManager::hideOverlay() {
	if (!_overlayVisible)
		return;

	_overlayVisible = false;

	clearOverlay();
}

void SurfaceSdlGraphicsManager::clearOverlay() {
	if (!_overlayscreen)
		return;

	if (!_overlayVisible)
		return;

#ifdef USE_OPENGL
	if (_opengl) {
		SDL_Surface *tmp = SDL_CreateRGBSurface(SDL_SWSURFACE, _overlayWidth, _overlayHeight,
				_overlayscreen->format->BytesPerPixel * 8,
				_overlayscreen->format->Rmask, _overlayscreen->format->Gmask,
				_overlayscreen->format->Bmask, _overlayscreen->format->Amask);

		SDL_LockSurface(tmp);
		SDL_LockSurface(_overlayscreen);

		glReadPixels(0, 0, _overlayWidth, _overlayHeight, GL_RGB, _overlayScreenGLFormat, tmp->pixels);

		// Flip pixels vertically
		byte *src = (byte *)tmp->pixels;
		byte *buf = (byte *)_overlayscreen->pixels + (_overlayHeight - 1) * _overlayscreen->pitch;
		int h = _overlayHeight;
		do {
			memcpy(buf, src, _overlayWidth * _overlayscreen->format->BytesPerPixel);
			src += tmp->pitch;
			buf -= _overlayscreen->pitch;
		} while (--h);

		SDL_UnlockSurface(_overlayscreen);
		SDL_UnlockSurface(tmp);

		SDL_FreeSurface(tmp);
	} else
#endif
	{
		SDL_LockSurface(_screen);
		SDL_LockSurface(_overlayscreen);
		Graphics::PixelBuffer srcBuf(_screenFormat, (byte *)_screen->pixels);
		Graphics::PixelBuffer dstBuf(_overlayFormat, (byte *)_overlayscreen->pixels);
		int h = _overlayHeight;

		do {
			dstBuf.copyBuffer(0, _overlayWidth, srcBuf);

			srcBuf.shiftBy(_overlayWidth);
			dstBuf.shiftBy(_overlayWidth);
		} while (--h);
		SDL_UnlockSurface(_screen);
		SDL_UnlockSurface(_overlayscreen);
	}
	_overlayDirty = true;
}

void SurfaceSdlGraphicsManager::grabOverlay(void *buf, int pitch) {
	if (_overlayscreen == NULL)
		return;

	if (SDL_LockSurface(_overlayscreen) == -1)
		error("SDL_LockSurface failed: %s", SDL_GetError());

	byte *src = (byte *)_overlayscreen->pixels;
	byte *dst = (byte *)buf;
	int h = _overlayHeight;
	do {
		memcpy(dst, src, _overlayWidth * _overlayscreen->format->BytesPerPixel);
		src += _overlayscreen->pitch;
		dst += pitch;
	} while (--h);

	SDL_UnlockSurface(_overlayscreen);
}

void SurfaceSdlGraphicsManager::copyRectToOverlay(const void *buf, int pitch, int x, int y, int w, int h) {
	if (_overlayscreen == NULL)
		return;

	const byte *src = (const byte *)buf;

	// Clip the coordinates
	if (x < 0) {
		w += x;
		src -= x * _overlayscreen->format->BytesPerPixel;
		x = 0;
	}

	if (y < 0) {
		h += y;
		src -= y * pitch;
		y = 0;
	}

	if (w > _overlayWidth - x) {
		w = _overlayWidth - x;
	}

	if (h > _overlayHeight - y) {
		h = _overlayHeight - y;
	}

	if (w <= 0 || h <= 0)
		return;

	if (SDL_LockSurface(_overlayscreen) == -1)
		error("SDL_LockSurface failed: %s", SDL_GetError());

	byte *dst = (byte *)_overlayscreen->pixels + y * _overlayscreen->pitch + x * _overlayscreen->format->BytesPerPixel;
	do {
		memcpy(dst, src, w * _overlayscreen->format->BytesPerPixel);
		dst += _overlayscreen->pitch;
		src += pitch;
	} while (--h);

	SDL_UnlockSurface(_overlayscreen);
}

void SurfaceSdlGraphicsManager::closeOverlay() {
	if (_overlayscreen) {
		SDL_FreeSurface(_overlayscreen);
		_overlayscreen = NULL;
#ifdef USE_OPENGL
		if (_opengl) {
			if (_overlayNumTex > 0) {
				glDeleteTextures(_overlayNumTex, _overlayTexIds);
				delete[] _overlayTexIds;
				_overlayNumTex = 0;
			}

#ifdef USE_OPENGL_SHADERS
			glDeleteBuffers(1, &_boxVerticesVBO);
			_boxVerticesVBO = 0;

			delete _boxShader;
			_boxShader = nullptr;
#endif
		}
#endif
	}
}

#pragma mark -
#pragma mark --- Mouse ---
#pragma mark -

bool SurfaceSdlGraphicsManager::showMouse(bool visible) {
	SDL_ShowCursor(visible);
	return true;
}

// ResidualVM specific method
bool SurfaceSdlGraphicsManager::lockMouse(bool lock) {
	if (lock)
		SDL_WM_GrabInput(SDL_GRAB_ON);
	else
		SDL_WM_GrabInput(SDL_GRAB_OFF);
	return true;
}

void SurfaceSdlGraphicsManager::warpMouse(int x, int y) {
	//ResidualVM specific
	SDL_WarpMouse(x, y);
}

void SurfaceSdlGraphicsManager::setMouseCursor(const void *buf, uint w, uint h, int hotspot_x, int hotspot_y, uint32 keycolor, bool dontScale, const Graphics::PixelFormat *format) {
	// ResidualVM: not use it
}

#pragma mark -
#pragma mark --- On Screen Display ---
#pragma mark -

#ifdef USE_OSD
void SurfaceSdlGraphicsManager::displayMessageOnOSD(const char *msg) {
	// ResidualVM: not use it
}
#endif


#ifdef USE_OPENGL
void SurfaceSdlGraphicsManager::setAntialiasing(bool enable) {
	// Antialiasing works without setting MULTISAMPLEBUFFERS, but as SDL's official
	// tests set both values, this seems to be the standard way to do it. It could
	// just be that in current OpenGL implementations setting SDL_GL_MULTISAMPLESAMPLES
	// implicitly sets SDL_GL_MULTISAMPLEBUFFERS as well.
	if (_antialiasing && enable) {
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, _antialiasing);
	} else {
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
	}
}
#endif

bool SurfaceSdlGraphicsManager::notifyEvent(const Common::Event &event) {
	//ResidualVM specific:
	switch ((int)event.type) {
	case Common::EVENT_KEYDOWN:
		break;
	case Common::EVENT_KEYUP:
		break;
	default:
		break;
	}

	return false;
}

void SurfaceSdlGraphicsManager::notifyVideoExpose() {
	_forceFull = true;
	//ResidualVM specific:
	updateScreen();
}

void SurfaceSdlGraphicsManager::transformMouseCoordinates(Common::Point &point) {
	return; // ResidualVM: not use it
}

void SurfaceSdlGraphicsManager::notifyMousePos(Common::Point mouse) {
	transformMouseCoordinates(mouse);
	// ResidualVM: not use that:
	//setMousePos(mouse.x, mouse.y);
}

#endif
