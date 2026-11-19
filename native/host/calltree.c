// 진단: 한 프레임 동안의 게스트 호출 나무를 남긴다. `make trace`로 만든 build/ecpe-trace(tr.c를 -DTRACE_STACK로 번역)에서만 채워진다.
// ctrl 파일의 `calltree <파일>`: 다음 표시부터 그다음 표시까지(한 틱의 갱신 + 그리기)를 적는다.
// 같은 부모 아래 같은 함수는 한 줄로 합치고 횟수를 적는다. 그래픽 API는 잎으로 남기며 DrawString은 글(UTF-8 아님, CP949 그대로)과 좌표를 적는다.
#include "host.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct node { uint32_t addr; int count, first, next, api; char *note; } node_t; // first: 첫 자식, next: 형제
static node_t *nodes;
static int nnodes, cap, depth, cur_stack[512], state; // state 0 꺼짐, 1 다음 표시 대기, 2 기록 중
static char path[1024];

static int node_new(uint32_t addr, int api, const char *note) {
    if (nnodes == cap) { cap = cap ? cap * 2 : 4096; nodes = realloc(nodes, cap * sizeof *nodes); }
    nodes[nnodes] = (node_t){addr, 1, -1, -1, api, note ? strdup(note) : NULL};
    return nnodes++;
}

static int child(int parent, uint32_t addr, int api, const char *note) {
    int last = -1;
    for (int k = nodes[parent].first; k >= 0; last = k, k = nodes[k].next)
        if (!api && !nodes[k].api && nodes[k].addr == addr) { nodes[k].count++; return k; }
    int n = node_new(addr, api, note);
    if (last < 0) nodes[parent].first = n; else nodes[last].next = n;
    return n;
}

void ts_enter(uint32_t a) {
    if (state != 2) { depth++; return; }
    if (depth + 1 < 512) cur_stack[depth + 1] = child(cur_stack[depth < 511 ? depth : 511], a, 0, NULL);
    depth++;
}
void ts_leave(void) { depth--; }
// guest_call이 새 호출 사슬을 시작할 때(타이머, 입력) 깊이를 0으로 되돌린다.
void ts_root(void) { depth = 0; }

void ts_api(cpu_t *c, uint32_t table, uint32_t index) {
    if (state != 2 || nnodes > 200000) return;
    char note[400] = "";
    uint32_t *r = c->r;
    if (table == 3) {
        static const char *const N[] = {[8]="PutPixel",[9]="DrawLine",[10]="DrawRect",[11]="FillRect",[12]="CopyFrameBuffer",[14]="CopyArea",[15]="DrawArc",[16]="FillArc",[17]="DrawString",[21]="FlushLcd",[30]="GetStringWidth"};
        if (index >= sizeof N / sizeof *N || !N[index]) return;
        if (index == 17) {
            int len = (int)arg(c, 4);
            const char *s = gstr(r[3]);
            if (len < 0) len = (int)strlen(s);
            snprintf(note, sizeof note, "DrawString x=%d y=%d color=%04x \"%.*s\"", (int)r[1], (int)r[2], R32_(c, arg(c, 5) + 12) & 0xffff, len > 300 ? 300 : len, s);
        } else if (index == 30) {
            int len = (int)r[2];
            const char *s = gstr(r[1]);
            if (len < 0) len = (int)strlen(s);
            snprintf(note, sizeof note, "GetStringWidth \"%.*s\"", len > 300 ? 300 : len, s);
        } else if (index == 10 || index == 11) snprintf(note, sizeof note, "%s x=%d y=%d w=%d h=%d color=%04x", N[index], (int)r[1], (int)r[2], (int)r[3], (int)arg(c, 4), R32_(c, arg(c, 5) + 12) & 0xffff);
        else if (index == 12) snprintf(note, sizeof note, "CopyFrameBuffer dx=%d dy=%d w=%d h=%d sx=%d sy=%d", (int)r[1], (int)r[2], (int)r[3], (int)arg(c, 4), (int)arg(c, 6), (int)arg(c, 7));
        else snprintf(note, sizeof note, "%s %d %d %d", N[index], (int)r[1], (int)r[2], (int)r[3]);
    } else return;
    child(cur_stack[depth < 511 ? depth : 511], c->LR, 1, note);
}

static void dump(FILE *o, int n, int ind) {
    for (int k = nodes[n].first; k >= 0; k = nodes[k].next) {
        if (nodes[k].api) fprintf(o, "%*s* %s (lr %#x)\n", ind * 2, "", nodes[k].note, (nodes[k].addr & ~1u) - 0x100000);
        else if (nodes[k].count > 1) fprintf(o, "%*s%x x%d\n", ind * 2, "", nodes[k].addr, nodes[k].count);
        else fprintf(o, "%*s%x\n", ind * 2, "", nodes[k].addr);
        dump(o, k, ind + 1);
    }
}

void ts_request(const char *file) { snprintf(path, sizeof path, "%s", file); state = 1; }

void ts_present(void) {
    if (state == 1) {
        for (int i = 0; i < nnodes; i++) free(nodes[i].note);
        nnodes = 0;
        cur_stack[0] = node_new(0, 0, NULL);
        for (int i = 1; i < 512; i++) cur_stack[i] = 0;
        state = 2;
    } else if (state == 2) {
        FILE *o = fopen(path, "w");
        if (o) { dump(o, 0, 0); fclose(o); }
        state = 0;
    }
}
