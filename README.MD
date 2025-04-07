# os-1000

This is a repo implementing the guide [OS in 1000 Lines of Code](https://operating-system-in-1000-lines.vercel.app/en)

## Running the project

_(this will probably change in the future to a makefile)_
```bash
./run.sh
```

## Tweaks I made so far

- Different type definitions, ie `u8` instad of `uint8_t`
- Using `volatile` instead of `__volatile__` (they seemingly do the same thing)
- Using `#define asm __asm__` to improve readability

## Todo
- Complete the guide
- Improve project structure
