#include <stdint.h>
typedef __SIZE_TYPE__ size_t;
typedef struct FILE FILE;
typedef struct { const char *a; void *b; const char *c; void *d; } DI;
void* malloc(size_t); void* calloc(size_t,size_t); void free(void*);
FILE* fopen(const char*,const char*); size_t fread(void*,size_t,size_t,FILE*); int fclose(FILE*);
void* dlopen(const char*,int); void* dlsym(void*,const char*); int dladdr(const void*,DI*);
int mprotect(void*,size_t,int);
#define SQ __builtin_sqrtf
#define FA __builtin_fabsf
#define EXP __attribute__((visibility("default")))
#define MAXR 4096
typedef struct { float x, y, z, rhw; uint32_t c; float u, v; } V;
typedef int (*Rw)(int, V*, int, uint16_t*, int);
static Rw orig; static uintptr_t base;
static int enabled = 1, calls = 0, locked = 0;
static float bx0 = 1e9f, bx1 = -1e9f, by0 = 1e9f, by1 = -1e9f, cx, cy, h = 1;
static float rt[MAXR * 2], RW = 4.f; static int rn = 0, rcur = 0; static uint8_t RC[3] = {0, 207, 255};
static void (*tfRW)(float*, const float*);
static void (*oPoly)(float,float,float,float,float,float,float,float,void*);
static void (*oDraw)(void*, void*, void*);

