#if defined(__EMSCRIPTEN__)

// 99% of the time we have zero work to do and can just persist the last frame.
// On the desktop version, we can just exit the frame cb early, and it's no big
// deal. In the browser, this isn't as big a win, and we still burn cpu cycles
// for no reason. So we tell emscripten to stop calling requestAnimationFrame
// over and over when we have no work. Unfortunately, sokol isn't setup to do
// this. Fortunately, we only need to reimplement two functions to access the
// emscripten functionality: main and _sapp_emsc_run.

void pause_main_loop(void)
{
    emscripten_pause_main_loop();
}

void resume_main_loop(void)
{
    emscripten_resume_main_loop();
}

void main_loop(void)
{
    // Arguments are unused by _sapp_emsc_run.
    _sapp_emsc_frame(0, 0);
}

_SOKOL_PRIVATE void seeminacalc_sapp_emsc_run_with_main_loop(const sapp_desc* desc) {
    _sapp_init_state(desc);
    sapp_js_pointer_init(&_sapp.html5_canvas_selector[1]);
    double w, h;
    if (_sapp.desc.html5_canvas_resize) {
        w = (double) _sapp.desc.width;
        h = (double) _sapp.desc.height;
    }
    else {
        emscripten_get_element_css_size(_sapp.html5_canvas_selector, &w, &h);
        emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, 0, false, _sapp_emsc_size_changed);
    }
    if (_sapp.desc.high_dpi) {
        _sapp.dpi_scale = emscripten_get_device_pixel_ratio();
    }
    _sapp.window_width = (int) w;
    _sapp.window_height = (int) h;
    _sapp.framebuffer_width = (int) (w * _sapp.dpi_scale);
    _sapp.framebuffer_height = (int) (h * _sapp.dpi_scale);
    emscripten_set_canvas_element_size(_sapp.html5_canvas_selector, _sapp.framebuffer_width, _sapp.framebuffer_height);
    #if defined(SOKOL_GLES2) || defined(SOKOL_GLES3)
        _sapp_emsc_webgl_init();
    #elif defined(SOKOL_WGPU)
        sapp_js_wgpu_init();
    #endif
    _sapp.valid = true;
    _sapp_emsc_register_eventhandlers();
    sapp_set_icon(&desc->icon);

    /* start the frame loop */
    // The only change in this function from _sapp_emsc_run
    emscripten_set_main_loop(main_loop, 0, 0);

    /* NOT A BUG: do not call _sapp_discard_state() here, instead this is
       called in _sapp_emsc_frame() when the application is ordered to quit
     */
}

int main(int argc, char* argv[]) {
    sapp_desc sokol_main(int argc, char **argv);
    sapp_desc desc = sokol_main(argc, argv);
    seeminacalc_sapp_emsc_run_with_main_loop(&desc);
    return 0;
}

#else
void pause_main_loop(void)
{
}

void resume_main_loop(void)
{
}
#endif
