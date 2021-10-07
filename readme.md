# [SeeMinaCalc](https://seeminacalc.glitch.me/)

A tool for visualising the (currently 171-dimensional) parameter space of `MinaCalc`, the difficulty calculator for the rhythm game [Etterna](https://github.com/etternagame/etterna). 

The intended target is WebAssembly. The only way to get data into the program is by drag and drop from the browser. Native executables are for development only.

## Building

For native binaries,

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
