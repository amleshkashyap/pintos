#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */

#define P_FXPOINT 17
#define Q_FXPOINT 14
#define F_FXPOINT (int) (1 << Q_FXPOINT)

#define GET_FXPOINT(n) (int64_t) (n * F_FXPOINT)
#define CONST_DIV_FXPOINT(x, y) (int64_t) (GET_FXPOINT (x)/y)


typedef int64_t fxpoint;

static inline fxpoint get_fxpoint (int n) { return (fxpoint) n * F_FXPOINT; }
static inline fxpoint add_fxpoint (fxpoint x, fxpoint y) { return x + y; }
static inline fxpoint sub_fxpoint (fxpoint x, fxpoint y) { return x - y; }
static inline fxpoint mult_fxpoint (fxpoint x, fxpoint y) { return ((int64_t) x) * y/F_FXPOINT; }
static inline fxpoint div_fxpoint (fxpoint x, fxpoint y) { return ((int64_t) x) * F_FXPOINT/y; }

static inline fxpoint add_fxpoint_int (fxpoint x, int n) { return x + get_fxpoint (n); }
static inline fxpoint sub_fxpoint_int (fxpoint x, int n) { return x - get_fxpoint (n); }
static inline fxpoint mult_fxpoint_int (fxpoint x, int n) { return (fxpoint) x * n; }
static inline fxpoint div_fxpoint_int (fxpoint x, int n) { return (fxpoint) x/n; }

static inline int fxtoi_zero (fxpoint x) { return (int) x/F_FXPOINT; }
static inline int fxtoi_nearest (fxpoint x)
{
  if (x >= 0) {
    return (int) (x + F_FXPOINT/2);
  }
  return (int) (x - F_FXPOINT/2); 
}

static fxpoint load_avg_coeff = CONST_DIV_FXPOINT (59, 60);
static fxpoint ready_threads_coeff = CONST_DIV_FXPOINT (1, 60);

static inline int calculate_priority (fxpoint recent_cpu, int nice)
{
  return PRI_MAX - fxtoi_zero (div_fxpoint_int (recent_cpu, 4)) - (nice * 2);
}

static inline fxpoint calculate_recent_cpu (fxpoint recent_cpu, fxpoint load_avg, int nice)
{
  load_avg = div_fxpoint(mult_fxpoint_int (load_avg, 2), add_fxpoint_int (mult_fxpoint_int (load_avg, 2), 1));
  return add_fxpoint_int ( mult_fxpoint (load_avg, recent_cpu), nice);
}

static inline fxpoint calculate_load_avg (fxpoint load_avg, int ready_threads)
{
  return add_fxpoint (mult_fxpoint (load_avg, load_avg_coeff), mult_fxpoint_int (ready_threads_coeff, ready_threads));
}

#endif /* threads/fixed-point.h */
