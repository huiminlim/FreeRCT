/*
 * This file is part of FreeRCT.
 * FreeRCT is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * FreeRCT is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with FreeRCT. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file video.h Graphics system handling. */

#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "stdafx.h"
#include "geometry.h"
#include "palette.h"

#include <chrono>
#include <map>
#include <set>
#include <string>

#include <GLFW/glfw3.h>

struct FontGlyph;
class ImageData;

using Realtime = std::chrono::high_resolution_clock::time_point;  ///< Represents a time point in real time.
using Duration = std::chrono::duration<double, std::milli>;       ///< Difference between two time points with millisecond precision.

/**
 * Get the current real time.
 * @return The time.
 */
inline Realtime Time()
{
	return std::chrono::high_resolution_clock::now();
}

/**
 * Get the time difference between two time points in milliseconds.
 * @return The time difference.
 */
inline double Delta(const Realtime &start, const Realtime &end = Time())
{
	Duration d = end - start;
	return d.count();
}

/**
 * Convert a 32-bit integer WXYZ colour to an OpenGL colour vector.
 * @param c Input colour.
 * @return OpenGL colour.
 */
inline WXYZPointF HexToColourWXYZ(uint32_t c)
{
	return WXYZPointF(((c & 0xff0000) >> 16) / 255.f, ((c & 0xff00) >> 8) / 255.f, (c & 0xff) / 255.f,
			((c & 0xff000000) >> 24) / 255.f);
}

/**
 * Convert a 24-bit integer RGB colour to an OpenGL colour vector.
 * @param c Input colour.
 * @return OpenGL colour.
 */
inline XYZPointF HexToColourRGB(uint32_t c)
{
	return XYZPointF(((c & 0xff0000) >> 16) / 255.f, ((c & 0xff00) >> 8) / 255.f, (c & 0xff) / 255.f);
}

/** Class responsible for rendering text. */
class TextRenderer {
public:
	void Initialize();

	/**
	 * Get the font size.
	 * @return Size of the font.
	 */
	GLuint GetFontSize() const
	{
		return this->font_size;
	}

	void LoadFont(const std::string &font_path, GLuint font_size);
	void Draw(const std::string &text, float x, float y, const XYZPointF &colour, float scale = 1.0f);
	PointF EstimateBounds(const std::string &text, float scale = 1.0f) const;

	const FontGlyph &GetFontGlyph(const char **text, size_t &length) const;

private:
	std::map<uint32, FontGlyph> characters;  ///< All character glyphs in the current font by their unicode codepoint.
	GLuint font_size;                        ///< Current font size.
	GLuint shader;                           ///< The font shader.
	GLuint vao;                              ///< The OpenGL vertex array.
	GLuint vbo;                              ///< The OpenGL vertex buffer.
};

extern TextRenderer _text_renderer;

/** How to align text during drawing. */
enum Alignment {
	ALG_LEFT,    ///< Align to the left edge.
	ALG_CENTER,  ///< Centre the text.
	ALG_RIGHT,   ///< Align to the right edge.
};

/** Class providing the interface to the OpenGL rendering backend. */
class VideoSystem {
public:
	void Initialize(const std::string &font, int font_size);

	static void MainLoopCycle();
	void MainLoop();
	void Shutdown();

	double FPS() const;

	/**
	 * Get the current width of the window.
	 * @return The width in pixels.
	 */
	float Width() const
	{
		return this->width;
	}

	/**
	 * Get the current height of the window.
	 * @return The height in pixels.
	 */
	float Height() const
	{
		return this->height;
	}

	/**
	 * Get the current mouse position.
	 * @return The mouse X coordinate.
	 */
	float MouseX() const
	{
		return this->mouse_x;
	}

	/**
	 * Get the current mouse position.
	 * @return The mouse Y coordinate.
	 */
	float MouseY() const
	{
		return this->mouse_y;
	}

	void SetResolution(const Point32 &res);

	/**
	 * List all available window resolutions.
	 * @return The resolutions.
	 */
	const std::set<Point32> &Resolutions() const
	{
		return this->resolutions;
	}

	void CoordsToGL(float *x, float *y) const;
	void CoordsToGL(double *x, double *y) const;

	GLuint LoadShader(const std::string &name);

	/**
	 * Get the height of a line of text.
	 * @return Height of the font.
	 */
	inline int GetTextHeight() const
	{
		return _text_renderer.GetFontSize();
	}

	void BlitText(const std::string &text, uint32 colour, int xpos, int ypos, int width = 0x7FFF, Alignment align = ALG_LEFT);
	void GetTextSize(const std::string &text, int *width, int *height);
	void GetNumberRangeSize(int64 smallest, int64 biggest, int *width, int *height);

	/**
	 * Draw a line from \a start to \a end using the specified \a colour.
	 * @param start Starting point at the screen.
	 * @param end End point at the screen.
	 * @param colour Colour to use.
	 */
	inline void DrawLine(const Point16 &start, const Point16 &end, uint32 colour)
	{
		this->DrawLine(start.x, start.y, end.x, end.y, HexToColourWXYZ(colour));
	}

