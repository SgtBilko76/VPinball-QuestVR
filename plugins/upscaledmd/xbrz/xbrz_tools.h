// ****************************************************************************
// * This file is part of the xBRZ project. It is distributed under           *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0         *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved          *
// *                                                                          *
// * Additionally and as a special exception, the author gives permission     *
// * to link the code of this program with the following libraries            *
// * (or with modified versions that use the same licenses), and distribute   *
// * linked combinations including the two: MAME, FreeFileSync, Snes9x, ePSXe *
// *                                                                          *
// * You must obey the GNU General Public License in all respects for all of  *
// * the code used other than MAME, FreeFileSync, Snes9x, ePSXe.              *
// * If you modify this file, you may extend this exception to your version   *
// * of the file, but you are not obligated to do so. If you do not wish to   *
// * do so, delete this exception statement from your version.                *
// ****************************************************************************

#ifndef XBRZ_TOOLS_H_825480175091875
#define XBRZ_TOOLS_H_825480175091875

#include <cassert>
#include <vector>
#include <algorithm>
#include <type_traits>


namespace xbrz
{
template <uint32_t N>
inline unsigned char getByte(uint32_t val)        { return static_cast<unsigned char>((val >> (8 * N)) & 0xff); }
inline unsigned char getByte(uint32_t val, int n) { return static_cast<unsigned char>((val >> (8 * n)) & 0xff); }

FORCE_INLINE unsigned char getAlpha(uint32_t pix) { return getByte<3>(pix); }
FORCE_INLINE unsigned char getRed  (uint32_t pix) { return getByte<2>(pix); }
FORCE_INLINE unsigned char getGreen(uint32_t pix) { return getByte<1>(pix); }
FORCE_INLINE unsigned char getBlue (uint32_t pix) { return getByte<0>(pix); }

inline uint32_t makePixel(uint32_t a, uint32_t r, uint32_t g, uint32_t b) { return (a << 24) | (r << 16) | (g << 8) | b; }
inline uint32_t makePixel(            uint32_t r, uint32_t g, uint32_t b) { return             (r << 16) | (g << 8) | b; }

FORCE_INLINE uint32_t rgb555to888(uint16_t pix) { return ((pix & 0x7C00) << 9) | ((pix & 0x03E0) << 6) | ((pix & 0x001F) << 3); }
FORCE_INLINE uint32_t rgb565to888(uint16_t pix) { return ((pix & 0xF800) << 8) | ((pix & 0x07E0) << 5) | ((pix & 0x001F) << 3); }

FORCE_INLINE uint16_t rgb888to555(uint32_t pix) { return static_cast<uint16_t>(((pix & 0xF80000) >> 9) | ((pix & 0x00F800) >> 6) | ((pix & 0x0000F8) >> 3)); }
FORCE_INLINE uint16_t rgb888to565(uint32_t pix) { return static_cast<uint16_t>(((pix & 0xF80000) >> 8) | ((pix & 0x00FC00) >> 5) | ((pix & 0x0000F8) >> 3)); }


template <class PixReader, class PixWriter> inline
void unscaledCopy(PixReader pixRead /* (int offs) -> uint32_t */,
                  PixWriter pixWrite /* (uint32_t pix) */, const int width, const int height)
{
    for (int offs = 0; offs < width * height; ++offs)
        pixWrite(pixRead(offs));
}


//nearest-neighbor (going over target image - slow for upscaling, since source is read multiple times missing out on cache! Fast for similar image sizes!)
template <class PixReader, class PixWriter>
void nearestNeighborScale(PixReader pixRead /* (int offs) -> uint32_t */, const int srcWidth, const int srcHeight,
                          PixWriter pixWrite /* (uint32_t pix)         */, const int trgWidth, const int trgHeight,
                          int yFirst, int yLast)
{
    yFirst = std::max(yFirst, 0);
    yLast  = std::min(yLast, trgHeight);
    if (yFirst >= yLast || srcHeight <= 0 || srcWidth <= 0) return;

    for (int y = yFirst; y < yLast; ++y)
    {
        const int ySrcOff = (srcHeight * y / trgHeight) * srcWidth;

        for (int x = 0; x < trgWidth*srcWidth; x += srcWidth)
        {
            const int xSrc = x / trgWidth;
            pixWrite(pixRead(ySrcOff + xSrc));
        }
    }
}


inline
unsigned char byteRound(float v) //std::round is prohibitively expensive!
{
    //assert(v >= 0);
    return static_cast<unsigned char>(std::min(v + 0.5f, 255.0f));
}


#if 0
inline
unsigned char byteCeil(float v)
{
    //assert(v >= 0);
    if (v >= 255.0f) return 255;
    unsigned char i = static_cast<unsigned char>(v);
    return v == i ? i : i + 1;
}
#endif


inline
unsigned int uintDivRound(unsigned int num, unsigned int den)
{
    assert(den != 0);
    return (num + den / 2) / den;
}


//caveat: treats alpha channel like regular color! => caller needs to pre/de-multiply alpha!
template <class PixReader, class PixWriter>
void bilinearScale(PixReader pixRead /* (int offs) -> Function<value(int channel)>       */, int srcWidth, int srcHeight,
                   PixWriter pixWrite /* ( const Function<value(int channel)>& interpolate ) */, int trgWidth, int trgHeight,
                   int yFirst, int yLast)
{
    yFirst = std::max(yFirst, 0);
    yLast  = std::min(yLast, trgHeight);
    if (yFirst >= yLast || srcHeight <= 0 || srcWidth <= 0)
        return;

    const float scaleX = static_cast<float>(trgWidth ) / static_cast<float>(srcWidth );
    const float scaleY = static_cast<float>(trgHeight) / static_cast<float>(srcHeight);

    //perf notes:
    //    -> double-based calculation is (slightly) faster than float
    //    -> pre-calculation gives significant boost; std::vector<> memory allocation is negligible!
    struct CoeffsX
    {
        int     x1 = 0;
        int     x2 = 0;
        float  xx1 = 0;
        float  x2x = 0;
    };
    std::vector<CoeffsX> buf(trgWidth);
    for (int x = 0; x < trgWidth; ++x)
    {
        const int x1 = srcWidth * x / trgWidth;
        int x2 = x1 + 1;
        if (x2 == srcWidth)
            --x2;

        const float xx1 = x / scaleX - x1;
        const float x2x = 1.f - xx1;

        buf[x] = {x1, x2, xx1, x2x};
    }

    for (int y = yFirst; y < yLast; ++y)
    {
        const int y1 = srcHeight * y / trgHeight;
        int y2 = y1 + 1;
        if (y2 == srcHeight)
            --y2;

        const float yy1 = y / scaleY - y1;
        const float y2y = 1.f - yy1;

        const int y1Off = y1 * srcWidth;
        const int y2Off = y2 * srcWidth;

        for (int x = 0; x < trgWidth; ++x)
        {
            //perf: do NOT "simplify" the variable layout without measurement!
            const CoeffsX& bufX = buf[x];
            const int     x1 = bufX.x1;
            const int     x2 = bufX.x2;
            const float xx1 = bufX.xx1;
            const float x2x = bufX.x2x;

            const float  x2xy2y = x2x * y2y;
            const float  xx1y2y = xx1 * y2y;
            const float  x2xyy1 = x2x * yy1;
            const float  xx1yy1 = xx1 * yy1;

            const auto pix11 = pixRead(y1Off + x1);
            const auto pix21 = pixRead(y1Off + x2);
            const auto pix12 = pixRead(y2Off + x1);
            const auto pix22 = pixRead(y2Off + x2);

            auto interpolate = [&](int channel)
            {
                /* https://en.wikipedia.org/wiki/Bilinear_interpolation
                     (c11(x2 - x) + c21(x - x1)) * (y2 - y ) +
                     (c12(x2 - x) + c22(x - x1)) * (y  - y1)      */
                return (float)pix11(channel) * x2xy2y + (float)pix21(channel) * xx1y2y +
                       (float)pix12(channel) * x2xyy1 + (float)pix22(channel) * xx1yy1;
            };
            pixWrite(std::move(interpolate));
        }
    }
}


#if 0
//nearest-neighbor (going over source image - fast for upscaling, since source is read only once
template <class PixSrc, class PixTrg, class PixConverter>
void nearestNeighborScaleOverSource(const PixSrc* src, int srcWidth, int srcHeight, int srcPitch /*[bytes]*/,
                                    /**/  PixTrg* trg, int trgWidth, int trgHeight, int trgPitch /*[bytes]*/,
                                    int yFirst, int yLast, PixConverter pixCvrt /*convert PixSrc to PixTrg*/)
{
    static_assert(std::is_integral_v<PixSrc>, "PixSrc* is expected to be cast-able to char*");
    static_assert(std::is_integral_v<PixTrg>, "PixTrg* is expected to be cast-able to char*");

    static_assert(std::is_same_v<decltype(pixCvrt(PixSrc())), PixTrg>, "PixConverter returning wrong pixel format");

    if (srcPitch < srcWidth * static_cast<int>(sizeof(PixSrc))  ||
        trgPitch < trgWidth * static_cast<int>(sizeof(PixTrg)))
    {
        assert(false);
        return;
    }

    yFirst = std::max(yFirst, 0);
    yLast  = std::min(yLast, srcHeight);
    if (yFirst >= yLast || trgWidth <= 0 || trgHeight <= 0) return;

    for (int y = yFirst; y < yLast; ++y)
    {
        //mathematically: ySrc = floor(srcHeight * yTrg / trgHeight)
        // => search for integers in: [ySrc, ySrc + 1) * trgHeight / srcHeight

        //keep within for loop to support MT input slices!
        const int yTrgFirst = ( y      * trgHeight + srcHeight - 1) / srcHeight; //=ceil(y * trgHeight / srcHeight)
        const int yTrgLast  = ((y + 1) * trgHeight + srcHeight - 1) / srcHeight; //=ceil(((y + 1) * trgHeight) / srcHeight)
        const int blockHeight = yTrgLast - yTrgFirst;

        if (blockHeight > 0)
        {
            const PixSrc* srcLine = byteAdvance(src, y         * srcPitch);
            /**/  PixTrg* trgLine = byteAdvance(trg, yTrgFirst * trgPitch);
            int xTrgFirst = 0;

            for (int x = 0; x < srcWidth; ++x)
            {
                const int xTrgLast = ((x + 1) * trgWidth + srcWidth - 1) / srcWidth;
                const int blockWidth = xTrgLast - xTrgFirst;
                if (blockWidth > 0)
                {
                    xTrgFirst = xTrgLast;

                    const auto trgPix = pixCvrt(srcLine[x]);
                    fillBlock(trgLine, trgPitch, trgPix, blockWidth, blockHeight);
                    trgLine += blockWidth;
                }
            }
        }
    }
}
#endif
}

#endif //XBRZ_TOOLS_H_825480175091875
