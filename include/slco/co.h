/*
 * This is a rudimentary implementation of stackless coroutines using
 * Duff's device.
 *
 * In retrospect this turned out a lot like Protothreads, and was
 * heavily influenced by Simon Tatham's "Coroutines in C," both of
 * which (ab)use Duff's device to retain state, though neither go into
 * great detail of blocking and resuming coroutines.
 *
 * Adam Dunkels, Oliver Schmidt, Thiemo Voigt, and Muneeb Ali. 2006.
 * Protothreads: simplifying event-driven programming of memory-
 * constrained embedded systems. In Proceedings of the 4th
 * international conference on Embedded networked sensor systems
 * (SenSys ’06). Association for Computing Machinery, New York, NY,
 * USA, 29–42. DOI:https://doi.org/10.1145/1182807.1182811
 *
 * https://www.chiark.greenend.org.uk/~sgtatham/coroutines.html
 */

/* XXX: Init take all arguments; Somehow validate macro inputs and order? */

#ifndef SLCO_CO_H_
#define SLCO_CO_H_

/*
 * A co_process is a generic callable structure. It points to the top-
 * most coroutine in a tree of coroutine calls.
 *
 * When a coroutine suspends (yields or is blocked), its resume point
 * is set to immediately after the yield call or immediately before
 * the blocking call. That is propagated from the deepest coroutine to
 * the first coroutine invoked from normal code. This means we can
 * resume the first coroutine and it will dive through the stack of all
 * nested coroutines to the point of suspension.
 *
 * TODO: Consider adding a buffer to this structure which would own the
 * coroutine itself. This could be the length of a cacheline, but might
 * not be useful if coroutines declare a lot of static child coroutines.
 */
typedef struct co_process_s co_process;

/*
 * The result of invoking a coroutine:
 *  CO_COMPLETE  - Exited normally
 *  CO_ERROR     - Exited abnormally, errno should be set to error
 *  CO_SCHEDULED - Suspended and rescheduled by an external function
 *  CO_WAITING   - Suspended but not rescheduled, recursively blocks up
 *                 call stack, effectively rescheduled for re-execution
 *  CO_YIELDED   - Yielding quantum, perhaps to return a value
 * Behavior is undefined if a coroutine that exited with CO_COMPLETE or
 * CO_ERROR is invoked or resumed.
 *
 * CO_WAITING usually does not make sense and CO_SCHEDULED is probably
 * what you want. I implemented CO_WAIT when I was building a
 * cooperatively "multi-threaded" example and adding a scheduler would
 * have been unecessarily complex. Usually you're only yielding up the
 * call stack when you're blocking on some external condition, in which
 * case you'd want to suspend re-execution until that condition is met
 * with a scheduler.
 */
typedef enum {
    CO_COMPLETE,
    CO_ERROR,
    CO_SCHEDULED,
    CO_WAITING,
    CO_YIELDED
} co_result;

/*
 * Function signature for coroutine implementation. Root co_process is
 * generated by CO_INVOKE and passed through all calls.
 */
typedef co_result (*co_fn)(void *xcptr, co_process);

/* See forward declaration */
struct co_process_s {
    co_fn fn;
    void *cptr;
};

/*
 * CO_DECL declares the cptr structure of a coroutine. Can be used to
 * forward declare the coroutine capture, or to declare a coroutine
 * variable e.g. ``CO_DECL(iota) monotonic_counter;``
 */
#define CO_DECL(NAME) struct co_##NAME##_cptr

/*
 * CO_ENTRY resolves the name of a coroutine's implementation function.
 * This is typically only used by the definitions of other macros which
 * invoke a coroutine.
 */
#define CO_ENTRY(NAME) co_##NAME##_do

/*
 * CO_BEGIN begins a coroutine definition. This macro resolves to both a
 * structure and function definition.
 *  NAME - The name of the coroutine
 *  CPTR - A brace-enclosed list of initializers which make up the
 *         coroutine capture. These persist between invocations
 */
#define CO_BEGIN(NAME, CPTR)                            \
    CO_DECL(NAME) { long line; struct CPTR; };          \
    co_result CO_ENTRY(NAME)(                           \
          void *xcptr, co_process proc) {               \
        CO_DECL(NAME) *NAME = xcptr;                    \
        long *cptrline = &NAME->line;                   \
        (void) proc;                                    \
        (void) cptrline;                                \
        switch (NAME->line) { case 0:

/*
 * CO_END ends a coroutine definition.
 */
#define CO_END } return CO_COMPLETE; }

/*
 * CO_INIT initializes a coroutine (sets line to zero) and accepts a
 * designator expression sequence, which translates directly to
 * initializing the coroutine capture variables.
 */
#define CO_INIT(NAME, PTR, ...) do {                    \
        *(PTR) = (CO_DECL(NAME)) {                      \
            .line = 0,                                  \
            __VA_ARGS__ };                              \
    } while (0)

/*
 * CO_INVOKE executes a coroutine from normal code. It handles setting
 * up the co_process.
 */
#define CO_INVOKE(NAME, CPTR) co_invoke_impl(CO_ENTRY(NAME), CPTR)

/*
 * CO_RESUME resumes an existing co_process.
 */
#define CO_RESUME(PROC) co_invoke_impl((PROC)->fn, (PROC)->cptr)

/*
 * CO_AWAIT resumes the passed coroutine. If the coroutine returns
 * CO_SCHEDULED the current coroutine returns that result up the call
 * stack after setting the current coroutine to resume immediately
 * before the call to the passed coroutine.
 *
 * If the passed coroutine returns CO_ERROR that is also forwarded up
 * the call stack.
 */
#define CO_AWAIT(NAME, PTR) do {                        \
        *cptrline = __LINE__; case __LINE__: ;          \
        co_result rc = CO_ENTRY(NAME)(PTR, proc);       \
        if (rc == CO_ERROR ||                           \
            rc == CO_SCHEDULED ||                       \
            rc == CO_WAITING) {                         \
            return rc;                                  \
        }                                               \
    } while (0)

/*
 * CO_AWAIT_EXTERN is identical to CO_AWAIT except instead of a
 * coroutine, the passed function is invoked with the current co_process
 * as the first argument and __VA_ARGS__ as subsequent. The passed
 * function must return co_result and match the function signature:
 * ``co_result (*)(co_process, ...)``
 */
#define CO_AWAIT_EXTERN(FN, ...) do {                   \
        *cptrline = __LINE__; case __LINE__: ;          \
        co_result rc = FN(proc, __VA_ARGS__);           \
        if (rc == CO_ERROR ||                           \
            rc == CO_SCHEDULED ||                       \
            rc == CO_WAITING) {                         \
            return rc;                                  \
        }                                               \
    } while (0)

/*
 * CO_YIELD sets the current coroutine to resume immediately after this
 * macro, and returns CO_YIELDED.
 */
#define CO_YIELD *cptrline = __LINE__; return CO_YIELDED; case __LINE__: ;

/*
 * CO_WAIT performs the same as CO_YIELD, except CO_WAITING is returned,
 * which suspends execution up the entire call stack. See comments on
 * co_result, this is not typically something you want!
 */
#define CO_WAIT *cptrline = __LINE__; return CO_WAITING; case __LINE__: ;

inline static co_result
co_invoke_impl(co_fn fn, void *cptr)
{
    co_process proc = { .fn = fn, .cptr = cptr };
    return (*fn)(cptr, proc);
}

#endif /* SLCO_CO_H_ */
