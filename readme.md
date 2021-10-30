# [SeeMinaCalc](https://seeminacalc.glitch.me/)

A tool for visualising the (currently 321-dimensional) parameter space of `MinaCalc`, the difficulty calculator for the rhythm game [Etterna](https://github.com/etternagame/etterna).

## Optimizer

There is an experimental optimizer for tuning parameters. It's intended to be used interactively, rather than being a global optimizer. Algorithm: stochastic gradient descent with AMSGrad. It's a black box problem, so finite differences, with randomized coordinate descent with a multiplicative weights update step to choose coordinates to descend along (rather than computing all partial derivatives every step). It can optimise over inline floating point values in the source code, not just those exposed as parameters by MinaCalc at run-time.

## Building

Only windows, with clang rather than msvc, and emscripten are supported. However, the ninja/makefiles are very simple so if you're comfortable with C/C++ it shouldn't be hard to get it running for other systems/compilers.

For native binaries using clang,

```
ninja [release]
```

For emscripten,

```
make
```

with emscripten's environment variables set up. Then serve './web/' on some server.

## License

MIT.

### Licenses

- [Etterna](https://github.com/etternagame/etterna) (Etterna, MIT License).
- [Sokol](https://github.com/floooh/sokol) (Andre Weissflog, zlib/libpng license). 
- [Dear Imgui](https://github.com/ocornut/imgui) (Omar Cornut, MIT License). 
- [cimgui](https://github.com/cimgui/cimgui) (Stephan Dilly/Victor Bombi, MIT License).
- [ImPlot](https://github.com/epezent/implot) (Evan Pezent, MIT License).
