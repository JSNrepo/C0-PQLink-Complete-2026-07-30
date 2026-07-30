/*
 * The prebuilt minimal Arduino core used by the reproducible AVR evidence
 * build exports C entry points.  Arduino sketches themselves are C++, so this
 * tiny bridge preserves the normal setup()/loop() sketch ABI.
 */
extern void _Z5setupv(void);
extern void _Z4loopv(void);

void setup(void)
{
    _Z5setupv();
}

void loop(void)
{
    _Z4loopv();
}
