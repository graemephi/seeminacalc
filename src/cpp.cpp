#ifdef _MSC_VER
#pragma warning(push, 0)
#pragma warning(disable : 4706) // assignment within conditional expression
#endif

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#endif

#define IMGUI_USE_STB_SPRINTF
#include "cimgui/imgui/imgui.cpp"
#include "cimgui/imgui/imgui_demo.cpp"
#include "cimgui/imgui/imgui_draw.cpp"
#include "cimgui/imgui/imgui_tables.cpp"
#include "cimgui/imgui/imgui_widgets.cpp"

#include "cimgui/implot/implot.cpp"
#include "cimgui/implot/implot_items.cpp"
#include "cimgui/implot/implot_demo.cpp"

#define CIMGUI_NO_EXPORT
#include "cimgui/cimgui.cpp"
#include "cimgui/cimplot.cpp"

#include "thread.cpp"

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#ifdef __clang__
#pragma clang diagnostic pop
#endif
