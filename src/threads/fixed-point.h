#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */

#define P_FXPOINT 17
#define Q_FXPOINT 14
#define F_FXPOINT (int64_t) (1 << Q_FXPOINT)

#define CONST_DIV_FXPOINT(x, y) ((int64_t) x * F_FXPOINT)/y


typedef int64_t fxpoint;

static inline fxpoint get_fxpoint (int n) { return ((fxpoint) n) * F_FXPOINT; }
static inline fxpoint add_fxpoint (fxpoint x, fxpoint y) { return x + y; }
static inline fxpoint sub_fxpoint (fxpoint x, fxpoint y) { return x - y; }
static inline fxpoint mult_fxpoint (fxpoint x, fxpoint y) { return ((fxpoint) x) * y / F_FXPOINT; }
static inline fxpoint div_fxpoint (fxpoint x, fxpoint y) { return ((fxpoint) x * F_FXPOINT)/y; }

static inline fxpoint add_fxpoint_int (fxpoint x, int n) { return x + get_fxpoint (n); }
static inline fxpoint sub_fxpoint_int (fxpoint x, int n) { return x - get_fxpoint (n); }
static inline fxpoint mult_fxpoint_int (fxpoint x, int n) { return x * ((fxpoint) n); }
static inline fxpoint div_fxpoint_int (fxpoint x, int n) { return (fxpoint) (x / ((fxpoint) n)); }

static inline int fxtoi_zero (fxpoint x) { return (int) (x / F_FXPOINT); }
static inline int fxtoi_nearest (fxpoint x)
{
  if (x == 0) return 0;
  if (x > 0) return (int) ( x + F_FXPOINT / 2 ) / F_FXPOINT;
  return (int) ( x - F_FXPOINT / 2 ) / F_FXPOINT;
}

static fxpoint load_avg_coeff = CONST_DIV_FXPOINT (59, 60);
static fxpoint ready_threads_coeff = CONST_DIV_FXPOINT (1, 60);
static fxpoint recent_cpu_coeff = CONST_DIV_FXPOINT (1, 4);

static inline int calculate_priority (fxpoint recent_cpu, int nice)
{
  int c = fxtoi_nearest (mult_fxpoint (recent_cpu, recent_cpu_coeff));
  int res = PRI_MAX - c - (nice * 2);
  if (res < PRI_MIN) return PRI_MIN;
  if (res > PRI_MAX) return PRI_MAX;
  return res;
}

static inline fxpoint calculate_recent_cpu (fxpoint recent_cpu, fxpoint load_avg, int nice)
{
  if (recent_cpu == 0) {
    return get_fxpoint (nice);
  }

  fxpoint c1 = mult_fxpoint_int (load_avg, 2);
  fxpoint c2 = add_fxpoint_int (c1, 1);
  fxpoint c3 = div_fxpoint (c1, c2);
  load_avg = mult_fxpoint (c3, recent_cpu);
  return add_fxpoint_int (load_avg, nice);
}

static inline fxpoint calculate_load_avg (fxpoint load_avg, int ready_threads)
{
  fxpoint c1 = mult_fxpoint (load_avg, load_avg_coeff);
  fxpoint c2 = mult_fxpoint_int (ready_threads_coeff, ready_threads);
  return (c1 + c2);
}

#endif /* threads/fixed-point.h */
