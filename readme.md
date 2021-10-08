# [SeeMinaCalc](https://seeminacalc.glitch.me/)

A tool for visualising the (currently 323-dimensional) parameter space of `MinaCalc`, the difficulty calculator for the rhythm game [Etterna](https://github.com/etternagame/etterna). 

## Building

Only clang on windows and emscripten are supported. However, the ninja/makefiles are very simple so if you're comfortable with C/C++ it shouldn't be hard to get it running for other systems / compilers.

For native binaries using clang,

```
ninja [release]
```

For emscripten (don't ask), 

```
make
```

with emscripten's environment variables set up. Then server './web/' on some server.

## License

MIT.

### Licenses

- [Etterna](https://github.com/etternagame/etterna) (Etterna, MIT License).
- [Sokol](https://github.com/floooh/sokol) (Andre Weissflog, zlib/libpng license). 
- [Dear Imgui](https://github.com/ocornut/imgui) (Omar Cornut, MIT License). 
- [cimgui](https://github.com/cimgui/cimgui) (Stephan Dilly/Victor Bombi, MIT License).
- [ImPlot](https://github.com/epezent/implot) (Evan Pezent, MIT License).
- Noto (Google, SIL Open Font License).