static int menuOn(void) {
    if (!base) return 0;
    volatile uint8_t* p = (volatile uint8_t*)(base + 0x96b514);
    if (p[0] || p[1]) return 1;
    static uint8_t* gm = 0;
    if (!gm) { void* g = dlopen("libGTASA.so", 2); gm = g ? (uint8_t*)dlsym(g, "gMobileMenu") : 0; }
    return gm && *(volatile uint32_t*)(gm + 0x24) == 1;
}
static int hk(int t, V* v, int n, uint16_t* idx, int ni) {
    if (!locked && !menuOn()) {
        uintptr_t lr = (uintptr_t)__builtin_return_address(0) & ~(uintptr_t)1;
        if (lr == base + 0x443aea) {
            int nv = ni / 3 + 2; if (nv > n) nv = n;
            for (int i = 0; i < nv; i++) {
                if (v[i].x < bx0) bx0 = v[i].x;
                if (v[i].x > bx1) bx1 = v[i].x;
                if (v[i].y < by0) by0 = v[i].y;
                if (v[i].y > by1) by1 = v[i].y;
            }
            if (++calls >= 40) { locked = 1; cx = (bx0 + bx1) / 2; cy = (by0 + by1) / 2; h = (bx1 - bx0) / 2; }
        }
    }
    return orig(t, v, n, idx, ni);
}
static int clipC(float* u0, float* q0, float* u1, float* q1) {
    const float R2 = 0.94f;
    float x = *u0, y = *q0, dx = *u1 - x, dy = *q1 - y;
    int i0 = x * x + y * y <= R2, i1 = (*u1) * (*u1) + (*q1) * (*q1) <= R2;
    if (i0 && i1) return 1;
    float a = dx * dx + dy * dy; if (a < 1e-9f) return 0;
    float b = 2 * (x * dx + y * dy), c = x * x + y * y - R2, d = b * b - 4 * a * c;
    if (d < 0) return 0;
    float sd = SQ(d), t0 = (-b - sd) / (2 * a), t1 = (-b + sd) / (2 * a);
    if (t0 < 0) t0 = 0; if (t1 > 1) t1 = 1; if (t0 > t1) return 0;
    *u0 = x + t0 * dx; *q0 = y + t0 * dy; *u1 = x + t1 * dx; *q1 = y + t1 * dy; return 1;
}
static void segDraw(float u0, float q0, float u1, float q1) {
    float x0 = cx + h * u0, y0 = cy - h * q0, x1 = cx + h * u1, y1 = cy - h * q1;
    float dx = x1 - x0, dy = y1 - y0, len = SQ(dx * dx + dy * dy); if (len < 0.3f) return;
    float w = RW * 0.5f, nx = -dy / len * w, ny = dx / len * w;
    uint8_t col[4] = {RC[0], RC[1], RC[2], 255};
    oPoly(x0 + nx, y0 + ny, x0 - nx, y0 - ny, x1 + nx, y1 + ny, x1 - nx, y1 - ny, col);
}
static void drawRoute(void) {
    if (!oPoly || !tfRW || rn <= rcur) return;
    float in[2], out[2], pu = 0, pq = 0;
    for (int i = rcur; i < rn; i++) {
        in[0] = rt[2 * i]; in[1] = rt[2 * i + 1]; tfRW(out, in);
        float a0 = pu, b0 = pq, a1 = out[0], b1 = out[1];
        if (clipC(&a0, &b0, &a1, &b1)) segDraw(a0, b0, a1, b1);
        pu = out[0]; pq = out[1];
    }
    if (pu * pu + pq * pq <= 0.94f) {
        float x = cx + h * pu, y = cy - h * pq, d = 5.f; uint8_t c[4] = {255, 40, 40, 255};
        oPoly(x - d, y - d, x + d, y - d, x - d, y + d, x + d, y + d, c);
    }
}
static void advRoute(void) {
    if (!base || rn <= rcur) return;
    float* o = (float*)(base + 0x994da4);
    while (rcur < rn - 1) {
        float dx = rt[2 * rcur] - o[0], dy = rt[2 * rcur + 1] - o[1];
        if (dx * dx + dy * dy < 225.f) rcur++; else break;
    }
    if (rcur == rn - 1) { float dx = rt[2 * rcur] - o[0], dy = rt[2 * rcur + 1] - o[1]; if (dx * dx + dy * dy < 225.f) { rn = 0; rcur = 0; } }
}
static void hDraw(void* th, void* rc, void* col) {
    uintptr_t o = ((uintptr_t)__builtin_return_address(0) & ~(uintptr_t)1) - base;
    if (o == 0x437fa4 && locked && enabled && !menuOn()) { advRoute(); drawRoute(); }
    oDraw(th, rc, col);
}
/* ---- nodes + A* ---- */
typedef struct { int32_t n; float x, y, z; int16_t c, veh, l, a, A, nc; int32_t nid[8]; int16_t na[8]; } ND;
_Static_assert(sizeof(ND) == 76, "ND harus 76 byte");
__asm__(".section .rodata\n.balign 4\n.global nb_s\n.hidden nb_s\nnb_s:\n.incbin \"nodes.bin\"\n.global nb_e\n.hidden nb_e\nnb_e:\n.balign 4\n.text\n");
extern const char nb_s[], nb_e[];
static ND* nd = 0; static int ndc = 0;
typedef struct { float f; int i; } HE;
static int *hidx = 0, *cf = 0, *pbuf = 0; static int hmask = 0, hn = 0, hcap = 0; static float* gs = 0; static HE* hp = 0;
EXP int rl_nodes_load(const char* path) {
    (void)path;
    if (nd) return ndc;
    uint32_t cnt = *(const uint32_t*)nb_s;
    if (cnt == 0 || cnt > 500000 || (size_t)(nb_e - nb_s) < 4 + (size_t)cnt * sizeof(ND)) return -2;
    nd = (ND*)(nb_s + 4); ndc = (int)cnt; return ndc;
}
static int nearest(float x, float y, float z) {
    int best = -1; float bd = 1e30f;
    for (int i = 0; i < ndc; i++) {
        if (nd[i].veh != 1) continue;
        float dx = nd[i].x - x, dy = nd[i].y - y, dz = nd[i].z - z, d = dx * dx + dy * dy + dz * dz;
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}
static uint32_t hk2(int a, int n) { uint32_t x = ((uint32_t)(uint16_t)a << 16) ^ (uint32_t)n; x *= 2654435761u; return x ^ (x >> 15); }
static void buildHash(void) {
    if (hidx || !ndc) return;
    int sz = 1; while (sz < ndc * 2) sz <<= 1;
    int* t = (int*)calloc(sz, sizeof(int)); if (!t) return;
    for (int i = 0; i < ndc; i++) { uint32_t k = hk2(nd[i].a, nd[i].n) & (sz - 1); while (t[k]) k = (k + 1) & (sz - 1); t[k] = i + 1; }
    hmask = sz - 1; hidx = t;
}
static int nodeFind(int n, int a) {
    if (!hidx) return -1;
    uint32_t k = hk2(a, n) & hmask;
    while (hidx[k]) { ND* p = &nd[hidx[k] - 1]; if (p->n == n && p->a == a) return hidx[k] - 1; k = (k + 1) & hmask; }
    return -1;
}
static float nd3(int a, int b) { float dx = nd[a].x - nd[b].x, dy = nd[a].y - nd[b].y, dz = nd[a].z - nd[b].z; return SQ(dx * dx + dy * dy + dz * dz); }
static void hpush(float f, int i) { if (hn >= hcap) return; int x = hn++; while (x > 0) { int p = (x - 1) / 2; if (hp[p].f <= f) break; hp[x] = hp[p]; x = p; } hp[x].f = f; hp[x].i = i; }
static HE hpop(void) {
    HE top = hp[0], last = hp[--hn]; int x = 0;
    for (;;) { int l = 2 * x + 1; if (l >= hn) break; int r = l + 1, s = (r < hn && hp[r].f < hp[l].f) ? r : l; if (hp[s].f >= last.f) break; hp[x] = hp[s]; x = s; }
    if (hn > 0) hp[x] = last;
    return top;
}
static int navAlloc(void) {
    if (gs) return 1;
    gs = (float*)malloc(ndc * sizeof(float)); cf = (int*)malloc(ndc * sizeof(int)); pbuf = (int*)malloc(ndc * sizeof(int));
    hcap = ndc * 8 + 16; hp = (HE*)malloc(hcap * sizeof(HE));
    if (!gs || !cf || !pbuf || !hp) { free(gs); free(cf); free(pbuf); free(hp); gs = 0; cf = 0; pbuf = 0; hp = 0; return 0; }
    return 1;
}
EXP int rl_nav(float px, float py, float pz, float tx, float ty, float tz) {
    if (!nd) return -1;
    buildHash(); if (!hidx) return -2;
    if (!navAlloc()) return -3;
    int s = nearest(px, py, pz), g = nearest(tx, ty, tz);
    if (s < 0 || g < 0) return -4;
    for (int i = 0; i < ndc; i++) { gs[i] = 1e30f; cf[i] = -1; }
    hn = 0; gs[s] = 0; hpush(nd3(s, g), s);
    int found = 0;
    while (hn > 0) {
        HE e = hpop(); int u = e.i;
        if (e.f > gs[u] + nd3(u, g) + 0.01f) continue;
        if (u == g) { found = 1; break; }
        ND* p = &nd[u];
        for (int j = 0; j < p->nc && j < 8; j++) {
            int v = nodeFind(p->nid[j], p->na[j]); if (v < 0 || nd[v].veh != 1) continue;
            float ng = gs[u] + nd3(u, v);
            if (ng < gs[v]) { gs[v] = ng; cf[v] = u; hpush(ng + nd3(v, g), v); }
        }
    }
    if (!found) return -5;
    int len = 0; for (int v = g; v >= 0 && len < ndc; v = cf[v]) pbuf[len++] = v;
    int cnt = len < MAXR ? len : MAXR;
    rn = 0;
    for (int i = 0; i < cnt; i++) { int v = pbuf[len - 1 - i]; rt[2 * i] = nd[v].x; rt[2 * i + 1] = nd[v].y; }
    rcur = 0; rn = cnt; return cnt;
}
EXP void rl_enable(int e) { enabled = e; }
EXP void rl_route_clear(void) { rn = 0; rcur = 0; }
EXP void rl_route_style(float w, int r, int g, int b) { if (w < 1.f) w = 1.f; if (w > 20.f) w = 20.f; RW = w; RC[0] = r; RC[1] = g; RC[2] = b; }
EXP int rl_route_n(void) { return rn; }
EXP int rl_route_pos(void) { return rcur; }
EXP const float* rl_route_ptr(void) { return rt; }
/* ---- hook ---- */
static uint32_t dec(uint32_t i) { uint32_t r = (i >> 8) & 0xf, v = i & 0xff; return r ? ((v >> (2 * r)) | (v << (32 - 2 * r))) : v; }
static int hookPLT(uintptr_t off, const char* sym, void* hkf, void** o) {
    void* g = dlopen("libGTASA.so", 2); void* s = g ? dlsym(g, sym) : 0; if (!s || !base) return -2;
    uint32_t* p = (uint32_t*)(base + off);
    if ((p[0] & 0xfffff000) != 0xe28fc000 || (p[1] & 0xfffff000) != 0xe28cc000 || (p[2] & 0xfffff000) != 0xe5bcf000) return -3;
    uintptr_t got = base + off + 8 + dec(p[0]); got += dec(p[1]); got += p[2] & 0xfff;
    *o = (void*)*(uintptr_t*)got;
    if (hkf) { mprotect((void*)(got & ~(uintptr_t)0xfff), 4096, 3); *(uintptr_t*)got = (uintptr_t)hkf; }
    return 0;
}
__attribute__((constructor)) static void init(void) {
    void* g = dlopen("libGTASA.so", 2);
    void* s = g ? dlsym(g, "_Z35RwIm2DRenderIndexedPrimitive_BUGFIX15RwPrimitiveTypeP14RwOpenGLVertexiPti") : 0;
    DI di; if (s && dladdr(s, &di)) base = (uintptr_t)di.b;
    tfRW = (void(*)(float*, const float*))(base + 0x43f694 + 1);
    hookPLT(0x1901a4, "_Z35RwIm2DRenderIndexedPrimitive_BUGFIX15RwPrimitiveTypeP14RwOpenGLVertexiPti", (void*)hk, (void**)&orig);
    hookPLT(0x19d530, "_ZN9CSprite2d4DrawERK5CRectRK5CRGBA", (void*)hDraw, (void**)&oDraw);
    hookPLT(0x1a0794, "_ZN9CSprite2d13Draw2DPolygonEffffffffRK5CRGBA", 0, (void**)&oPoly);
}