	/**
	 * Draw the outline of a rectangle at the screen.
	 * @param rect %Rectangle to draw.
	 * @param colour Colour to use.
	 */
	inline void DrawRectangle(const Rectangle32 &rect, uint32 colour)
	{
		WXYZPointF col = HexToColourWXYZ(colour);
		this->DrawLine(rect.base.x, rect.base.y, rect.base.x + rect.width, rect.base.y, col);
		this->DrawLine(rect.base.x, rect.base.y, rect.base.x, rect.base.y + rect.height, col);
		this->DrawLine(rect.base.x + rect.width, rect.base.y + rect.height, rect.base.x + rect.width, rect.base.y, col);
		this->DrawLine(rect.base.x + rect.width, rect.base.y + rect.height, rect.base.x, rect.base.y + rect.height, col);
	}

	/**
	 * Paint a rectangle at the screen.
	 * @param rect %Rectangle to draw.
	 * @param colour Colour to use.
	 */
	inline void FillRectangle(const Rectangle32 &rect, uint32 colour)
	{
		this->FillPlainColour(rect.base.x + rect.width, rect.base.y + rect.height, rect.base.x + rect.width, rect.base.y + rect.height, HexToColourWXYZ(colour));
	}

	void FillPlainColour(float x, float y, float w, float h, const WXYZPointF &colour);
	void DrawLine(float x1, float y1, float x2, float y2, const WXYZPointF &colour);
	void DrawPlainColours(const std::vector<Point<float>> &points, const WXYZPointF &colour);

	void DrawImage(const ImageData *img, const Point32 &pos, const WXYZPointF &col = WXYZPointF(1.0f, 1.0f, 1.0f, 1.0f));
	void TileImage(const ImageData *img, const Rectangle32 &rect, const WXYZPointF &col = WXYZPointF(1.0f, 1.0f, 1.0f, 1.0f));

	void BlitImages(const Point32 &pt, const ImageData *spr, uint16 numx, uint16 numy, const Recolouring &recolour, GradientShift shift = GS_NORMAL);

	/**
	 * Blit a row of sprites.
	 * @param xmin Start X position.
	 * @param numx Number of sprites to draw.
	 * @param y Y position.
	 * @param spr Sprite to draw.
	 * @param recolour Sprite recolouring definition.
	 */
	inline void BlitHorizontal(int32 xmin, uint16 numx, int32 y, const ImageData *spr, const Recolouring &recolour)
	{
		this->BlitImages({xmin, y}, spr, numx, 1, recolour);
	}

	/**
	 * Blit a column of sprites.
	 * @param ymin Start Y position.
	 * @param numy Number of sprites to draw.
	 * @param x X position.
	 * @param spr Sprite to draw.
	 * @param recolour Sprite recolouring definition.
	 */
	inline void BlitVertical(int32 ymin, uint16 numy, int32 x, const ImageData *spr, const Recolouring &recolour)
	{
		this->BlitImages({x, ymin}, spr, 1, numy, recolour);
	}

	/**
	 * Blit pixels from the \a spr relative to #blit_rect into the area.
	 * @param img_base Coordinate of the sprite data.
	 * @param spr The sprite to blit.
	 * @param recolour Sprite recolouring definition.
	 * @param shift Gradient shift.
	 */
	inline void BlitImage(const Point32 &img_base, const ImageData *spr, const Recolouring &recolour, GradientShift shift)
	{
		this->BlitImages(img_base, spr, 1, 1, recolour, shift);
	}

	void PushClip(const Rectangle32 &rect);
	void PopClip();

	void FinishRepaint();

private:
	bool MainLoopDoCycle();

	GLuint LoadShaders(const char *vp, const char *fp);

	void EnsureImageLoaded(const ImageData *img);
	void DoDrawImage(GLuint texture, float x1, float y1, float x2, float y2,
			const WXYZPointF &col = WXYZPointF(1.0f, 1.0f, 1.0f, 1.0f), const WXYZPointF &tex = WXYZPointF(0.0f, 0.0f, 1.0f, 1.0f));

	static void FramebufferSizeCallback(GLFWwindow *window, int w, int h);
	static void MouseClickCallback(GLFWwindow *window, int button, int action, int mods);
	static void MouseMoveCallback(GLFWwindow *window, double x, double y);
	static void ScrollCallback(GLFWwindow *window, double xdelta, double ydelta);
	static void KeyCallback(GLFWwindow *window, int key, int scancode, int action, int mods);
	static void TextCallback(GLFWwindow *window, uint32 codepoint);

	uint32_t width;   ///< Current window width in pixels.
	uint32_t height;  ///< Current window height in pixels.
	double mouse_x;   ///< Current mouse X position.
	double mouse_y;   ///< Current mouse Y position.

	std::set<Point32> resolutions;  ///< Available window resolutions.

	Realtime last_frame;  ///< Time when the last frame started.
	Realtime cur_frame;   ///< Time when the current frame started.

	std::map<const ImageData*, GLuint> image_textures;  ///< Textures for all loaded images.

	GLuint image_shader;   ///< Shader for images.
	GLuint colour_shader;  ///< Shader for plain colours.
	GLuint vao;            ///< The OpenGL vertex array.
	GLuint vbo;            ///< The OpenGL vertex buffer.
	GLuint ebo;            ///< The OpenGL element buffer.

	std::vector<Rectangle32> clip;  ///< Current clipping area stack.

	GLFWwindow *window;  ///< The GLFW window.
};

extern uint32 _icon_data[32][32];

extern VideoSystem _video;

#endif
