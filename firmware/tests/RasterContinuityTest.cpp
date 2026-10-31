#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include "../boards/175/MotoNav/RasterContinuity.h"

static uint8_t getPx(const uint8_t *buf, uint16_t w, int x, int y) {
    uint32_t a = (((uint32_t)(y >> 4) * ((w + 15) / 16) + (x >> 4)) << 8) + ((y & 15) << 4) + (x & 15);
    return (buf[a >> 1] >> ((a & 1) << 2)) & 15;
}

static std::vector<uint8_t> packet(const uint8_t *rle, size_t size, int w, int h, int ox=0, int oy=0, bool delta=false) {
    std::vector<uint8_t> out;
    auto u16=[&](int v){out.push_back(v&255);out.push_back((v>>8)&255);};
    auto i32=[&](int v){for(int i=0;i<4;i++)out.push_back((uint32_t(v)>>(8*i))&255);};
    u16(1);i32(ox*2);i32(-oy*2);u16(w);u16(h);
    u16(2000 | RASTER_FLAG_BUILDINGS | (delta ? RASTER_FLAG_DELTA : 0));
    if(delta){i32(0);i32(0);}
    out.insert(out.end(),rle,rle+size);
    return out;
}

static size_t encodeRle(const uint8_t *grid, int n, uint8_t *out) {
    size_t p = 0;
    int i = 0;
    while (i < n) {
        uint8_t c = grid[i] & 15;
        int run = 1;
        while (i + run < n && run < 271 && (grid[i + run] & 15) == c) run++;
        if (run <= 15) {
            out[p++] = (uint8_t)(((run - 1) << 4) | c);
        } else {
            out[p++] = (uint8_t)((15 << 4) | c);
            out[p++] = (uint8_t)(run - 16);
        }
        i += run;
    }
    return p;
}

int main() {
    const uint16_t W = 64, H = 64;
    const int N = W * H;
    uint8_t gridA[N], gridB[N], xorGrid[N];
    uint8_t rleA[N * 2], rleDelta[N * 2];
    uint8_t bufA[N / 2], bufB[N / 2];

    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            gridA[y * W + x] = (uint8_t)((x * 3 + y * 5) & 7);
        }
    }
    size_t lenA = encodeRle(gridA, N, rleA);
    auto a = packet(rleA,lenA,W,H);
    RasterHeader full={};
    assert(rasterCheck(a.data(),a.size(),full,N,sizeof(bufA)));
    rasterDecode(a.data(),full,bufA);
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            assert(getPx(bufA, W, x, y) == gridA[y * W + x]);
        }
    }

    int shifts[3][2] = {{0, 0}, {16, -16}, {7, -11}};
    for (int s = 0; s < 3; s++) {
        int ox = shifts[s][0], oy = shifts[s][1];
        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                int sx = x + ox, sy = y + oy;
                uint8_t base = (sx >= 0 && sx < W && sy >= 0 && sy < H) ? gridA[sy * W + sx] : 0;
                uint8_t actual = (uint8_t)(((x + ox) * 3 + (y + oy) * 5 + ((x & 7) == 0 ? 4 : 0)) & 7);
                gridB[y * W + x] = actual;
                xorGrid[y * W + x] = (sx >= 0 && sx < W && sy >= 0 && sy < H) ? (actual ^ base) : actual;
            }
        }
        size_t lenD = encodeRle(xorGrid, N, rleDelta);
        auto d = packet(rleDelta,lenD,W,H,ox,oy,true);
        RasterHeader delta={};
        assert(rasterCheck(d.data(),d.size(),delta,N,sizeof(bufB)));
        rasterDecode(d.data(),delta,bufB,bufA);
        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                assert(getPx(bufB, W, x, y) == gridB[y * W + x]);
            }
        }
    }
    printf("PASS: RasterContinuity full & delta decode\n");
    return 0;
}
